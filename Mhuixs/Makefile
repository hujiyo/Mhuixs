# Makefile for Logex
# Logex的编译与Mhuixs独立

CC = gcc
CFLAGS = -Wall -Wextra -g -O0 -std=c99 -DLOGEX_BUILD
LDFLAGS = -lm -ldl
TARGET = logex
OBJS = calculator.o evaluator.o lexer.o bignum.o context.o function.o package.o lib/bitmap.o

# 默认目标：只编译计算器
all: $(TARGET)

# 链接目标文件生成计算器程序
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -rdynamic -o $(TARGET) $(OBJS) $(LDFLAGS)

# 编译 calculator.c
calculator.o: calculator.c evaluator.h context.h function.h package.h
	$(CC) $(CFLAGS) -c calculator.c

# 编译 evaluator.c
evaluator.o: evaluator.c evaluator.h lexer.h bignum.h context.h function.h package.h
	$(CC) $(CFLAGS) -c evaluator.c

# 编译 lexer.c
lexer.o: lexer.c lexer.h bignum.h
	$(CC) $(CFLAGS) -c lexer.c

# 编译 bignum.c
bignum.o: bignum.c bignum.h
	$(CC) $(CFLAGS) -c bignum.c

# 编译 context.c
context.o: context.c context.h bignum.h
	$(CC) $(CFLAGS) -c context.c

# 编译 function.c
function.o: function.c function.h bignum.h
	$(CC) $(CFLAGS) -c function.c

# 编译 package.c
package.o: package.c package.h function.h
	$(CC) $(CFLAGS) -c package.c

# 编译 lib/bitmap.c
lib/bitmap.o: lib/bitmap.c lib/bitmap.h
	$(CC) $(CFLAGS) -c lib/bitmap.c -o lib/bitmap.o

# 运行计算器
run: $(TARGET)
	./$(TARGET)

# 清理编译生成的文件
clean:
	rm -f $(OBJS) $(TARGET)

# 重新编译
rebuild: clean all

# 安装到系统（可选）
install: $(TARGET)
	cp $(TARGET) /usr/local/bin/logex

.PHONY: all run clean rebuild install