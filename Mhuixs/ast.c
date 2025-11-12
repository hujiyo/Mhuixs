#include "ast.h"
#include "evaluator.h"
#include "context.h"
#include "function.h"
#include "package.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* 前向声明 */
static int bignum_compare_local(const BigNum *a, const BigNum *b);

/* 辅助函数：复制字符串 */
static char* ast_strdup(const char *str) {
    if (!str) return NULL;
    size_t len = strlen(str) + 1;
    char *copy = malloc(len);
    if (copy) {
        memcpy(copy, str, len);
    }
    return copy;
}

/* 创建表达式节点 */
ASTNode* ast_create_expression(const char *expr) {
    ASTNode *node = malloc(sizeof(ASTNode));
    if (!node) return NULL;
    
    node->type = AST_EXPRESSION;
    node->data.expression.expression = ast_strdup(expr);
    
    if (!node->data.expression.expression) {
        free(node);
        return NULL;
    }
    
    return node;
}

/* 创建赋值节点 */
ASTNode* ast_create_assignment(const char *var, const char *expr) {
    ASTNode *node = malloc(sizeof(ASTNode));
    if (!node) return NULL;
    
    node->type = AST_ASSIGNMENT;
    node->data.assignment.variable = ast_strdup(var);
    node->data.assignment.expression = ast_strdup(expr);
    
    if (!node->data.assignment.variable || !node->data.assignment.expression) {
        free(node->data.assignment.variable);
        free(node->data.assignment.expression);
        free(node);
        return NULL;
    }
    
    return node;
}

/* 创建if节点 */
ASTNode* ast_create_if(const char *condition, ASTNode *then_block, ASTNode *else_block) {
    ASTNode *node = malloc(sizeof(ASTNode));
    if (!node) return NULL;
    
    node->type = AST_IF;
    node->data.if_stmt.condition = ast_strdup(condition);
    node->data.if_stmt.then_block = then_block;
    node->data.if_stmt.else_block = else_block;
    
    if (!node->data.if_stmt.condition) {
        free(node);
        return NULL;
    }
    
    return node;
}

/* 创建for节点 */
ASTNode* ast_create_for(const char *var, const char *start, const char *end, const char *step, ASTNode *body) {
    ASTNode *node = malloc(sizeof(ASTNode));
    if (!node) return NULL;
    
    node->type = AST_FOR;
    node->data.for_stmt.variable = ast_strdup(var);
    node->data.for_stmt.start_expr = ast_strdup(start);
    node->data.for_stmt.end_expr = ast_strdup(end);
    node->data.for_stmt.step_expr = step ? ast_strdup(step) : NULL;
    node->data.for_stmt.body = body;
    
    if (!node->data.for_stmt.variable || !node->data.for_stmt.start_expr || !node->data.for_stmt.end_expr) {
        free(node->data.for_stmt.variable);
        free(node->data.for_stmt.start_expr);
        free(node->data.for_stmt.end_expr);
        free(node->data.for_stmt.step_expr);
        free(node);
        return NULL;
    }
    
    return node;
}

/* 创建while节点 */
ASTNode* ast_create_while(const char *condition, ASTNode *body) {
    ASTNode *node = malloc(sizeof(ASTNode));
    if (!node) return NULL;
    
    node->type = AST_WHILE;
    node->data.while_stmt.condition = ast_strdup(condition);
    node->data.while_stmt.body = body;
    
    if (!node->data.while_stmt.condition) {
        free(node);
        return NULL;
    }
    
    return node;
}

/* 创建do-while节点 */
ASTNode* ast_create_do_while(ASTNode *body, const char *condition) {
    ASTNode *node = malloc(sizeof(ASTNode));
    if (!node) return NULL;
    
    node->type = AST_DO_WHILE;
    node->data.do_while_stmt.body = body;
    node->data.do_while_stmt.condition = ast_strdup(condition);
    
    if (!node->data.do_while_stmt.condition) {
        free(node);
        return NULL;
    }
    
    return node;
}

/* 创建语句块节点 */
ASTNode* ast_create_block(void) {
    ASTNode *node = malloc(sizeof(ASTNode));
    if (!node) return NULL;
    
    node->type = AST_BLOCK;
    node->data.block.statements = NULL;
    node->data.block.count = 0;
    node->data.block.capacity = 0;
    
    return node;
}

/* 向语句块添加语句 */
int ast_block_add_statement(ASTNode *block, ASTNode *stmt) {
    if (!block || block->type != AST_BLOCK || !stmt) {
        return -1;
    }
    
    /* 扩容 */
    if (block->data.block.count >= block->data.block.capacity) {
        int new_capacity = block->data.block.capacity == 0 ? 4 : block->data.block.capacity * 2;
        ASTNode **new_statements = realloc(block->data.block.statements, new_capacity * sizeof(ASTNode*));
        if (!new_statements) {
            return -1;
        }
        block->data.block.statements = new_statements;
        block->data.block.capacity = new_capacity;
    }
    
    block->data.block.statements[block->data.block.count++] = stmt;
    return 0;
}

/* 创建import节点 */
ASTNode* ast_create_import(const char *package_name) {
    ASTNode *node = malloc(sizeof(ASTNode));
    if (!node) return NULL;
    
    node->type = AST_IMPORT;
    node->data.import_stmt.package_name = ast_strdup(package_name);
    
    if (!node->data.import_stmt.package_name) {
        free(node);
        return NULL;
    }
    
    return node;
}

/* 销毁AST节点 */
void ast_destroy(ASTNode *node) {
    if (!node) return;
    
    switch (node->type) {
        case AST_EXPRESSION:
            free(node->data.expression.expression);
            break;
            
        case AST_ASSIGNMENT:
            free(node->data.assignment.variable);
            free(node->data.assignment.expression);
            break;
            
        case AST_IF:
            free(node->data.if_stmt.condition);
            ast_destroy(node->data.if_stmt.then_block);
            ast_destroy(node->data.if_stmt.else_block);
            break;
            
        case AST_FOR:
            free(node->data.for_stmt.variable);
            free(node->data.for_stmt.start_expr);
            free(node->data.for_stmt.end_expr);
            free(node->data.for_stmt.step_expr);
            ast_destroy(node->data.for_stmt.body);
            break;
            
        case AST_WHILE:
            free(node->data.while_stmt.condition);
            ast_destroy(node->data.while_stmt.body);
            break;
            
        case AST_DO_WHILE:
            ast_destroy(node->data.do_while_stmt.body);
            free(node->data.do_while_stmt.condition);
            break;
            
        case AST_BLOCK:
            for (int i = 0; i < node->data.block.count; i++) {
                ast_destroy(node->data.block.statements[i]);
            }
            free(node->data.block.statements);
            break;
            
        case AST_IMPORT:
            free(node->data.import_stmt.package_name);
            break;
    }
    
    free(node);
}

/* 执行AST节点 */
int ast_execute(ASTNode *node, void *ctx, void *func_registry, void *pkg_manager, int precision, char *result_str, size_t max_len) {
    if (!node) return EVAL_ERROR;
    
    Context *context = (Context*)ctx;
    
    switch (node->type) {
        case AST_EXPRESSION: {
            return eval_to_string(node->data.expression.expression, result_str, max_len, ctx, precision);
        }
        
        case AST_ASSIGNMENT: {
            /* 构造赋值语句字符串 */
            char stmt[1024];
            snprintf(stmt, sizeof(stmt), "%s = %s", 
                    node->data.assignment.variable, 
                    node->data.assignment.expression);
            return eval_statement(stmt, result_str, max_len, ctx, func_registry, pkg_manager, precision);
        }
        
        case AST_IF: {
            /* 计算条件 */
            BigNum condition_result;
            bignum_init(&condition_result);
            int ret = eval_expression(node->data.if_stmt.condition, &condition_result, ctx, precision);
            
            if (ret != EVAL_SUCCESS) {
                bignum_free(&condition_result);
                return ret;
            }
            
            int is_true = bignum_is_true(&condition_result);
            bignum_free(&condition_result);
            
            /* 执行相应分支 */
            if (is_true && node->data.if_stmt.then_block) {
                return ast_execute(node->data.if_stmt.then_block, ctx, func_registry, pkg_manager, precision, result_str, max_len);
            } else if (!is_true && node->data.if_stmt.else_block) {
                return ast_execute(node->data.if_stmt.else_block, ctx, func_registry, pkg_manager, precision, result_str, max_len);
            }
            
            result_str[0] = '\0';
            return EVAL_SUCCESS;
        }
        
        case AST_FOR: {
            /* 计算起始值、结束值和步长 */
            BigNum start_val, end_val, step_val;
            bignum_init(&start_val);
            bignum_init(&end_val);
            bignum_init(&step_val);
            
            int ret = eval_expression(node->data.for_stmt.start_expr, &start_val, ctx, precision);
            if (ret != EVAL_SUCCESS) {
                bignum_free(&start_val);
                bignum_free(&end_val);
                bignum_free(&step_val);
                return ret;
            }
            
            ret = eval_expression(node->data.for_stmt.end_expr, &end_val, ctx, precision);
            if (ret != EVAL_SUCCESS) {
                bignum_free(&start_val);
                bignum_free(&end_val);
                bignum_free(&step_val);
                return ret;
            }
            
            /* 步长默认为1 */
            if (node->data.for_stmt.step_expr) {
                ret = eval_expression(node->data.for_stmt.step_expr, &step_val, ctx, precision);
                if (ret != EVAL_SUCCESS) {
                    bignum_free(&start_val);
                    bignum_free(&end_val);
                    bignum_free(&step_val);
                    return ret;
                }
            } else {
                BigNum *one = bignum_from_string("1");
                if (!one) {
                    bignum_free(&start_val);
                    bignum_free(&end_val);
                    bignum_free(&step_val);
                    return EVAL_ERROR;
                }
                bignum_copy(one, &step_val);
                bignum_destroy(one);
            }
            
            /* 执行循环 */
            BigNum current;
            bignum_init(&current);
            bignum_copy(&start_val, &current);
            
            char last_result[1024] = "";
            
            while (1) {
                /* 检查循环条件 */
                int cmp = bignum_compare_local(&current, &end_val);
                if (cmp >= 0) break;  /* current >= end */
                
                /* 设置循环变量 */
                if (context_set(context, node->data.for_stmt.variable, &current) != 0) {
                    ret = EVAL_ERROR;
                    break;
                }
                
                /* 执行循环体 */
                ret = ast_execute(node->data.for_stmt.body, ctx, func_registry, pkg_manager, precision, last_result, sizeof(last_result));
                if (ret != EVAL_SUCCESS) {
                    break;
                }
                
                /* 增加步长 */
                BigNum *next = bignum_add(&current, &step_val);
                if (!next) {
                    ret = EVAL_ERROR;
                    break;
                }
                bignum_free(&current);
                bignum_copy(next, &current);
                bignum_destroy(next);
            }
            
            bignum_free(&start_val);
            bignum_free(&end_val);
            bignum_free(&step_val);
            bignum_free(&current);
            
            if (ret == EVAL_SUCCESS) {
                strncpy(result_str, last_result, max_len - 1);
                result_str[max_len - 1] = '\0';
            }
            
            return ret;
        }
        
        case AST_WHILE: {
            char last_result[1024] = "";
            
            while (1) {
                /* 计算条件 */
                BigNum condition_result;
                bignum_init(&condition_result);
                int ret = eval_expression(node->data.while_stmt.condition, &condition_result, ctx, precision);
                
                if (ret != EVAL_SUCCESS) {
                    bignum_free(&condition_result);
                    return ret;
                }
                
                int is_true = bignum_is_true(&condition_result);
                bignum_free(&condition_result);
                
                if (!is_true) break;
                
                /* 执行循环体 */
                ret = ast_execute(node->data.while_stmt.body, ctx, func_registry, pkg_manager, precision, last_result, sizeof(last_result));
                if (ret != EVAL_SUCCESS) {
                    return ret;
                }
            }
            
            strncpy(result_str, last_result, max_len - 1);
            result_str[max_len - 1] = '\0';
            return EVAL_SUCCESS;
        }
        
        case AST_DO_WHILE: {
            char last_result[1024] = "";
            
            do {
                /* 执行循环体 */
                int ret = ast_execute(node->data.do_while_stmt.body, ctx, func_registry, pkg_manager, precision, last_result, sizeof(last_result));
                if (ret != EVAL_SUCCESS) {
                    return ret;
                }
                
                /* 计算条件 */
                BigNum condition_result;
                bignum_init(&condition_result);
                ret = eval_expression(node->data.do_while_stmt.condition, &condition_result, ctx, precision);
                
                if (ret != EVAL_SUCCESS) {
                    bignum_free(&condition_result);
                    return ret;
                }
                
                int is_true = bignum_is_true(&condition_result);
                bignum_free(&condition_result);
                
                if (!is_true) break;
                
            } while (1);
            
            strncpy(result_str, last_result, max_len - 1);
            result_str[max_len - 1] = '\0';
            return EVAL_SUCCESS;
        }
        
        case AST_BLOCK: {
            char last_result[1024] = "";
            
            for (int i = 0; i < node->data.block.count; i++) {
                int ret = ast_execute(node->data.block.statements[i], ctx, func_registry, pkg_manager, precision, last_result, sizeof(last_result));
                if (ret != EVAL_SUCCESS) {
                    return ret;
                }
            }
            
            strncpy(result_str, last_result, max_len - 1);
            result_str[max_len - 1] = '\0';
            return EVAL_SUCCESS;
        }
        
        case AST_IMPORT: {
            /* 构造import语句字符串 */
            char stmt[256];
            snprintf(stmt, sizeof(stmt), "import %s", node->data.import_stmt.package_name);
            return eval_statement(stmt, result_str, max_len, ctx, func_registry, pkg_manager, precision);
        }
        
        default:
            return EVAL_ERROR;
    }
}

/* 辅助函数：比较两个大数（需要在这里实现，因为evaluator.c中的是静态函数） */
static int bignum_compare_local(const BigNum *a, const BigNum *b) {
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
