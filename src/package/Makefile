# Makefile for BoolEval Packages
# 包管理系统的独立构建文件

CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c99 -DLOGEX_BUILD -I.. -I../share -I../lib
LDFLAGS = -lm
PACKAGE_CFLAGS = -shared -fPIC $(CFLAGS)

# 包列表
PACKAGES = libmath.so libexample.so libstring.so libtype.so liblist.so

# 默认目标：编译所有包
all: $(PACKAGES)

# 编译 math 包
libmath.so: math_package.c ../function.h ../bignum.h ../context.h
	$(CC) $(PACKAGE_CFLAGS) -o $@ $< $(LDFLAGS)

# 编译 example 包
libexample.so: example_package.c ../function.h ../bignum.h
	$(CC) $(PACKAGE_CFLAGS) -o $@ $<

# 编译 string 包
libstring.so: string_package.c ../function.h ../bignum.h
	$(CC) $(PACKAGE_CFLAGS) -o $@ $<

# 编译 type 包
libtype.so: type_package.c ../function.h ../bignum.h
	$(CC) $(PACKAGE_CFLAGS) -o $@ $<

# 编译 list 包
liblist.so: list_package.c ../function.h ../bignum.h ../lib/list.h
	$(CC) $(PACKAGE_CFLAGS) -o $@ $<

# 清理编译生成的文件
clean:
	rm -f *.o $(PACKAGES)

# 重新编译
rebuild: clean all

# 帮助信息
help:
	@echo "Logex 包管理系统构建工具"
	@echo ""
	@echo "可用目标："
	@echo "  all      - 编译所有包（默认）"
	@echo "  clean    - 清理编译生成的 .so 文件"
	@echo "  rebuild  - 清理后重新编译"
	@echo "  help     - 显示此帮助信息"
	@echo ""
	@echo "当前包列表："
	@echo "  - libmath.so    (数学扩展包)"
	@echo "  - libexample.so (示例包)"
	@echo "  - libstring.so  (字符串类型转换包)"
	@echo "  - libtype.so    (类型转换包)"
	@echo "  - liblist.so    (列表操作包)"

.PHONY: all clean rebuild help

