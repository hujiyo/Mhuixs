# Mhuixs 数据库客户端

欢迎使用 Mhuixs 数据库客户端！本目录包含三个版本的客户端实现：C、C++ 和 Python。

## 目录结构

```
muixclt/
├── C/                  # C版本客户端
│   ├── clt.c          # 主程序
│   ├── pkg.c          # 数据包处理
│   ├── pkg.h          # 数据包处理头文件
│   ├── lexer.c        # 词法分析器
│   ├── lexer.h        # 词法分析器头文件
│   ├── logo.c         # Logo显示
│   ├── logo.h         # Logo显示头文件
│   └── Makefile       # 构建脚本
├── Cpp/               # C++版本客户端
│   ├── clt.cpp        # 主程序
│   └── CMakeLists.txt # 构建脚本
├── Python/            # Python版本客户端
│   ├── clt.py         # 主程序
│   └── requirements.txt # 依赖文件
├── build.sh           # 总构建脚本
├── NAQL.txt          # NAQL语言文档
└── README.md         # 说明文档（本文件）
```

## 快速开始

### 1. 检查依赖并构建所有版本

```bash
# 进入客户端目录
cd muixclt

# 构建所有版本的客户端
./build.sh

# 或者检查依赖
./build.sh deps
```

### 2. 单独构建特定版本

```bash
# 仅构建C版本
./build.sh c

# 仅构建C++版本  
./build.sh cpp

# 仅准备Python版本
./build.sh python
```

### 3. 运行客户端

```bash
# C版本
./C/mhuixs-client

# C++版本
./Cpp/build/bin/mhuixs-client-cpp

# Python版本
python3 ./Python/clt.py
```

## 系统要求

### 基本依赖

- **操作系统**: Linux (Ubuntu/Debian/CentOS/RHEL)
- **编译器**: GCC 4.9+ 或 Clang 3.5+
- **构建工具**: Make, CMake 3.12+
- **Python**: Python 3.6+

### 开发库

- **OpenSSL**: libssl-dev (Ubuntu/Debian) 或 openssl-devel (CentOS/RHEL)
- **Readline**: libreadline-dev (Ubuntu/Debian) 或 readline-devel (CentOS/RHEL)

### 安装依赖命令

**Ubuntu/Debian:**
```bash
sudo apt-get update
sudo apt-get install build-essential cmake python3 libssl-dev libreadline-dev
```

**CentOS/RHEL:**
```bash
sudo yum install gcc gcc-c++ cmake python3 openssl-devel readline-devel
```

## 详细构建说明

### C 版本

C版本使用传统的Makefile构建系统：

```bash
cd C
make all          # 编译
make clean        # 清理
make install-deps # 安装依赖（Ubuntu/Debian）
make run          # 编译并运行
make help         # 显示帮助
```

### C++ 版本

C++版本使用CMake构建系统：

```bash
cd Cpp
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

或使用构建脚本：
```bash
./build.sh cpp
```

### Python 版本

Python版本使用标准库，无需编译：

```bash
cd Python
chmod +x clt.py
python3 clt.py
```

## 使用说明

### 命令行选项

所有版本的客户端都支持以下选项：

```bash
-h, --help              显示帮助信息
-v, --version           显示版本信息
-s, --server <IP>       指定服务器IP地址 (默认: 127.0.0.1)
-p, --port <端口>       指定服务器端口 (默认: 18482)
-f, --file <文件>       从文件批量执行查询
-V, --verbose           详细模式
-q, --quiet             静默模式
```

### 交互式命令

在交互模式下，支持以下内置命令：

```
\q, \quit              退出客户端
\h, \help              显示帮助信息
\c, \connect           连接到服务器
\d, \disconnect        断开连接
\s, \status            显示连接状态
\v, \verbose           切换详细模式
\clear                 清屏
```

### 使用示例

#### 基本使用

```bash
# 启动交互模式（自动连接到本地服务器）
./C/mhuixs-client

# 连接到指定服务器
./C/mhuixs-client -s 192.168.1.100 -p 18482

# 批量执行查询文件
./C/mhuixs-client -f queries.naql

# 详细模式
./C/mhuixs-client -v
```

#### NAQL 查询示例

```sql
# 创建表
HOOK TABLE users;

# 添加字段
FIELD ADD id i4 PKEY;
FIELD ADD name str NOTNULL;
FIELD ADD age i4;
FIELD ADD email str UNIQUE;

# 插入数据
ADD 1 '张三' 25 'zhangsan@example.com';
ADD 2 '李四' 30 'lisi@example.com';

# 查询数据
GET;                        # 查询所有数据
WHERE name == '张三';       # 条件查询
GET FIELD name;             # 查询指定字段

# 更新数据
SET 1 2 30;                 # 更新第1行第2个字段为30

# 删除数据
DEL 2;                      # 删除第2行
```

#### 批处理文件示例

创建文件 `example.naql`：

```sql
# 示例批处理文件
HOOK TABLE test_table;
FIELD ADD id i4;
FIELD ADD name str;
ADD 1 'test1';
ADD 2 'test2';
GET;
```

执行批处理：

```bash
./C/mhuixs-client -f example.naql
```

## 版本对比

| 特性 | C版本 | C++版本 | Python版本 |
|------|-------|---------|------------|
| 性能 | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐ |
| 内存占用 | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐ |
| 易于维护 | ⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ |
| 跨平台 | ⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ |
| 开发效率 | ⭐⭐ | ⭐⭐⭐ | ⭐⭐⭐⭐⭐ |

### 选择建议

- **C版本**: 适合对性能要求极高的场景，如嵌入式系统
- **C++版本**: 平衡性能和开发效率，适合企业级应用
- **Python版本**: 适合快速原型开发、脚本化使用和学习

## 故障排除

### 常见问题

1. **编译错误**: 缺少依赖库
   ```bash
   # 检查依赖
   ./build.sh deps
   
   # 安装依赖
   sudo apt-get install libssl-dev libreadline-dev  # Ubuntu/Debian
   sudo yum install openssl-devel readline-devel    # CentOS/RHEL
   ```

2. **连接失败**: 服务器未启动或网络问题
   ```bash
   # 检查服务器状态
   telnet 127.0.0.1 18482
   
   # 使用详细模式查看错误信息
   ./C/mhuixs-client -v
   ```

3. **权限错误**: 执行权限不足
   ```bash
   chmod +x build.sh
   chmod +x Python/clt.py
   ```

### 调试模式

```bash
# C版本调试
cd C
make debug
gdb ./mhuixs-client

# C++版本调试
cd Cpp/build
make debug_run

# Python版本调试
python3 -m pdb Python/clt.py
```

## 开发指南

### 添加新功能

1. 修改对应版本的源代码
2. 更新构建脚本（如需要）
3. 添加测试用例
4. 更新文档

### 代码规范

- **C版本**: 遵循C99标准
- **C++版本**: 遵循C++17标准，使用现代C++特性
- **Python版本**: 遵循PEP 8规范

### 测试

```bash
# 运行所有测试
./build.sh test

# 单独测试某个版本
./C/mhuixs-client --help
python3 ./Python/clt.py --help
./Cpp/build/bin/mhuixs-client-cpp --help
```

## 协议说明

客户端与服务器使用自定义的二进制协议通信：

- **传输**: SSL/TLS加密的TCP连接
- **端口**: 18482 (默认)
- **包格式**: 包头(16字节) + 数据 + 校验和(4字节)
- **字节序**: 网络字节序 (大端)

详细协议规范请参考源代码中的 `pkg.h` 或 `pkg.c` 文件。

## 许可证

版权所有 (c) Mhuixs-team 2024  
许可证协议: 任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品

## 联系方式

- **Email**: hj18914255909@outlook.com
- **微信**: wx17601516389
- **项目主页**: [GitHub Repository]

## 更新日志

### v1.0.0 (2024-12-XX)
- ✅ 完成C版本客户端
- ✅ 完成C++版本客户端
- ✅ 完成Python版本客户端
- ✅ 实现SSL加密通信
- ✅ 支持NAQL查询语言
- ✅ 提供交互式和批处理模式
- ✅ 跨平台构建支持
- ✅ 完整的文档和示例 