# mhuixs

Mhuixs 内存数据结构服务端的 Python 客户端。协议是 MHP/1 —— 请求带长度前缀的
参数数组，响应是带长度前缀的 JSON 文档，二进制安全。

包内只有标准库依赖，Python 3.8+。

## 安装

```bash
pip install mhuixs
```

从源码安装：

```bash
pip install ./client/python
# 或从仓库直接装：
pip install "git+https://github.com/hujiyo/Mhuixs.git#subdirectory=client/python"
```

## 快速开始

先起服务端（令牌来自环境变量或 `Mhuixs.config` 的 `authtoken`）：

```bash
MHUIXS_TOKEN=你的令牌 ./mhuixs server
```

然后在 Python 里：

```python
import mhuixs

db = mhuixs.Mhuixs("127.0.0.1", 18185, token="你的令牌")

db.create_list("todo")
db.rpush("todo", "milk", 2)       # 字符串存字符串，数字存数字
db.llen("todo")                   # -> 2
db.lget("todo", 0)                # -> "milk"
db.lget("todo", 1)                # -> 2

db.create_table("users", ["id:int", "name:str"])
db.tadd("users", 1, "张三")
db.tget("users", 0, "name")       # -> "张三"

db.create_kvalot("conf")
db.kset("conf", "theme", "dark")
db.kget("conf", "theme")          # -> "dark"

db.create_bitmap("flags")
db.bset("flags", 3, 1)            # -> True
db.bcount("flags", 0, 7)          # -> 1

db.save()                         # 让服务端落盘
db.close()
```

也可以当上下文管理器：

```python
with mhuixs.Mhuixs(token="你的令牌") as db:
    print(db.hooks())
```

## 方法一览

| 分组 | 方法 | 返回 |
|---|---|---|
| 管理 | `create_list` / `create_bitmap` / `create_kvalot` / `create_table` | `None` |
| 管理 | `drop` | `None` |
| 管理 | `hooks` | `int` |
| 管理 | `info` / `type` | `str` |
| LIST | `rpush` / `lpush` | `int`（新长度） |
| LIST | `lpop` / `rpop` / `lget` / `lrem` | 值 |
| LIST | `llen` | `int` |
| LIST | `lset` | `None` |
| BITMAP | `bset` / `bget` | `bool` |
| BITMAP | `bcount` / `bsize` | `int` |
| KVALOT | `kset` / `kdel` / `klen` | `int` |
| KVALOT | `kget` | 值 |
| KVALOT | `kexists` | `bool` |
| TABLE | `tadd` / `tdel` / `trows` | `int` |
| TABLE | `tget` | 值或 `None`（空格子） |
| TABLE | `tset` | `None` |
| TABLE | `tfields` | `list[str]` |
| 会话 | `ping` | `bool` |
| 会话 | `save` / `shutdown` | `str` |

`tfields` 受服务端类型化返回值 32 项上限约束；字段更多时用 `call()` 看
`truncated`，或读 `info()` 的文本。

## 参数类型规则

类型化方法按 Python 类型决定值怎么存：

| Python 值 | 存进去是什么 |
|---|---|
| `str` / `bytes` | 字符串（含换行的值也能原样存） |
| `int` | 数字 |
| `float` / `Decimal` | 数字（按十进制定点写入，不使用科学计数法） |
| `bool` | 数字 `1` / `0` |
| `None` | 不支持，抛 `ValueError` / `TypeError` |

需要把数字样子的字符串强制存成字符串时，不需要额外包装 —— 类型化方法里的
`str` 本来就是字符串：

```python
db.rpush("numbers", "123")        # 字符串 "123"
db.rpush("numbers", 123)          # 数字 123
```

裸接口 `call()` / `execute()` 不做这层包装：普通 `str` 交给服务端自动判定
（`"123"` 会变数字），要强制字符串时用 `mhuixs.Forced`：

```python
db.execute("rpush", "numbers", mhuixs.Forced("123"))   # 字符串
```

服务端单个参数上限 255 字节、单条命令 48 个参数；客户端会在发送前检查并给出
明确错误。参数里不能有 NUL 字节，`Forced` 的值不能含 CR/LF（协议限制）。

## 返回值类型

| 服务端 kind | Python 类型 |
|---|---|
| `int` | `int` |
| `num` 无小数点 | `int` |
| `num` 有小数点 | `decimal.Decimal` |
| `str` | `str` |
| `bool` | `bool` |
| `null` | `None` |
| `list` | `list` |

小数用 `Decimal` 是为了不丢精度。`call()` 返回的是原始 JSON 文档，小数按标准
JSON 解析成 `float`；需要无损数值时用 `execute()` 或类型化方法。

服务端返回的非 UTF-8 字节会以 `surrogateescape` 形式进入 Python 字符串，
用 `value.encode("utf-8", "surrogateescape")` 可以还原原始字节。

## 原始接口

```python
db.call("llen", "todo")
# {'ok': True, 'code': 'ok', 'kind': 'int', 'value': 2, 'text': 'OK 2'}

db.execute("llen", "todo")        # -> 2
```

- `call()` 永远返回完整 dict；服务端业务失败（`ok: false`）不抛异常。
- `execute()` 在 `ok: false` 时抛对应异常，否则返回值。

## 异常

所有异常都继承 `mhuixs.MhuixsError`。服务端失败响应按稳定的 `code` 映射：

| code | 异常 |
|---|---|
| `bad_usage` | `BadUsageError` |
| `no_hook` | `NoHookError` |
| `no_object` | `NoObjectError` |
| `type_mismatch` | `TypeMismatchError` |
| `out_of_range` | `OutOfRangeError` |
| `no_key` | `NoKeyError` |
| `no_field` | `NoFieldError` |
| `empty` | `EmptyError` |
| `name_taken` | `NameTakenError` |
| `unknown_command` | `UnknownCommandError` |
| 其他 | `CommandError` |

连接断开或关闭抛 `ConnectionError`；认证失败抛 `AuthenticationError`；
分帧损坏抛 `ProtocolError`。业务异常带 `.code` / `.text` 属性。

## 命令行客户端

安装后会有一个 `mhuixs-cli` 命令；没安装时用 `python -m mhuixs`：

```bash
mhuixs-cli                          # 交互模式
mhuixs-cli 'llen todo'              # 执行一条命令
mhuixs-cli - < script.mhx           # 从 stdin / 管道逐行读
python -m mhuixs 'rpush todo milk'  # 等价写法
```

令牌按 `--token` > 环境变量 `MHUIXS_TOKEN` > `Mhuixs.config` 的 `authtoken`
顺序回落。输出版式用 `--format value|pretty|text|json`（默认 `value`）。
交互模式里 `:help` 看元命令，`:quit` 退出。

## 线程与连接

一个 `Mhuixs` 实例对应一条连接，内部有锁，多个线程共用一个实例时命令会串行
执行。想并行请每个线程各建一个实例。服务端当前一次只服务一个连接，所以不做
连接池和自动重连。

## 测试

```bash
cd client/python
python -m unittest discover -s tests -v
```

测试只用标准库，不依赖真实服务端：协议黄金字节、半包/粘包、认证、错误映射
都由本地假服务端覆盖。

## 协议与许可

- 线格式规范：[`src/proto.h`](https://github.com/hujiyo/Mhuixs/blob/main/src/proto.h)
- 项目主页：<https://github.com/hujiyo/Mhuixs>
- 许可证：Apache License 2.0
