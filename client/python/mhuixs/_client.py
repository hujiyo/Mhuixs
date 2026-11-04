"""Mhuixs 客户端。

一个实例对应一条 TCP 连接：构造即连接，有令牌就先 AUTH。方法按数据结构
分组（LIST / BITMAP / KVALOT / TABLE），返回值已经解包成 Python 类型；
需要服务端原始 JSON 文档时用 :meth:`Mhuixs.call`。

一个实例只对应一条连接，不做连接池，也不自动重连。实例内部有一把锁，
多个线程共用一个实例时命令会串行执行；要让多个线程并行发命令，每个线程
各建一个实例即可（服务端同时接受多个连接，命令本身仍是串行执行的）。
"""

import json
import socket
import threading
from collections import namedtuple
from decimal import Decimal

from ._errors import (
    AuthenticationError,
    ConnectionError,
    MhuixsError,
    ProtocolError,
    error_from_document,
)
from ._protocol import Forced, encode_request, parse_reply, to_bytes


#: 多行结果：``columns`` 是列名列表，``values`` 是行列表（行内按列顺序）。
Rows = namedtuple("Rows", ["columns", "values"])


def _unwrap(doc):
    """把服务端的 JSON 文档解包成 Python 值；失败响应抛对应异常。"""
    if not doc.get("ok"):
        raise error_from_document(doc)

    kind = doc.get("kind")
    if kind is None:
        # create / drop / lset / tset / info / :save 这类没有类型化返回值，
        # text 才是唯一有信息量的东西。
        return doc.get("text")
    if kind == "null":
        return None
    if kind == "rows":
        return Rows(doc.get("columns") or [], doc.get("value") or [])
    return doc.get("value")


def _typed_arg(value):
    """类型化方法的参数映射：字符串永远是字符串，数字才是数字。

    普通字符串用 ``+`` 强制成字符串；含 CR/LF 时改用 ``$`` —— ``+`` 靠
    CRLF 划界装不下换行，而这种内容也一定不会被服务端读成数字。
    """
    if isinstance(value, Forced):
        return value
    if isinstance(value, (str, bytes, bytearray, memoryview)):
        body = to_bytes(value)
        if b"\r" in body or b"\n" in body:
            return value
        return Forced(value)
    return value


def _field_specs(fields):
    """把表字段描述统一成 ``["id:int", "name:str"]`` 这种文本。"""
    if isinstance(fields, str):
        return [fields]
    if isinstance(fields, dict):
        return ["%s:%s" % (k, v) for k, v in fields.items()]

    specs = []
    for item in fields:
        if isinstance(item, str):
            specs.append(item)
        elif isinstance(item, (tuple, list)) and len(item) == 2:
            specs.append("%s:%s" % (item[0], item[1]))
        else:
            raise TypeError("表字段要写成 'name:type' 或 (name, type)，得到 %r" % (item,))
    return specs


class Mhuixs:
    """连到 Mhuixs 服务端的客户端。

    :param host: 服务端地址，默认 127.0.0.1
    :param port: 服务端端口，默认 18185
    :param token: 访问令牌；``None`` 表示不发 AUTH（服务端会拒绝后续命令）
    :param timeout: 连接与读写的超时秒数
    """

    def __init__(self, host="127.0.0.1", port=18185, token=None, timeout=10.0):
        self._host = host
        self._port = port
        self._timeout = timeout
        self._lock = threading.Lock()
        self._sock = None
        self._buf = b""

        try:
            self._sock = socket.create_connection((host, port), timeout)
            self._sock.settimeout(timeout)
            if token is not None:
                self._authenticate(token)
        except Exception:
            self.close()
            raise

    # ---------------- 公开接口 ----------------

    def call(self, *args):
        """发一条命令，返回完整的 JSON 文档（dict）。

        服务端业务失败（``ok: false``）**不抛异常**，由调用方自己看 ``code``；
        连接、协议、认证错误仍然抛异常。小数按标准 JSON 解析成 float；
        要无损数值请用 :meth:`execute`。
        """
        if not args:
            raise ValueError("至少要有一个参数")
        return self._request(list(args), parse_float=float)

    def execute(self, *args):
        """发一条命令，返回解包后的 Python 值；``ok: false`` 抛对应异常。

        - ``int`` -> ``int``
        - ``num`` -> ``int``（无小数点）或 ``decimal.Decimal``（有小数点）
        - ``str`` / ``bool`` / ``list`` -> 对应类型
        - ``rows`` -> :class:`Rows`（列名 + 行列表）
        - ``null`` -> ``None``
        - 没有 ``kind`` 的响应 -> 服务端 ``text`` 字符串
        """
        if not args:
            raise ValueError("至少要有一个参数")
        return _unwrap(self._request(list(args), parse_float=Decimal))

    def pipeline(self, commands, batch_size=256):
        """批量发送命令，逐条返回解包后的结果。

        ``commands`` 是 ``(命令, 参数...)`` 的可迭代对象。每 ``batch_size`` 条
        一组：一组一次发出去、再按顺序收回应答，比逐条往返快得多，也不会
        把全部命令先攒在内存里。

        任何一条命令失败时，这一组剩下的应答仍会被读完（保持连接同步），
        然后抛出第一个错误。返回值是生成器，要全部收集就 ``list(...)``。
        """
        batch_size = int(batch_size)
        if batch_size < 1:
            raise ValueError("batch_size 至少为 1")

        batch = []
        for command in commands:
            if isinstance(command, (str, bytes, bytearray, memoryview)):
                raise TypeError("每条命令要写成 (命令, 参数...)，得到 %r" % (command,))
            command = tuple(command)
            if not command:
                raise ValueError("空命令")
            batch.append(command)
            if len(batch) >= batch_size:
                yield from self._pipeline_batch(batch)
                batch = []
        if batch:
            yield from self._pipeline_batch(batch)

    def close(self):
        """关闭连接。重复调用无副作用。"""
        with self._lock:
            sock, self._sock = self._sock, None
            self._buf = b""
        if sock is not None:
            try:
                sock.close()
            except OSError:
                pass

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()

    def __del__(self):
        try:
            self.close()
        except Exception:
            pass

    # ---------------- 管理 ----------------

    def create_list(self, name):
        """创建 LIST 并挂到 HOOK。返回 None。"""
        self._run("create", "list", name)

    def create_bitmap(self, name):
        """创建 BITMAP 并挂到 HOOK。返回 None。"""
        self._run("create", "bitmap", name)

    def create_kvalot(self, name):
        """创建 KVALOT 并挂到 HOOK。返回 None。"""
        self._run("create", "kvalot", name)

    def create_table(self, name, fields):
        """创建 TABLE 并挂到 HOOK。

        ``fields`` 可以是 ``["id:int", "name:str"]``、``[("id", "int"), ...]``
        或 ``{"id": "int", "name": "str"}``。返回 None。
        """
        self._run("create", "table", name, *_field_specs(fields))

    def drop(self, name):
        """摘除 HOOK（连同它引用的数据）。不存在的钩子视为成功。返回 None。"""
        self._run("drop", name)

    def rename(self, old, new):
        """把钩子 ``old`` 改名为 ``new``；``new`` 已存在时其数据被覆盖。

        返回服务端文本。整块替换数据的常见做法：新数据先建在临时钩子下，
        灌完再 ``rename`` 到正式名字，读方不会看到空表。
        """
        return self._run("rename", old, new)

    def swap(self, first, second):
        """交换两个钩子的名字。返回服务端文本。"""
        return self._run("swap", first, second)

    def hooks(self):
        """返回当前 HOOK 数量（``int``）。"""
        return self._run("hooks")

    def info(self, name):
        """返回某个 HOOK 的内容摘要（服务端排版好的 ``str``）。"""
        return self._run("info", name)

    def type(self, name):
        """返回 HOOK 指向的数据类型名，例如 ``"list"``。"""
        return self._run("type", name)

    # ---------------- LIST ----------------

    def rpush(self, name, *values):
        """右侧插入一个或多个值，返回插入后的元素个数（``int``）。"""
        return self._run("rpush", name, *values)

    def lpush(self, name, *values):
        """左侧插入一个或多个值，返回插入后的元素个数（``int``）。"""
        return self._run("lpush", name, *values)

    def lpop(self, name):
        """从左侧弹出并返回一个值；空列表时服务端报 ``empty``。"""
        return self._run("lpop", name)

    def rpop(self, name):
        """从右侧弹出并返回一个值；空列表时服务端报 ``empty``。"""
        return self._run("rpop", name)

    def llen(self, name):
        """返回元素个数（``int``）。"""
        return self._run("llen", name)

    def lget(self, name, index):
        """读取下标 ``index`` 处的值。"""
        return self._run("lget", name, index)

    def lset(self, name, index, value):
        """改写下标 ``index`` 处的值。返回 None。"""
        self._run("lset", name, index, value)

    def lrem(self, name, index):
        """移除下标 ``index`` 处的值并返回它。"""
        return self._run("lrem", name, index)

    def lrange(self, name, start, end):
        """读取闭区间 ``[start, end]``，负下标从尾部数。返回 :class:`Rows`。

        区间越界自动截断，起点在终点之后得到空结果（不报错）。
        """
        return self._run("lrange", name, start, end)

    # ---------------- BITMAP ----------------

    def bset(self, name, offset, bit):
        """设置某一位，返回设置后的位值（``bool``）。"""
        return self._run("bset", name, offset, bit)

    def bget(self, name, offset):
        """读取某一位（``bool``）。"""
        return self._run("bget", name, offset)

    def bcount(self, name, start, end):
        """统计闭区间 ``[start, end]`` 内 1 的个数（``int``）。"""
        return self._run("bcount", name, start, end)

    def bsize(self, name):
        """返回位数（``int``）。"""
        return self._run("bsize", name)

    # ---------------- KVALOT ----------------

    def kset(self, name, key, value):
        """写入键值（键已存在则覆盖），返回键数量（``int``）。"""
        return self._run("kset", name, key, value)

    def kget(self, name, key):
        """读取键对应的值；键不存在时服务端报 ``no_key``。"""
        return self._run("kget", name, key)

    def kdel(self, name, key):
        """删除键，返回剩余键数量（``int``）。"""
        return self._run("kdel", name, key)

    def klen(self, name):
        """返回键数量（``int``）。"""
        return self._run("klen", name)

    def kexists(self, name, key):
        """键是否存在（``bool``）。"""
        return self._run("kexists", name, key)

    def mget(self, name, *keys):
        """一次取多个键，返回两列 :class:`Rows`（``key`` / ``value``）。

        结果顺序与请求一致；不存在的键值为 ``None``。
        """
        return self._run("mget", name, *keys)

    # ---------------- TABLE ----------------

    def tadd(self, name, *values):
        """追加一行，返回行数（``int``）。"""
        return self._run("tadd", name, *values)

    def tget(self, name, row, col):
        """读取单元格；空格子返回 ``None``。``col`` 可以是列名或下标。"""
        return self._run("tget", name, row, col)

    def tset(self, name, row, col, value):
        """改写单元格。返回 None。"""
        self._run("tset", name, row, col, value)

    def tdel(self, name, row):
        """删除一行，返回剩余行数（``int``）。"""
        return self._run("tdel", name, row)

    def tupsert(self, name, key_col, key, *columns):
        """按 ``key_col == key`` 更新一行；找不到就追加。返回 ``1``（插入）/ ``0``（更新）。

        ``columns`` 可以是一个 dict，或若干 ``(列, 值)`` 对::

            db.tupsert("t", "id", 1, {"name": "alice", "age": 30})
            db.tupsert("t", "id", 1, ("name", "alice"), ("age", 30))

        只更新给出的列，其余列保持原值。``key_col`` 上有索引时走索引，
        否则服务端做全表扫描。
        """
        if len(columns) == 1 and isinstance(columns[0], dict):
            items = list(columns[0].items())
        else:
            items = list(columns)

        pairs = []
        for item in items:
            if not isinstance(item, (tuple, list)) or len(item) != 2:
                raise TypeError("列和值要写成 (列, 值) 对或一个 dict，得到 %r"
                                % (item,))
            pairs.extend([item[0], item[1]])
        return self._run("tupsert", name, key_col, key, *pairs)

    def trows(self, name):
        """返回行数（``int``）。"""
        return self._run("trows", name)

    def tfields(self, name):
        """返回字段名列表（``list[str]``）。

        服务端的类型化列表最多带 32 项；字段更多时这里只拿到前 32 个，
        完整信息可以用 :meth:`call` 看响应的 ``truncated`` 字段，或读
        :meth:`info` 的文本。
        """
        return self._run("tfields", name)

    def trange(self, name, start, end, *columns):
        """读取闭区间的行，可只取部分列（不给就取全部）。返回 :class:`Rows`。

        行下标语义与 :meth:`lrange` 一致：负下标从尾部数，越界自动截断。
        """
        return self._run("trange", name, start, end, *columns)

    def torder(self, name, col, start, end, *columns):
        """按某列索引里的名次读取闭区间，返回 :class:`Rows`。

        名次是索引排序后的位置（0 是值最小的行），负下标从尾部数；该列
        必须已有索引。和无状态的 :meth:`trange` 一样，服务端不保存游标，
        位置由调用方持有。相同值的先后不保证稳定。
        """
        return self._run("torder", name, col, start, end, *columns)

    def iter_rows(self, name, *columns, page_size=1000, start=0, end=None):
        """分段读取表的行区间，逐页返回 :class:`Rows`。

        默认读整张表。``page_size`` 是每页行数；``start`` / ``end`` 是闭区间，
        负下标从尾部数（与 :meth:`trange` 一致），``end=None`` 表示读到最后一行。
        每页是一次独立的 ``trange``，所以 ``page_size`` 决定单次响应的内存峰值。

        并发写入会让后续页的行号漂移；需要读写并行时，请由调用方负责错开或重试。
        """
        page_size = int(page_size)
        if page_size < 1:
            raise ValueError("page_size 至少为 1")

        total = self.trows(name)
        if total == 0:
            return

        start = int(start)
        if start < 0:
            start += total
        if start < 0:
            start = 0

        if end is None:
            end = total - 1
        else:
            end = int(end)
            if end < 0:
                end += total
        if end >= total:
            end = total - 1
        if end < start:
            return

        lo = start
        while lo <= end:
            hi = min(lo + page_size - 1, end)
            yield self.trange(name, lo, hi, *columns)
            lo = hi + 1

    def tindex(self, name, col):
        """为某列建/重建有序索引，返回建索引时的行数（``int``）。

        建好之后用 :meth:`tfind` / :meth:`tselect` 查询；之后的追加 / 删除 /
        改单元格 / 换行都会增量维护它，不需要重建。只有无法维护时（例如
        内存不足）才会退回 stale（``IndexStaleError``），此时再调用本方法。
        """
        return self._run("tindex", name, col)

    def tunindex(self, name, col):
        """删除某列的索引（幂等）。返回服务端文本。"""
        return self._run("tunindex", name, col)

    def tindexes(self, name):
        """列出已有索引，返回两列 :class:`Rows`（``column`` / ``status``）。

        ``status`` 是 ``"fresh"`` 或 ``"stale"``。
        """
        return self._run("tindexes", name)

    def tfind(self, name, col, value, *columns):
        """按列值等值查找，返回 :class:`Rows`；该列必须已有新鲜索引。

        没建索引抛 :class:`NoIndexError`，索引过期抛 :class:`IndexStaleError`。
        """
        return self._run("tfind", name, col, value, *columns)

    def tselect(self, name, col, low, high, *columns):
        """按列值闭区间 ``[low, high]`` 查找，返回 :class:`Rows`。

        索引要求与 :meth:`tfind` 相同。
        """
        return self._run("tselect", name, col, low, high, *columns)

    # ---------------- 会话 ----------------

    def ping(self):
        """探活：发一条最轻的查询，能回来就返回 True。"""
        self.hooks()
        return True

    def save(self):
        """让服务端把注册表落盘，返回服务端的文本说明。"""
        return self.execute(":save")

    def shutdown(self):
        """让服务端停机。回包后本连接会被关闭；返回服务端的文本说明。"""
        try:
            return self.execute(":shutdown")
        finally:
            self.close()

    # ---------------- 内部 ----------------

    def _run(self, cmd, *args):
        """类型化命令的统一入口：字符串参数强制成字符串，结果解包。"""
        return _unwrap(self._request([cmd] + [_typed_arg(a) for a in args],
                                     parse_float=Decimal))

    def _authenticate(self, token):
        self._send(["AUTH", token])
        tag, payload = self._read_reply()
        if tag == "+":
            return
        message = payload.decode("utf-8", "replace") if payload else ""
        raise AuthenticationError(message or "认证失败")

    def _request(self, args, parse_float):
        with self._lock:
            self._send(args)
            tag, payload = self._read_reply()
        return self._decode_reply(tag, payload, parse_float)

    def _pipeline_batch(self, commands):
        """一组命令一次发出，按顺序收回全部应答，再逐条解包。

        先收完再解包：中间某条失败时，后面的应答已经被读掉，
        连接不会停在"上一条的回复"上。
        """
        frames = []
        for command in commands:
            args = [command[0]] + [_typed_arg(a) for a in command[1:]]
            frames.append(encode_request(args))

        with self._lock:
            self._send_encoded(b"".join(frames))
            replies = [self._read_reply() for _ in frames]

        for tag, payload in replies:
            yield _unwrap(self._decode_reply(tag, payload, Decimal))

    @staticmethod
    def _decode_reply(tag, payload, parse_float):
        if tag == "+":
            # +OK 只用于 AUTH；认证之外出现说明服务端行为不对，别猜。
            raise ProtocolError("意外的简单串响应：%s"
                                % payload.decode("utf-8", "replace"))
        if tag == "-":
            message = payload.decode("utf-8", "replace")
            kind = message.split(None, 1)[0].upper() if message else ""
            if kind == "NOAUTH":
                raise AuthenticationError(message)
            raise ProtocolError(message)

        text = payload.decode("utf-8", "surrogateescape")
        try:
            doc = json.loads(text, parse_float=parse_float)
        except ValueError as e:
            raise ProtocolError("服务端回了不是合法 JSON 的响应：%s" % (e,))
        if not isinstance(doc, dict):
            raise ProtocolError("服务端响应不是 JSON 对象：%r" % (type(doc).__name__,))
        return doc

    def _send(self, args):
        self._send_encoded(encode_request(args))

    def _send_encoded(self, data):
        sock = self._sock
        if sock is None:
            raise ConnectionError("连接已关闭")
        try:
            sock.sendall(data)
        except OSError as e:
            raise ConnectionError("发送失败：%s" % (e,))

    def _read_reply(self):
        while True:
            parsed = parse_reply(self._buf)
            if parsed is not None:
                tag, payload, consumed = parsed
                self._buf = self._buf[consumed:]
                return tag, payload

            sock = self._sock
            if sock is None:
                raise ConnectionError("连接已关闭")
            try:
                chunk = sock.recv(65536)
            except OSError as e:
                raise ConnectionError("接收失败：%s" % (e,))
            if not chunk:
                raise ConnectionError("服务端关闭了连接")
            self._buf += chunk
