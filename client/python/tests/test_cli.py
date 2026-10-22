"""CLI 的切词、续行、排版与配置优先级；这些是旧脚本必须保持的行为。"""

import json
import os
import re
import unittest
from unittest import mock

import mhuixs
from mhuixs._protocol import Forced
from mhuixs.cli import (
    CommandFeed,
    QuoteOpen,
    CliError,
    render,
    resolve_host_port,
    resolve_token,
    split_line,
    to_protocol_args,
    tokenize,
)


class TokenizeTests(unittest.TestCase):
    def test_words_and_quotes(self):
        self.assertEqual(tokenize('rpush todo "two words"'),
                         [("rpush", False), ("todo", False),
                          ("two words", True)])
        self.assertEqual(tokenize("lset x 0 'a b'"),
                         [("lset", False), ("x", False), ("0", False),
                          ("a b", True)])

    def test_comments(self):
        self.assertEqual(tokenize("llen todo   # 注释"),
                         [("llen", False), ("todo", False)])
        self.assertEqual(tokenize('rpush todo "a # b"'),
                         [("rpush", False), ("todo", False), ("a # b", True)])

    def test_unclosed_quote(self):
        with self.assertRaises(QuoteOpen):
            tokenize('rpush todo "milk')

    def test_limits(self):
        with self.assertRaises(CliError):
            split_line(" ".join(["x"] * 49))
        with self.assertRaises(CliError):
            split_line('"' + "x" * 256 + '"')

    def test_protocol_args(self):
        args = to_protocol_args([("rpush", False), ("todo", False),
                                 ("123", True)])
        self.assertEqual(args[0], "rpush")
        self.assertEqual(args[1], "todo")
        self.assertIsInstance(args[2], Forced)
        self.assertEqual(args[2].value, "123")

        # 命令名即使被引号包过也保持普通字符串（与旧行为一致）
        args = to_protocol_args([("llen", True), ("todo", False)])
        self.assertEqual(args[0], "llen")
        self.assertNotIsInstance(args[0], Forced)


class FeedTests(unittest.TestCase):
    def test_multiline_quote(self):
        feed = CommandFeed()
        self.assertIsNone(feed.feed('rpush todo "milk'))
        self.assertEqual(feed.feed('egg"'),
                         ["rpush", "todo", Forced("milk egg")])

    def test_blank_and_comment(self):
        feed = CommandFeed()
        self.assertIsNone(feed.feed(""))
        self.assertIsNone(feed.feed("   # 整行注释"))
        self.assertEqual(feed.feed("llen todo"), ["llen", "todo"])

    def test_flush(self):
        feed = CommandFeed()
        feed.feed('rpush todo "half')
        self.assertEqual(feed.flush(), 'rpush todo "half')
        self.assertIsNone(feed.flush())


class RenderTests(unittest.TestCase):
    def test_value_mode(self):
        self.assertEqual(render({"ok": True, "kind": "int", "value": 2,
                                 "text": "OK 2"}, "value"), ("2", True))
        self.assertEqual(render({"ok": True, "kind": "bool", "value": True,
                                 "text": "OK 1"}, "value"), ("true", True))
        self.assertEqual(render({"ok": True, "kind": "list",
                                 "value": ["a", "b"], "text": "OK"},
                                "value"), ("a\nb", True))
        self.assertEqual(render({"ok": True, "kind": "null",
                                 "text": "OK (empty)"}, "value"), ("", True))

    def test_pretty_and_text(self):
        result = {"ok": True, "kind": "int", "value": 2, "text": "OK 2"}
        self.assertEqual(render(result, "pretty"), ("OK 2", True))
        self.assertEqual(render(result, "text"), ("OK 2", True))

    def test_json(self):
        result = {"ok": True, "kind": "int", "value": 2, "text": "OK 2"}
        body, ok = render(result, "json")
        self.assertTrue(ok)
        self.assertEqual(json.loads(body), result)

    def test_failure_prefix(self):
        self.assertEqual(render({"ok": False, "code": "no_hook",
                                 "text": "ERR no such hook"}, "value"),
                         ("ERR no such hook", False))
        self.assertEqual(render({"ok": False, "code": "no_hook",
                                 "text": "missing"}, "value"),
                         ("ERR missing", False))


class ConfigTests(unittest.TestCase):
    def test_token_priority(self):
        with mock.patch.dict(os.environ, {"MHUIXS_TOKEN": "env"}, clear=False):
            self.assertEqual(resolve_token("flag"), "flag")
            self.assertEqual(resolve_token(None), "env")

        with mock.patch.dict(os.environ, {}, clear=True):
            with mock.patch("mhuixs.cli.read_config",
                            return_value={"authtoken": "file"}):
                self.assertEqual(resolve_token(None), "file")

    def test_host_port_fallbacks(self):
        with mock.patch.dict(os.environ, {}, clear=True):
            with mock.patch("mhuixs.cli.read_config",
                            return_value={"bind": "10.0.0.5", "port": "19000"}):
                self.assertEqual(resolve_host_port(None, None),
                                 ("10.0.0.5", 19000))
                self.assertEqual(resolve_host_port("127.0.0.1", 1),
                                 ("127.0.0.1", 1))

        with mock.patch.dict(os.environ, {"MHUIXS_HOST": "192.168.0.2",
                                          "MHUIXS_PORT": "19001"}, clear=True):
            with mock.patch("mhuixs.cli.read_config", return_value={}):
                self.assertEqual(resolve_host_port(None, None),
                                 ("192.168.0.2", 19001))


class VersionTests(unittest.TestCase):
    def test_matches_c_source(self):
        root = os.path.abspath(os.path.join(os.path.dirname(__file__),
                                            "..", "..", ".."))
        path = os.path.join(root, "src", "Mhuixs.c")
        if not os.path.exists(path):
            self.skipTest("没有 C 源码，跳过版本同步检查")
        with open(path, "r", encoding="utf-8", errors="replace") as fp:
            text = fp.read()
        match = re.search(r'#define\s+MHUIXS_VERSION\s+"([^"]+)"', text)
        self.assertIsNotNone(match)
        self.assertEqual(match.group(1), mhuixs.__version__)


if __name__ == "__main__":
    unittest.main()
