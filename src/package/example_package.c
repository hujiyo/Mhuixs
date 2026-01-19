/**
 * 示例包 - 演示如何编写符合规范的包
 * 
 * 编译方法：
 *   gcc -shared -fPIC -o package/libexample.so package/example_package.c -I.
 * 
 * 使用方法：
 *   expr > import example
 *   已导入 example 包 (3 个函数)
 *   
 *   expr > double(5)
 *   = 10
 *   
 *   expr > triple(3)
 *   = 9
 *   
 *   expr > square(4)
 *   = 16
 */

#include "../function.h"
#include "../bignum.h"
#include <string.h>

/* 示例函数1: double(x) = x * 2 */
static int example_double(const BHS *args, int arg_count, BHS *result, int precision) {
    if (arg_count != 1) return -1;
    
    BHS two;
    bignum_from_string("2", &two);
    
    return bignum_mul(&args[0], &two, result);
}

/* 示例函数2: triple(x) = x * 3 */
static int example_triple(const BHS *args, int arg_count, BHS *result, int precision) {
    if (arg_count != 1) return -1;
    
    BHS three;
    bignum_from_string("3", &three);
    
    return bignum_mul(&args[0], &three, result);
}

/* 示例函数3: square(x) = x * x */
static int example_square(const BHS *args, int arg_count, BHS *result, int precision) {
    if (arg_count != 1) return -1;
    
    return bignum_mul(&args[0], &args[0], result);
}

/**
 * 包注册函数（必需）
 * 
 * 这个函数会在包加载时被调用，用于注册包中的所有函数。
 * 
 * @param registry 函数注册表
 * @return 成功注册的函数数量，失败返回 -1
 */
int package_register(FunctionRegistry *registry) {
    if (registry == NULL) return -1;
    
    int count = 0;
    
    /* 注册函数 */
    count += (function_register(registry, "double", example_double, 1, 1, 
                                "将数值翻倍 double(x)") == 0);
    count += (function_register(registry, "triple", example_triple, 1, 1, 
                                "将数值乘以3 triple(x)") == 0);
    count += (function_register(registry, "square", example_square, 1, 1, 
                                "计算平方 square(x)") == 0);
    
    return count;
}

/**
 * 包常量注册函数（可选）
 * 
 * 如果包需要注册常量，可以实现这个函数。
 * 
 * @param ctx 变量上下文
 * @return 0 成功, -1 失败
 */
int package_register_constants(void *ctx) {
    /* 这个包不需要注册常量 */
    return 0;
}

