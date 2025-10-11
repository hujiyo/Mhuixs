#include "package.h"
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>
#include <dirent.h>
#include <sys/stat.h>

/* 包注册函数类型 */
typedef int (*PackageRegisterFunc)(FunctionRegistry *registry);
typedef int (*PackageRegisterConstantsFunc)(void *ctx);

/* 初始化包管理器 */
void package_manager_init(PackageManager *manager, const char *package_dir) {
    if (manager == NULL) return;
    
    for (int i = 0; i < MAX_PACKAGES; i++) {
        manager->packages[i].is_loaded = 0;
        manager->packages[i].name[0] = '\0';
        manager->packages[i].handle = NULL;
        manager->packages[i].func_count = 0;
    }
    manager->count = 0;
    
    /* 设置包目录 */
    if (package_dir == NULL) {
        strncpy(manager->package_dir, "./package", sizeof(manager->package_dir) - 1);
    } else {
        strncpy(manager->package_dir, package_dir, sizeof(manager->package_dir) - 1);
    }
    manager->package_dir[sizeof(manager->package_dir) - 1] = '\0';
}

/* 加载包 */
int package_load(PackageManager *manager, const char *package_name, 
                 FunctionRegistry *registry, void *ctx) {
    if (manager == NULL || package_name == NULL || registry == NULL) {
        return -1;
    }
    
    /* 检查是否已加载 */
    if (package_is_loaded(manager, package_name)) {
        /* 已加载，返回之前注册的函数数量 */
        for (int i = 0; i < MAX_PACKAGES; i++) {
            if (manager->packages[i].is_loaded && 
                strcmp(manager->packages[i].name, package_name) == 0) {
                return manager->packages[i].func_count;
            }
        }
    }
    
    /* 检查是否有空位 */
    if (manager->count >= MAX_PACKAGES) {
        return -1;  /* 包表已满 */
    }
    
    /* 构造动态库路径 */
    char lib_path[512];
    snprintf(lib_path, sizeof(lib_path), "%s/lib%s.so", 
             manager->package_dir, package_name);
    
    /* 加载动态库 */
    void *handle = dlopen(lib_path, RTLD_LAZY);
    if (handle == NULL) {
        fprintf(stderr, "无法加载包 '%s': %s\n", package_name, dlerror());
        return -1;
    }
    
    /* 查找 package_register 函数 */
    PackageRegisterFunc register_func = (PackageRegisterFunc)dlsym(handle, "package_register");
    if (register_func == NULL) {
        fprintf(stderr, "包 '%s' 缺少 package_register() 函数\n", package_name);
        dlclose(handle);
        return -1;
    }
    
    /* 调用注册函数 */
    int func_count = register_func(registry);
    if (func_count < 0) {
        fprintf(stderr, "包 '%s' 注册失败\n", package_name);
        dlclose(handle);
        return -1;
    }
    
    /* 尝试注册常量（可选） */
    if (ctx != NULL) {
        PackageRegisterConstantsFunc register_constants = 
            (PackageRegisterConstantsFunc)dlsym(handle, "package_register_constants");
        if (register_constants != NULL) {
            register_constants(ctx);
        }
    }
    
    /* 找到空位保存包信息 */
    int idx = -1;
    for (int i = 0; i < MAX_PACKAGES; i++) {
        if (!manager->packages[i].is_loaded) {
            idx = i;
            break;
        }
    }
    
    if (idx == -1) {
        dlclose(handle);
        return -1;
    }
    
    /* 保存包信息 */
    PackageInfo *pkg = &manager->packages[idx];
    strncpy(pkg->name, package_name, MAX_PACKAGE_NAME_LEN - 1);
    pkg->name[MAX_PACKAGE_NAME_LEN - 1] = '\0';
    pkg->handle = handle;
    pkg->is_loaded = 1;
    pkg->func_count = func_count;
    
    manager->count++;
    
    return func_count;
}

/* 检查包是否已加载 */
int package_is_loaded(PackageManager *manager, const char *package_name) {
    if (manager == NULL || package_name == NULL) return 0;
    
    for (int i = 0; i < MAX_PACKAGES; i++) {
        if (manager->packages[i].is_loaded && 
            strcmp(manager->packages[i].name, package_name) == 0) {
            return 1;
        }
    }
    
    return 0;
}

/* 列出所有已加载的包 */
void package_list(PackageManager *manager, char *output, size_t max_len) {
    if (manager == NULL || output == NULL || max_len == 0) return;
    
    if (manager->count == 0) {
        snprintf(output, max_len, "无已加载的包");
        return;
    }
    
    int offset = 0;
    offset += snprintf(output + offset, max_len - offset, "已加载的包：\n");
    
    for (int i = 0; i < MAX_PACKAGES && offset < (int)max_len - 1; i++) {
        if (manager->packages[i].is_loaded) {
            offset += snprintf(output + offset, max_len - offset,
                             "  %s (%d 个函数)\n", 
                             manager->packages[i].name,
                             manager->packages[i].func_count);
        }
    }
}

/* 卸载所有包 */
void package_manager_cleanup(PackageManager *manager) {
    if (manager == NULL) return;
    
    for (int i = 0; i < MAX_PACKAGES; i++) {
        if (manager->packages[i].is_loaded && manager->packages[i].handle != NULL) {
            dlclose(manager->packages[i].handle);
            manager->packages[i].is_loaded = 0;
            manager->packages[i].handle = NULL;
        }
    }
    
    manager->count = 0;
}

/* 扫描可用的包 */
int package_scan_available(PackageManager *manager, char *output, size_t max_len) {
    if (manager == NULL || output == NULL || max_len == 0) return 0;
    
    DIR *dir = opendir(manager->package_dir);
    if (dir == NULL) {
        snprintf(output, max_len, "无法打开包目录: %s", manager->package_dir);
        return 0;
    }
    
    int count = 0;
    int offset = 0;
    offset += snprintf(output + offset, max_len - offset, "可用的包：\n");
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && offset < (int)max_len - 1) {
        /* 检查是否是 .so 文件 */
        size_t name_len = strlen(entry->d_name);
        if (name_len > 6 && 
            strncmp(entry->d_name, "lib", 3) == 0 &&
            strcmp(entry->d_name + name_len - 3, ".so") == 0) {
            
            /* 提取包名（去掉 lib 前缀和 .so 后缀） */
            char package_name[MAX_PACKAGE_NAME_LEN];
            strncpy(package_name, entry->d_name + 3, name_len - 6);
            package_name[name_len - 6] = '\0';
            
            /* 检查是否已加载 */
            int is_loaded = package_is_loaded(manager, package_name);
            
            offset += snprintf(output + offset, max_len - offset,
                             "  %s%s\n", 
                             package_name,
                             is_loaded ? " (已加载)" : "");
            count++;
        }
    }
    
    closedir(dir);
    
    if (count == 0) {
        snprintf(output, max_len, "包目录 '%s' 中没有可用的包", manager->package_dir);
    }
    
    return count;
}
