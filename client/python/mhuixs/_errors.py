"""客户端异常。

服务端返回的每个失败响应都带一个稳定的 ``code``（见 `src/execute.c` 的
``mhx_code``），客户端据此抛出对应的异常子类，程序按类型分支即可，
不需要解析人类可读文案。
"""


class MhuixsError(Exception):
    """本客户端的异常基类。"""


class ConnectionError(MhuixsError):
    """连不上、连接中途断开，或连接已经关闭。"""


class ProtocolError(MhuixsError):
    """线格式不符合 MHP/1：响应分帧损坏、响应类型不认识等。"""


class AuthenticationError(MhuixsError):
    """AUTH 未通过：令牌不对，或第一条请求不是 AUTH。"""


class CommandError(MhuixsError):
    """服务端执行命令失败。``code`` 是稳定 token，``text`` 是人类可读说明。"""

    def __init__(self, code, text):
        super().__init__(text)
        self.code = code
        self.text = text


class BadUsageError(CommandError):
    """参数个数或格式不对。"""


class NoHookError(CommandError):
    """钩子不存在。"""


class NoObjectError(CommandError):
    """钩子没有挂对象。"""


class TypeMismatchError(CommandError):
    """钩子类型与命令不符。"""


class OutOfRangeError(CommandError):
    """下标、偏移或区间越界。"""


class NoKeyError(CommandError):
    """键不存在。"""


class NoFieldError(CommandError):
    """列不存在。"""


class EmptyError(CommandError):
    """容器为空。"""


class NameTakenError(CommandError):
    """钩子名已被占用。"""


class UnknownCommandError(CommandError):
    """命令不认识。"""


# 服务端 code -> 异常类。没有列出的 code 用 CommandError 兜底。
_CODE_MAP = {
    "bad_usage": BadUsageError,
    "no_hook": NoHookError,
    "no_object": NoObjectError,
    "type_mismatch": TypeMismatchError,
    "out_of_range": OutOfRangeError,
    "no_key": NoKeyError,
    "no_field": NoFieldError,
    "empty": EmptyError,
    "name_taken": NameTakenError,
    "unknown_command": UnknownCommandError,
}


def error_from_document(doc):
    """把 ``{"ok": false, "code": ..., "text": ...}`` 变成对应的异常对象。"""
    code = doc.get("code") or "failed"
    text = doc.get("text") or code
    return _CODE_MAP.get(code, CommandError)(code, text)
