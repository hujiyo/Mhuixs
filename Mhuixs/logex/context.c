#include "context.h"
#include <string.h>
#include <ctype.h>
#include <stdio.h>

/* 初始化上下文 */
void context_init(Context *ctx) {
    if (ctx == NULL) return;
    
    ctx->count = 0;
    for (int i = 0; i < MAX_VARIABLES; i++) {
        ctx->vars[i].name[0] = '\0';
        ctx->vars[i].is_defined = 0;
        bignum_init(&ctx->vars[i].value);
    }
}

/* 验证变量名是否合法（字母开头，后跟字母/数字/下划线，支持 UTF-8 字符） */
static int is_valid_name(const char *name) {
    if (name == NULL || name[0] == '\0') return 0;
    
    unsigned char uc = (unsigned char)name[0];
    /* 第一个字符必须是字母、下划线或 UTF-8 字符 */
    if (!isalpha(uc) && name[0] != '_' && uc < 0x80) return 0;
    
    /* 后续字符必须是字母、数字、下划线或 UTF-8 字符 */
    for (int i = 1; name[i] != '\0'; i++) {
        uc = (unsigned char)name[i];
        if (!isalnum(uc) && name[i] != '_' && uc < 0x80) return 0;
    }
    
    return 1;
}

/* 查找变量索引 */
static int find_variable(const Context *ctx, const char *name) {
    if (ctx == NULL || name == NULL) return -1;
    
    for (int i = 0; i < ctx->count; i++) {
        if (ctx->vars[i].is_defined && strcmp(ctx->vars[i].name, name) == 0) {
            return i;
        }
    }
    
    return -1;
}

/* 设置变量值 */
int context_set(Context *ctx, const char *name, const BigNum *value) {
    if (ctx == NULL || name == NULL || value == NULL) return -1;
    
    /* 验证变量名 */
    if (!is_valid_name(name)) return -1;
    
    /* 检查变量名长度 */
    if (strlen(name) >= MAX_VAR_NAME_LEN) return -1;
    
    /* 查找是否已存在 */
    int idx = find_variable(ctx, name);
    
    if (idx >= 0) {
        /* 更新已存在的变量 */
        /* 先释放旧值（避免内存泄漏），然后复制新值 */
        bignum_free(&ctx->vars[idx].value);
        if (bignum_copy(value, &ctx->vars[idx].value) != 0) {
            return -1;
        }
        return 0;
    }
    
    /* 创建新变量 */
    if (ctx->count >= MAX_VARIABLES) return -1;
    
    idx = ctx->count;
    strncpy(ctx->vars[idx].name, name, MAX_VAR_NAME_LEN - 1);
    ctx->vars[idx].name[MAX_VAR_NAME_LEN - 1] = '\0';
    bignum_init(&ctx->vars[idx].value);
    if (bignum_copy(value, &ctx->vars[idx].value) != 0) {
        return -1;
    }
    ctx->vars[idx].is_defined = 1;
    ctx->count++;
    
    return 0;
}

/* 获取变量值 */
int context_get(const Context *ctx, const char *name, BigNum *value) {
    if (ctx == NULL || name == NULL || value == NULL) return -1;
    
    int idx = find_variable(ctx, name);
    if (idx < 0) return -1;
    
    /* 深拷贝变量值，避免共享内存 */
    return bignum_copy(&ctx->vars[idx].value, value);
}

/* 检查变量是否存在 */
int context_has(const Context *ctx, const char *name) {
    return find_variable(ctx, name) >= 0 ? 1 : 0;
}

/* 列出所有变量 */
void context_list(const Context *ctx, char *buffer, size_t max_len) {
    if (ctx == NULL || buffer == NULL || max_len == 0) return;
    
    int pos = 0;
    
    if (ctx->count == 0) {
        snprintf(buffer, max_len, "（没有定义变量）");
        return;
    }
    
    pos += snprintf(buffer + pos, max_len - pos, "已定义变量：\n");
    
    for (int i = 0; i < ctx->count && pos < (int)max_len - 1; i++) {
        if (ctx->vars[i].is_defined) {
            char value_str[256];
            bignum_to_string(&ctx->vars[i].value, value_str, sizeof(value_str), -1);
            pos += snprintf(buffer + pos, max_len - pos, "  %s = %s\n", 
                          ctx->vars[i].name, value_str);
        }
    }
}

/* 清空所有变量 */
void context_clear(Context *ctx) {
    if (ctx == NULL) return;
    context_init(ctx);
}

