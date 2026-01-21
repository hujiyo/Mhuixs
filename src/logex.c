/**
 * Logex - 统一的脚本解释器、虚拟机和编译器
 * 
 * 用法：
 * - logex                    : 交互式 REPL 模式
 * - logex script.lgx         : 执行源代码文件
 * - logex script.ls          : 执行字节码文件
 * - logex script.ls -v       : 详细模式执行字节码
 * - logex input.lgx -o out.ls: 编译源码为字节码
 * - logex input.lgx -o out.ls -d : 编译并反汇编
 * 
 * 特性：
 * - 多行编辑模式（REPL）
 * - 编译 + VM 执行
 * - 直接执行字节码
 * - 编译源码为字节码（替代 gll）
 */

#include "interpreter.h"
#include "compiler.h"
#include "bytecode.h"
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

/* 函数声明 */
int run_repl(void);
int execute_bytecode(const char *bytecode_file, int verbose);
int execute_source_file(const char *source_file);
int compile_source(const char *input_file, const char *output_file, int disassemble);
void print_usage(const char *prog_name);

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

/* 打印使用说明 */
void print_usage(const char *prog_name) {
    printf("Logex - 统一的脚本解释器、虚拟机和编译器\n\n");
    printf("用法:\n");
    printf("  %s                         交互式 REPL 模式\n", prog_name);
    printf("  %s <script.lgx>            执行源代码文件\n", prog_name);
    printf("  %s <script.ls> [-v]        执行字节码文件\n", prog_name);
    printf("  %s <input.lgx> -o <out.ls> 编译源码为字节码\n", prog_name);
    printf("\n");
    printf("选项:\n");
    printf("  -o <file>         指定输出字节码文件（编译模式）\n");
    printf("  -d                反汇编输出（编译模式，调试用）\n");
    printf("  -v, --verbose     详细模式（执行字节码时）\n");
    printf("  -h, --help        显示此帮助信息\n");
    printf("\n");
    printf("示例:\n");
    printf("  %s                         # 启动 REPL\n", prog_name);
    printf("  %s test.lgx                # 执行源码\n", prog_name);
    printf("  %s test.ls -v              # 详细模式执行字节码\n", prog_name);
    printf("  %s test.lgx -o test.ls     # 编译为字节码\n", prog_name);
    printf("  %s test.lgx -o test.ls -d  # 编译并反汇编\n", prog_name);
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

/* 执行字节码文件 */
int execute_bytecode(const char *bytecode_file, int verbose) {
    if (verbose) {
        printf("加载字节码: %s\n", bytecode_file);
    }
    
    /* 创建解释器 */
    Interpreter *interp = interpreter_create();
    if (!interp) {
        fprintf(stderr, "错误: 无法创建解释器\n");
        return 1;
    }
    
    if (verbose) {
        printf("✓ 解释器创建成功\n");
    }
    
    /* 执行字节码 */
    InterpreterResult result;
    if (interpreter_execute_bytecode(interp, bytecode_file, &result) != 0) {
        fprintf(stderr, "错误: %s\n", result.value);
        interpreter_destroy(interp);
        return 1;
    }
    
    if (verbose) {
        printf("✓ 字节码加载成功\n");
        printf("开始执行...\n\n");
    }
    
    /* 输出结果 */
    if (result.type == RESULT_VALUE) {
        printf("%s\n", result.value);
    } else if (result.type == RESULT_ERROR) {
        fprintf(stderr, "运行时错误: %s\n", result.value);
        interpreter_destroy(interp);
        return 1;
    }
    
    if (verbose) {
        printf("\n✓ 执行完成\n");
    }
    
    interpreter_destroy(interp);
    return 0;
}

/* 执行源代码文件 */
int execute_source_file(const char *source_file) {
    /* 创建解释器 */
    Interpreter *interp = interpreter_create();
    if (!interp) {
        fprintf(stderr, "错误: 无法创建解释器\n");
        return 1;
    }
    
    /* 使用 VM 模式 */
    interpreter_set_vm_mode(interp, 1);
    
    /* 执行文件 */
    InterpreterResult result;
    if (interpreter_execute_file(interp, source_file, &result) != 0) {
        fprintf(stderr, "执行失败: %s\n", result.value);
        interpreter_destroy(interp);
        return 1;
    }
    
    /* 输出结果 */
    if (result.type == RESULT_VALUE) {
        printf("%s\n", result.value);
    } else if (result.type == RESULT_ERROR) {
        fprintf(stderr, "错误: %s\n", result.value);
        interpreter_destroy(interp);
        return 1;
    }
    
    interpreter_destroy(interp);
    return 0;
}

/* 编译源代码为字节码 */
int compile_source(const char *input_file, const char *output_file, int disassemble) {
    printf("编译: %s -> %s\n", input_file, output_file);
    
    /* 创建编译器 */
    Compiler *comp = compiler_create();
    if (!comp) {
        fprintf(stderr, "错误: 无法创建编译器\n");
        return 1;
    }
    
    /* 编译 */
    if (compiler_compile_file(comp, input_file, output_file) != 0) {
        fprintf(stderr, "编译错误: %s\n", compiler_get_error(comp));
        compiler_destroy(comp);
        return 1;
    }
    
    printf("✓ 编译成功\n");
    
    /* 反汇编 */
    if (disassemble) {
        printf("\n");
        bytecode_disassemble(comp->program);
    }
    
    compiler_destroy(comp);
    return 0;
}

/* REPL 模式 */
int run_repl(void) {
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

/* 主函数 */
int main(int argc, char *argv[]) {
    /* 无参数：REPL 模式 */
    if (argc == 1) {
        return run_repl();
    }
    
    /* 解析命令行参数 */
    const char *input_file = NULL;
    const char *output_file = NULL;
    int verbose = 0;
    int disassemble = 0;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = 1;
        } else if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 < argc) {
                output_file = argv[++i];
            } else {
                fprintf(stderr, "错误: -o 需要指定输出文件\n");
                return 1;
            }
        } else if (strcmp(argv[i], "-d") == 0) {
            disassemble = 1;
        } else if (!input_file) {
            input_file = argv[i];
        }
    }
    
    if (!input_file) {
        fprintf(stderr, "错误: 未指定输入文件\n");
        print_usage(argv[0]);
        return 1;
    }
    
    /* 检查是否为编译模式 */
    if (output_file) {
        /* 编译模式：logex input.lgx -o output.ls */
        return compile_source(input_file, output_file, disassemble);
    }
    
    /* 检查文件扩展名 */
    const char *ext = strrchr(input_file, '.');
    if (ext && strcmp(ext, ".ls") == 0) {
        /* 字节码文件：直接执行 */
        return execute_bytecode(input_file, verbose);
    } else {
        /* 源代码文件：编译后执行 */
        return execute_source_file(input_file);
    }
}
