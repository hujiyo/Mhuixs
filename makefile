# 定义编译器
CC = gcc

# 定义编译选项
CFLAGS = -Wall -Iinclude

# 定义目标文件
TARGET = myprogram

# 定义源文件
SRCS = src/main.c src/utils.c

# 定义目标文件
OBJS = $(SRCS:.c=.o)

# 默认目标
all: $(TARGET)

# 链接目标文件生成可执行文件
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# 编译源文件生成目标文件
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# 清理生成的文件
clean:
	rm -f $(OBJS) $(TARGET)
