# Mhuixs C客户端 Makefile
# 版权所有 (c) Mhuixs-team 2024

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -g -I../share
LDFLAGS = -lreadline

# TLS支持（可通过环境变量或make参数控制）
# 使用方法: make TLS=1 或 make tls
ifdef TLS
    CFLAGS += -Dstart_tls
    LDFLAGS += -lssl -lcrypto
    BUILD_MODE = TLS
else
    BUILD_MODE = TCP
endif

# 源文件
SOURCES = muixclt.c lexer.c variable.c netlink.c controller.c token.c
SHARE_SOURCES = ../share/pkg.c ../share/logo.c ../share/merr.c
OBJECTS = $(SOURCES:.c=.o) $(SHARE_SOURCES:.c=.o)
TARGET = muixclt

# 默认目标（TCP模式）
all: $(TARGET)

# TLS模式目标
tls: TLS=1
tls: $(TARGET)

# 编译目标
$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS)
	@echo "✓ C版本客户端编译完成: $(TARGET) ($(BUILD_MODE)模式)"

# 编译对象文件
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# 清理
clean:
	rm -f $(OBJECTS) $(TARGET)
	@echo "✓ 清理完成"

# 安装依赖（Ubuntu/Debian）
install-deps:
	@echo "安装依赖包..."
	sudo apt-get update
	sudo apt-get install -y build-essential libreadline-dev
	@echo "✓ 基础依赖安装完成"

# 安装TLS依赖（Ubuntu/Debian）
install-deps-tls:
	@echo "安装TLS依赖包..."
	sudo apt-get update
	sudo apt-get install -y build-essential libssl-dev libreadline-dev
	@echo "✓ TLS依赖安装完成"

# 安装依赖（CentOS/RHEL）
install-deps-rhel:
	@echo "安装依赖包..."
	sudo yum install -y gcc readline-devel
	@echo "✓ 基础依赖安装完成"

# 安装TLS依赖（CentOS/RHEL）
install-deps-tls-rhel:
	@echo "安装TLS依赖包..."
	sudo yum install -y gcc openssl-devel readline-devel
	@echo "✓ TLS依赖安装完成"

# 运行（TCP模式）
run: $(TARGET)
	@echo "运行客户端 (TCP模式)..."
	./$(TARGET)

# 运行（TLS模式）
run-tls: tls
	@echo "运行客户端 (TLS模式)..."
	./$(TARGET)

# 测试连接（TCP模式）
test: $(TARGET)
	@echo "测试TCP连接到本地服务器..."
	echo "\\s" | ./$(TARGET)

# 测试连接（TLS模式）
test-tls: tls
	@echo "测试TLS连接到本地服务器..."
	echo "\\s" | ./$(TARGET)

# 调试版本（TCP模式）
debug: CFLAGS += -DDEBUG -O0
debug: clean $(TARGET)

# 调试版本（TLS模式）
debug-tls: CFLAGS += -DDEBUG -O0 -Dstart_tls
debug-tls: LDFLAGS += -lssl -lcrypto
debug-tls: BUILD_MODE = TLS-DEBUG
debug-tls: clean $(TARGET)

# 发布版本（TCP模式）
release: CFLAGS = -Wall -Wextra -std=c99 -O3 -DNDEBUG
release: clean $(TARGET)
	strip $(TARGET)

# 发布版本（TLS模式）
release-tls: CFLAGS = -Wall -Wextra -std=c99 -O3 -DNDEBUG -Dstart_tls
release-tls: LDFLAGS += -lssl -lcrypto
release-tls: BUILD_MODE = TLS-RELEASE
release-tls: clean $(TARGET)
	strip $(TARGET)

# 显示编译信息
info:
	@echo "编译信息:"
	@echo "  CC: $(CC)"
	@echo "  CFLAGS: $(CFLAGS)"
	@echo "  LDFLAGS: $(LDFLAGS)"
	@echo "  BUILD_MODE: $(BUILD_MODE)"
	@echo "  SOURCES: $(SOURCES)"

# 帮助
help:
	@echo "Mhuixs C客户端 Makefile"
	@echo ""
	@echo "编译模式:"
	@echo "  all           - 编译客户端（TCP模式，默认）"
	@echo "  tls           - 编译客户端（TLS模式）"
	@echo "  TLS=1 make    - 通过环境变量启用TLS模式"
	@echo ""
	@echo "基本操作:"
	@echo "  clean         - 清理编译文件"
	@echo "  info          - 显示编译信息"
	@echo "  help          - 显示此帮助"
	@echo ""
	@echo "依赖安装:"
	@echo "  install-deps      - 安装基础依赖（Ubuntu/Debian）"
	@echo "  install-deps-tls  - 安装TLS依赖（Ubuntu/Debian）"
	@echo "  install-deps-rhel - 安装基础依赖（CentOS/RHEL）"
	@echo "  install-deps-tls-rhel - 安装TLS依赖（CentOS/RHEL）"
	@echo ""
	@echo "运行测试:"
	@echo "  run           - 运行客户端（TCP模式）"
	@echo "  run-tls       - 运行客户端（TLS模式）"
	@echo "  test          - 测试TCP连接"
	@echo "  test-tls      - 测试TLS连接"
	@echo ""
	@echo "调试发布:"
	@echo "  debug         - 编译调试版本（TCP模式）"
	@echo "  debug-tls     - 编译调试版本（TLS模式）"
	@echo "  release       - 编译发布版本（TCP模式）"
	@echo "  release-tls   - 编译发布版本（TLS模式）"
	@echo ""
	@echo "使用示例:"
	@echo "  make              # 编译TCP版本"
	@echo "  make tls          # 编译TLS版本"
	@echo "  make TLS=1        # 通过环境变量编译TLS版本"
	@echo "  make clean tls    # 清理并编译TLS版本"
	@echo "  make run-tls      # 编译并运行TLS版本"

.PHONY: all tls clean install-deps install-deps-tls install-deps-rhel install-deps-tls-rhel run run-tls test test-tls debug debug-tls release release-tls info help 