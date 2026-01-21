# Makefile for Logex
# Logex的编译与Mhuixs独立

CC = gcc
CFLAGS = -Wall -Wextra -g -O0 -std=c99 -DLOGEX_BUILD -I. -Ilib -Ishare
LDFLAGS = -lm -ldl

# 旧版 Logex（直接解释器）
TARGET_OLD = logex
OBJS_OLD = calculator.o evaluator.o lexer.o bignum.o context.o function.o package.o parser.o ast.o error.o lib/bitmap.o lib/list.o

# 新版 Logex REPL（多行编辑 + VM）
TARGET_REPL = logex
OBJS_REPL = logex.o interpreter.o evaluator.o lexer.o bignum.o context.o function.o package.o parser.o ast.o error.o \
            vm.o compiler.o bytecode.o builtin.o lib/bitmap.o lib/list.o lib/tblh.o

# VM 工具
TARGET_GLL = gll

# 默认目标：编译新版 Logex
all: $(TARGET_REPL)

# 编译新版 REPL（多行编辑 + VM）
$(TARGET_REPL): $(OBJS_REPL)
	$(CC) $(CFLAGS) -rdynamic -o $(TARGET_REPL) $(OBJS_REPL) $(LDFLAGS)

# 编译旧版 Logex（兼容）
$(TARGET_OLD): $(OBJS_OLD)
	$(CC) $(CFLAGS) -rdynamic -o $(TARGET_OLD) $(OBJS_OLD) $(LDFLAGS)

# 编译 VM 工具
vm_tools: $(TARGET_GLL)

$(TARGET_GLL): gll.o compiler.o bytecode.o lexer.o parser.o ast.o bignum.o context.o function.o package.o error.o builtin.o lib/bitmap.o lib/list.o lib/tblh.o
	$(CC) $(CFLAGS) -o $(TARGET_GLL) $^ $(LDFLAGS)

# 编译 calculator.c
calculator.o: calculator.c evaluator.h context.h function.h package.h
	$(CC) $(CFLAGS) -c calculator.c

# 编译 evaluator.c
evaluator.o: evaluator.c evaluator.h lexer.h bignum.h context.h function.h package.h parser.h ast.h
	$(CC) $(CFLAGS) -c evaluator.c

# 编译 lexer.c
lexer.o: lexer.c lexer.h bignum.h error.h
	$(CC) $(CFLAGS) -c lexer.c

# 编译 error.c
error.o: error.c error.h
	$(CC) $(CFLAGS) -c error.c

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

# 编译 lib/tblh.c
lib/tblh.o: lib/tblh.c lib/tblh.h
	$(CC) $(CFLAGS) -c lib/tblh.c -o lib/tblh.o

# 编译 logex.c
logex.o: logex.c interpreter.h compiler.h bytecode.h
	$(CC) $(CFLAGS) -c logex.c

# 编译 interpreter.c
interpreter.o: interpreter.c interpreter.h vm.h compiler.h
	$(CC) $(CFLAGS) -c interpreter.c

# 编译 vm.c
vm.o: vm.c vm.h bytecode.h builtin.h
	$(CC) $(CFLAGS) -c vm.c

# 编译 compiler.c
compiler.o: compiler.c compiler.h bytecode.h parser.h
	$(CC) $(CFLAGS) -c compiler.c

# 编译 bytecode.c
bytecode.o: bytecode.c bytecode.h
	$(CC) $(CFLAGS) -c bytecode.c

# 编译 builtin.c
builtin.o: builtin.c builtin.h bignum.h
	$(CC) $(CFLAGS) -c builtin.c

# 编译 gll.c
gll.o: gll.c compiler.h
	$(CC) $(CFLAGS) -c gll.c

# 运行新版 Logex
run: $(TARGET_REPL)
	./$(TARGET_REPL)

# 运行旧版 Logex
run_old: $(TARGET_OLD)
	./$(TARGET_OLD)

# 编译测试程序（不包含calculator.o以避免main函数冲突）
TEST_OBJS = evaluator.o lexer.o bignum.o context.o function.o package.o parser.o ast.o error.o lib/bitmap.o lib/list.o
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
	rm -f *.o lib/*.o $(TARGET_OLD) $(TARGET_REPL) $(TARGET_GLL) test_control_flow debug_control_flow debug_do_while

# 重新编译
rebuild: clean all

# 安装到系统（可选）
install: $(TARGET_REPL) $(TARGET_GLL)
	cp $(TARGET_REPL) /usr/local/bin/logex
	cp $(TARGET_GLL) /usr/local/bin/gll
	@echo "✓ 安装完成: logex (统一解释器) 和 gll (编译器)"
	@echo "  logex_vm 功能已合并到 logex 中"

.PHONY: all run run_old clean rebuild install test debug debug_do vm_tools