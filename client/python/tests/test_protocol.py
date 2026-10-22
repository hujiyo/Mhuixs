"""MHP/1 编解码的离线测试：黄金字节 + 半包/粘包。"""

import unittest
from decimal import Decimal

from mhuixs._errors import ProtocolError
from mhuixs._protocol import Forced, encode_request, parse_reply


class EncodeTests(unittest.TestCase):
    def test_plain_strings_are_bulk(self):
        # 裸字符串走 $，由服务端自动判定数字/字符串（保持旧 call() 语义）
        self.assertEqual(encode_request(["llen", "todo"]),
                         b"*2\r\n$4\r\nllen\r\n$4\r\ntodo\r\n")

    def test_forced_is_simple_string(self):
        self.assertEqual(encode_request([Forced("llen"), Forced("123")]),
                         b"*2\r\n+llen\r\n+123\r\n")

    def test_numbers(self):
        self.assertEqual(encode_request([42]), b"*1\r\n$2\r\n42\r\n")
        self.assertEqual(encode_request([True]), b"*1\r\n$1\r\n1\r\n")
        self.assertEqual(encode_request([False]), b"*1\r\n$1\r\n0\r\n")
        self.assertEqual(encode_request([Decimal("1.20")]),
                         b"*1\r\n$4\r\n1.20\r\n")

    def test_decimal_exponent_is_expanded(self):
        # 服务端的数字语法不认科学计数法
        self.assertEqual(encode_request([Decimal("1E+3")]),
                         b"*1\r\n$4\r\n1000\r\n")

    def test_floats(self):
        self.assertEqual(encode_request([0.1]), b"*1\r\n$3\r\n0.1\r\n")
        self.assertEqual(encode_request([1e-7]),
                         b"*1\r\n$9\r\n0.0000001\r\n")
        self.assertEqual(encode_request([1e20]),
                         b"*1\r\n$21\r\n100000000000000000000\r\n")

    def test_utf8_length_is_bytes(self):
        self.assertEqual(encode_request(["牛奶"]),
                         b"*1\r\n$6\r\n" + "牛奶".encode("utf-8") + b"\r\n")

    def test_bytes_are_binary_safe(self):
        self.assertEqual(encode_request([b"a\xffb"]),
                         b"*1\r\n$3\r\na\xffb\r\n")

    def test_crlf_allowed_in_bulk(self):
        self.assertEqual(encode_request(["a\r\nb"]),
                         b"*1\r\n$4\r\na\r\nb\r\n")

    def test_forced_rejects_crlf(self):
        with self.assertRaises(ValueError):
            encode_request([Forced("a\nb")])
        with self.assertRaises(ValueError):
            encode_request([Forced("a\rb")])

    def test_nul_is_rejected(self):
        with self.assertRaises(ValueError):
            encode_request(["a\x00b"])
        with self.assertRaises(ValueError):
            encode_request([b"a\x00b"])

    def test_argument_limits(self):
        with self.assertRaises(ValueError):
            encode_request(["x" * 256])
        with self.assertRaises(ValueError):
            encode_request(["x"] * 49)
        with self.assertRaises(ValueError):
            encode_request([])

    def test_unsupported_values(self):
        with self.assertRaises(TypeError):
            encode_request([None])
        with self.assertRaises(ValueError):
            encode_request([float("inf")])
        with self.assertRaises(ValueError):
            encode_request([float("nan")])
        with self.assertRaises(ValueError):
            encode_request([Decimal("Infinity")])
        with self.assertRaises(ValueError):
            encode_request([Decimal("NaN")])


class ParseTests(unittest.TestCase):
    def test_simple_and_error(self):
        self.assertEqual(parse_reply(b"+OK\r\n"), ("+", b"OK", 5))
        self.assertEqual(parse_reply(b"-NOAUTH invalid token\r\n"),
                         ("-", b"NOAUTH invalid token", 23))

    def test_bulk(self):
        self.assertEqual(parse_reply(b"$5\r\nhello\r\n"),
                         ("$", b"hello", 11))
        self.assertEqual(parse_reply(b"$0\r\n\r\n"), ("$", b"", 6))

    def test_body_may_contain_crlf(self):
        data = b"$4\r\na\r\nb\r\n"
        self.assertEqual(parse_reply(data), ("$", b"a\r\nb", len(data)))

    def test_every_prefix_is_incomplete(self):
        data = b"$4\r\na\r\nb\r\n"
        for i in range(len(data)):
            self.assertIsNone(parse_reply(data[:i]), "前缀长度 %d" % i)

    def test_sticky_replies(self):
        data = b"+OK\r\n" + b"$2\r\n{}\r\n"
        first = parse_reply(data)
        self.assertEqual(first, ("+", b"OK", 5))
        rest = data[first[2]:]
        self.assertEqual(parse_reply(rest), ("$", b"{}", len(rest)))

    def test_bad_framing(self):
        with self.assertRaises(ProtocolError):
            parse_reply(b"$3\r\nabcXX")
        with self.assertRaises(ProtocolError):
            parse_reply(b"$x\r\n")
        with self.assertRaises(ProtocolError):
            parse_reply(b"$-1\r\n")
        with self.assertRaises(ProtocolError):
            parse_reply(b"!oops\r\n")

    def test_empty_buffer(self):
        self.assertIsNone(parse_reply(b""))


if __name__ == "__main__":
    unittest.main()
