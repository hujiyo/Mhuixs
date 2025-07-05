# Mhuixs C客户端 Makefile
# 版权所有 (c) Mhuixs-team 2024

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -g
LDFLAGS = -lssl -lcrypto -lreadline

# 源文件
SOURCES = muixclt.c pkg.c lexer.c logo.c netlink.c
OBJECTS = $(SOURCES:.c=.o)
TARGET = mhuixs-client

# 默认目标
all: $(TARGET)

# 编译目标
$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS)
	@echo "✓ C版本客户端编译完成: $(TARGET)"

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
	sudo apt-get install -y build-essential libssl-dev libreadline-dev
	@echo "✓ 依赖安装完成"

# 安装依赖（CentOS/RHEL）
install-deps-rhel:
	@echo "安装依赖包..."
	sudo yum install -y gcc openssl-devel readline-devel
	@echo "✓ 依赖安装完成"

# 运行
run: $(TARGET)
	./$(TARGET)

# 测试连接
test: $(TARGET)
	@echo "测试连接到本地服务器..."
	echo "\\s" | ./$(TARGET)

# 调试版本
debug: CFLAGS += -DDEBUG -O0
debug: clean $(TARGET)

# 发布版本
release: CFLAGS = -Wall -Wextra -std=c99 -O3 -DNDEBUG
release: clean $(TARGET)
	strip $(TARGET)

# 帮助
help:
	@echo "Mhuixs C客户端 Makefile"
	@echo ""
	@echo "可用目标:"
	@echo "  all           - 编译客户端（默认）"
	@echo "  clean         - 清理编译文件"
	@echo "  install-deps  - 安装依赖（Ubuntu/Debian）"
	@echo "  install-deps-rhel - 安装依赖（CentOS/RHEL）"
	@echo "  run           - 编译并运行"
	@echo "  test          - 测试连接"
	@echo "  debug         - 编译调试版本"
	@echo "  release       - 编译发布版本"
	@echo "  help          - 显示此帮助"

.PHONY: all clean install-deps install-deps-rhel run test debug release help 