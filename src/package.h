#ifndef PACKAGE_H
#define PACKAGE_H

#include "function.h"

/**
 * 包管理系统
 * 
 * 自动从 package/ 目录加载动态库包，无需手动注册函数。
 * 
 * 使用方法：
 *   1. 将包的 .so 文件放入 package/ 目录
 *   2. 在程序中使用 import 语句：import mypackage
 *   3. 包会自动加载，函数自动注册
 * 
 * 包的规范：
 *   - 必须导出 package_register() 函数
 *   - 函数签名：int package_register(FunctionRegistry *registry)
 *   - 返回值：成功注册的函数数量
 */

#define MAX_PACKAGE_NAME_LEN 64
#define MAX_PACKAGES 32

/* 包信息 */
typedef struct {
    char name[MAX_PACKAGE_NAME_LEN];  /* 包名 */
    void *handle;                      /* 动态库句柄 */
    int is_loaded;                     /* 是否已加载 */
    int func_count;                    /* 注册的函数数量 */
} PackageInfo;

/* 包管理器 */
typedef struct {
    PackageInfo packages[MAX_PACKAGES];
    int count;
    char package_dir[256];             /* 包目录路径 */
} PackageManager;

/**
 * 初始化包管理器
 * 
 * @param manager 包管理器
 * @param package_dir 包目录路径（默认为 "./package"）
 */
void package_manager_init(PackageManager *manager, const char *package_dir);

/**
 * 加载包
 * 
 * @param manager 包管理器
 * @param package_name 包名（不含 .so 后缀）
 * @param registry 函数注册表
 * @param ctx 变量上下文（用于注册常量，可选）
 * @return 成功注册的函数数量，失败返回 -1
 */
int package_load(PackageManager *manager, const char *package_name, 
                 FunctionRegistry *registry, void *ctx);

/**
 * 检查包是否已加载
 * 
 * @param manager 包管理器
 * @param package_name 包名
 * @return 1 已加载，0 未加载
 */
int package_is_loaded(PackageManager *manager, const char *package_name);

/**
 * 列出所有已加载的包
 * 
 * @param manager 包管理器
 * @param output 输出缓冲区
 * @param max_len 缓冲区大小
 */
void package_list(PackageManager *manager, char *output, size_t max_len);

/**
 * 卸载所有包（清理资源）
 * 
 * @param manager 包管理器
 */
void package_manager_cleanup(PackageManager *manager);

/**
 * 扫描 package 目录，列出所有可用的包
 * 
 * @param manager 包管理器
 * @param output 输出缓冲区
 * @param max_len 缓冲区大小
 * @return 可用包的数量
 */
int package_scan_available(PackageManager *manager, char *output, size_t max_len);

#endif /* PACKAGE_H */

