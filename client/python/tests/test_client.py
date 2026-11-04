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
    IndexStaleError,
    Mhuixs,
    MhuixsError,
    NameTakenError,
    NoFieldError,
    NoHookError,
    NoIndexError,
    NoKeyError,
    OutOfRangeError,
    ProtocolError,
    Rows,
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


class RowsTests(ClientTest):
    """kind == "rows" 的解包：列名与二维值都保留。"""

    def handler(self, req):
        cmd = req[0]["body"]
        if cmd == b"lrange":
            return reply_json({"ok": True, "code": "ok", "kind": "rows",
                               "columns": ["value"],
                               "value": [["a"], [2]],
                               "text": "OK 2 item(s)"})
        if cmd == b"trange":
            return reply_json({"ok": True, "code": "ok", "kind": "rows",
                               "columns": ["d", "name"],
                               "value": [[20240101, "a"], [20240102, None]],
                               "text": "OK 2 row(s)"})
        if cmd == b"mget":
            return reply_json({"ok": True, "code": "ok", "kind": "rows",
                               "columns": ["key", "value"],
                               "value": [["k", 1.5], ["nope", None]],
                               "text": "OK 2 key(s), 1 missing"})
        if cmd == b"tindex":
            return reply_json({"ok": True, "code": "ok", "kind": "int",
                               "value": 2, "text": "OK indexed t.d, 2 row(s)"})
        if cmd == b"tunindex":
            return reply_json({"ok": True, "code": "ok",
                               "text": "OK dropped index on 'd'"})
        if cmd == b"tindexes":
            return reply_json({"ok": True, "code": "ok", "kind": "rows",
                               "columns": ["column", "status"],
                               "value": [["d", "fresh"]],
                               "text": "OK 1 index(es)"})
        if cmd == b"tfind":
            return reply_json({"ok": True, "code": "ok", "kind": "rows",
                               "columns": ["d", "name"],
                               "value": [[20240101, "a"]],
                               "text": "OK 1 row(s)"})
        if cmd == b"tselect":
            return reply_json({"ok": True, "code": "ok", "kind": "rows",
                               "columns": ["d"],
                               "value": [[20240101], [20240102]],
                               "text": "OK 2 row(s)"})
        if cmd == b"torder":
            return reply_json({"ok": True, "code": "ok", "kind": "rows",
                               "columns": ["d", "name"],
                               "value": [[20240101, "a"], [20240102, "b"]],
                               "text": "OK 2 row(s)"})
        if cmd == b"boom":
            code = req[1]["body"].decode("ascii")
            return reply_json({"ok": False, "code": code, "text": "ERR"})
        raise AssertionError("没准备这个命令：%r" % (cmd,))

    def test_rows_unpack(self):
        server = self.start(self.handler, token="secret")
        db = self.connect(server)

        rows = db.lrange("l", 0, 1)
        self.assertIsInstance(rows, Rows)
        self.assertEqual(rows.columns, ["value"])
        self.assertEqual(rows.values, [["a"], [2]])

        rows = db.trange("t", 0, 1)
        self.assertEqual(rows.columns, ["d", "name"])
        self.assertEqual(rows.values, [[20240101, "a"], [20240102, None]])

    def test_rows_value_types(self):
        server = self.start(self.handler, token="secret")
        db = self.connect(server)

        rows = db.mget("cache", "k", "nope")
        self.assertEqual(rows.columns, ["key", "value"])
        self.assertEqual(rows.values[0][0], "k")
        self.assertEqual(rows.values[0][1], Decimal("1.5"))
        self.assertIsInstance(rows.values[0][1], Decimal)
        self.assertIsNone(rows.values[1][1])

    def test_columns_are_forwarded(self):
        server = self.start(self.handler, token="secret")
        db = self.connect(server)

        db.trange("t", 0, 1, "d", "name")
        sent = server.received[0]
        self.assertEqual([a["body"] for a in sent],
                         [b"trange", b"t", b"0", b"1", b"d", b"name"])
        # 列名是字符串参数，走 +；行号是数字，走 $
        self.assertEqual([a["forced"] for a in sent],
                         [False, True, False, False, True, True])

    def test_torder_forwarding(self):
        server = self.start(self.handler, token="secret")
        db = self.connect(server)

        rows = db.torder("t", "d", 0, 1, "name")
        self.assertEqual(rows.columns, ["d", "name"])
        self.assertEqual(rows.values, [[20240101, "a"], [20240102, "b"]])

        sent = server.received[0]
        self.assertEqual([a["body"] for a in sent],
                         [b"torder", b"t", b"d", b"0", b"1", b"name"])

    def test_index_commands(self):
        server = self.start(self.handler, token="secret")
        db = self.connect(server)

        self.assertEqual(db.tindex("t", "d"), 2)
        self.assertEqual(db.tunindex("t", "d"), "OK dropped index on 'd'")

        rows = db.tindexes("t")
        self.assertEqual(rows.columns, ["column", "status"])
        self.assertEqual(rows.values, [["d", "fresh"]])

        rows = db.tfind("t", "d", 20240101, "name")
        self.assertEqual(rows.values, [[20240101, "a"]])

        rows = db.tselect("t", "d", 20240101, 20240102)
        self.assertEqual(rows.columns, ["d"])
        self.assertEqual(rows.values, [[20240101], [20240102]])

        sent = server.received[-1]
        self.assertEqual([a["body"] for a in sent],
                         [b"tselect", b"t", b"d", b"20240101", b"20240102"])

    def test_index_errors_map(self):
        server = self.start(self.handler, token="secret")
        db = self.connect(server)

        with self.assertRaises(NoIndexError):
            db.execute("boom", "no_index")
        with self.assertRaises(IndexStaleError):
            db.execute("boom", "index_stale")


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
        "no_index": NoIndexError,
        "index_stale": IndexStaleError,
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


class PipelineTests(ClientTest):
    """流水线：一组命令一次发出，应答按顺序收回。"""

    def handler(self, req):
        cmd = req[0]["body"]
        if cmd == b"echo":
            return reply_json({"ok": True, "code": "ok", "kind": "str",
                               "value": req[1]["body"].decode("utf-8"),
                               "text": "OK"})
        if cmd == b"boom":
            return reply_json({"ok": False, "code": "bad_usage", "text": "ERR boom"})
        raise AssertionError("没准备这个命令：%r" % (cmd,))

    def test_results_in_order(self):
        server = self.start(self.handler, token="secret")
        db = self.connect(server)

        results = list(db.pipeline([("echo", "a"), ("echo", "b"), ("echo", "c")]))
        self.assertEqual(results, ["a", "b", "c"])
        self.assertEqual([r[0]["body"] for r in server.received],
                         [b"echo", b"echo", b"echo"])
        self.assertEqual([r[1]["body"] for r in server.received],
                         [b"a", b"b", b"c"])

    def test_batches(self):
        server = self.start(self.handler, token="secret")
        db = self.connect(server)

        results = list(db.pipeline([("echo", str(i)) for i in range(5)],
                                   batch_size=2))
        self.assertEqual(results, ["0", "1", "2", "3", "4"])

    def test_error_drains_remaining_replies(self):
        server = self.start(self.handler, token="secret")
        db = self.connect(server)

        with self.assertRaises(BadUsageError):
            list(db.pipeline([("echo", "a"), ("boom",), ("echo", "c")]))

        # 失败那一组的应答已经读完，连接仍停在正确位置
        self.assertEqual(list(db.pipeline([("echo", "after")])), ["after"])

    def test_rejects_bad_shapes(self):
        server = self.start(self.handler, token="secret")
        db = self.connect(server)

        with self.assertRaises(TypeError):
            list(db.pipeline(["echo"]))
        with self.assertRaises(ValueError):
            list(db.pipeline([()]))
        with self.assertRaises(ValueError):
            list(db.pipeline([("echo", "a")], batch_size=0))


class IterRowsTests(ClientTest):
    """分段读取：每页一次 trange，闭区间。"""

    def handler(self, req):
        cmd = req[0]["body"]
        if cmd == b"trows":
            total = 0 if req[1]["body"] == b"empty" else 5
            return reply_json({"ok": True, "code": "ok", "kind": "int",
                               "value": total, "text": "OK %d" % total})
        if cmd == b"trange":
            start = int(req[2]["body"])
            end = int(req[3]["body"])
            cols = [a["body"].decode("utf-8") for a in req[4:]] or ["i", "v"]
            rows = [[i, "v%d" % i] for i in range(start, end + 1)]
            return reply_json({"ok": True, "code": "ok", "kind": "rows",
                               "columns": cols, "value": rows,
                               "text": "OK %d row(s)" % len(rows)})
        raise AssertionError("没准备这个命令：%r" % (cmd,))

    def ranges(self, server):
        return [(int(r[2]["body"]), int(r[3]["body"]))
                for r in server.received if r[0]["body"] == b"trange"]

    def test_pages(self):
        server = self.start(self.handler, token="secret")
        db = self.connect(server)

        pages = list(db.iter_rows("t", "i", "v", page_size=2))
        self.assertEqual([p.values for p in pages],
                         [[[0, "v0"], [1, "v1"]],
                          [[2, "v2"], [3, "v3"]],
                          [[4, "v4"]]])
        self.assertEqual(self.ranges(server), [(0, 1), (2, 3), (4, 4)])

    def test_subrange_and_negative_index(self):
        server = self.start(self.handler, token="secret")
        db = self.connect(server)

        list(db.iter_rows("t", page_size=2, start=1, end=3))
        self.assertEqual(self.ranges(server), [(1, 2), (3, 3)])

        list(db.iter_rows("t", page_size=10, start=-2))
        self.assertEqual(self.ranges(server)[-1], (3, 4))

    def test_empty_table_yields_nothing(self):
        server = self.start(self.handler, token="secret")
        db = self.connect(server)

        self.assertEqual(list(db.iter_rows("empty")), [])

    def test_rejects_bad_page_size(self):
        server = self.start(self.handler, token="secret")
        db = self.connect(server)

        with self.assertRaises(ValueError):
            list(db.iter_rows("t", page_size=0))


class NewCommandTests(ClientTest):
    """新增命令的参数编码与返回值解包。"""

    def handler(self, req):
        cmd = req[0]["body"]
        if cmd == b"tupsert":
            return reply_json({"ok": True, "code": "ok", "kind": "int",
                               "value": 1, "text": "OK tupsert"})
        if cmd == b"rename":
            return reply_json({"ok": True, "code": "ok",
                               "text": "OK renamed 'a' -> 'b'"})
        if cmd == b"swap":
            return reply_json({"ok": True, "code": "ok",
                               "text": "OK swapped 'a' <-> 'b'"})
        raise AssertionError("没准备这个命令：%r" % (cmd,))

    def test_tupsert_sends_column_pairs(self):
        server = self.start(self.handler, token="secret")
        db = self.connect(server)

        self.assertEqual(db.tupsert("t", "id", 1,
                                    {"name": "alice", "age": 30}), 1)
        self.assertEqual([a["body"] for a in server.received[0]],
                         [b"tupsert", b"t", b"id", b"1",
                          b"name", b"alice", b"age", b"30"])

    def test_tupsert_accepts_pairs(self):
        server = self.start(self.handler, token="secret")
        db = self.connect(server)

        db.tupsert("t", "id", 1, ("name", "alice"))
        self.assertEqual([a["body"] for a in server.received[0]],
                         [b"tupsert", b"t", b"id", b"1", b"name", b"alice"])

    def test_rename_and_swap(self):
        server = self.start(self.handler, token="secret")
        db = self.connect(server)

        self.assertEqual(db.rename("a", "b"), "OK renamed 'a' -> 'b'")
        self.assertEqual(db.swap("a", "b"), "OK swapped 'a' <-> 'b'")
        self.assertEqual([r[0]["body"] for r in server.received],
                         [b"rename", b"swap"])


if __name__ == "__main__":
    unittest.main()
