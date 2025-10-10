#include "function.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* 初始化函数注册表 */
void function_registry_init(FunctionRegistry *registry) {
    if (registry == NULL) return;
    
    for (int i = 0; i < MAX_FUNCTIONS; i++) {
        registry->functions[i].is_defined = 0;
        registry->functions[i].name[0] = '\0';
    }
    registry->count = 0;
}

/* 注册一个函数 */
int function_register(FunctionRegistry *registry, 
                     const char *name, 
                     NativeFunction func,
                     int min_args,
                     int max_args,
                     const char *description) {
    if (registry == NULL || name == NULL || func == NULL) {
        return -1;
    }
    
    if (registry->count >= MAX_FUNCTIONS) {
        return -1;  /* 函数表已满 */
    }
    
    /* 检查函数名是否已存在 */
    if (function_lookup(registry, name) != NULL) {
        return -1;  /* 函数名已存在 */
    }
    
    /* 找到第一个空位 */
    int idx = -1;
    for (int i = 0; i < MAX_FUNCTIONS; i++) {
        if (!registry->functions[i].is_defined) {
            idx = i;
            break;
        }
    }
    
    if (idx == -1) return -1;
    
    /* 注册函数 */
    FunctionInfo *info = &registry->functions[idx];
    strncpy(info->name, name, MAX_FUNC_NAME_LEN - 1);
    info->name[MAX_FUNC_NAME_LEN - 1] = '\0';
    info->func = func;
    info->min_args = min_args;
    info->max_args = max_args;
    if (description) {
        strncpy(info->description, description, 255);
        info->description[255] = '\0';
    } else {
        info->description[0] = '\0';
    }
    info->is_defined = 1;
    
    registry->count++;
    return 0;
}

/* 查找函数 */
FunctionInfo* function_lookup(FunctionRegistry *registry, const char *name) {
    if (registry == NULL || name == NULL) return NULL;
    
    for (int i = 0; i < MAX_FUNCTIONS; i++) {
        if (registry->functions[i].is_defined &&
            strcmp(registry->functions[i].name, name) == 0) {
            return &registry->functions[i];
        }
    }
    
    return NULL;
}

/* 调用函数 */
int function_call(FunctionInfo *info, 
                 const BigNum *args, 
                 int arg_count, 
                 BigNum *result, 
                 int precision) {
    if (info == NULL || args == NULL || result == NULL) {
        return -1;
    }
    
    /* 检查参数数量 */
    if (arg_count < info->min_args) {
        return -1;  /* 参数太少 */
    }
    
    if (info->max_args >= 0 && arg_count > info->max_args) {
        return -1;  /* 参数太多 */
    }
    
    /* 调用C函数 */
    return info->func(args, arg_count, result, precision);
}

/* 列出所有已注册的函数 */
void function_list(FunctionRegistry *registry, char *output, size_t max_len) {
    if (registry == NULL || output == NULL || max_len == 0) return;
    
    if (registry->count == 0) {
        snprintf(output, max_len, "无可用函数");
        return;
    }
    
    int offset = 0;
    offset += snprintf(output + offset, max_len - offset, "可用函数列表：\n");
    
    for (int i = 0; i < MAX_FUNCTIONS && offset < (int)max_len - 1; i++) {
        if (registry->functions[i].is_defined) {
            FunctionInfo *info = &registry->functions[i];
            
            if (info->description[0] != '\0') {
                offset += snprintf(output + offset, max_len - offset,
                                 "  %s - %s\n", 
                                 info->name, info->description);
            } else {
                offset += snprintf(output + offset, max_len - offset,
                                 "  %s\n", info->name);
            }
        }
    }
}


