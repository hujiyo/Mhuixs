/*
#版权所有 (c) Mhuixs-team 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2024.11
Email:hj18914255909@outlook.com
*/
/*
为了避免由于错误操作引发的问题，
table需要增添表格恢复功能
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define _MHUIXS_ //Mhuixs服务端标志宏

#include "merr.h"
#include "env.h"//环境变量模块
#include "getid.h"//ID分配器模块
#include "hook.h"//HOOK模块
#include "mstring.h"//字符串模块
// #include "usergroup.h"//用户组管理模块(暂时注释)
// #include "netplug.h"//网络模块(暂时注释)

/* Logex主函数声明 */
int logex_main(int argc, char *argv[]);

/*
存储在Mhuixs数据库的所有数据结构都需要使用钩子进行引用
每重新定义一个数据结构，mhuixs的钩子注册表中就会自动添加一个钩子
作用：
1.很好的防止用户不小心忘记钩子名称，导致无法访问指定数据结构
  究竟在内存的哪个位置了。注意，这是非常危险的，因为相当于有
  一块数据占着内存却无法访问。
2.HOOK可以更加简单的对数据结构进行权限控制.
3.数据压缩和储存在磁盘中的基本单位都是hook.
*/

int main(int argc, char *argv[])
{
    /*
     * 模式检测:
     * 1. 编译模式(-o参数):只运行Logex编译器,不启动任何Mhuixs模块
     * 2. Logex执行模式(.lgx/.ls文件或REPL):启动核心模块后运行Logex
     * 3. 服务器模式(无参数或其他):启动所有模块(未来实现)
     */
    
    int is_compile_mode = 0;
    int is_logex_mode = 0;
    
    // 检测编译模式: logex input.lgx -o output.ls
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0) {
            is_compile_mode = 1;
            break;
        }
    }
    
    // 检测Logex执行模式: logex script.lgx 或 logex script.ls
    if (!is_compile_mode && argc > 1) {
        const char *arg = argv[1];
        if (strstr(arg, ".lgx") || strstr(arg, ".ls") || 
            strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0 ||
            strcmp(arg, "-v") == 0 || strcmp(arg, "--verbose") == 0) {
            is_logex_mode = 1;
        }
    }
    
    // 无参数也是Logex REPL模式
    if (argc == 1) {
        is_logex_mode = 1;
    }
    
    /* ==================== 编译模式:不启动任何模块 ==================== */
    if (is_compile_mode) {
        printf("[Logex Compiler Mode]\n");
        return logex_main(argc, argv);
    }
    
    /* ==================== Logex执行模式:启动核心模块 ==================== */
    if (is_logex_mode) {
        printf("[Logex Runtime Mode - Initializing core modules]\n");
        
        // 环境变量模块
        if (env_init() != 0) {
            printf("\nENV module failed!\n");
            return 1;
        }
        printf("✓ ENV module initialized\n");
        
        // 初始化日志模块
        char* log_path = mstr_to_cstr(Env.MhuixsHomePath);
        int log_result = logger_init(log_path);
        free(log_path);
        if (log_result != 0) {
            printf("\nLogger module failed!\n");
            return 1;
        }
        printf("✓ Logger module initialized\n");
        
        // ID分配器模块
        if (idalloc_init() != success) {
            printf("\nID allocator module failed!\n");
            return 1;
        }
        printf("✓ ID allocator module initialized\n");
        
        // HOOK注册模块(已通过hook.h包含)
        printf("✓ HOOK system ready\n");
        
        printf("\n[Core modules initialized, starting Logex]\n\n");
        return logex_main(argc, argv);
    }
    
    /* ==================== 服务器模式:启动所有模块(未来) ==================== */
    printf("[Mhuixs Server Mode - Not implemented yet]\n");
    printf("Hint: Use without arguments to start Logex REPL\n");
    
    // 环境变量模块
    if (env_init() != 0) {
        printf("\nENV module failed!\n");
        return 1;
    }
    
    // 初始化日志模块
    char* log_path = mstr_to_cstr(Env.MhuixsHomePath);
    int log_result = logger_init(log_path);
    free(log_path);
    if (log_result != 0) {
        printf("\nLogger module failed!\n");
        return 1;
    }
    
    // ID分配器模块
    if (idalloc_init() != success) {
        printf("\nID allocator module failed!\n");
        return 1;
    }
    
    // 用户组管理模块(暂时注释,等待整合)
    // if (init_User_group_manager() != 0) {
    //     printf("\nUser group manager module failed!\n");
    //     return 1;
    // }
    
    // 网络模块(暂时注释,等待整合)
    // if (netplug_init(Env.port) != 0) {
    //     printf("\nNetplug module init failed!\n");
    //     return 1;
    // }
    // 
    // printf("Starting Mhuixs server...\n");
    // if (netplug_start() != 0) {
    //     printf("\nNetplug module start failed!\n");
    //     return 1;
    // }
    
    printf("\nServer mode not fully implemented. Exiting.\n");
    return 0;
}
