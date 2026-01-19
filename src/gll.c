/**
 * GLL - Logex 编译器主程序
 * 用法: gll input.lgx -o output.ls
 */

#include <stdio.h>
#include <string.h>
#include "compiler.h"

void print_usage(const char *prog_name) {
    printf("GLL - Logex Compiler\n");
    printf("Usage: %s <input.lgx> [-o <output.ls>] [-d]\n", prog_name);
    printf("Options:\n");
    printf("  -o <file>    指定输出文件 (默认: input.ls)\n");
    printf("  -d           反汇编输出 (调试用)\n");
    printf("  -h, --help   显示帮助信息\n");
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    const char *input_file = NULL;
    const char *output_file = NULL;
    int disassemble = 0;
    
    /* 解析命令行参数 */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
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
    
    /* 生成默认输出文件名 */
    char default_output[256];
    if (!output_file) {
        strncpy(default_output, input_file, sizeof(default_output) - 4);
        char *dot = strrchr(default_output, '.');
        if (dot) *dot = '\0';
        strcat(default_output, ".ls");
        output_file = default_output;
    }
    
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
