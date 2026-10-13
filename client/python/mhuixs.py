#!/usr/bin/env python3
"""Mhuixs 的 Python 客户端，也是 MHP/1 协议的一份参考实现。

它存在的意义不只是"能用 Python 连 Mhuixs"，更是证明这套协议**别的语言
照着文档就能实现**：整个文件只做拼字节和读字节，不依赖 Mhuixs 的任何
内部东西。想知道协议长什么样，读 _send 和 _read_reply 两个方法就够了。

    import mhuixs
    db = mhuixs.Mhuixs("127.0.0.1", 18185, token="你的令牌")
    print(db.call("rpush", "todo", "milk"))
    print(db.call("llen", "todo"))
    db.close()

命令行用法（每条命令一个参数，按 shell 规则切词）：

    python mhuixs.py --token 你的令牌 'create list todo' 'rpush todo milk' 'llen todo'

协议要点（完整规范见 src/proto.h）：

    请求  *<n> CRLF，后面跟 n 个参数，每个是 $<字节长度> CRLF <原始字节> CRLF
    响应  +OK            认证成功
          -XXX 说明       出错，服务端随后断开
          $<长度> CRLF <JSON 文档> CRLF    一切正常结果

    $ 元素按内容自动判定数字还是字符串（等价文本模式不加引号）；
    + 元素强制当字符串（等价文本模式加引号），本文件用 Forced 包一层。
"""

import json
import shlex
import socket
import sys


class MhuixsError(Exception):
    """服务端回了一个 - 开头的错误行（认证失败、协议错、连接被关等）。"""


class Forced:
    """包一层，表示这个参数按**字符串**处理。

    等价于文本模式里给值加引号：Forced("123") 存进去是字符串，不是数字。
    对应协议里的 + 元素。
    """

    __slots__ = ("value",)

    def __init__(self, value):
        self.value = value


def _to_bytes(value):
    if isinstance(value, bytes):
        return value
    if isinstance(value, str):
        return value.encode("utf-8")
    return str(value).encode("utf-8")


class Mhuixs:
    def __init__(self, host="127.0.0.1", port=18185, token=None, timeout=10.0):
        self._sock = socket.create_connection((host, port), timeout)
        self._buf = b""
        if token is not None:
            # 认证的应答是 +OK，不是 JSON 文档，所以这里不走 call()
            self._send(("AUTH", token))
            self._read_reply()

    # ---------------- 公开接口 ----------------

    def call(self, *args):
        """发一条命令，返回解析好的 JSON 结果（dict）。"""
        if not args:
            raise ValueError("至少要有一个参数")
        self._send(args)
        payload = self._read_reply()
        return json.loads(payload.decode("utf-8"))

    def close(self):
        if self._sock is not None:
            self._sock.close()
            self._sock = None

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()

    # ---------------- 协议编解码 ----------------

    def _send(self, args):
        out = bytearray()
        out += b"*%d\r\n" % len(args)
        for a in args:
            forced = isinstance(a, Forced)
            body = _to_bytes(a.value if forced else a)
            if forced:
                # 简单串：到 CRLF 为止，所以它自己不能含 CR/LF
                if b"\r" in body or b"\n" in body:
                    raise ValueError("Forced 的值不能含换行（协议里的 + 元素靠 CRLF 划界）")
                out += b"+"
                out += body
                out += b"\r\n"
            else:
                out += b"$%d\r\n" % len(body)
                out += body
                out += b"\r\n"
        self._sock.sendall(bytes(out))

    def _read_reply(self):
        line = self._read_line()
        if not line:
            raise MhuixsError("收到空响应行")

        tag, rest = line[:1], line[1:]

        if tag == b"+":
            return rest.decode("utf-8", "replace")
        if tag == b"-":
            raise MhuixsError(rest.decode("utf-8", "replace"))
        if tag == b"$":
            n = int(rest)
            if n < 0:
                return None
            return self._read_exact(n)

        raise MhuixsError("不认识的响应类型: %r" % tag)

    def _read_line(self):
        while b"\r\n" not in self._buf:
            self._fill()
        line, self._buf = self._buf.split(b"\r\n", 1)
        return line

    def _read_exact(self, n):
        while len(self._buf) < n + 2:
            self._fill()
        data = self._buf[:n]
        tail = self._buf[n:n + 2]
        if tail != b"\r\n":
            raise MhuixsError("分帧损坏：数据体后面不是 CRLF")
        self._buf = self._buf[n + 2:]
        return data

    def _fill(self):
        chunk = self._sock.recv(65536)
        if not chunk:
            raise MhuixsError("服务端关闭了连接")
        self._buf += chunk


# ---------------- 命令行入口 ----------------

def _main(argv):
    import argparse

    p = argparse.ArgumentParser(description="Mhuixs 命令行客户端")
    p.add_argument("--host", default="127.0.0.1")
    p.add_argument("--port", type=int, default=18185)
    p.add_argument("--token", default=None,
                   help="访问令牌；不传则不发 AUTH（服务端会拒绝非 AUTH 请求）")
    p.add_argument("--text", action="store_true",
                   help="只打印结果里的 text 字段，而不是整个 JSON")
    p.add_argument("command", nargs="+",
                   help="一条命令，例如 'rpush todo milk'（按 shell 规则切词）")
    opts = p.parse_args(argv)

    with Mhuixs(opts.host, opts.port, opts.token) as db:
        for cmdline in opts.command:
            words = shlex.split(cmdline)
            if not words:
                continue
            result = db.call(*words)
            if opts.text:
                print(result.get("text", ""))
            else:
                print(json.dumps(result, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(_main(sys.argv[1:]))
    except MhuixsError as e:
        print("mhuixs: %s" % e, file=sys.stderr)
        sys.exit(1)
