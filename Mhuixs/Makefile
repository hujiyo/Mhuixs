# Makefile for Logex
# Logex的编译与Mhuixs独立

CC = gcc
CFLAGS = -Wall -Wextra -g -O0 -std=c99 -DLOGEX_BUILD
LDFLAGS = -lm -ldl
TARGET = logex
OBJS = calculator.o evaluator.o lexer.o bignum.o context.o function.o package.o parser.o ast.o lib/bitmap.o lib/list.o

# 默认目标：只编译计算器
all: $(TARGET)

# 链接目标文件生成计算器程序
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -rdynamic -o $(TARGET) $(OBJS) $(LDFLAGS)

# 编译 calculator.c
calculator.o: calculator.c evaluator.h context.h function.h package.h
	$(CC) $(CFLAGS) -c calculator.c

# 编译 evaluator.c
evaluator.o: evaluator.c evaluator.h lexer.h bignum.h context.h function.h package.h parser.h ast.h
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

# 编译 parser.c
parser.o: parser.c parser.h ast.h lexer.h
	$(CC) $(CFLAGS) -c parser.c

# 编译 ast.c
ast.o: ast.c ast.h evaluator.h context.h function.h package.h
	$(CC) $(CFLAGS) -c ast.c

# 编译 lib/bitmap.c
lib/bitmap.o: lib/bitmap.c lib/bitmap.h
	$(CC) $(CFLAGS) -c lib/bitmap.c -o lib/bitmap.o

# 编译 lib/list.c
lib/list.o: lib/list.c lib/list.h
	$(CC) $(CFLAGS) -c lib/list.c -o lib/list.o

# 运行计算器
run: $(TARGET)
	./$(TARGET)

# 编译测试程序（不包含calculator.o以避免main函数冲突）
TEST_OBJS = evaluator.o lexer.o bignum.o context.o function.o package.o parser.o ast.o lib/bitmap.o lib/list.o
test_control_flow: test_control_flow.o $(TEST_OBJS)
	$(CC) $(CFLAGS) -rdynamic -o test_control_flow test_control_flow.o $(TEST_OBJS) $(LDFLAGS)

test_control_flow.o: test_control_flow.c evaluator.h context.h function.h package.h
	$(CC) $(CFLAGS) -c test_control_flow.c

# 运行测试
test: test_control_flow
	./test_control_flow

# 编译调试程序
debug_control_flow: debug_control_flow.o $(TEST_OBJS)
	$(CC) $(CFLAGS) -rdynamic -o debug_control_flow debug_control_flow.o $(TEST_OBJS) $(LDFLAGS)

debug_control_flow.o: debug_control_flow.c evaluator.h context.h function.h package.h
	$(CC) $(CFLAGS) -c debug_control_flow.c

# 运行调试
debug: debug_control_flow
	./debug_control_flow

# 编译do-while调试程序
debug_do_while: debug_do_while.o $(TEST_OBJS)
	$(CC) $(CFLAGS) -rdynamic -o debug_do_while debug_do_while.o $(TEST_OBJS) $(LDFLAGS)

debug_do_while.o: debug_do_while.c evaluator.h context.h function.h package.h lexer.h
	$(CC) $(CFLAGS) -c debug_do_while.c

# 运行do-while调试
debug_do: debug_do_while
	./debug_do_while

# 清理编译生成的文件
clean:
	rm -f $(OBJS) $(TARGET) test_control_flow test_control_flow.o debug_control_flow debug_control_flow.o debug_do_while debug_do_while.o

# 重新编译
rebuild: clean all

# 安装到系统（可选）
install: $(TARGET)
	cp $(TARGET) /usr/local/bin/logex

.PHONY: all run clean rebuild install test debug debug_do