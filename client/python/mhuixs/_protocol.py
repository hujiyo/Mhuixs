"""MHP/1 线格式的编解码，也是这份协议的 Python 参考实现。

这一层只做「参数 <-> 字节」，不碰 socket，所以半包、粘包、二进制安全这些
最容易写错的部分可以离线单测。完整规范见 ``src/proto.h``。

请求（客户端 -> 服务端）::

    *<n>\r\n                       n = 参数个数
      $<len>\r\n<原始字节>\r\n       自动判定：能读成数字就是数字，否则字符串
      +<文本>\r\n                   强制字符串（值的文本里不能有 CR/LF）

响应（服务端 -> 客户端）::

    +OK\r\n                        仅认证成功
    -<TAG> <消息>\r\n              分帧/认证失败，服务端随后断开
    $<len>\r\n<JSON 文档>\r\n      一切走到会话层的请求

参数带长度而不是拼成一行，是为了二进制安全：值里可以有空格、换行甚至 NUL
（命令层会拒绝 NUL，协议层本身能搬）。``+`` 元素承载文本模式里「加引号」
的语义，代价是它自己不能含 CR/LF —— 需要换行时走 ``$``，那种内容不会被
误判成数字。
"""

import math
from decimal import Decimal

from ._errors import ProtocolError


CRLF = b"\r\n"

# 与命令层同源的上限（src/execute.h 的 MHX_MAX_ARGS / MHX_MAX_ARG_BYTES）。
# 本地先拦一道，是为了把问题在发出去之前就说清楚，而不是等服务端回 internal。
MAX_ARGS = 48
MAX_ARG_BYTES = 255

# 单条响应的上限。服务端的会话输出缓冲是 128KB，这里给足余量，
# 主要作用是别让行为异常的服务端把客户端内存撑爆。
MAX_REPLY_BYTES = 16 * 1024 * 1024


class Forced:
    """把参数标记成「强制字符串」，对应协议里的 ``+`` 元素。

    等价于文本模式里给值加引号：``Forced("123")`` 存进去是字符串，不是数字。
    """

    __slots__ = ("value",)

    def __init__(self, value):
        self.value = value

    def __eq__(self, other):
        if isinstance(other, Forced):
            return self.value == other.value
        return NotImplemented

    def __repr__(self):
        return "Forced(%r)" % (self.value,)


def to_bytes(value):
    """转成协议里能搬的字节；字符串用 UTF-8。

    ``surrogateescape`` 让非 UTF-8 字节可以经由 Python 字符串原样往返，
    不会在编解码时被替换成 ``?``。
    """
    if isinstance(value, bytes):
        return value
    if isinstance(value, (bytearray, memoryview)):
        return bytes(value)
    if isinstance(value, str):
        return value.encode("utf-8", "surrogateescape")
    raise TypeError("参数必须是 str 或 bytes，得到 %s" % type(value).__name__)


def _number_text(value):
    """数字 -> 服务端能解析的十进制文本。

    服务端的数字语法只认 ``-?digits(.digits)?``，**不认科学计数法**，
    所以浮点数不能直接 ``repr()``（``1e+20`` 会被它当成字符串）。
    这里用 Decimal 展开成定点写法。
    """
    if isinstance(value, bool):
        return b"1" if value else b"0"
    if isinstance(value, int):
        return str(value).encode("ascii")

    if isinstance(value, float):
        if not math.isfinite(value):
            raise ValueError("不支持 inf / nan：服务端的数字语法没有它们")
        text = format(Decimal(repr(value)), "f")
    else:  # Decimal
        if not value.is_finite():
            raise ValueError("不支持 NaN / Infinity：服务端的数字语法没有它们")
        text = format(value, "f")

    return text.encode("ascii")


def _check_body(body, index):
    if len(body) > MAX_ARG_BYTES:
        raise ValueError("第 %d 个参数超过 %d 字节上限（现在 %d 字节）"
                         % (index + 1, MAX_ARG_BYTES, len(body)))
    if b"\x00" in body:
        raise ValueError("第 %d 个参数含有 NUL 字节：命令层按 C 字符串处理，"
                         "存进去会被截断，所以客户端直接拒绝" % (index + 1))


def encode_request(args):
    """把参数序列编码成一条完整请求。

    - ``Forced`` -> ``+``，强制字符串
    - ``str`` / ``bytes`` -> ``$``，由服务端自动判定数字或字符串
    - ``int`` / ``float`` / ``Decimal`` / ``bool`` -> ``$`` 数字文本
    """
    args = list(args)
    if not args:
        raise ValueError("至少要有一个参数")
    if len(args) > MAX_ARGS:
        raise ValueError("参数太多：%d 个，上限 %d 个" % (len(args), MAX_ARGS))

    out = bytearray(b"*%d\r\n" % len(args))
    for i, arg in enumerate(args):
        if isinstance(arg, Forced):
            body = to_bytes(arg.value)
            if b"\r" in body or b"\n" in body:
                raise ValueError("Forced 的值不能含换行（协议里的 + 元素靠 CRLF 划界）")
            _check_body(body, i)
            out += b"+" + body + CRLF
        elif isinstance(arg, (str, bytes, bytearray, memoryview)):
            body = to_bytes(arg)
            _check_body(body, i)
            out += b"$%d\r\n" % len(body) + body + CRLF
        elif isinstance(arg, (bool, int, float, Decimal)):
            body = _number_text(arg)
            _check_body(body, i)
            out += b"$%d\r\n" % len(body) + body + CRLF
        else:
            raise TypeError("第 %d 个参数的类型不支持：%s"
                            % (i + 1, type(arg).__name__))
    return bytes(out)


def parse_reply(buf):
    """从缓冲区头部解析一条响应。

    返回 ``(tag, payload, consumed)``；数据还不够就返回 ``None``，
    一个字节都不消费。``tag`` 是 ``"+"`` / ``"-"`` / ``"$"``。
    调用方在拿到结果后自行丢弃 ``consumed`` 个字节，剩余部分可以继续解析
    —— 这就是粘包与流水线。
    """
    if not buf:
        return None

    tag = buf[0:1]
    if tag in (b"+", b"-"):
        end = buf.find(CRLF)
        if end < 0:
            if len(buf) > MAX_REPLY_BYTES:
                raise ProtocolError("响应行过长，超过 %d 字节上限" % MAX_REPLY_BYTES)
            return None
        return tag.decode("ascii"), buf[1:end], end + 2

    if tag == b"$":
        end = buf.find(CRLF)
        if end < 0:
            if len(buf) > MAX_REPLY_BYTES:
                raise ProtocolError("响应行过长，超过 %d 字节上限" % MAX_REPLY_BYTES)
            return None
        raw_len = buf[1:end]
        if not raw_len.isdigit():
            raise ProtocolError("响应长度不是非负整数：%r" % (raw_len,))
        n = int(raw_len)
        if n > MAX_REPLY_BYTES:
            raise ProtocolError("响应体过大：%d 字节，上限 %d" % (n, MAX_REPLY_BYTES))

        total = end + 2 + n + 2
        if len(buf) < total:
            return None
        body = buf[end + 2:end + 2 + n]
        if buf[end + 2 + n:total] != CRLF:
            raise ProtocolError("分帧损坏：数据体后面不是 CRLF")
        return "$", body, total

    raise ProtocolError("不认识的响应类型：%r" % (tag,))
