#!/usr/bin/env python3
"""mhuixs-cli —— 连上 Mhuixs 服务端、一行一条命令的命令行客户端。

它只做三件事：切词、发命令、排版结果。协议编解码一行都不重复实现，
全部复用包内的 ``_protocol``（那份同时是 MHP/1 的参考实现）。

    mhuixs-cli                                    # 连 127.0.0.1:18185，进交互
    mhuixs-cli --host 10.0.0.5                    # 换地址
    mhuixs-cli 'llen todo'                        # 给命令就直接跑完退出
    mhuixs-cli - < script.mhx                     # 从 stdin / 管道逐行读

没安装时在源码目录 `client/python` 下用 ``python -m mhuixs``，参数完全一样。

交互模式下 `:help` 看用法，`:quit` 退出。

令牌按三处顺序回落（与原生服务端读令牌的顺序一致）：
    1. --token 命令行参数
    2. 环境变量 MHUIXS_TOKEN
    3. 配置文件里的 authtoken（可执行文件同目录的 Mhuixs.config 优先，
       其次是 ~/.mhuixs/Mhuixs.config）

切词规则与原生文本模式一致：空格分词，单双引号包裹的参数按**字符串**处理
（对应协议里的 + 元素）。所以值里有空格、或者想存 "123" 这种数字样子的
字符串时，都要加引号。

选项（--host / --format 等）要写在命令前面 —— argparse 会把第一条命令之后
的内容全当命令，`cmd --json` 会报 "unrecognized arguments"。
"""

import argparse
import cmd
import json
import os
import sys

from ._client import Mhuixs
from ._errors import AuthenticationError, ConnectionError, MhuixsError
from ._protocol import MAX_ARG_BYTES, MAX_ARGS, Forced
from ._version import __version__

DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORT = 18185

# 输出版式：
#   value   只要值（默认）—— 数字就是数字，LIST 一行一项，方便直接管道给别的程序
#   pretty  带 OK / ERR 前缀，观感与原生 mhuixs 交互模式一致
#   text    只要 JSON 里的 text 字段（服务端那串人类可读文本）
#   json    完整 JSON 文档，字段齐全，给调试和程序用
MODES = ("value", "pretty", "text", "json")

_QUOTES = ("'", '"')


class CliError(Exception):
    """客户端自己的错（用法不对、参数超限），与协议错分开报。"""


class QuoteOpen(CliError):
    """引号还没闭合 —— 唯一一种"多读一行就可能有救"的切词错误。

    单独分出来，是为了让续行逻辑只对真正"半行"的情况生效：参数太多、
    参数超长这类错误是确定性的，再攒一万行也不会变合法。
    """


# ---------------------------------------------------------------- 切词

def tokenize(line):
    """把一行切成 [(文本, 是否被引号包裹), ...]。

    第二个元素是引号语义：被引号包过的参数对应协议里的 + 元素（强制当字符串），
    没包的走 $ 元素（服务端按内容自动判定数字还是字符串）。

    手工扫一遍而不是用 shlex，是因为引号语义必须留下来 —— shlex 只给切好的词，
    事后再去原行里"找"引号在重复词、行内 '#' 上都会对错。

    原生文本模式把行内 '#' 之后的内容也当注释，这里保持一致：引号外的 '#' 一律
    开始注释，引号内的 '#' 是普通字符。代价是"值里想带 # 就得加引号"，
    这个取舍与原生完全相同。

    引号未闭合抛 CliError（多行值的中途，调用方应继续读下一行）。
    """
    body = line
    quote = None
    i = 0
    while i < len(line):
        c = line[i]
        if quote:
            if c == quote:
                quote = None
        elif c in _QUOTES:
            quote = c
        elif c == "#":
            body = line[:i]
            break
        i += 1

    tokens = []
    n = len(body)
    i = 0
    while i < n:
        while i < n and body[i].isspace():
            i += 1
        if i >= n:
            break

        quoted = False
        buf = []
        while i < n and not body[i].isspace():
            c = body[i]
            if c in _QUOTES:
                q = c
                i += 1
                closed = False
                while i < n:
                    if body[i] == q:
                        closed = True
                        i += 1
                        break
                    buf.append(body[i])
                    i += 1
                if not closed:
                    raise QuoteOpen("引号没有闭合")
                quoted = True
            else:
                buf.append(c)
                i += 1
        tokens.append(("".join(buf), quoted))

    return tokens


def split_line(line):
    """把一行按本客户端的切词规则切开，返回 tokenize() 的结果。

    单独留出来给"只要词、不要协议参数"的调用方（压测脚本造数据就是这种），
    免得它们各自复制一份切词口径。
    """
    pairs = tokenize(line)
    if not pairs:
        return []

    if len(pairs) > MAX_ARGS:
        raise CliError("参数太多：%d 个，上限 %d 个" % (len(pairs), MAX_ARGS))

    for i, (text, _) in enumerate(pairs):
        if len(text.encode("utf-8")) > MAX_ARG_BYTES:
            raise CliError("第 %d 个参数超过 %d 字节上限（现在 %d 字节）"
                           % (i + 1, MAX_ARG_BYTES, len(text.encode("utf-8"))))
    return pairs


def to_protocol_args(pairs):
    """把 split_line() 的结果转成协议参数：命令名不带引号语义，其余按引号决定。"""
    return [Forced(text) if (quoted and i > 0) else text
            for i, (text, quoted) in enumerate(pairs)]


def parse_line(line):
    """切成协议参数：第一个词是命令名，其余按引号决定是否 Forced。"""
    return to_protocol_args(split_line(line))


class CommandFeed:
    """把"行"攒成"命令"：处理引号跨行、注释、空行。

    交互模式和管道模式共用它，两边的切词与续行口径就不会漂。

    引号没闭合时攒着等下一行，但**续行按空格拼接** —— 不是疏忽，是协议决定的：
    + 元素靠 CRLF 划界（src/proto.h），值里不可能带换行，所以"多行字符串"
    在这条路上不成立。要带换行的值，得走命令层自己的路径，不是这里。
    """

    def __init__(self):
        self.pending = ""

    def feed(self, raw):
        """喂一行（不含换行符）。返回参数列表；还没凑齐一条就返回 None。"""
        if not raw.strip():
            return None                                  # 空行
        if not self.pending and raw.lstrip().startswith("#"):
            return None                                  # 整行注释

        text = (self.pending + " " + raw.strip()).strip() if self.pending \
            else raw.strip()
        try:
            args = parse_line(text)
        except QuoteOpen:
            self.pending = text                          # 半行，等下一行凑齐
            return None

        # 其它 CliError（参数太多 / 超长）是确定性的，直接往外抛，
        # 让调用方当场报错，而不是攒到输入结束才说
        self.pending = ""
        return args

    def flush(self):
        """收尾：返回还没凑齐的半行（没有则 None）。"""
        left, self.pending = self.pending, ""
        return left or None


# ---------------------------------------------------------------- 令牌与配置

def _config_candidates():
    """配置文件的查找顺序，与原生程序启动时一致。

    安装成命令时 ``sys.argv[0]`` 是启动脚本的路径；``python -m mhuixs``
    时是包内 ``__main__.py`` 的路径。两种情况下都先看「启动位置旁边」，
    再回落到 ``~/.mhuixs/Mhuixs.config``。
    """
    here = os.path.dirname(os.path.abspath(sys.argv[0] or __file__))
    return [
        os.path.join(here, "Mhuixs.config"),
        os.path.join(os.path.expanduser("~"), ".mhuixs", "Mhuixs.config"),
    ]


def read_config():
    """读出配置文件里的键值（不存在就返回空 dict）。

    格式与原生 env.c 一致：一行一个「键 值」，'#' 开头是注释，空白断词。
    """
    cfg = {}
    for path in _config_candidates():
        try:
            with open(path, "r", encoding="utf-8", errors="replace") as fp:
                for raw in fp:
                    line = raw.strip()
                    if not line or line.startswith("#"):
                        continue
                    parts = line.split(None, 1)
                    if len(parts) != 2:
                        continue
                    cfg.setdefault(parts[0], parts[1].strip())
        except OSError:
            continue
    return cfg


def resolve_token(explicit):
    """--token > MHUIXS_TOKEN > 配置文件 authtoken。都没有返回 None。"""
    if explicit:
        return explicit
    env = os.environ.get("MHUIXS_TOKEN")
    if env:
        return env
    return read_config().get("authtoken") or None


def resolve_host_port(explicit_host, explicit_port):
    cfg = read_config()
    host = (explicit_host or os.environ.get("MHUIXS_HOST")
            or cfg.get("bind") or DEFAULT_HOST)
    port = explicit_port
    if port is None:
        raw = os.environ.get("MHUIXS_PORT") or cfg.get("port")
        try:
            port = int(raw) if raw else DEFAULT_PORT
        except ValueError:
            port = DEFAULT_PORT
    return host, port


# ---------------------------------------------------------------- 结果排版

def render(result, mode):
    """把服务端回的 JSON 文档排成文本。返回 (文本, 是否成功)；文本不带结尾换行。"""
    ok = bool(result.get("ok"))
    code = result.get("code", "ok")
    text = result.get("text", "")

    if mode == "json":
        return json.dumps(result, ensure_ascii=False), ok
    if mode == "text":
        return text, ok

    if not ok:
        return _fail_text(result), False

    if mode == "pretty":
        # 服务端的 text 就是原生交互模式那一行（多数命令自带 "OK" 前缀），
        # 本地不再补，否则会打成 "OK OK 2"。没前缀的命令（如 info）保持原样。
        return text, True

    # mode == "value"：成功时只给值
    if "kind" not in result:
        return text, True
    return _render_value(result), True


def _fail_text(result):
    """失败响应排成一行。失败一律带前缀，免得错误混进正常输出里被下游当数据。

    服务端的 text 自己带 "ERR" 或稳定 token 时不重复加 —— 原生文本模式
    本身就是这个前缀，加上去会变成 "ERR ERR ..."。
    """
    code = result.get("code", "failed")
    body = result.get("text") or code
    if body.startswith("ERR") or body.startswith(code):
        return body
    return "ERR %s" % body


def _render_value(result):
    kind = result.get("kind")
    value = result.get("value")

    if kind == "list":
        items = [str(v) for v in (value or [])]
        if items and result.get("truncated"):
            items.append("（已截断，只显示了前 %d 项）" % len(items))
        return "\n".join(items)
    if kind == "bool":
        return "true" if value else "false"
    if kind == "null":
        return ""
    if kind in ("int", "num", "str"):
        return str(value)
    return result.get("text", "")


# ---------------------------------------------------------------- 交互界面

class Shell(cmd.Cmd):
    """交互式会话：一行一条命令，元命令在本地处理，其余发服务端。"""

    prompt = "mhuixs> "

    # 元命令在本地解释，绝不发给服务端 —— 服务端的元命令（:save 之类）得由
    # 我们显式转成一次调用，随行发过去只会得到 unknown_command。
    # 名字（不带冒号）-> 处理方法名
    META = {
        "help": "help", "?": "help",
        "quit": "quit", "q": "quit", "exit": "quit",
        "format": "format", "mode": "format",
        "fail": "fail",
        "run": "run",
        "commands": "commands",
        "ping": "ping", "status": "status",
        "save": "save", "shutdown": "shutdown",
    }

    def __init__(self, db, mode="value", target="", strict=False):
        # 老版本 Python 上 cmd.Cmd 不是新式类，显式初始化最保险
        cmd.Cmd.__init__(self, stdout=sys.stdout)
        self.db = db
        self.mode = mode
        self.target = target
        self.strict = strict      # 出错即停：给脚本 / 管道用
        self.failed = False       # 本次会话出现过失败的响应没有
        self.feed = CommandFeed()

    # ---- 基础设施 ----

    def _out(self, text):
        if text:
            sys.stdout.write(text + "\n")
        sys.stdout.flush()

    def _error(self, message):
        if "mhuixs-cli: " not in message:
            message = "mhuixs-cli: " + message
        sys.stderr.write(message + "\n")
        sys.stderr.flush()

    def _mark_failed(self, message=None):
        if message:
            self._error(message)
        self.failed = True
        return bool(self.strict)   # 出错即停开着就返回"该结束了"

    def cmdloop(self, intro=None):
        """横幅由 main() 按模式自己打，这里不再让 cmd 重复打一遍。"""
        return cmd.Cmd.cmdloop(self, intro="")

    def emptyline(self):
        return False              # 空行什么都不做（默认会重放上一条命令）

    def default(self, line):
        """一行交给谁处理：':' 开头是本地元命令，其余全部发服务端。"""
        stripped = line.strip()
        if stripped.startswith(":"):
            return self.meta(stripped)

        try:
            args = self.feed.feed(line)
        except CliError as e:
            return self._mark_failed(str(e))
        if not args:
            return False           # 空行 / 注释 / 还在等引号闭合的半行
        return self.dispatch(args)

    def meta(self, line):
        """执行一条本地元命令。认不出来就报错，不往服务端转。"""
        word, _, arg = line[1:].partition(" ")
        name = word.strip().lower()
        handler = getattr(self, "do_" + self.META[name], None) if name in self.META else None
        if handler is None:
            return self._mark_failed(
                "未知的元命令：%s（可用 :help :commands :format :fail :run "
                ":ping :status :save :shutdown :quit）" % (":" + name if name else ":"))
        return handler(arg.strip())

    def perform(self, args):
        """执行一条切好的命令。返回 (结果文档, None) 或 (None, 出错说明)。

        引号里的值若含换行，_protocol 会抛 ValueError —— 那是协议的硬限制
        （+ 元素靠 CRLF 划界，见 src/proto.h），本行命令怎么写都绕不过去，
        所以原样报出来，不去替它猜意图。
        """
        try:
            return self.db.call(*args), None
        except MhuixsError as e:
            return None, "连接已断开：%s" % e
        except OSError as e:
            return None, "连接错误：%s" % e
        except ValueError as e:
            # 客户端自己的参数问题（引号里的值含换行等），不是连接出问题
            return None, str(e)

    def dispatch(self, args):
        """发一条切好的命令并排版结果。返回 True 表示该结束会话。"""
        result, err = self.perform(args)
        if err:
            return self._mark_failed(err)

        body, ok = render(result, self.mode)
        self._out(body)
        if ok:
            return False
        return self._mark_failed(result.get("code", "failed"))

    # ---- 元命令 ----

    def do_help(self, arg):
        """用法：客户端元命令 + 原生命令清单"""
        self._out(META_HELP)
        return False

    def do_quit(self, arg):
        """断开并退出（只关这条连接，不动服务端）"""
        self._out("再见")
        return True

    do_q = do_quit
    do_exit = do_quit

    def do_format(self, arg):
        """切输出格式：:format value|pretty|text|json（不给参数就看当前）"""
        name = arg.strip().lower()
        if not name:
            self._out("输出格式：%s" % self.mode)
            return False
        if name not in MODES:
            return self._mark_failed("用法：:format %s" % "|".join(MODES))
        self.mode = name
        self._out("输出格式：%s" % name)
        return False

    def do_mode(self, arg):
        """同 :format"""
        return self.do_format(arg)

    def do_fail(self, arg):
        """出错即停开关：:fail on|off（脚本 / 管道用）"""
        name = arg.strip().lower()
        if name not in ("on", "off"):
            return self._mark_failed("用法：:fail on|off")
        self.strict = (name == "on")
        self._out("出错即停：%s" % ("开" if self.strict else "关"))
        return False

    def do_run(self, arg):
        """再执行一段命令：:run rpush todo "two words"（引号被外层 shell 吃掉时用）"""
        if not arg.strip():
            return self._mark_failed("用法：:run <命令>")
        return self.default(arg)

    def do_commands(self, arg):
        """打印服务端的命令清单"""
        self._out(COMMANDS_HELP.rstrip("\n"))
        return False

    def do_ping(self, arg):
        """探活：走一条最轻的查询，能回来说明连接是通的"""
        _, err = self.perform(["hooks"])
        if err:
            return self._mark_failed("连接异常：%s" % err)
        self._out("连接正常：%s" % self.target)
        return False

    def do_status(self, arg):
        """看当前连的是谁、用什么版式"""
        self._out("服务端   %s" % self.target)
        self._out("输出     %s" % self.mode)
        self._out("出错即停  %s" % ("开" if self.strict else "关"))
        return False

    def do_save(self, arg):
        """让服务端把注册表落盘"""
        return self._meta(":save")

    def do_shutdown(self, arg):
        """让服务端停机（当前连接会被掐断）"""
        return self._meta(":shutdown")

    def do_EOF(self, arg):
        """Ctrl-D：与 :quit 一致"""
        sys.stdout.write("\n")
        return self.do_quit(arg)

    def _meta(self, name):
        """把一条命令透传给服务端，结果按当前版式打出来。

        :shutdown 之后连接就没了，所以无论回包还是断连都算正常收场。
        """
        result, err = self.perform([name])
        if err:
            if name == ":shutdown":
                self._out("服务端已停机")
                return True
            return self._mark_failed("连接已断开：%s" % err)

        body, ok = render(result, self.mode)
        self._out(body)
        if name == ":shutdown":
            return True
        return False if ok else self._mark_failed(result.get("code", "failed"))


class BatchStop(Exception):
    """批处理里"出错即停"用的跳出信号。"""


class BatchShell(Shell):
    """批处理用的外壳：正常结果进 stdout（保持可管道），错误带行号进 stderr。"""

    def __init__(self, db, mode="value", target="", strict=False, label=None):
        Shell.__init__(self, db, mode=mode, target=target, strict=strict)
        self.label = label
        self.lineno = 0

    def _where(self):
        return "%s:" % self.label if self.label else ""

    def _error(self, message):
        sys.stderr.write("%s%d: %s\n" % (self._where(), self.lineno, message))
        sys.stderr.flush()

    def _mark_failed(self, message=None):
        if message:
            self._error(message)
        self.failed = True
        if self.strict:
            raise BatchStop()
        return False


META_HELP = """数据命令（一行一条，参数按 shell 规则切词，引号内的值按字符串处理）
  例：create list todo / rpush todo milk egg / llen todo
  完整清单见 :commands

客户端元命令（在本地解释，不发给服务端）
  :help / :?          打印这段说明
  :commands           打印命令清单
  :format <版式>       切输出：value|pretty|text|json（value 是默认）
  :fail on|off        出错即停，脚本 / 管道里用（默认 off）
  :run <命令>         再执行一段命令（引号被外层 shell 吃掉时用）
  :ping               探活
  :status             看当前连的是谁、用什么版式
  :save               让服务端落盘
  :shutdown           让服务端停机
  :quit / Ctrl-D      断开退出
"""

# 命令清单。与 src/execute.c 的 mhx_help_text() 是同一份内容 ——
# 命令名与参数只有命令层说了算，这里是随客户端带的一份副本（没法从服务端取：
# 清单是会话层的元命令，服务端会话不认，客户端只能自己存）。
# 命令有增减时两边一起改。
COMMANDS_HELP = """数据结构
  create <list|bitmap|kvalot> <name>       创建并挂钩
  create table <name> <field:type> [...]   创建表，如 id:int name:str
  drop <name>                              摘除钩子（释放其数据）
  hooks                                    列出所有钩子（含内容摘要）
  info <name>                              查看某个钩子的内容
  type <name>                              只看钩子指向的类型

LIST
  rpush <name> <v> [<v>...]                右侧插入（可多个）
  lpush <name> <v> [<v>...]                左侧插入
  lpop  <name> / rpop <name>               左/右侧弹出并返回
  llen  <name>                             元素个数
  lget  <name> <i>                         读取下标 i
  lset  <name> <i> <v>                     改写下标 i
  lrem  <name> <i>                         移除下标 i，返回被移除的值

BITMAP
  bset   <name> <off> <0|1>                设置某位
  bget   <name> <off>                      读取某位
  bcount <name> <st> <ed>                  统计闭区间 [st,ed] 内 1 的个数
  bsize  <name>                            位数

KVALOT（键一律按字符串处理）
  kset <name> <key> <value>                写入（键已存在则覆盖）
  kget <name> <key>                        读取
  kdel <name> <key>                        删除
  klen <name>                              键数量
  kexists <name> <key>                     键是否存在

TABLE（col 可用列名或下标）
  tadd  <name> [<v>...]                    追加一行
  tget  <name> <row> <col>                 读取单元格
  tset  <name> <row> <col> <v>             改写单元格
  tdel  <name> <row>                       删除一行
  trows <name>                             行数
  tfields <name>                           字段列表

值语法
  123       -> 数字
  "hello"   -> 字符串（引号强制）
  hello     -> 字符串（非数字时自动回退）
"""


# ---------------------------------------------------------------- 批处理

def run_batch(db, lines, mode, strict, echo=False, label=None, target=""):
    """非交互执行：逐行跑，返回进程退出码。

    退出码只反映"这一轮有没有失败过"；strict 决定失败后停不停。
    与交互模式共用 Shell：切词、元命令、排版全走同一条路，只有"错往哪写"
    不同 —— 这里错误带上行号进 stderr，成功的结果进 stdout，于是命令输出
    可以直接重定向或管道给别的程序。元命令（:format / :quit / …）在本地
    解释，不会发给服务端。
    """
    shell = BatchShell(db, mode=mode, target=target, strict=strict, label=label)

    for raw in lines:
        shell.lineno += 1
        line = raw.rstrip("\r\n")
        if shell.lineno == 1 and line.startswith("﻿"):
            line = line[1:]                     # 跳过 UTF-8 BOM

        if echo and line.strip():
            sys.stdout.write("mhuixs> %s\n" % line.strip())
        try:
            if shell.onecmd(line):              # True = :quit / Ctrl-D
                break
        except BatchStop:                       # 出错即停
            break

    left = shell.feed.flush()
    if left:
        shell.failed = True
        shell._error("引号没有闭合：%s" % left)
    return 1 if shell.failed else 0


def build_parser():
    p = argparse.ArgumentParser(
        prog="mhuixs-cli",
        description="Mhuixs 命令行客户端（交互 / 一次性命令 / 管道三种用法）",
        epilog="令牌顺序：--token > 环境变量 MHUIXS_TOKEN > 配置文件的 authtoken",
    )
    p.add_argument("--host", default=None,
                   help="服务端地址（默认取配置的 bind，否则 127.0.0.1）")
    p.add_argument("--port", type=int, default=None,
                   help="服务端端口（默认取配置的 port，否则 18185）")
    p.add_argument("--token", default=None, help="访问令牌")
    p.add_argument("--format", dest="mode", choices=MODES, default="value",
                   help="输出格式，默认 value（只要值）")
    p.add_argument("--json", dest="mode", action="store_const", const="json",
                   help="等价于 --format json")
    p.add_argument("--fail", dest="strict", action="store_true",
                   help="出错即停，退出码非 0（脚本用）")
    p.add_argument("--echo", action="store_true",
                   help="管道 / 一次性模式下连命令本身一起回显")
    p.add_argument("--version", action="version", version="mhuixs-cli %s" % __version__)
    p.add_argument("command", nargs="*",
                   help="一条或多条命令；给 - 则从 stdin 读；都不给则进交互模式")
    return p


def main(argv=None):
    opts = build_parser().parse_args(argv)
    host, port = resolve_host_port(opts.host, opts.port)
    token = resolve_token(opts.token)
    target = "%s:%d" % (host, port)

    if token is None:
        sys.stderr.write("mhuixs-cli: 没有拿到访问令牌\n")
        sys.stderr.write("  用 --token 指定，或设环境变量 MHUIXS_TOKEN，"
                         "或在配置里填 authtoken\n")
        return 2

    try:
        # 认证失败时构造函数会抛 AuthenticationError，连接由它自己关掉
        db = Mhuixs(host, port, token=token)
    except AuthenticationError as e:
        sys.stderr.write("mhuixs-cli: 连接 %s 被拒绝：%s\n" % (target, e))
        return 2
    except ConnectionError as e:
        sys.stderr.write("mhuixs-cli: 连不上 %s：%s\n" % (target, e))
        sys.stderr.write("  确认服务端在跑：mhuixs server\n")
        return 2
    except MhuixsError as e:
        sys.stderr.write("mhuixs-cli: 连接 %s 失败：%s\n" % (target, e))
        return 2
    except OSError as e:
        sys.stderr.write("mhuixs-cli: 连不上 %s：%s\n" % (target, e))
        sys.stderr.write("  确认服务端在跑：mhuixs server\n")
        return 2

    try:
        if opts.command:
            if opts.command == ["-"]:
                label = None if sys.stdin.isatty() else "<stdin>"
                return run_batch(db, sys.stdin, opts.mode, opts.strict,
                                 echo=opts.echo, label=label, target=target)
            # 一次性模式：每个命令行参数算一条命令
            return run_batch(db, opts.command, opts.mode, opts.strict,
                             echo=opts.echo, target=target)

        if not sys.stdin.isatty():
            # 有管道但没写 - ：直接读 stdin，比对着不是终端的标准输入发呆强
            return run_batch(db, sys.stdin, opts.mode, opts.strict,
                             echo=opts.echo, label="<stdin>", target=target)

        show_banner(target)
        shell = Shell(db, mode=opts.mode, target=target, strict=opts.strict)
        try:
            shell.cmdloop()
        except KeyboardInterrupt:
            sys.stdout.write("\n（Ctrl-C 退出）\n")
        return 1 if shell.failed else 0
    finally:
        db.close()


def show_banner(target):
    sys.stdout.write("Mhuixs CLI %s  已连接 %s\n" % (__version__, target))
    sys.stdout.write("输入命令，:help 看用法，:quit 退出\n\n")
    sys.stdout.flush()


def cli():
    """控制台入口。"""
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        sys.exit(130)
    except MhuixsError as e:
        sys.stderr.write("mhuixs-cli: %s\n" % e)
        sys.exit(1)


if __name__ == "__main__":
    cli()
