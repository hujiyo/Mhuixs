#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <ctype.h>
#include "evaluator.h"
#include "context.h"
#include "function.h"
#include "package.h"

#define MAX_INPUT 256
#define MAX_RESULT 1024
#define CLEAR_SCREEN "\033[2J\033[H"
#define MOVE_UP "\033[A"
#define CLEAR_LINE "\033[2K\r"

/* 布尔运算符列表（对外 Unicode 符号） */
static const char *operators[] = {
    "^",  /* 合取 (AND) */
    "v",  /* 析取 (OR) */
    "!",  /* 否定 (NOT) */
    "→",  /* 蕴含 (IMPLIES) */
    "↔",  /* 等价 (IFF) */
    "⊽"   /* 异或 (XOR) */
};

#define NUM_OPERATORS 6
static int current_operator_index = 0;  /* 当前选中的运算符索引 */
static int has_pending_operator = 0;   /* 是否有待确认的运算符 */

/* 终端设置 */
static struct termios orig_termios;

/* 恢复终端设置 */
void disable_raw_mode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

/* 启用原始模式以读取方向键 */
void enable_raw_mode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disable_raw_mode);
    
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);  /* 禁用回显和规范模式 */
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

/* 恢复正常模式 */
void restore_normal_mode() {
    struct termios normal = orig_termios;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &normal);
}

/* 清除当前行并重新显示 */
void redisplay_line(const char *prompt, const char *buffer) {
    printf("\r\033[K%s%s", prompt, buffer);
    fflush(stdout);
}

/* 打印启动信息 */
void print_welcome() {
    printf("=== Logex ===\n");
    printf("Logic + Expression - 逻辑与表达式的完美融合\n");
    printf("支持布尔运算、数值计算和变量机制的数学脚本语言\n");
    printf("(输入 'help' 查看帮助, 'exit' 退出)\n");
}

/* 打印帮助信息 */
void print_help() {
    printf("\n【Logex - 逻辑表达式语言】\n");
    printf("\n数值运算符:\n");
    printf("  +, -, *, /        加减乘除\n");
    printf("  **                幂运算\n");
    printf("  %%                 取模\n");
    printf("\n布尔运算符:\n");
    printf("  ^                 合取 (AND)\n");
    printf("  v                 析取 (OR)\n");
    printf("  !                 否定 (NOT)\n");
    printf("  →                 蕴含 (IMPLIES)\n");
    printf("  ↔                 等价 (IFF)\n");
    printf("  ⊽                 异或 (XOR)\n");
    printf("\n类型转换规则:\n");
    printf("  • 数值 ≠ 0 视为 true (布尔值 1)\n");
    printf("  • 数值 = 0 视为 false (布尔值 0)\n");
    printf("  • 布尔结果 (0/1) 可参与数值计算\n");
    printf("\n变量机制:\n");
    printf("  let A = 1         定义变量 A\n");
    printf("  let B = A ^ 0     使用变量 A\n");
    printf("  A                 显示变量 A 的值\n");
    printf("  let C = A v B     变量可用于任何表达式\n");
    printf("\n包管理:\n");
    printf("  import math       导入数学函数包\n");
    printf("  import string     导入字符串处理包\n");
    printf("  import example    导入示例包\n");
    printf("  packages          列出所有可用的包\n");
    printf("\n可用包:\n");
    printf("  • math: sin, cos, tan, sqrt, abs, max, min, ln, log 等\n");
    printf("  • string: num(字符串转数字), str(数字转字符串)\n");
    printf("  数学常量: π (pi), e, φ (phi)\n");
    printf("\n示例:\n");
    printf("  数值计算:    123.456 + 789\n");
    printf("  布尔计算:    1^0 v 0\n");
    printf("  混合计算:    (5 > 3) * 100      结果: 100\n");
    printf("  变量使用:    let X = 5\n");
    printf("               let Y = X * 2\n");
    printf("               Y + 1              结果: 11\n");
    printf("  函数调用:    import math\n");
    printf("               sin(π/2)           结果: 1\n");
    printf("               sqrt(16)           结果: 4\n");
    printf("  字符串处理:  import string\n");
    printf("               let price = \"99.99\"\n");
    printf("               num(price) * 2     结果: 199.98\n");
    printf("\n精度: %d 位小数 (可在 bignum.h 修改)\n", BIGNUM_DEFAULT_PRECISION);
    printf("\n快捷键: ↑↓切换运算符, Backspace删除, Ctrl+C退出\n");
    printf("命令: help, exit, vars (显示变量), clear (清空变量), funcs (显示函数), packages (显示包)\n");
}

/* 读取一行输入，支持智能运算符预测 */
int read_expression(char *buffer, int max_len) {
    int pos = 0;
    const char *prompt = "expr > ";
    
    enable_raw_mode();
    
    printf("%s", prompt);
    fflush(stdout);
    
    buffer[0] = '\0';
    has_pending_operator = 0;
    
    while (1) {
        char c;
        if (read(STDIN_FILENO, &c, 1) != 1) continue;
        
        if (c == 27) {  /* ESC 序列 (方向键) */
            char seq[2];
            if (read(STDIN_FILENO, &seq[0], 1) != 1) continue;
            if (read(STDIN_FILENO, &seq[1], 1) != 1) continue;
            
            if (seq[0] == '[') {
                /* 支持运算符预测 */
                if (seq[1] == 'A') {  /* 上箭头 */
                    if (has_pending_operator) {
                        /* 如果已有待确认运算符，替换它 */
                        current_operator_index = (current_operator_index - 1 + NUM_OPERATORS) % NUM_OPERATORS;
                        
                        /* 删除旧运算符 - 简单回退到上一个运算符之前 */
                        const char *prev_op = operators[(current_operator_index + 1) % NUM_OPERATORS];
                        int prev_op_len = strlen(prev_op);
                        pos -= prev_op_len;
                        
                        /* 添加新运算符 */
                        const char *op = operators[current_operator_index];
                        int op_len = strlen(op);
                        if (pos + op_len < max_len - 1) {
                            strcpy(buffer + pos, op);
                            pos += op_len;
                            buffer[pos] = '\0';
                        }
                    } else {
                        /* 没有待确认运算符，添加一个 */
                        const char *op = operators[current_operator_index];
                        int op_len = strlen(op);
                        if (pos + op_len < max_len - 1) {
                            strcpy(buffer + pos, op);
                            pos += op_len;
                            buffer[pos] = '\0';
                            has_pending_operator = 1;
                        }
                    }
                    redisplay_line(prompt, buffer);
                } else if (seq[1] == 'B') {  /* 下箭头 */
                    if (has_pending_operator) {
                        /* 如果已有待确认运算符，替换它 */
                        current_operator_index = (current_operator_index + 1) % NUM_OPERATORS;
                        
                        /* 删除旧运算符 - 简单回退到上一个运算符之前 */
                        const char *prev_op = operators[(current_operator_index - 1 + NUM_OPERATORS) % NUM_OPERATORS];
                        int prev_op_len = strlen(prev_op);
                        pos -= prev_op_len;
                        
                        /* 添加新运算符 */
                        const char *op = operators[current_operator_index];
                        int op_len = strlen(op);
                        if (pos + op_len < max_len - 1) {
                            strcpy(buffer + pos, op);
                            pos += op_len;
                            buffer[pos] = '\0';
                        }
                    } else {
                        /* 没有待确认运算符，添加一个 */
                        const char *op = operators[current_operator_index];
                        int op_len = strlen(op);
                        if (pos + op_len < max_len - 1) {
                            strcpy(buffer + pos, op);
                            pos += op_len;
                            buffer[pos] = '\0';
                            has_pending_operator = 1;
                        }
                    }
                    redisplay_line(prompt, buffer);
                }
            }
        } else if (c == 127 || c == 8) {  /* Backspace */
            if (pos > 0) {
                /* 处理UTF-8多字节字符的删除 */
                int bytes_to_delete = 1;
                if (pos >= 3 && (buffer[pos-3] & 0xE0) == 0xE0) {
                    bytes_to_delete = 3;  /* 3字节UTF-8 */
                } else if (pos >= 2 && (buffer[pos-2] & 0xC0) == 0xC0) {
                    bytes_to_delete = 2;  /* 2字节UTF-8 */
                }
                
                pos -= bytes_to_delete;
                buffer[pos] = '\0';
                has_pending_operator = 0;  /* 删除字符后清除待确认状态 */
                redisplay_line(prompt, buffer);
            }
        } else if (c == '\n' || c == '\r') {  /* Enter */
            buffer[pos] = '\0';
            printf("\n");
            restore_normal_mode();
            return 1;
        } else if (c == 3) {  /* Ctrl+C */
            restore_normal_mode();
            return 0;
        } else if (c >= 32 && c < 127) {  /* 可打印ASCII字符 */
            if (pos < max_len - 1) {
                buffer[pos++] = c;
                buffer[pos] = '\0';
                has_pending_operator = 0;  /* 输入新字符后清除待确认状态 */
                redisplay_line(prompt, buffer);
            }
        } else if ((c & 0x80) != 0) {  /* UTF-8多字节字符 */
            if (pos < max_len - 4) {
                buffer[pos++] = c;
                
                /* 读取后续字节 */
                int extra_bytes = 0;
                if ((c & 0xE0) == 0xC0) extra_bytes = 1;
                else if ((c & 0xF0) == 0xE0) extra_bytes = 2;
                else if ((c & 0xF8) == 0xF0) extra_bytes = 3;
                
                for (int i = 0; i < extra_bytes; i++) {
                    char next;
                    if (read(STDIN_FILENO, &next, 1) == 1) {
                        buffer[pos++] = next;
                    }
                }
                
                buffer[pos] = '\0';
                has_pending_operator = 0;  /* 输入新字符后清除待确认状态 */
                redisplay_line(prompt, buffer);
            }
        }
    }
}

int main() {
    char input[MAX_INPUT];
    char result_str[MAX_RESULT];
    Context ctx;
    FunctionRegistry func_registry;
    PackageManager pkg_manager;
    
    /* 初始化变量上下文、函数注册表和包管理器 */
    context_init(&ctx);
    function_registry_init(&func_registry);
    package_manager_init(&pkg_manager, "./package");
    
    print_welcome();
    
    while (1) {
        /* 读取表达式 */
        if (!read_expression(input, MAX_INPUT)) {
            printf("\n");
            break;
        }
        
        /* 检查特殊命令 */
        if (strcmp(input, "exit") == 0 || strcmp(input, "quit") == 0) {
            break;
        }
        
        if (strcmp(input, "help") == 0) {
            print_help();
            continue;
        }
        
        if (strcmp(input, "vars") == 0) {
            char var_list[2048];
            context_list(&ctx, var_list, sizeof(var_list));
            printf("%s\n", var_list);
            continue;
        }
        
        if (strcmp(input, "funcs") == 0) {
            char func_list[4096];
            function_list(&func_registry, func_list, sizeof(func_list));
            printf("%s\n", func_list);
            continue;
        }
        
        if (strcmp(input, "clear") == 0) {
            context_clear(&ctx);
            printf("已清空所有变量\n");
            continue;
        }
        
        if (strcmp(input, "packages") == 0) {
            char pkg_list[4096];
            package_scan_available(&pkg_manager, pkg_list, sizeof(pkg_list));
            printf("%s\n", pkg_list);
            continue;
        }
        
        /* 如果输入为空，继续 */
        if (strlen(input) == 0) {
            continue;
        }
        
        /* 使用统一求值器（支持变量、函数和包） */
        int ret = eval_statement(input, result_str, MAX_RESULT, &ctx, &func_registry, &pkg_manager, -1);
        
        if (ret == EVAL_DIV_ZERO) {
            printf("错误: 除零错误\n");
        } else if (ret == EVAL_ERROR) {
            printf("错误: 语法错误、变量未定义或函数未定义\n");
        } else if (ret == 1) {
            /* 赋值语句 */
            printf("%s\n", result_str);
        } else if (ret == 2) {
            /* 导入语句 */
            printf("%s\n", result_str);
        } else {
            /* 表达式求值 */
            printf("= %s\n", result_str);
        }
    }
    
    /* 清理资源 */
    package_manager_cleanup(&pkg_manager);
    
    return 0;
}
