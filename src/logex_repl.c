/**
 * Logex REPL - 多行编辑模式
 * 
 * 特性：
 * - 普通回车：换行继续编辑
 * - Ctrl+Enter：编译并执行当前所有输入
 * - 先编译为字节码，再由 VM 执行
 */

#include "interpreter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

#define MAX_INPUT_SIZE 8192
#define MAX_LINE_SIZE 512

/* 多行输入缓冲区 */
typedef struct {
    char buffer[MAX_INPUT_SIZE];
    int length;
    int line_count;
} MultiLineBuffer;

/* 初始化缓冲区 */
void buffer_init(MultiLineBuffer *buf) {
    buf->buffer[0] = '\0';
    buf->length = 0;
    buf->line_count = 0;
}

/* 添加一行到缓冲区 */
int buffer_add_line(MultiLineBuffer *buf, const char *line) {
    int line_len = strlen(line);
    
    if (buf->length + line_len + 1 >= MAX_INPUT_SIZE) {
        return -1;  /* 缓冲区满 */
    }
    
    if (buf->length > 0) {
        strcat(buf->buffer, "\n");
        buf->length++;
    }
    
    strcat(buf->buffer, line);
    buf->length += line_len;
    buf->line_count++;
    
    return 0;
}

/* 清空缓冲区 */
void buffer_clear(MultiLineBuffer *buf) {
    buffer_init(buf);
}

/* 检测特殊按键 */
#ifdef _WIN32
int detect_ctrl_enter(void) {
    if (_kbhit()) {
        int ch = _getch();
        if (ch == 10 || ch == 13) {  /* Enter */
            /* 检查 Ctrl 是否按下 */
            if (GetAsyncKeyState(VK_CONTROL) & 0x8000) {
                return 1;  /* Ctrl+Enter */
            }
            return 0;  /* 普通 Enter */
        }
        /* 其他按键，放回 */
        _ungetch(ch);
    }
    return -1;  /* 无按键 */
}
#else
int detect_ctrl_enter(void) {
    /* Linux/Unix: 使用 termios */
    struct termios old_tio, new_tio;
    tcgetattr(STDIN_FILENO, &old_tio);
    new_tio = old_tio;
    new_tio.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);
    
    int ch = getchar();
    
    tcsetattr(STDIN_FILENO, TCSANOW, &old_tio);
    
    if (ch == 10 || ch == 13) {
        /* 简化处理：无法直接检测 Ctrl，使用 Ctrl+D 作为执行 */
        return 0;
    }
    
    return -1;
}
#endif

/* 读取一行（支持 Ctrl+Enter 检测） */
int read_line_with_ctrl(char *line, int max_len, int *is_ctrl_enter) {
    *is_ctrl_enter = 0;
    
    if (fgets(line, max_len, stdin) == NULL) {
        return -1;  /* EOF */
    }
    
    /* 移除末尾换行符 */
    int len = strlen(line);
    if (len > 0 && line[len-1] == '\n') {
        line[len-1] = '\0';
        len--;
    }
    if (len > 0 && line[len-1] == '\r') {
        line[len-1] = '\0';
    }
    
    return 0;
}

/* 打印欢迎信息 */
void print_welcome(void) {
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║           Logex REPL - 多行编辑模式                       ║\n");
    printf("║                                                            ║\n");
    printf("║  • 回车键：换行继续编辑                                   ║\n");
    printf("║  • 输入 :run 或空行后回车：编译并执行                     ║\n");
    printf("║  • :clear - 清空当前输入                                  ║\n");
    printf("║  • :vars  - 显示所有变量                                  ║\n");
    printf("║  • :help  - 显示帮助                                      ║\n");
    printf("║  • :quit  - 退出                                          ║\n");
    printf("║                                                            ║\n");
    printf("║  模式：编译 → 字节码 → VM 执行                           ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    printf("\n");
}

/* 打印帮助 */
void print_help(void) {
    printf("\n=== Logex 命令 ===\n");
    printf(":run    - 编译并执行当前输入\n");
    printf(":clear  - 清空当前输入缓冲区\n");
    printf(":vars   - 显示所有变量\n");
    printf(":funcs  - 显示所有函数\n");
    printf(":vm     - 切换到 VM 模式（默认）\n");
    printf(":direct - 切换到直接解释模式\n");
    printf(":help   - 显示此帮助\n");
    printf(":quit   - 退出 REPL\n");
    printf("\n=== 示例 ===\n");
    printf("> let x = 10\n");
    printf("> let y = 20\n");
    printf("> x + y\n");
    printf("> :run\n");
    printf("30\n\n");
}

/* 主函数 */
int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    print_welcome();
    
    /* 创建解释器 */
    Interpreter *interp = interpreter_create();
    if (!interp) {
        fprintf(stderr, "错误: 无法创建解释器\n");
        return 1;
    }
    
    /* 默认使用 VM 模式 */
    interpreter_set_vm_mode(interp, 1);
    printf("当前模式: VM (编译+执行)\n\n");
    
    MultiLineBuffer buffer;
    buffer_init(&buffer);
    
    char line[MAX_LINE_SIZE];
    int line_num = 1;
    
    while (1) {
        /* 显示提示符 */
        if (buffer.length == 0) {
            printf("> ");
        } else {
            printf("%d> ", line_num);
        }
        fflush(stdout);
        
        /* 读取一行 */
        int is_ctrl_enter;
        if (read_line_with_ctrl(line, sizeof(line), &is_ctrl_enter) != 0) {
            break;  /* EOF */
        }
        
        /* 处理命令 */
        if (line[0] == ':') {
            if (strcmp(line, ":quit") == 0 || strcmp(line, ":q") == 0) {
                break;
            } else if (strcmp(line, ":help") == 0 || strcmp(line, ":h") == 0) {
                print_help();
                continue;
            } else if (strcmp(line, ":clear") == 0 || strcmp(line, ":c") == 0) {
                buffer_clear(&buffer);
                line_num = 1;
                printf("缓冲区已清空\n");
                continue;
            } else if (strcmp(line, ":vars") == 0) {
                char vars[2048];
                interpreter_list_variables(interp, vars, sizeof(vars));
                printf("\n=== 变量列表 ===\n%s\n", vars);
                continue;
            } else if (strcmp(line, ":funcs") == 0) {
                char funcs[2048];
                interpreter_list_functions(interp, funcs, sizeof(funcs));
                printf("\n=== 函数列表 ===\n%s\n", funcs);
                continue;
            } else if (strcmp(line, ":vm") == 0) {
                interpreter_set_vm_mode(interp, 1);
                printf("已切换到 VM 模式（编译+执行）\n");
                continue;
            } else if (strcmp(line, ":direct") == 0) {
                interpreter_set_vm_mode(interp, 0);
                printf("已切换到直接解释模式\n");
                continue;
            } else if (strcmp(line, ":run") == 0 || strcmp(line, ":r") == 0) {
                /* 执行当前缓冲区 */
                if (buffer.length == 0) {
                    printf("缓冲区为空\n");
                    continue;
                }
                goto execute_buffer;
            } else {
                printf("未知命令: %s (输入 :help 查看帮助)\n", line);
                continue;
            }
        }
        
        /* 空行 + 有内容 = 执行 */
        if (strlen(line) == 0 && buffer.length > 0) {
            goto execute_buffer;
        }
        
        /* 空行 + 无内容 = 继续 */
        if (strlen(line) == 0) {
            continue;
        }
        
        /* 添加到缓冲区 */
        if (buffer_add_line(&buffer, line) != 0) {
            printf("错误: 输入过长\n");
            buffer_clear(&buffer);
            line_num = 1;
            continue;
        }
        
        line_num++;
        continue;
        
execute_buffer:
        /* 执行缓冲区内容 */
        printf("\n--- 编译并执行 ---\n");
        
        InterpreterResult result;
        if (interpreter_execute(interp, buffer.buffer, "<stdin>", &result) == 0) {
            switch (result.type) {
                case RESULT_VALUE:
                    printf("= %s\n", result.value);
                    break;
                case RESULT_ASSIGNMENT:
                    /* 赋值语句不输出 */
                    break;
                case RESULT_IMPORT:
                    printf("%s\n", result.message);
                    break;
                case RESULT_NONE:
                    break;
                case RESULT_ERROR:
                    printf("错误: %s\n", result.value);
                    break;
            }
        } else {
            printf("执行失败\n");
        }
        
        printf("\n");
        
        /* 清空缓冲区 */
        buffer_clear(&buffer);
        line_num = 1;
    }
    
    printf("\n再见！\n");
    interpreter_destroy(interp);
    
    return 0;
}
