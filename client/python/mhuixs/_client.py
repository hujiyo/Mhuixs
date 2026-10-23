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
from decimal import Decimal

from ._errors import (
    AuthenticationError,
    ConnectionError,
    MhuixsError,
    ProtocolError,
    error_from_document,
)
from ._protocol import Forced, encode_request, parse_reply, to_bytes


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
        - ``null`` -> ``None``
        - 没有 ``kind`` 的响应 -> 服务端 ``text`` 字符串
        """
        if not args:
            raise ValueError("至少要有一个参数")
        return _unwrap(self._request(list(args), parse_float=Decimal))

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
        sock = self._sock
        if sock is None:
            raise ConnectionError("连接已关闭")
        try:
            sock.sendall(encode_request(args))
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
