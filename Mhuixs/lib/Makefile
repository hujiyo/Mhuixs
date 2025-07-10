 # Mhuixs 会话管理模块 Makefile (纯C版本)
# 版权所有 (c) Mhuixs-team 2025

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -g -fPIC
LDFLAGS = -lpthread -lrt

# 目录设置
SRCDIR = .
OBJDIR = obj
BINDIR = bin
LIBDIR = lib

# 源文件
SOURCES = session.c session_pool.c session_network.c session_execution.c
OBJECTS = $(SOURCES:%.c=$(OBJDIR)/%.o)

# 目标文件
LIBRARY = $(LIBDIR)/libsession.a
SHARED_LIB = $(LIBDIR)/libsession.so
EXAMPLE = $(BINDIR)/session_example

# 头文件依赖
HEADERS = session.h

# 默认目标
all: directories $(LIBRARY) $(SHARED_LIB) $(EXAMPLE)

# 创建目录
directories:
	@mkdir -p $(OBJDIR)
	@mkdir -p $(BINDIR)
	@mkdir -p $(LIBDIR)

# 编译目标文件
$(OBJDIR)/%.o: $(SRCDIR)/%.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# 创建静态库
$(LIBRARY): $(OBJECTS)
	ar rcs $@ $^
	@echo "静态库创建成功: $@"

# 创建动态库
$(SHARED_LIB): $(OBJECTS)
	$(CC) -shared -o $@ $^ $(LDFLAGS)
	@echo "动态库创建成功: $@"

# 编译示例程序
$(EXAMPLE): $(SRCDIR)/session_example.c $(LIBRARY)
	$(CC) $(CFLAGS) -o $@ $< -L$(LIBDIR) -lsession $(LDFLAGS)
	@echo "示例程序创建成功: $@"

# 清理
clean:
	rm -rf $(OBJDIR) $(BINDIR) $(LIBDIR)
	@echo "清理完成"

# 安装
install: $(LIBRARY) $(SHARED_LIB)
	@echo "安装库文件..."
	sudo cp $(LIBRARY) /usr/local/lib/
	sudo cp $(SHARED_LIB) /usr/local/lib/
	sudo cp $(HEADERS) /usr/local/include/
	sudo ldconfig
	@echo "安装完成"

# 卸载
uninstall:
	@echo "卸载库文件..."
	sudo rm -f /usr/local/lib/libsession.a
	sudo rm -f /usr/local/lib/libsession.so
	sudo rm -f /usr/local/include/session.h
	sudo ldconfig
	@echo "卸载完成"

# 测试
test: $(EXAMPLE)
	@echo "运行测试..."
	./$(EXAMPLE)

# 调试版本
debug: CFLAGS += -DDEBUG -g3 -O0
debug: clean all

# 发布版本
release: CFLAGS += -DNDEBUG -O3
release: clean all

# 代码格式化
format:
	@echo "格式化代码..."
	clang-format -i $(SRCDIR)/*.c $(SRCDIR)/*.h

# 静态分析
analyze:
	@echo "静态分析..."
	cppcheck --enable=all --std=c99 $(SRCDIR)/*.c

# 内存检查
memcheck: $(EXAMPLE)
	@echo "内存检查..."
	valgrind --leak-check=full --show-leak-kinds=all ./$(EXAMPLE)

# 性能分析
profile: CFLAGS += -pg
profile: clean $(EXAMPLE)
	@echo "性能分析版本已创建"

# 帮助
help:
	@echo "可用目标:"
	@echo "  all       - 编译所有目标 (默认)"
	@echo "  clean     - 清理生成文件"
	@echo "  install   - 安装库文件到系统"
	@echo "  uninstall - 从系统卸载库文件"
	@echo "  test      - 运行测试程序"
	@echo "  debug     - 编译调试版本"
	@echo "  release   - 编译发布版本"
	@echo "  format    - 格式化代码"
	@echo "  analyze   - 静态分析"
	@echo "  memcheck  - 内存检查"
	@echo "  profile   - 性能分析版本"
	@echo "  help      - 显示此帮助"

# 依赖关系
$(OBJDIR)/session.o: session.c session.h
$(OBJDIR)/session_pool.o: session_pool.c session.h
$(OBJDIR)/session_network.o: session_network.c session.h
$(OBJDIR)/session_execution.o: session_execution.c session.h

.PHONY: all clean install uninstall test debug release format analyze memcheck profile help directories