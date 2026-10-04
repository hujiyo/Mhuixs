# Makefile for Mhuixs
#
# 内核骨架：数据结构库 + HOOK 注册系统
# 已剥离：Logex 语言层、网络层、用户组/加密层
#
# 这是 2024.11 最初设计的最小还原：
#   内存数据结构 + HOOK 统一引用 + 权限

CC      = gcc
CFLAGS  = -Wall -Wextra -g -O2 -std=c99 -I. -Ilib
LDFLAGS = -lm -lpthread

TARGET = mhuixs

OBJS = Mhuixs.o \
       execute.o \
       registry.o \
       lib/env.o \
       lib/getid.o \
       lib/merr.o \
       lib/logo.o \
       lib/hook.o \
       lib/bitmap.o \
       lib/bitcpy.o \
       lib/hash.o \
       lib/bignum.o \
       lib/list.o \
       lib/tblh.o \
       lib/kvalh.o

# lib/pkg.o 暂时挂起：
#   pkg 是客户端/服务端之间的打包协议（htonl/ntohl 网络字节序），
#   依赖 arpa/inet.h，在 Windows 上不可编译。
#   网络层剥离期间无任何模块引用它，等网络层回归时再启用。
#   （文件保留在 lib/pkg.c lib/pkg.h）

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)
	@echo "Mhuixs 编译完成 -> $(TARGET)"

Mhuixs.o: Mhuixs.c lib/merr.h lib/env.h lib/getid.h lib/hook.h lib/mstring.h registry.h execute.h
	$(CC) $(CFLAGS) -c Mhuixs.c

execute.o: execute.c execute.h registry.h lib/hook.h lib/bignum.h lib/list.h lib/bitmap.h
	$(CC) $(CFLAGS) -c execute.c

registry.o: registry.c registry.h lib/merr.h lib/env.h lib/mstring.h lib/hash.h lib/hook.h lib/bignum.h
	$(CC) $(CFLAGS) -c registry.c

lib/env.o: lib/env.c lib/env.h lib/mstring.h
	$(CC) $(CFLAGS) -c lib/env.c -o lib/env.o

lib/getid.o: lib/getid.c lib/getid.h lib/bitmap.h lib/merr.h
	$(CC) $(CFLAGS) -c lib/getid.c -o lib/getid.o

lib/merr.o: lib/merr.c lib/merr.h
	$(CC) $(CFLAGS) -c lib/merr.c -o lib/merr.o

lib/logo.o: lib/logo.c lib/logo.h
	$(CC) $(CFLAGS) -c lib/logo.c -o lib/logo.o

lib/hook.o: lib/hook.c lib/hook.h lib/merr.h lib/getid.h lib/mstring.h lib/bignum.h registry.h
	$(CC) $(CFLAGS) -c lib/hook.c -o lib/hook.o

lib/bitmap.o: lib/bitmap.c lib/bitmap.h lib/bitcpy.h lib/bignum.h
	$(CC) $(CFLAGS) -c lib/bitmap.c -o lib/bitmap.o

lib/bitcpy.o: lib/bitcpy.c lib/bitcpy.h
	$(CC) $(CFLAGS) -c lib/bitcpy.c -o lib/bitcpy.o

lib/hash.o: lib/hash.c lib/hash.h
	$(CC) $(CFLAGS) -c lib/hash.c -o lib/hash.o

lib/bignum.o: lib/bignum.c lib/bignum.h lib/list.h
	$(CC) $(CFLAGS) -c lib/bignum.c -o lib/bignum.o

lib/list.o: lib/list.c lib/list.h lib/bignum.h
	$(CC) $(CFLAGS) -c lib/list.c -o lib/list.o

lib/tblh.o: lib/tblh.c lib/tblh.h lib/bignum.h lib/mstring.h
	$(CC) $(CFLAGS) -c lib/tblh.c -o lib/tblh.o

lib/kvalh.o: lib/kvalh.c lib/kvalh.h lib/bignum.h lib/mstring.h lib/hash.h
	$(CC) $(CFLAGS) -c lib/kvalh.c -o lib/kvalh.o

lib/pkg.o: lib/pkg.c lib/pkg.h
	$(CC) $(CFLAGS) -c lib/pkg.c -o lib/pkg.o

run: $(TARGET)
	./$(TARGET)

# 回归测试（复用已编译对象，去掉 Mhuixs.o 因为测试自带 main）
TEST_OBJS = $(filter-out Mhuixs.o,$(OBJS))

# 1) 基础库：LIST 所有权与深拷贝 / BITMAP / TABLE
LIB_TEST_SRC = ../test/test_lib_status.c
# 2) 持久化：TABLE / KVALOT 挂到 HOOK 之后落盘-恢复往返
ROUNDTRIP_SRC = ../test/test_persist_roundtrip.c
# 3) HOOK 生命周期：注册 -> 挂数据 -> 注销 的释放检查
REG_TEST_SRC = ../test/test_registry_lifecycle.c

test: run_libtest run_roundtrip run_regtest
	@echo ""
	./run_libtest
	@echo ""
	./run_roundtrip
	@echo ""
	./run_regtest

run_libtest: $(TEST_OBJS) $(LIB_TEST_SRC)
	$(CC) $(CFLAGS) -o run_libtest $(LIB_TEST_SRC) $(TEST_OBJS) $(LDFLAGS)

run_roundtrip: $(TEST_OBJS) $(ROUNDTRIP_SRC)
	$(CC) $(CFLAGS) -o run_roundtrip $(ROUNDTRIP_SRC) $(TEST_OBJS) $(LDFLAGS)

# 生命周期测试要读进程工作集，Windows 上需链接 psapi
run_regtest: $(TEST_OBJS) $(REG_TEST_SRC)
ifeq ($(OS),Windows_NT)
	$(CC) $(CFLAGS) -o run_regtest $(REG_TEST_SRC) $(TEST_OBJS) $(LDFLAGS) -lpsapi
else
	$(CC) $(CFLAGS) -o run_regtest $(REG_TEST_SRC) $(TEST_OBJS) $(LDFLAGS)
endif

clean:
	rm -f $(OBJS) $(TARGET) run_libtest run_roundtrip run_regtest rt_test.dat .log.inf

rebuild: clean all

.PHONY: all run test clean rebuild
