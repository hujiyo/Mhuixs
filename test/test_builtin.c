/**
 * 测试 Logex 内置函数
 */

#include <stdio.h>
#include <string.h>
#include "evaluator.h"
#include "context.h"
#include "function.h"
#include "package.h"

int main() {
    printf("========================================\n");
    printf("Logex 内置函数测试\n");
    printf("========================================\n\n");
    
    Context ctx;
    context_init(&ctx);
    
    FunctionRegistry registry;
    function_registry_init(&registry);
    
    PackageManager pkg_mgr;
    package_manager_init(&pkg_mgr, "./package");
    
    char result_str[1024];
    int ret;
    
    /* 测试 1: 创建 LIST */
    printf("测试 1: 创建 LIST\n");
    ret = eval_statement("list()", result_str, sizeof(result_str), &ctx, &registry, &pkg_mgr, 10);
    if (ret >= 0) {
        printf("✅ list() 成功: %s\n", result_str);
    } else {
        printf("❌ list() 失败\n");
    }
    
    /* 测试 2: LIST 操作 */
    printf("\n测试 2: LIST 操作\n");
    ret = eval_statement("let mylist = list()", result_str, sizeof(result_str), &ctx, &registry, &pkg_mgr, 10);
    ret = eval_statement("let mylist = rpush(mylist, 100)", result_str, sizeof(result_str), &ctx, &registry, &pkg_mgr, 10);
    ret = eval_statement("let mylist = rpush(mylist, 200)", result_str, sizeof(result_str), &ctx, &registry, &pkg_mgr, 10);
    ret = eval_statement("llen(mylist)", result_str, sizeof(result_str), &ctx, &registry, &pkg_mgr, 10);
    if (ret >= 0) {
        printf("✅ LIST 操作成功，长度: %s\n", result_str);
    } else {
        printf("❌ LIST 操作失败\n");
    }
    
    /* 测试 3: TYPE 转换 */
    printf("\n测试 3: TYPE 转换\n");
    ret = eval_statement("num(\"123.456\")", result_str, sizeof(result_str), &ctx, &registry, &pkg_mgr, 10);
    if (ret >= 0) {
        printf("✅ num(\"123.456\") = %s\n", result_str);
    } else {
        printf("❌ num() 失败\n");
    }
    
    ret = eval_statement("str(789)", result_str, sizeof(result_str), &ctx, &registry, &pkg_mgr, 10);
    if (ret >= 0) {
        printf("✅ str(789) = %s\n", result_str);
    } else {
        printf("❌ str() 失败\n");
    }
    
    /* 测试 4: BITMAP 操作 */
    printf("\n测试 4: BITMAP 操作\n");
    ret = eval_statement("let bm = bmp(0)", result_str, sizeof(result_str), &ctx, &registry, &pkg_mgr, 10);
    ret = eval_statement("let bm = bset(bm, 10, 1)", result_str, sizeof(result_str), &ctx, &registry, &pkg_mgr, 10);
    ret = eval_statement("bget(bm, 10)", result_str, sizeof(result_str), &ctx, &registry, &pkg_mgr, 10);
    if (ret >= 0) {
        printf("✅ BITMAP 操作成功，bget(bm, 10) = %s\n", result_str);
    } else {
        printf("❌ BITMAP 操作失败\n");
    }
    
    printf("\n========================================\n");
    printf("测试完成\n");
    printf("========================================\n");
    
    context_clear(&ctx);
    package_manager_cleanup(&pkg_mgr);
    return 0;
}
