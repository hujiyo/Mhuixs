"""Mhuixs 的 Python 客户端。

    import mhuixs

    db = mhuixs.Mhuixs("127.0.0.1", 18185, token="你的令牌")
    db.create_list("todo")
    db.rpush("todo", "milk", 2)     # 字符串存字符串，数字存数字
    print(db.llen("todo"))          # -> 2
    print(db.lget("todo", 0))       # -> "milk"
    db.close()

需要服务端原始 JSON 文档时用 ``db.call(...)``；要解包后的 Python 值用
``db.execute(...)`` 或按数据结构分组的方法。协议参考实现见
``mhuixs._protocol``。
"""

from ._version import __version__
from ._client import Mhuixs
from ._errors import (
    AuthenticationError,
    BadUsageError,
    CommandError,
    ConnectionError,
    EmptyError,
    MhuixsError,
    NameTakenError,
    NoFieldError,
    NoHookError,
    NoKeyError,
    NoObjectError,
    OutOfRangeError,
    ProtocolError,
    TypeMismatchError,
    UnknownCommandError,
)
from ._protocol import Forced

__all__ = [
    "Mhuixs",
    "Forced",
    "MhuixsError",
    "ConnectionError",
    "ProtocolError",
    "AuthenticationError",
    "CommandError",
    "BadUsageError",
    "NoHookError",
    "NoObjectError",
    "TypeMismatchError",
    "OutOfRangeError",
    "NoKeyError",
    "NoFieldError",
    "EmptyError",
    "NameTakenError",
    "UnknownCommandError",
    "__version__",
]
