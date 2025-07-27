<img src=".logo/Mhuixs-logo.png" height="130px" />     

# Mhuixs 数据库

[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/hujiyo/Mhuixs)


## 1. 介绍

Mhuixs 是一个基于内存的数据库，致力于为用户提供高效、灵活的数据管理能力。
本项目结合了关系型与非关系型数据库的优势，目标是实现国产化、易于集成、支持多样化数据结构，并具备简洁的语言特性。
**当前状态**: 客户端 muixclt 已基本完成，支持完整的 NAQL 语法解析和网络通信功能。

## 2. 目标 & 特色

基于内存的高速数据库在未来预估具有广阔的应用前景。Mhuixs 虽然定位为轻量级软件，但目标具备以下特点：

- 同时支持关系型表数据管理和非关系型键值对池数据管理，功能全面。
- 基于内存,使用创建索引、哈希表等方法定位数据对象指针
- 支持多种数据结构，满足不同场景需求。
- 提供简洁且安全的权限分级管理，保障数据安全。
- 语法设计简明，便于理解和使用，适合 AI 及开发者快速上手。
- 特殊数据压缩调控策略,在内存和处理性能之间取得平衡
- 支持 MCP 协议,全面兼容AI agent。

## 3. 当前总览

项目文件目录结构如下：

- `module`：核心数据结构实现（表、键库、列表、位图、流等）。
- `lib`：基础库与工具集（类型定义、内存池、加密、会话等）。
- `hub`：服务端主控模块与执行引擎。
- `muixclt`：**客户端命令行工具（已完成）**，包含完整的 NAQL 解析器和网络通信功能。
- `share`：共享模块。

### 开发进度

- ✅ **muixclt 客户端**: 已完成基本功能，支持 NAQL 语法解析、网络通信、变量管理等基础功能
- 🚧 **server 服务端**: 开发中

## 4. 客户端 muixclt

<details>
<summary>客户端 muixclt 说明文档（点击展开）</summary>

# Mhuixs 数据库客户端

## 核心特性

muixclt 是一个完整的 NAQL 解释器，采用厚客户端模式，具备：

- **NAQL 词法分析器**: 1926行代码，支持完整的 NAQL 语法解析
- **MUIX 包协议**: 自定义二进制协议，提供可靠的数据传输
- **HUJI 命令协议**: 高效的命令序列化和传输机制
- **本地控制器**: 支持 IF/WHILE/FOR 等控制语句的本地执行
- **变量系统**: 客户端变量管理和宏替换功能

## 构建和运行

```bash
# 构建
cd Mhuixs/muixclt && make

# 运行
./muixclt                    # 交互模式
./muixclt -f script.naql     # 批处理模式
./muixclt -s IP -p PORT      # 连接指定服务器
```

**系统要求**: Linux, GCC 4.9+, OpenSSL, Readline

### TCP/TLS支持说明

muixclt客户端现在支持通过编译时宏 `start_tls` 来控制是否使用TLS加密连接。默认情况下，客户端使用明文TCP连接。

#### 编译模式

##### TCP模式（默认）
```bash
make                # 编译TCP版本
make info           # 查看编译信息
make run            # 运行TCP版本
make test           # 测试TCP连接
```

##### TLS模式
```bash
make tls            # 编译TLS版本
make run-tls        # 运行TLS版本
make test-tls       # 测试TLS连接
```

##### 通过环境变量控制
```bash
make TLS=1          # 通过环境变量启用TLS
make TLS=1 run      # 编译并运行TLS版本
```


##### 查看编译信息
```bash
make info
```

输出示例：
```
编译信息:
  CC: gcc
  CFLAGS: -Wall -Wextra -std=c99 -O2 -g
  LDFLAGS: -lreadline
  BUILD_MODE: TCP
  SOURCES: muixclt.c pkg.c lexer.c variable.c flow_controller.c logo.c netlink.c controller.c merr.c stdstr.c funseq.c
```

#### 依赖安装

##### 基础依赖（TCP模式）
```bash
# Ubuntu/Debian
make install-deps

# CentOS/RHEL
make install-deps-rhel
```

##### TLS依赖（TLS模式）
```bash
# Ubuntu/Debian
make install-deps-tls

# CentOS/RHEL
make install-deps-tls-rhel
```

#### 运行时区别

##### TCP模式
- 连接提示：`正在连接到 127.0.0.1:18482 (TCP模式)...`
- 成功提示：`✓ 已成功连接到 Mhuixs 服务器 (TCP明文)`
- 数据传输：明文传输，性能较高

##### TLS模式
- 连接提示：`正在连接到 127.0.0.1:18482 (TLS模式)...`
- 成功提示：`✓ 已成功连接到 Mhuixs 服务器 (TLS加密)`
- 数据传输：加密传输，安全性高

#### 调试和发布版本

##### 调试版本
```bash
make debug          # TCP调试版本
make debug-tls      # TLS调试版本
```

##### 发布版本
```bash
make release        # TCP发布版本
make release-tls    # TLS发布版本
```

### 注意事项

1. **服务器兼容性**：确保服务器支持对应的连接模式
2. **证书配置**：TLS模式需要正确的证书配置
3. **性能考虑**：TLS模式会有额外的加密开销
4. **依赖管理**：TLS模式需要安装OpenSSL开发库


### 帮助信息

运行 `make help` 查看所有可用的编译选项和使用方法。 #################


## 协议架构

### MUIX 包协议 (pkg.h)

用于客户端与服务器间的可靠数据传输：

```
包结构: [MUIX(4字节)] + [长度(4字节)] + [$(1字节)] + [用户数据]
- 魔数: 'M''U''I''X'
- 长度: 大端字节序，表示用户数据长度
- 结束符: '$'
- 最大包大小: 4KB
```

核心功能：
- `create_packet()`: 创建数据包
- `serialize_packet()`: 序列化为网络传输格式
- `deserialize_packet()`: 反序列化接收数据
- `find_packet_boundary()`: 流式数据中定位完整包

### HUJI 命令协议 (muixclt.c)

NAQL 语句转换为二进制命令格式：

```
命令格式: [HUJI(4字节)] + [编号(4字节)] + [参数流]
参数流: [参数数目(1字节)] + [@] + [参数1长度] + [@] + [参数1] + [@] + ...
```

**命令编号体系**:
- 1-50: 基础语法 (GET, HOOK, DESC 等)
- 51-70: 事务控制 (MULTI, EXEC, ASYNC)
- 71-90: 控制语句 (IF, WHILE, FOR - 本地处理)
- 101-150: TABLE 操作 (FIELD, ADD, SET 等)
- 151-200: KVALOT 操作 (EXISTS, SET, GET 等)
- 201-280: STREAM/LIST 操作
- 281-330: BITMAP 操作
- 361-370: 变量管理 (本地处理)

**本地处理**: 控制语句和变量操作在客户端本地执行，不发送给服务器。

## NAQL 语法示例

```sql
# 表操作
HOOK TABLE users;
FIELD ADD id i4 PKEY;
FIELD ADD name str NOTNULL;
ADD 1 'Alice' 25;
GET WHERE id == 1;

# 键值操作
HOOK KVALOT cache;
SET user:1 'Alice';
GET user:1;

# 控制语句 (本地执行)
$counter = 0;
FOR i 1 10 1;
    ADD $i 'user$i';
    $counter = $counter + 1;
END;
```

## 交互命令

```
\q, \quit      退出
\h, \help      帮助
\c, \connect   连接服务器
\s, \status    显示状态
\v, \verbose   切换详细模式
```

## 调试功能

```bash
./muixclt -d    # 调试模式，显示协议数据包内容
./muixclt -v    # 详细模式，显示网络通信信息
```

调试模式下可查看：
- NAQL 语句的词法分析结果
- 生成的 HUJI 协议数据包
- 网络传输的 MUIX 包格式

</details>


#### 注意 : readme由chatGPT生成，可能会有错误，欢迎指正。实际以项目代码为准。

## 5. 参与与交流

欢迎对数据库、AI数据库方向感兴趣的朋友加入，一起完成 Mhuixs！

- Email：Mhuxis@outlook.com | Mhuxis.db@gmail.com | Mhuxis.db@outlook.com 
- WeChat：wx17601516389

## 6.重要日期

- 项目开始：2024.10.17
- muixclt 客户端完成：2024.12.20
- 本次README更新时间：2024.12.20

## 7.NAQL草案 >~<

NAQL：NAture-language Query Language

旨在设计一种最接近口语的、最简单、给AI可以直接现场学会的数据查询语言。

未来我们会出一版专门提供给AI看的查询语言 " 学习资料 "。

[👉 NAQL基础语法文档](Mhuixs/share/NAQL.txt)

**NAQL 语法特点**：
- 接近自然语言的语法设计
- 简洁明了的命令结构
- 支持复杂的数据查询和操作
- 专为 AI 优化的语法规则

基础语法是lexer直接解释和转化的，lexer之前还需要有个环节将不定形式的语法转化为标准语法

## 8.致谢 && 依赖

0. 致敬开源社区

```
感谢开源社区，特别是上述第三方库的作者们。正是他们的无私奉献和优秀工作，
让Mhuixs能够站在巨人的肩膀上发展。我在这里祝你们心想事成、事业有成、阖家幸福、幸福安康。

————HuJiYo 2025

Thanks to the open-source community, especially the authors of the aforementioned 
third-party libraries. It is their selfless dedication and excellent work that have 
enabled Mhuixs to develop by standing on the shoulders of giants. Here, I wish you 
all the best in your endeavors, success in your careers, happiness for your families, 
and health and well-being.

————HuJiYo 2025
```


1. 感谢Claude、GPT、Qwen、Deepseek、DouBao系列模型（~QvQ~）。

2. 感谢坚持不放弃的自己（doge）！



