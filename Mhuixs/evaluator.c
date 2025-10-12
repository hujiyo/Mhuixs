#include "evaluator.h"
#include "bignum.h"
#include "lexer.h"
#include "context.h"
#include "function.h"
#include "package.h"
#include <string.h>
#include <ctype.h>
#include <stdio.h>

/* 辅助函数：比较两个大数（考虑符号）
 * 返回: 1 if a>b, 0 if a==b, -1 if a<b */
static int bignum_compare(const BigNum *a, const BigNum *b) {
    if (a == NULL || b == NULL) return 0;
    
    /* 处理符号 */
    if (a->type_data.num.is_negative && !b->type_data.num.is_negative) return -1;  /* 负数 < 正数 */
    if (!a->type_data.num.is_negative && b->type_data.num.is_negative) return 1;   /* 正数 > 负数 */
    
    /* 同号情况 */
    /* 先比较整数部分长度 */
    int a_int_len = a->length - a->type_data.num.decimal_pos;
    int b_int_len = b->length - b->type_data.num.decimal_pos;
    
    int sign_multiplier = a->type_data.num.is_negative ? -1 : 1;
    
    if (a_int_len > b_int_len) return 1 * sign_multiplier;
    if (a_int_len < b_int_len) return -1 * sign_multiplier;
    
    char *a_digits = BIGNUM_DIGITS(a);
    char *b_digits = BIGNUM_DIGITS(b);
    
    /* 从高位到低位比较 */
    for (int i = a->length - 1; i >= 0; i--) {
        int b_idx = i - a->type_data.num.decimal_pos + b->type_data.num.decimal_pos;
        int b_digit = (b_idx >= 0 && b_idx < b->length) ? b_digits[b_idx] : 0;
        int a_digit = a_digits[i];
        
        if (a_digit > b_digit) return 1 * sign_multiplier;
        if (a_digit < b_digit) return -1 * sign_multiplier;
    }
    
    /* 检查b是否有更多的小数位 */
    for (int i = a->type_data.num.decimal_pos; i < b->type_data.num.decimal_pos; i++) {
        if (i < b->length && b_digits[i] != 0) return -1 * sign_multiplier;
    }
    
    return 0;
}

/* 辅助函数：判断大数是否为真 */
int bignum_is_true(const BigNum *num) {
    if (num == NULL) return 0;
    
    char *digits = BIGNUM_DIGITS(num);
    
    /* 检查是否所有位都是0 */
    for (int i = 0; i < num->length; i++) {
        if (digits[i] != 0) return 1;
    }
    return 0;
}

/* 辅助函数：将大数转换为布尔值（已弃用，不再使用） */

/* 以下词法分析功能已移到 lexer.c 统一实现 */

/* 前向声明 */
static int parse_expression(Lexer *lex, BigNum *result, Context *ctx, FunctionRegistry *func_registry, int precision);

/* 解析主表达式（原子、一元运算符、括号、函数调用） */
static int parse_primary(Lexer *lex, BigNum *result, Context *ctx, FunctionRegistry *func_registry, int precision) {
    /* 数字 */
    if (lex->current_type == TOK_NUMBER) {
        BigNum *temp = bignum_from_string(lex->current_value);
        if (temp == NULL) {
            return EVAL_ERROR;
        }
        if (bignum_copy(temp, result) != BIGNUM_SUCCESS) {
            bignum_destroy(temp);
            return EVAL_ERROR;
        }
        bignum_destroy(temp);
        lexer_next(lex);
        return EVAL_SUCCESS;
    }
    
    /* 字符串字面量 */
    if (lex->current_type == TOK_STRING) {
        BigNum *temp = bignum_from_raw_string(lex->current_value);
        if (temp == NULL) {
            return EVAL_ERROR;
        }
        if (bignum_copy(temp, result) != BIGNUM_SUCCESS) {
            bignum_destroy(temp);
            return EVAL_ERROR;
        }
        bignum_destroy(temp);
        lexer_next(lex);
        return EVAL_SUCCESS;
    }
    
    /* 位图字面量 */
    if (lex->current_type == TOK_BITMAP) {
        BigNum *temp = bignum_from_binary_string(lex->current_value);
        if (temp == NULL) {
            return EVAL_ERROR;
        }
        if (bignum_copy(temp, result) != BIGNUM_SUCCESS) {
            bignum_destroy(temp);
            return EVAL_ERROR;
        }
        bignum_destroy(temp);
        lexer_next(lex);
        return EVAL_SUCCESS;
    }
    
    /* 标识符（变量或函数调用） */
    if (lex->current_type == TOK_IDENTIFIER) {
        char name[MAX_FUNC_NAME_LEN];
        strncpy(name, lex->current_value, MAX_FUNC_NAME_LEN - 1);
        name[MAX_FUNC_NAME_LEN - 1] = '\0';
        
        lexer_next(lex);
        
        /* 检查是否是函数调用 */
        if (lex->current_type == TOK_LPAREN) {
            /* 函数调用 */
            if (func_registry == NULL) {
                return EVAL_ERROR;  /* 没有函数注册表 */
            }
            
            FunctionInfo *func = function_lookup(func_registry, name);
            if (func == NULL) {
                return EVAL_ERROR;  /* 函数未定义 */
            }
            
            lexer_next(lex);  /* 跳过 ( */
            
            /* 解析参数列表 - 使用栈分配保持与function_call接口兼容 */
            BigNum args[MAX_FUNC_ARGS];
            int arg_count = 0;
            
            /* 初始化参数数组 */
            for (int i = 0; i < MAX_FUNC_ARGS; i++) {
                bignum_init(&args[i]);
            }
            
            /* 处理空参数列表 */
            if (lex->current_type == TOK_RPAREN) {
                lexer_next(lex);
            } else {
                while (1) {
                    if (arg_count >= MAX_FUNC_ARGS) {
                        /* 清理已分配的参数 */
                        for (int i = 0; i < arg_count; i++) {
                            bignum_free(&args[i]);
                        }
                        return EVAL_ERROR;  /* 参数太多 */
                    }
                    
                    /* 解析一个参数表达式 */
                    int ret = parse_expression(lex, &args[arg_count], ctx, func_registry, precision);
                    if (ret != EVAL_SUCCESS) {
                        /* 清理已分配的参数 */
                        for (int i = 0; i <= arg_count; i++) {
                            bignum_free(&args[i]);
                        }
                        return ret;
                    }
                    arg_count++;
                    
                    /* 检查是否还有更多参数 */
                    if (lex->current_type == TOK_COMMA) {
                        lexer_next(lex);
                        continue;
                    } else if (lex->current_type == TOK_RPAREN) {
                        lexer_next(lex);
                        break;
                    } else {
                        /* 清理已分配的参数 */
                        for (int i = 0; i < arg_count; i++) {
                            bignum_free(&args[i]);
                        }
                        return EVAL_ERROR;  /* 语法错误 */
                    }
                }
            }
            
            /* 调用函数 */
            int call_ret = function_call(func, args, arg_count, result, precision);
            
            /* 清理参数 */
            for (int i = 0; i < arg_count; i++) {
                bignum_free(&args[i]);
            }
            
            return call_ret;
        } else {
            /* 变量引用 */
            if (ctx == NULL || context_get(ctx, name, result) != 0) {
                return EVAL_ERROR;  /* 变量未定义 */
            }
            return EVAL_SUCCESS;
        }
    }
    
    /* 一元负号 */
    if (lex->current_type == TOK_MINUS) {
        lexer_next(lex);
        int ret = parse_primary(lex, result, ctx, func_registry, precision);
        if (ret != EVAL_SUCCESS) return ret;
        result->type_data.num.is_negative = !result->type_data.num.is_negative;
        return EVAL_SUCCESS;
    }
    
    /* 一元正号 */
    if (lex->current_type == TOK_PLUS) {
        lexer_next(lex);
        return parse_primary(lex, result, ctx, func_registry, precision);
    }
    
    /* 逻辑非 */
    if (lex->current_type == TOK_NOT) {
        lexer_next(lex);
        BigNum temp;
        bignum_init(&temp);
        int ret = parse_primary(lex, &temp, ctx, func_registry, precision);
        if (ret != EVAL_SUCCESS) {
            bignum_free(&temp);
            return ret;
        }
        
        /* 将结果转换为布尔值，然后取反 */
        int is_true = bignum_is_true(&temp);
        bignum_free(&temp);
        
        bignum_init(result);
        char *digits = BIGNUM_DIGITS(result);
        digits[0] = is_true ? 0 : 1;
        result->length = 1;
        result->type_data.num.decimal_pos = 0;
        result->type_data.num.is_negative = 0;
        return EVAL_SUCCESS;
    }
    
    /* 位非 */
    if (lex->current_type == TOK_BITNOT) {
        lexer_next(lex);
        BigNum temp;
        bignum_init(&temp);
        int ret = parse_primary(lex, &temp, ctx, func_registry, precision);
        if (ret != EVAL_SUCCESS) {
            bignum_free(&temp);
            return ret;
        }
        
        /* 只能对位图类型进行位非运算 */
        if (temp.type != BIGNUM_TYPE_BITMAP) {
            bignum_free(&temp);
            return EVAL_ERROR;
        }
        
        BigNum *bitwise_result = bignum_bitnot(&temp);
        bignum_free(&temp);
        
        if (bitwise_result == NULL) {
            return EVAL_ERROR;
        }
        
        if (bignum_copy(bitwise_result, result) != BIGNUM_SUCCESS) {
            bignum_destroy(bitwise_result);
            return EVAL_ERROR;
        }
        bignum_destroy(bitwise_result);
        return EVAL_SUCCESS;
    }
    
    /* 括号 */
    if (lex->current_type == TOK_LPAREN) {
        lexer_next(lex);
        int ret = parse_expression(lex, result, ctx, func_registry, precision);
        if (ret != EVAL_SUCCESS) return ret;
        if (lex->current_type != TOK_RPAREN) {
            return EVAL_ERROR;
        }
        lexer_next(lex);
        return EVAL_SUCCESS;
    }
    
    return EVAL_ERROR;
}

/* 解析幂运算（右结合） */
static int parse_power(Lexer *lex, BigNum *result, Context *ctx, FunctionRegistry *func_registry, int precision) {
    int ret = parse_primary(lex, result, ctx, func_registry, precision);
    if (ret != EVAL_SUCCESS) return ret;
    
    if (lex->current_type == TOK_POWER) {
        lexer_next(lex);
        
        BigNum right;
        bignum_init(&right);
        ret = parse_power(lex, &right, ctx, func_registry, precision);  /* 右结合 */
        if (ret != EVAL_SUCCESS) {
            bignum_free(&right);
            return ret;
        }
        
        BigNum *temp = bignum_pow(result, &right, precision);
        bignum_free(&right);
        
        if (temp == NULL) {
            return EVAL_ERROR;
        }
        
        bignum_free(result);
        if (bignum_copy(temp, result) != BIGNUM_SUCCESS) {
            bignum_destroy(temp);
            return EVAL_ERROR;
        }
        bignum_destroy(temp);
    }
    
    return EVAL_SUCCESS;
}

/* 解析乘除取模运算 */
static int parse_term(Lexer *lex, BigNum *result, Context *ctx, FunctionRegistry *func_registry, int precision) {
    int ret = parse_power(lex, result, ctx, func_registry, precision);
    if (ret != EVAL_SUCCESS) return ret;
    
    while (lex->current_type == TOK_MULTIPLY || lex->current_type == TOK_DIVIDE || lex->current_type == TOK_MOD) {
        TokenType op = lex->current_type;
        lexer_next(lex);
        
        BigNum right;
        bignum_init(&right);
        ret = parse_power(lex, &right, ctx, func_registry, precision);
        if (ret != EVAL_SUCCESS) {
            bignum_free(&right);
            return ret;
        }
        
        /* 类型检查 */
        if (result->type != right.type) {
            bignum_free(&right);
            return EVAL_ERROR;  /* 类型不匹配 */
        }
        
        /* 位图类型不支持乘除取模运算 */
        if (result->type == BIGNUM_TYPE_BITMAP) {
            bignum_free(&right);
            return EVAL_ERROR;
        }
        
        BigNum *temp = NULL;
        if (op == TOK_MULTIPLY) {
            temp = bignum_mul(result, &right);
        } else if (op == TOK_DIVIDE) {
            temp = bignum_div(result, &right, precision);
        } else {  /* TOK_MOD */
            temp = bignum_mod(result, &right);
        }
        bignum_free(&right);
        
        if (temp == NULL) {
            return (op == TOK_DIVIDE || op == TOK_MOD) ? EVAL_DIV_ZERO : EVAL_ERROR;
        }
        
        bignum_free(result);
        if (bignum_copy(temp, result) != BIGNUM_SUCCESS) {
            bignum_destroy(temp);
            return EVAL_ERROR;
        }
        bignum_destroy(temp);
    }
    
    return EVAL_SUCCESS;
}

/* 解析加减运算 */
static int parse_add_sub(Lexer *lex, BigNum *result, Context *ctx, FunctionRegistry *func_registry, int precision) {
    int ret = parse_term(lex, result, ctx, func_registry, precision);
    if (ret != EVAL_SUCCESS) return ret;
    
    while (lex->current_type == TOK_PLUS || lex->current_type == TOK_MINUS) {
        TokenType op = lex->current_type;
        lexer_next(lex);
        
        BigNum right;
        bignum_init(&right);
        ret = parse_term(lex, &right, ctx, func_registry, precision);
        if (ret != EVAL_SUCCESS) {
            bignum_free(&right);
            return ret;
        }
        
        /* 类型检查 */
        if (result->type != right.type) {
            bignum_free(&right);
            return EVAL_ERROR;  /* 类型不匹配 */
        }
        
        /* 位图类型不支持加减运算 */
        if (result->type == BIGNUM_TYPE_BITMAP) {
            bignum_free(&right);
            return EVAL_ERROR;
        }
        
        BigNum *temp = NULL;
        if (op == TOK_PLUS) {
            temp = bignum_add(result, &right);
        } else {
            temp = bignum_sub(result, &right);
        }
        bignum_free(&right);
        
        if (temp == NULL) {
            return EVAL_ERROR;
        }
        
        bignum_free(result);
        if (bignum_copy(temp, result) != BIGNUM_SUCCESS) {
            bignum_destroy(temp);
            return EVAL_ERROR;
        }
        bignum_destroy(temp);
    }
    
    return EVAL_SUCCESS;
}

/* 解析移位运算（在加减之后，比较之前） */
static int parse_shift(Lexer *lex, BigNum *result, Context *ctx, FunctionRegistry *func_registry, int precision) {
    int ret = parse_add_sub(lex, result, ctx, func_registry, precision);
    if (ret != EVAL_SUCCESS) return ret;
    
    while (lex->current_type == TOK_BITSHL || lex->current_type == TOK_BITSHR) {
        TokenType op = lex->current_type;
        lexer_next(lex);
        
        BigNum right;
        bignum_init(&right);
        ret = parse_add_sub(lex, &right, ctx, func_registry, precision);
        if (ret != EVAL_SUCCESS) {
            bignum_free(&right);
            return ret;
        }
        
        /* 左操作数必须是位图，右操作数必须是数字 */
        if (result->type != BIGNUM_TYPE_BITMAP || right.type != BIGNUM_TYPE_NUMBER) {
            bignum_free(&right);
            return EVAL_ERROR;
        }
        
        BigNum *temp = NULL;
        if (op == TOK_BITSHL) {
            temp = bignum_bitshl(result, &right);
        } else {
            temp = bignum_bitshr(result, &right);
        }
        bignum_free(&right);
        
        if (temp == NULL) {
            return EVAL_ERROR;
        }
        
        bignum_free(result);
        if (bignum_copy(temp, result) != BIGNUM_SUCCESS) {
            bignum_destroy(temp);
            return EVAL_ERROR;
        }
        bignum_destroy(temp);
    }
    
    return EVAL_SUCCESS;
}

/* 解析比较运算 (==, !=, <, <=, >, >=) */
static int parse_comparison(Lexer *lex, BigNum *result, Context *ctx, FunctionRegistry *func_registry, int precision) {
    int ret = parse_shift(lex, result, ctx, func_registry, precision);
    if (ret != EVAL_SUCCESS) return ret;
    
    while (lex->current_type == TOK_EQ || lex->current_type == TOK_NE ||
           lex->current_type == TOK_LT || lex->current_type == TOK_LE ||
           lex->current_type == TOK_GT || lex->current_type == TOK_GE) {
        TokenType op = lex->current_type;
        lexer_next(lex);
        
        BigNum right;
        bignum_init(&right);
        ret = parse_shift(lex, &right, ctx, func_registry, precision);
        if (ret != EVAL_SUCCESS) {
            bignum_free(&right);
            return ret;
        }
        
        /* 比较两个数 */
        int cmp = bignum_compare(result, &right);
        bignum_free(&right);
        
        int bool_result = 0;
        switch (op) {
            case TOK_EQ:  /* == 等于 */
                bool_result = (cmp == 0) ? 1 : 0;
                break;
            case TOK_NE:  /* != 不等于 */
                bool_result = (cmp != 0) ? 1 : 0;
                break;
            case TOK_LT:  /* < 小于 */
                bool_result = (cmp < 0) ? 1 : 0;
                break;
            case TOK_LE:  /* <= 小于等于 */
                bool_result = (cmp <= 0) ? 1 : 0;
                break;
            case TOK_GT:  /* > 大于 */
                bool_result = (cmp > 0) ? 1 : 0;
                break;
            case TOK_GE:  /* >= 大于等于 */
                bool_result = (cmp >= 0) ? 1 : 0;
                break;
            default:
                return EVAL_ERROR;
        }
        
        bignum_free(result);
        bignum_init(result);
        char *digits = BIGNUM_DIGITS(result);
        digits[0] = bool_result;
        result->length = 1;
        result->type_data.num.decimal_pos = 0;
        result->type_data.num.is_negative = 0;
    }
    
    return EVAL_SUCCESS;
}

/* 解析位与 (&) */
static int parse_bitand(Lexer *lex, BigNum *result, Context *ctx, FunctionRegistry *func_registry, int precision) {
    int ret = parse_comparison(lex, result, ctx, func_registry, precision);
    if (ret != EVAL_SUCCESS) return ret;
    
    while (lex->current_type == TOK_BITAND) {
        lexer_next(lex);
        
        BigNum right;
        bignum_init(&right);
        ret = parse_comparison(lex, &right, ctx, func_registry, precision);
        if (ret != EVAL_SUCCESS) {
            bignum_free(&right);
            return ret;
        }
        
        /* 两边都必须是位图类型 */
        if (result->type != BIGNUM_TYPE_BITMAP || right.type != BIGNUM_TYPE_BITMAP) {
            bignum_free(&right);
            return EVAL_ERROR;
        }
        
        BigNum *temp = bignum_bitand(result, &right);
        bignum_free(&right);
        
        if (temp == NULL) {
            return EVAL_ERROR;
        }
        
        bignum_free(result);
        if (bignum_copy(temp, result) != BIGNUM_SUCCESS) {
            bignum_destroy(temp);
            return EVAL_ERROR;
        }
        bignum_destroy(temp);
    }
    
    return EVAL_SUCCESS;
}

/* 解析位或 (|) */
static int parse_bitor(Lexer *lex, BigNum *result, Context *ctx, FunctionRegistry *func_registry, int precision) {
    int ret = parse_bitand(lex, result, ctx, func_registry, precision);
    if (ret != EVAL_SUCCESS) return ret;
    
    while (lex->current_type == TOK_BITOR) {
        lexer_next(lex);
        
        BigNum right;
        bignum_init(&right);
        ret = parse_bitand(lex, &right, ctx, func_registry, precision);
        if (ret != EVAL_SUCCESS) {
            bignum_free(&right);
            return ret;
        }
        
        /* 两边都必须是位图类型 */
        if (result->type != BIGNUM_TYPE_BITMAP || right.type != BIGNUM_TYPE_BITMAP) {
            bignum_free(&right);
            return EVAL_ERROR;
        }
        
        BigNum *temp = bignum_bitor(result, &right);
        bignum_free(&right);
        
        if (temp == NULL) {
            return EVAL_ERROR;
        }
        
        bignum_free(result);
        if (bignum_copy(temp, result) != BIGNUM_SUCCESS) {
            bignum_destroy(temp);
            return EVAL_ERROR;
        }
        bignum_destroy(temp);
    }
    
    return EVAL_SUCCESS;
}

/* 解析合取 (AND) 和位异或 (^) - 根据操作数类型动态选择 */
static int parse_and(Lexer *lex, BigNum *result, Context *ctx, FunctionRegistry *func_registry, int precision) {
    int ret = parse_bitor(lex, result, ctx, func_registry, precision);
    if (ret != EVAL_SUCCESS) return ret;
    
    while (lex->current_type == TOK_AND) {
        lexer_next(lex);
        
        BigNum right;
        bignum_init(&right);
        ret = parse_bitor(lex, &right, ctx, func_registry, precision);
        if (ret != EVAL_SUCCESS) {
            bignum_free(&right);
            return ret;
        }
        
        /* 根据操作数类型选择操作 */
        if (result->type == BIGNUM_TYPE_BITMAP && right.type == BIGNUM_TYPE_BITMAP) {
            /* 位图类型：^ 表示异或 */
            BigNum *temp = bignum_bitxor(result, &right);
            bignum_free(&right);
            
            if (temp == NULL) {
                return EVAL_ERROR;
            }
            
            bignum_free(result);
            if (bignum_copy(temp, result) != BIGNUM_SUCCESS) {
                bignum_destroy(temp);
                return EVAL_ERROR;
            }
            bignum_destroy(temp);
        } else {
            /* 布尔 AND: 两边都为真则为真 */
            int left_true = bignum_is_true(result);
            int right_true = bignum_is_true(&right);
            bignum_free(&right);
            
            bignum_free(result);
            bignum_init(result);
            char *digits = BIGNUM_DIGITS(result);
            digits[0] = (left_true && right_true) ? 1 : 0;
            result->length = 1;
            result->type_data.num.decimal_pos = 0;
            result->type_data.num.is_negative = 0;
        }
    }
    
    return EVAL_SUCCESS;
}

/* 解析析取 (OR) */
static int parse_or(Lexer *lex, BigNum *result, Context *ctx, FunctionRegistry *func_registry, int precision) {
    int ret = parse_and(lex, result, ctx, func_registry, precision);
    if (ret != EVAL_SUCCESS) return ret;
    
    while (lex->current_type == TOK_OR) {
        lexer_next(lex);
        
        BigNum right;
        bignum_init(&right);
        ret = parse_and(lex, &right, ctx, func_registry, precision);
        if (ret != EVAL_SUCCESS) {
            bignum_free(&right);
            return ret;
        }
        
        /* 布尔 OR: 任一为真则为真 */
        int left_true = bignum_is_true(result);
        int right_true = bignum_is_true(&right);
        bignum_free(&right);
        
        bignum_free(result);
        bignum_init(result);
        char *digits = BIGNUM_DIGITS(result);
        digits[0] = (left_true || right_true) ? 1 : 0;
        result->length = 1;
        result->type_data.num.decimal_pos = 0;
        result->type_data.num.is_negative = 0;
    }
    
    return EVAL_SUCCESS;
}

/* 解析蕴含 (IMPLIES) */
static int parse_impl(Lexer *lex, BigNum *result, Context *ctx, FunctionRegistry *func_registry, int precision) {
    int ret = parse_or(lex, result, ctx, func_registry, precision);
    if (ret != EVAL_SUCCESS) return ret;
    
    while (lex->current_type == TOK_IMPL) {
        lexer_next(lex);
        
        BigNum right;
        bignum_init(&right);
        ret = parse_or(lex, &right, ctx, func_registry, precision);
        if (ret != EVAL_SUCCESS) {
            bignum_free(&right);
            return ret;
        }
        
        /* 布尔蕴含: !left || right */
        int left_true = bignum_is_true(result);
        int right_true = bignum_is_true(&right);
        bignum_free(&right);
        
        bignum_free(result);
        bignum_init(result);
        char *digits = BIGNUM_DIGITS(result);
        digits[0] = (!left_true || right_true) ? 1 : 0;
        result->length = 1;
        result->type_data.num.decimal_pos = 0;
        result->type_data.num.is_negative = 0;
    }
    
    return EVAL_SUCCESS;
}

/* 解析等价 (IFF) */
static int parse_iff(Lexer *lex, BigNum *result, Context *ctx, FunctionRegistry *func_registry, int precision) {
    int ret = parse_impl(lex, result, ctx, func_registry, precision);
    if (ret != EVAL_SUCCESS) return ret;
    
    while (lex->current_type == TOK_IFF) {
        lexer_next(lex);
        
        BigNum right;
        bignum_init(&right);
        ret = parse_impl(lex, &right, ctx, func_registry, precision);
        if (ret != EVAL_SUCCESS) {
            bignum_free(&right);
            return ret;
        }
        
        /* 布尔等价: left == right */
        int left_true = bignum_is_true(result);
        int right_true = bignum_is_true(&right);
        bignum_free(&right);
        
        bignum_free(result);
        bignum_init(result);
        char *digits = BIGNUM_DIGITS(result);
        digits[0] = (left_true == right_true) ? 1 : 0;
        result->length = 1;
        result->type_data.num.decimal_pos = 0;
        result->type_data.num.is_negative = 0;
    }
    
    return EVAL_SUCCESS;
}

/* 解析异或 (XOR) - 优先级最低 */
static int parse_xor(Lexer *lex, BigNum *result, Context *ctx, FunctionRegistry *func_registry, int precision) {
    int ret = parse_iff(lex, result, ctx, func_registry, precision);
    if (ret != EVAL_SUCCESS) return ret;
    
    while (lex->current_type == TOK_XOR) {
        lexer_next(lex);
        
        BigNum right;
        bignum_init(&right);
        ret = parse_iff(lex, &right, ctx, func_registry, precision);
        if (ret != EVAL_SUCCESS) {
            bignum_free(&right);
            return ret;
        }
        
        /* 布尔异或: left != right */
        int left_true = bignum_is_true(result);
        int right_true = bignum_is_true(&right);
        bignum_free(&right);
        
        bignum_free(result);
        bignum_init(result);
        char *digits = BIGNUM_DIGITS(result);
        digits[0] = (left_true != right_true) ? 1 : 0;
        result->length = 1;
        result->type_data.num.decimal_pos = 0;
        result->type_data.num.is_negative = 0;
    }
    
    return EVAL_SUCCESS;
}

/* 解析完整表达式 */
static int parse_expression(Lexer *lex, BigNum *result, Context *ctx, FunctionRegistry *func_registry, int precision) {
    return parse_xor(lex, result, ctx, func_registry, precision);
}

/* 主函数：求值表达式（不支持函数调用，保持向后兼容） */
int eval_expression(const char *expr, BigNum *result, void *ctx, int precision) {
    if (expr == NULL || result == NULL) {
        return EVAL_ERROR;
    }
    
    if (precision < 0) precision = BIGNUM_DEFAULT_PRECISION;
    
    Lexer lex;
    lexer_init(&lex, expr);
    
    if (lexer_next(&lex) == TOK_ERROR) {
        return EVAL_ERROR;
    }
    
    int ret = parse_expression(&lex, result, (Context *)ctx, NULL, precision);
    
    if (ret != EVAL_SUCCESS) {
        return ret;
    }
    
    if (lex.current_type != TOK_END) {
        return EVAL_ERROR;
    }
    
    return EVAL_SUCCESS;
}

/* 求值并转换为字符串 */
int eval_to_string(const char *expr, char *result_str, size_t max_len, void *ctx, int precision) {
    if (expr == NULL || result_str == NULL || max_len == 0) {
        return EVAL_ERROR;
    }
    
    BigNum result;
    bignum_init(&result);
    int ret = eval_expression(expr, &result, ctx, precision);
    
    if (ret != EVAL_SUCCESS) {
        bignum_free(&result);
        return ret;
    }
    
    int to_string_ret = bignum_to_string(&result, result_str, max_len, precision);
    bignum_free(&result);
    return to_string_ret;
}

/* 执行语句（支持赋值语句: let A = expr, 导入语句: import math） */
int eval_statement(const char *stmt, char *result_str, size_t max_len, void *ctx, void *func_registry, void *pkg_manager, int precision) {
    if (stmt == NULL || result_str == NULL || max_len == 0) {
        return EVAL_ERROR;
    }
    
    if (precision < 0) precision = BIGNUM_DEFAULT_PRECISION;
    
    Lexer lex;
    lexer_init(&lex, stmt);
    
    if (lexer_next(&lex) == TOK_ERROR) {
        return EVAL_ERROR;
    }
    
    /* 检查是否是 import 语句 */
    if (lex.current_type == TOK_IMPORT) {
        lexer_next(&lex);
        
        /* 期望一个库名（标识符） */
        if (lex.current_type != TOK_IDENTIFIER) {
            return EVAL_ERROR;
        }
        
        char lib_name[MAX_FUNC_NAME_LEN];
        strncpy(lib_name, lex.current_value, MAX_FUNC_NAME_LEN - 1);
        lib_name[MAX_FUNC_NAME_LEN - 1] = '\0';
        
        lexer_next(&lex);
        
        /* 检查是否到达结尾 */
        if (lex.current_type != TOK_END) {
            return EVAL_ERROR;
        }
        
        /* 导入包 */
        PackageManager *manager = (PackageManager *)pkg_manager;
        FunctionRegistry *registry = (FunctionRegistry *)func_registry;
        
        if (manager == NULL || registry == NULL) {
            return EVAL_ERROR;
        }
        
        /* 使用包管理器加载包 */
        int count = package_load(manager, lib_name, registry, ctx);
        
        if (count < 0) {
            snprintf(result_str, max_len, "错误: 无法加载包 '%s'", lib_name);
            return EVAL_ERROR;
        }
        
        snprintf(result_str, max_len, "已导入 %s 包 (%d 个函数)", lib_name, count);
        return 2;  /* 返回2表示是导入语句 */
    }
    
    /* 检查是否是赋值语句（let variable = expr 或 variable = expr） */
    int has_let = 0;
    if (lex.current_type == TOK_LET) {
        has_let = 1;
        lexer_next(&lex);
    }
    
    /* 检查是否是标识符开头（可能是赋值语句） */
    if (lex.current_type == TOK_IDENTIFIER) {
        char var_name[MAX_VAR_NAME_LEN];
        strncpy(var_name, lex.current_value, MAX_VAR_NAME_LEN - 1);
        var_name[MAX_VAR_NAME_LEN - 1] = '\0';
        
        /* 先保存当前位置，以便回退 */
        int saved_pos = lex.pos;
        TokenType saved_type = lex.current_type;
        char saved_value[BIGNUM_MAX_DIGITS];
        strncpy(saved_value, lex.current_value, BIGNUM_MAX_DIGITS - 1);
        saved_value[BIGNUM_MAX_DIGITS - 1] = '\0';
        
        lexer_next(&lex);
        
        /* 检查是否跟着赋值符号 */
        if (lex.current_type == TOK_ASSIGN) {
            /* 确实是赋值语句 */
            lexer_next(&lex);
            
            /* 解析表达式 */
            BigNum value;
            bignum_init(&value);
            int ret = parse_expression(&lex, &value, (Context *)ctx, (FunctionRegistry *)func_registry, precision);
            if (ret != EVAL_SUCCESS) {
                bignum_free(&value);
                return ret;
            }
            
            /* 检查是否到达结尾 */
            if (lex.current_type != TOK_END) {
                bignum_free(&value);
                return EVAL_ERROR;
            }
            
            /* 格式化输出 - 在设置变量之前先转换为字符串 */
            /* 对于 bitmap 类型，需要 length + 2 字节（B + bits + \0） */
            size_t value_str_size = 4096;
            if (value.type == BIGNUM_TYPE_BITMAP && value.length + 2 > value_str_size) {
                value_str_size = value.length + 10;  /* 额外空间用于 'B' 和 '\0' */
            }
            char *value_str = (char*)malloc(value_str_size);
            if (!value_str) {
                bignum_free(&value);
                return EVAL_ERROR;
            }
            if (bignum_to_string(&value, value_str, value_str_size, precision) != BIGNUM_SUCCESS) {
                bignum_free(&value);
                free(value_str);
                return EVAL_ERROR;
            }
            
            /* 设置变量 */
            if (ctx == NULL || context_set((Context *)ctx, var_name, &value) != 0) {
                bignum_free(&value);
                free(value_str);
                return EVAL_ERROR;
            }
            
            bignum_free(&value);
            snprintf(result_str, max_len, "%s = %s", var_name, value_str);
            free(value_str);
            
            return 1;  /* 返回1表示是赋值语句 */
        } else if (has_let) {
            /* let 后面必须跟赋值语句 */
            return EVAL_ERROR;
        } else {
            /* 不是赋值语句，回退并当作表达式处理 */
            lex.pos = saved_pos;
            lex.current_type = saved_type;
            strncpy(lex.current_value, saved_value, BIGNUM_MAX_DIGITS - 1);
            lex.current_value[BIGNUM_MAX_DIGITS - 1] = '\0';
        }
    } else if (has_let) {
        /* let 后面必须跟标识符 */
        return EVAL_ERROR;
    }
    
    /* 不是 let/import 语句，当作表达式处理 */
    BigNum result;
    bignum_init(&result);
    int ret = parse_expression(&lex, &result, (Context *)ctx, (FunctionRegistry *)func_registry, precision);
    
    if (ret != EVAL_SUCCESS) {
        return ret;
    }
    
    if (lex.current_type != TOK_END) {
        bignum_free(&result);
        return EVAL_ERROR;
    }
    
    int to_string_ret = bignum_to_string(&result, result_str, max_len, precision);
    bignum_free(&result);
    return to_string_ret;
}

