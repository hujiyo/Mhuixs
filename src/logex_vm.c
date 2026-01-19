/**
 * Logex VM - 虚拟机执行器主程序
 * 用法: logex_vm program.ls
 */

#include <stdio.h>
#include <string.h>
#include "vm.h"

void print_usage(const char *prog_name) {
    printf("Logex VM - Virtual Machine Executor\n");
    printf("Usage: %s <program.ls> [-v]\n", prog_name);
    printf("Options:\n");
    printf("  -v           详细模式 (显示执行信息)\n");
    printf("  -h, --help   显示帮助信息\n");
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    const char *bytecode_file = NULL;
    int verbose = 0;
    
    /* 解析命令行参数 */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-v") == 0) {
            verbose = 1;
        } else if (!bytecode_file) {
            bytecode_file = argv[i];
        }
    }
    
    if (!bytecode_file) {
        fprintf(stderr, "错误: 未指定字节码文件\n");
        print_usage(argv[0]);
        return 1;
    }
    
    if (verbose) {
        printf("加载字节码: %s\n", bytecode_file);
    }
    
    /* 创建虚拟机 */
    VM *vm = vm_create();
    if (!vm) {
        fprintf(stderr, "错误: 无法创建虚拟机\n");
        return 1;
    }
    
    /* 加载字节码 */
    if (vm_load_file(vm, bytecode_file) != 0) {
        fprintf(stderr, "错误: 无法加载字节码文件: %s\n", vm_get_error(vm));
        vm_destroy(vm);
        return 1;
    }
    
    if (verbose) {
        printf("✓ 字节码加载成功\n");
        printf("开始执行...\n\n");
    }
    
    /* 执行 */
    if (vm_run(vm) != 0) {
        fprintf(stderr, "运行时错误: %s\n", vm_get_error(vm));
        vm_destroy(vm);
        return 1;
    }
    
    /* 输出结果 */
    BigNum *result = vm_get_result(vm);
    if (result) {
        char result_str[1024];
        bignum_to_string(result, result_str, sizeof(result_str), 10);
        printf("%s\n", result_str);
    }
    
    if (verbose) {
        printf("\n✓ 执行完成\n");
    }
    
    vm_destroy(vm);
    return 0;
}
