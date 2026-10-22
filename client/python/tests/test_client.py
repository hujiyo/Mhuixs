"""客户端与线格式的集成测试：真 TCP、假服务端、不依赖 C 程序。"""

import json
import socket
import threading
import time
import unittest
from decimal import Decimal

from mhuixs import (
    AuthenticationError,
    BadUsageError,
    ConnectionError,
    EmptyError,
    Mhuixs,
    MhuixsError,
    NameTakenError,
    NoFieldError,
    NoHookError,
    NoKeyError,
    OutOfRangeError,
    ProtocolError,
    TypeMismatchError,
    UnknownCommandError,
)

CRLF = b"\r\n"


# ---------------------------------------------------------------- 假服务端

def read_request(fp):
    """从字节流里读一条 MHP/1 请求；连接结束返回 None。"""
    line = fp.readline()
    if not line:
        return None
    if not line.endswith(CRLF):
        raise AssertionError("请求头没有以 CRLF 结尾：%r" % line)
    head = line[:-2]
    if head[:1] != b"*":
        raise AssertionError("请求没有以 * 开头：%r" % head)
    count = int(head[1:])

    args = []
    for _ in range(count):
        line = fp.readline()
        if not line.endswith(CRLF):
            raise AssertionError("参数头没有以 CRLF 结尾：%r" % line)
        head = line[:-2]
        tag = head[:1]
        if tag == b"$":
            size = int(head[1:])
            body = fp.read(size)
            if fp.read(2) != CRLF:
                raise AssertionError("参数体后面不是 CRLF")
        elif tag == b"+":
            body = head[1:]
        else:
            raise AssertionError("不认识的参数类型：%r" % tag)
        args.append({"body": body, "forced": tag == b"+"})
    return args


def reply_json(obj):
    payload = json.dumps(obj, ensure_ascii=False,
                         separators=(",", ":")).encode("utf-8")
    return b"$%d\r\n" % len(payload) + payload + CRLF


def reply_raw(text):
    payload = text.encode("utf-8") if isinstance(text, str) else text
    return b"$%d\r\n" % len(payload) + payload + CRLF


class Close:
    """让假服务端先把数据发出去，再结束连接。"""

    def __init__(self, data):
        self.data = data


class FakeServer(threading.Thread):
    def __init__(self, handler, token=None, chunk_delay=0.0):
        super().__init__(daemon=True)
        self.handler = handler
        self.token = token
        self.chunk_delay = chunk_delay
        self.received = []          # 认证之后的请求
        self.error = None

        self._sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self._sock.bind(("127.0.0.1", 0))
        self._sock.listen(1)
        self.port = self._sock.getsockname()[1]

    def run(self):
        try:
            conn, _ = self._sock.accept()
            with conn:
                self._serve(conn)
        except OSError:
            pass                    # 客户端提前关连接属于正常收场
        except Exception as e:      # 断言失败要带回主线程
            self.error = e
        finally:
            try:
                self._sock.close()
            except OSError:
                pass

    def _serve(self, conn):
        fp = conn.makefile("rb")
        first = read_request(fp)

        if self.token is not None:
            if not first or first[0]["body"] != b"AUTH":
                conn.sendall(b"-NOAUTH authenticate first\r\n")
                return
            if first[1]["body"] != self.token.encode("utf-8"):
                conn.sendall(b"-NOAUTH invalid token\r\n")
                return
            conn.sendall(b"+OK\r\n")

        request = first if self.token is None else read_request(fp)
        while request is not None:
            self.received.append(request)
            out = self.handler(request)
            if out is None:
                return
            if isinstance(out, Close):
                self._send(conn, out.data)
                return
            self._send(conn, out)
            request = read_request(fp)

    def _send(self, conn, data):
        if not self.chunk_delay:
            conn.sendall(data)
            return
        for i in range(0, len(data), 3):
            conn.sendall(data[i:i + 3])
            time.sleep(self.chunk_delay)

    def shutdown(self):
        try:
            self._sock.close()
        except OSError:
            pass
        self.join(2)


class ClientTest(unittest.TestCase):
    def start(self, handler, token=None, chunk_delay=0.0):
        server = FakeServer(handler, token=token, chunk_delay=chunk_delay)
        server.start()
        # LIFO：先 join（shutdown），再检查线程里有没有漏出来的异常
        self.addCleanup(lambda: self.assertIsNone(server.error, server.error))
        self.addCleanup(server.shutdown)
        return server

    def connect(self, server, token="secret"):
        db = Mhuixs("127.0.0.1", server.port, token=token, timeout=2.0)
        self.addCleanup(db.close)
        return db


class AuthTests(ClientTest):
    def test_token_is_sent_first(self):
        server = self.start(lambda req: reply_json(
            {"ok": True, "code": "ok", "text": "OK"}), token="secret")
        db = self.connect(server)
        db.hooks()
        self.assertEqual(server.received[0][0]["body"], b"hooks")

    def test_bad_token(self):
        server = self.start(lambda req: reply_json(
            {"ok": True, "code": "ok", "text": "OK"}), token="secret")
        with self.assertRaises(AuthenticationError):
            Mhuixs("127.0.0.1", server.port, token="wrong", timeout=2.0)

    def test_no_token_means_no_auth(self):
        server = self.start(lambda req: reply_json(
            {"ok": True, "code": "ok", "kind": "int", "value": 1, "text": "OK 1"}),
            token=None)
        db = self.connect(server, token=None)
        self.assertEqual(db.llen("todo"), 1)
        self.assertEqual(server.received[0][0]["body"], b"llen")


class ValueTests(ClientTest):
    def handler(self, req):
        cmd = req[0]["body"]
        if cmd == b"create":
            return reply_json({"ok": True, "code": "ok",
                               "text": "OK created list 'todo'"})
        if cmd == b"rpush":
            return reply_json({"ok": True, "code": "ok", "kind": "int",
                               "value": 2, "text": "OK rpush todo, len=2"})
        if cmd == b"lget":
            index = req[2]["body"]
            if index == b"0":
                return reply_raw('{"ok":true,"code":"ok","kind":"num",'
                                 '"value":1.50,"text":"OK 1.50"}')
            if index == b"1":
                return reply_json({"ok": True, "code": "ok", "kind": "int",
                                   "value": 42, "text": "OK 42"})
            return reply_json({"ok": True, "code": "ok", "kind": "str",
                               "value": "牛奶", "text": "OK 牛奶"})
        if cmd == b"bget":
            return reply_json({"ok": True, "code": "ok", "kind": "bool",
                               "value": True, "text": "OK 1"})
        if cmd == b"tget":
            return reply_json({"ok": True, "code": "ok", "kind": "null",
                               "text": "OK (empty)"})
        if cmd == b"tfields":
            return reply_json({"ok": True, "code": "ok", "kind": "list",
                               "value": ["id", "name"], "text": "OK 2 field(s)"})
        if cmd == b"info":
            return reply_json({"ok": True, "code": "ok",
                               "text": "todo : list"})
        raise AssertionError("没准备这个命令：%r" % (cmd,))

    def test_typed_returns(self):
        server = self.start(self.handler, token="secret")
        db = self.connect(server)

        self.assertIsNone(db.create_list("todo"))
        self.assertEqual(db.rpush("todo", "milk", 2), 2)

        self.assertEqual(db.lget("todo", 0), Decimal("1.50"))
        self.assertIsInstance(db.lget("todo", 0), Decimal)
        self.assertEqual(db.lget("todo", 1), 42)
        self.assertIsInstance(db.lget("todo", 1), int)
        self.assertEqual(db.lget("todo", 2), "牛奶")
        self.assertIs(db.bget("bits", 0), True)
        self.assertIsNone(db.tget("t", 0, "name"))
        self.assertEqual(db.tfields("t"), ["id", "name"])
        self.assertEqual(db.info("todo"), "todo : list")

    def test_call_keeps_raw_document(self):
        server = self.start(self.handler, token="secret")
        db = self.connect(server)

        doc = db.call("lget", "todo", 0)
        self.assertTrue(doc["ok"])
        self.assertEqual(doc["kind"], "num")
        self.assertEqual(doc["value"], 1.5)
        self.assertIsInstance(doc["value"], float)

        self.assertEqual(db.execute("lget", "todo", 0), Decimal("1.50"))

    def test_typed_arguments(self):
        server = self.start(self.handler, token="secret")
        db = self.connect(server)
        db.rpush("todo", "milk", 2)

        cmd = server.received[0]
        self.assertEqual([a["body"] for a in cmd],
                         [b"rpush", b"todo", b"milk", b"2"])
        # 命令名和字符串参数走 +；数字走 $
        self.assertEqual([a["forced"] for a in cmd],
                         [False, True, True, False])
        self.assertFalse(cmd[3]["forced"])


class ErrorTests(ClientTest):
    CODES = {
        "bad_usage": BadUsageError,
        "no_hook": NoHookError,
        "type_mismatch": TypeMismatchError,
        "out_of_range": OutOfRangeError,
        "no_key": NoKeyError,
        "no_field": NoFieldError,
        "empty": EmptyError,
        "name_taken": NameTakenError,
        "unknown_command": UnknownCommandError,
    }

    def test_codes_map_to_exceptions(self):
        def handler(req):
            code = req[1]["body"].decode("ascii")
            return reply_json({"ok": False, "code": code,
                               "text": "ERR %s" % code})

        server = self.start(handler, token="secret")
        db = self.connect(server)
        for code, expected in self.CODES.items():
            with self.assertRaises(expected) as caught:
                db.execute("boom", code)
            self.assertEqual(caught.exception.code, code)

    def test_call_does_not_raise_on_failure(self):
        def handler(req):
            return reply_json({"ok": False, "code": "no_hook",
                               "text": "ERR no such hook"})

        server = self.start(handler, token="secret")
        db = self.connect(server)
        doc = db.call("llen", "missing")
        self.assertFalse(doc["ok"])
        self.assertEqual(doc["code"], "no_hook")


class TransportTests(ClientTest):
    def test_half_packet(self):
        data = reply_json({"ok": True, "code": "ok", "kind": "str",
                           "value": "一\r\n条", "text": "OK"})
        server = self.start(lambda req: data, token="secret", chunk_delay=0.005)
        db = self.connect(server)
        self.assertEqual(db.lget("x", 0), "一\r\n条")

    def test_shutdown_closes_connection(self):
        def handler(req):
            if req[0]["body"] == b":shutdown":
                return Close(reply_json({"ok": True, "code": "ok",
                                         "text": "服务正在停机"}))
            raise AssertionError("只期望 :shutdown")

        server = self.start(handler, token="secret")
        db = self.connect(server)
        self.assertEqual(db.shutdown(), "服务正在停机")
        with self.assertRaises(ConnectionError):
            db.hooks()

    def test_connection_drop(self):
        server = self.start(lambda req: None, token="secret")
        db = self.connect(server)
        with self.assertRaises(ConnectionError):
            db.llen("todo")

    def test_bad_framing(self):
        server = self.start(lambda req: b"$3\r\nabcXX", token="secret")
        db = self.connect(server)
        with self.assertRaises(ProtocolError):
            db.llen("todo")

    def test_non_utf8_value_roundtrips(self):
        raw = b'{"ok":true,"code":"ok","kind":"str","value":"a\xffb","text":""}'
        server = self.start(lambda req: reply_raw(raw), token="secret")
        db = self.connect(server)
        value = db.lget("x", 0)
        self.assertEqual(value.encode("utf-8", "surrogateescape"), b"a\xffb")


if __name__ == "__main__":
    unittest.main()
