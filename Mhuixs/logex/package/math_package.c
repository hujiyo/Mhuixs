/**
 * Math 包 - 数学函数库
 * 
 * 编译方法：
 *   gcc -shared -fPIC -o package/libmath.so package/math_package.c -I. -lm
 * 
 * 包含函数：
 *   - 三角函数：sin, cos, tan, asin, acos, atan, atan2
 *   - 指数对数：exp, ln, log, log10, log2
 *   - 幂和根：sqrt, cbrt
 *   - 取整：floor, ceil, round, trunc
 *   - 其他：abs, sign, max, min
 * 
 * 包含常量：
 *   - pi, π: 圆周率
 *   - e: 自然常数
 *   - phi, φ: 黄金比例
 */

#include "../function.h"
#include "../bignum.h"
#include "../context.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

/* 辅助函数：将BigNum转换为double */
static double bignum_to_double(const BigNum *num) {
    if (num == NULL) return 0.0;
    
    char *digits = BIGNUM_DIGITS(num);
    
    double result = 0.0;
    double multiplier = 1.0;
    
    for (int i = 0; i < num->length; i++) {
        if (i == num->decimal_pos) {
            multiplier = 1.0;
        } else if (i > num->decimal_pos) {
            multiplier *= 10.0;
        }
        result += digits[i] * multiplier;
        if (i < num->decimal_pos) {
            multiplier /= 10.0;
        }
    }
    
    return num->is_negative ? -result : result;
}

/* 辅助函数：将double转换为BigNum */
static int double_to_bignum(double value, BigNum *num, int precision) {
    if (num == NULL) return -1;
    
    char buffer[512];
    snprintf(buffer, sizeof(buffer), "%.*f", precision, value);
    return bignum_from_string(buffer, num);
}

/* =========================== 三角函数 =========================== */

static int math_sin(const BigNum *args, int arg_count, BigNum *result, int precision) {
    if (arg_count != 1) return -1;
    double x = bignum_to_double(&args[0]);
    return double_to_bignum(sin(x), result, precision);
}

static int math_cos(const BigNum *args, int arg_count, BigNum *result, int precision) {
    if (arg_count != 1) return -1;
    double x = bignum_to_double(&args[0]);
    return double_to_bignum(cos(x), result, precision);
}

static int math_tan(const BigNum *args, int arg_count, BigNum *result, int precision) {
    if (arg_count != 1) return -1;
    double x = bignum_to_double(&args[0]);
    return double_to_bignum(tan(x), result, precision);
}

static int math_asin(const BigNum *args, int arg_count, BigNum *result, int precision) {
    if (arg_count != 1) return -1;
    double x = bignum_to_double(&args[0]);
    if (x < -1.0 || x > 1.0) return -1;
    return double_to_bignum(asin(x), result, precision);
}

static int math_acos(const BigNum *args, int arg_count, BigNum *result, int precision) {
    if (arg_count != 1) return -1;
    double x = bignum_to_double(&args[0]);
    if (x < -1.0 || x > 1.0) return -1;
    return double_to_bignum(acos(x), result, precision);
}

static int math_atan(const BigNum *args, int arg_count, BigNum *result, int precision) {
    if (arg_count != 1) return -1;
    double x = bignum_to_double(&args[0]);
    return double_to_bignum(atan(x), result, precision);
}

static int math_atan2(const BigNum *args, int arg_count, BigNum *result, int precision) {
    if (arg_count != 2) return -1;
    double y = bignum_to_double(&args[0]);
    double x = bignum_to_double(&args[1]);
    return double_to_bignum(atan2(y, x), result, precision);
}

/* =========================== 指数对数函数 =========================== */

static int math_exp(const BigNum *args, int arg_count, BigNum *result, int precision) {
    if (arg_count != 1) return -1;
    double x = bignum_to_double(&args[0]);
    return double_to_bignum(exp(x), result, precision);
}

static int math_ln(const BigNum *args, int arg_count, BigNum *result, int precision) {
    if (arg_count != 1) return -1;
    double x = bignum_to_double(&args[0]);
    if (x <= 0.0) return -1;
    return double_to_bignum(log(x), result, precision);
}

static int math_log(const BigNum *args, int arg_count, BigNum *result, int precision) {
    if (arg_count < 1 || arg_count > 2) return -1;
    
    double x = bignum_to_double(&args[0]);
    if (x <= 0.0) return -1;
    
    if (arg_count == 1) {
        return double_to_bignum(log10(x), result, precision);
    } else {
        double base = bignum_to_double(&args[1]);
        if (base <= 0.0 || base == 1.0) return -1;
        return double_to_bignum(log(x) / log(base), result, precision);
    }
}

static int math_log10(const BigNum *args, int arg_count, BigNum *result, int precision) {
    if (arg_count != 1) return -1;
    double x = bignum_to_double(&args[0]);
    if (x <= 0.0) return -1;
    return double_to_bignum(log10(x), result, precision);
}

static int math_log2(const BigNum *args, int arg_count, BigNum *result, int precision) {
    if (arg_count != 1) return -1;
    double x = bignum_to_double(&args[0]);
    if (x <= 0.0) return -1;
    return double_to_bignum(log2(x), result, precision);
}

/* =========================== 幂和根函数 =========================== */

static int math_sqrt(const BigNum *args, int arg_count, BigNum *result, int precision) {
    if (arg_count != 1) return -1;
    double x = bignum_to_double(&args[0]);
    if (x < 0.0) return -1;
    return double_to_bignum(sqrt(x), result, precision);
}

static int math_cbrt(const BigNum *args, int arg_count, BigNum *result, int precision) {
    if (arg_count != 1) return -1;
    double x = bignum_to_double(&args[0]);
    return double_to_bignum(cbrt(x), result, precision);
}

/* =========================== 取整函数 =========================== */

static int math_floor(const BigNum *args, int arg_count, BigNum *result, int precision) {
    if (arg_count != 1) return -1;
    double x = bignum_to_double(&args[0]);
    return double_to_bignum(floor(x), result, precision);
}

static int math_ceil(const BigNum *args, int arg_count, BigNum *result, int precision) {
    if (arg_count != 1) return -1;
    double x = bignum_to_double(&args[0]);
    return double_to_bignum(ceil(x), result, precision);
}

static int math_round(const BigNum *args, int arg_count, BigNum *result, int precision) {
    if (arg_count != 1) return -1;
    double x = bignum_to_double(&args[0]);
    return double_to_bignum(round(x), result, precision);
}

static int math_trunc(const BigNum *args, int arg_count, BigNum *result, int precision) {
    if (arg_count != 1) return -1;
    double x = bignum_to_double(&args[0]);
    return double_to_bignum(trunc(x), result, precision);
}

/* =========================== 其他函数 =========================== */

static int math_abs(const BigNum *args, int arg_count, BigNum *result, int precision) {
    if (arg_count != 1) return -1;
    if (bignum_copy(&args[0], result) != 0) return -1;
    result->is_negative = 0;
    return 0;
}

static int math_sign(const BigNum *args, int arg_count, BigNum *result, int precision) {
    if (arg_count != 1) return -1;
    
    bignum_init(result);
    
    char *arg_digits = BIGNUM_DIGITS(&args[0]);
    char *result_digits = BIGNUM_DIGITS(result);
    
    int is_zero = 1;
    for (int i = 0; i < args[0].length; i++) {
        if (arg_digits[i] != 0) {
            is_zero = 0;
            break;
        }
    }
    
    if (is_zero) {
        result_digits[0] = 0;
    } else if (args[0].is_negative) {
        result_digits[0] = 1;
        result->is_negative = 1;
    } else {
        result_digits[0] = 1;
    }
    
    result->length = 1;
    return 0;
}

static int math_max(const BigNum *args, int arg_count, BigNum *result, int precision) {
    if (arg_count < 1) return -1;
    
    if (bignum_copy(&args[0], result) != 0) return -1;
    
    for (int i = 1; i < arg_count; i++) {
        double current = bignum_to_double(result);
        double next = bignum_to_double(&args[i]);
        if (next > current) {
            bignum_free(result);
            if (bignum_copy(&args[i], result) != 0) return -1;
        }
    }
    
    return 0;
}

static int math_min(const BigNum *args, int arg_count, BigNum *result, int precision) {
    if (arg_count < 1) return -1;
    
    if (bignum_copy(&args[0], result) != 0) return -1;
    
    for (int i = 1; i < arg_count; i++) {
        double current = bignum_to_double(result);
        double next = bignum_to_double(&args[i]);
        if (next < current) {
            bignum_free(result);
            if (bignum_copy(&args[i], result) != 0) return -1;
        }
    }
    
    return 0;
}

/* =========================== 包注册函数 =========================== */

int package_register(FunctionRegistry *registry) {
    if (registry == NULL) return -1;
    
    int count = 0;
    
    /* 三角函数 */
    count += (function_register(registry, "sin", math_sin, 1, 1, "正弦函数 sin(x)") == 0);
    count += (function_register(registry, "cos", math_cos, 1, 1, "余弦函数 cos(x)") == 0);
    count += (function_register(registry, "tan", math_tan, 1, 1, "正切函数 tan(x)") == 0);
    count += (function_register(registry, "asin", math_asin, 1, 1, "反正弦函数 asin(x)") == 0);
    count += (function_register(registry, "acos", math_acos, 1, 1, "反余弦函数 acos(x)") == 0);
    count += (function_register(registry, "atan", math_atan, 1, 1, "反正切函数 atan(x)") == 0);
    count += (function_register(registry, "atan2", math_atan2, 2, 2, "两参数反正切 atan2(y,x)") == 0);
    
    /* 指数对数函数 */
    count += (function_register(registry, "exp", math_exp, 1, 1, "自然指数函数 e^x") == 0);
    count += (function_register(registry, "ln", math_ln, 1, 1, "自然对数 ln(x)") == 0);
    count += (function_register(registry, "log", math_log, 1, 2, "对数 log(x) 或 log(x,base)") == 0);
    count += (function_register(registry, "log10", math_log10, 1, 1, "常用对数 log10(x)") == 0);
    count += (function_register(registry, "log2", math_log2, 1, 1, "二进制对数 log2(x)") == 0);
    
    /* 幂和根 */
    count += (function_register(registry, "sqrt", math_sqrt, 1, 1, "平方根 √x") == 0);
    count += (function_register(registry, "cbrt", math_cbrt, 1, 1, "立方根 ∛x") == 0);
    
    /* 取整函数 */
    count += (function_register(registry, "floor", math_floor, 1, 1, "向下取整 floor(x)") == 0);
    count += (function_register(registry, "ceil", math_ceil, 1, 1, "向上取整 ceil(x)") == 0);
    count += (function_register(registry, "round", math_round, 1, 1, "四舍五入 round(x)") == 0);
    count += (function_register(registry, "trunc", math_trunc, 1, 1, "截断取整 trunc(x)") == 0);
    
    /* 其他 */
    count += (function_register(registry, "abs", math_abs, 1, 1, "绝对值 |x|") == 0);
    count += (function_register(registry, "sign", math_sign, 1, 1, "符号函数 sign(x)") == 0);
    count += (function_register(registry, "max", math_max, 1, -1, "最大值 max(a,b,...)") == 0);
    count += (function_register(registry, "min", math_min, 1, -1, "最小值 min(a,b,...)") == 0);
    
    return count;
}

int package_register_constants(void *ctx) {
    if (ctx == NULL) return -1;
    
    Context *context = (Context *)ctx;
    BigNum value;
    
    /* π (pi) */
    double_to_bignum(3.141592653589793238462643383279502884197, &value, BIGNUM_DEFAULT_PRECISION);
    context_set(context, "π", &value);
    context_set(context, "pi", &value);
    
    /* e (自然常数) */
    double_to_bignum(2.718281828459045235360287471352662497757, &value, BIGNUM_DEFAULT_PRECISION);
    context_set(context, "e", &value);
    
    /* φ (黄金比例) */
    double_to_bignum(1.618033988749894848204586834365638117720, &value, BIGNUM_DEFAULT_PRECISION);
    context_set(context, "φ", &value);
    context_set(context, "phi", &value);
    
    return 0;
}
