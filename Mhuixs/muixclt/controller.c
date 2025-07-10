#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "controller.h"
#include "variable.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define MAX_FLOW_BLOCKS 32
#define MAX_STATEMENTS_PER_BLOCK 256

// 创建流程控制器
FlowController* flow_controller_create(void) {
    FlowController* controller = malloc(sizeof(FlowController));
    if(!controller) return NULL;
    
    controller->blocks = malloc(MAX_FLOW_BLOCKS * sizeof(FlowBlock));
    if(!controller->blocks) {
        free(controller);
        return NULL;
    }
    
    controller->block_count = 0;
    controller->max_blocks = MAX_FLOW_BLOCKS;
    controller->in_multiline = false;
    
    // 初始化所有控制块
    for(int i = 0; i < MAX_FLOW_BLOCKS; i++) {
        memset(&controller->blocks[i], 0, sizeof(FlowBlock));
    }
    
    printf("流程控制器已创建\n");
    return controller;
}

// 销毁流程控制器
void flow_controller_destroy(FlowController* controller) {
    if(!controller) return;
    
    // 清理所有控制块的缓存语句
    for(int i = 0; i < controller->block_count; i++) {
        FlowBlock* block = &controller->blocks[i];
        if(block->cached_statements) {
            for(int j = 0; j < block->statement_count; j++) {
                free(block->cached_statements[j]);
            }
            free(block->cached_statements);
        }
        if(block->condition_expr) {
            free(block->condition_expr);
        }
    }
    
    free(controller->blocks);
    free(controller);
    printf("流程控制器已销毁\n");
}

// 条件评估函数
int flow_controller_evaluate_condition(const char* condition) {
    if(!condition) return 0;
    
    // 简单的条件评估
    if(strcmp(condition, "0") == 0) {
        printf("评估条件: %s -> 假\n", condition);
        return 0;
    } else if(strcmp(condition, "1") == 0) {
        printf("评估条件: %s -> 真\n", condition);
        return 1;
    } else if(condition[0] == '$') {
        // 变量条件
        const char* var_value = get_local_variable(condition);
        if(var_value) {
            int val = atoi(var_value);
            printf("评估条件: %s (值=%s) -> %s\n", condition, var_value, val ? "真" : "假");
            return val ? 1 : 0;
        } else {
            printf("评估条件: %s (未定义) -> 假\n", condition);
            return 0;
        }
    } else {
        // 其他表达式，暂时返回真（调试）
        printf("评估条件: %s -> 真 (默认)\n", condition);
        return 1;
    }
}

// 获取当前控制块
static FlowBlock* get_current_block(FlowController* controller) {
    if(!controller || controller->block_count <= 0) return NULL;
    return &controller->blocks[controller->block_count - 1];
}

// 推入IF控制块
int flow_controller_push_if(FlowController* controller, const char* condition) {
    if(!controller || controller->block_count >= controller->max_blocks) return -1;
    
    FlowBlock* block = &controller->blocks[controller->block_count];
    block->type = FLOW_TYPE_IF;
    block->condition_result = flow_controller_evaluate_condition(condition);
    block->has_executed = false;
    block->is_active = true;
    block->cached_statements = NULL;
    block->statement_count = 0;
    block->condition_expr = condition ? strdup(condition) : NULL;
    
    controller->block_count++;
    controller->in_multiline = true;
    
    printf("推入IF控制块，条件结果: %s\n", block->condition_result ? "真" : "假");
    return 0;
}

// 推入ELIF控制块
int flow_controller_push_elif(FlowController* controller, const char* condition) {
    FlowBlock* current = get_current_block(controller);
    if(!current || (current->type != FLOW_TYPE_IF && current->type != FLOW_TYPE_ELIF)) {
        return -1;
    }
    
    // ELIF只有在前面的IF/ELIF都没有执行时才可能执行
    bool prev_executed = current->has_executed;
    int condition_result = prev_executed ? 0 : flow_controller_evaluate_condition(condition);
    
    // 更新当前块
    current->type = FLOW_TYPE_ELIF;
    current->condition_result = condition_result;
    if(current->condition_expr) free(current->condition_expr);
    current->condition_expr = condition ? strdup(condition) : NULL;
    
    printf("处理ELIF，条件结果: %s\n", condition_result ? "真" : "假");
    return 0;
}

// 推入ELSE控制块
int flow_controller_push_else(FlowController* controller) {
    FlowBlock* current = get_current_block(controller);
    if(!current || (current->type != FLOW_TYPE_IF && current->type != FLOW_TYPE_ELIF)) {
        return -1;
    }
    
    // ELSE只有在前面的IF/ELIF都没有执行时才执行
    bool prev_executed = current->has_executed;
    
    // 更新当前块
    current->type = FLOW_TYPE_ELSE;
    current->condition_result = !prev_executed;
    
    printf("处理ELSE，条件结果: %s\n", current->condition_result ? "真" : "假");
    return 0;
}

// 推入WHILE控制块
int flow_controller_push_while(FlowController* controller, const char* condition) {
    if(!controller || controller->block_count >= controller->max_blocks) return -1;
    
    FlowBlock* block = &controller->blocks[controller->block_count];
    block->type = FLOW_TYPE_WHILE;
    block->condition_result = flow_controller_evaluate_condition(condition);
    block->has_executed = false;
    block->is_active = true;
    block->cached_statements = NULL;
    block->statement_count = 0;
    block->condition_expr = condition ? strdup(condition) : NULL;
    
    controller->block_count++;
    controller->in_multiline = true;
    
    printf("推入WHILE控制块，条件结果: %s\n", block->condition_result ? "真" : "假");
    return 0;
}

// 推入FOR控制块
int flow_controller_push_for(FlowController* controller, const char* var_name, 
                            int start, int end, int step) {
    if(!controller || controller->block_count >= controller->max_blocks) return -1;
    
    FlowBlock* block = &controller->blocks[controller->block_count];
    block->type = FLOW_TYPE_FOR;
    block->condition_result = true; // FOR循环默认为真
    block->has_executed = false;
    block->is_active = true;
    block->cached_statements = NULL;
    block->statement_count = 0;
    block->condition_expr = NULL;
    
    // FOR循环特有数据
    strncpy(block->var_name, var_name, sizeof(block->var_name) - 1);
    block->var_name[sizeof(block->var_name) - 1] = '\0';
    block->start_value = start;
    block->end_value = end;
    block->step_value = step;
    block->current_value = start;
    
    // 设置循环变量初始值
    char val_str[32];
    snprintf(val_str, sizeof(val_str), "%d", start);
    set_local_variable(var_name, val_str);
    
    controller->block_count++;
    controller->in_multiline = true;
    
    printf("推入FOR控制块: %s = %d 到 %d，步长 %d\n", var_name, start, end, step);
    return 0;
}

// 缓存语句
int flow_controller_cache_statement(FlowController* controller, const char* statement) {
    FlowBlock* current = get_current_block(controller);
    if(!current || !current->is_active) return -1;
    
    // 扩展语句数组
    char** new_statements = realloc(current->cached_statements, 
                                   (current->statement_count + 1) * sizeof(char*));
    if(!new_statements) return -1;
    
    current->cached_statements = new_statements;
    current->cached_statements[current->statement_count] = strdup(statement);
    current->statement_count++;
    
    printf("缓存控制块语句: %s\n", statement);
    return 0;
}

// 执行控制块
static int execute_block(FlowBlock* block, StatementExecutor executor) {
    if(!block || !block->condition_result) {
        printf("跳过控制块执行（条件为假）\n");
        return 0;
    }
    
    printf("执行控制块，包含 %d 条语句\n", block->statement_count);
    block->has_executed = true;
    
    for(int i = 0; i < block->statement_count; i++) {
        printf("  执行: %s\n", block->cached_statements[i]);
        if(executor) {
            int result = executor(block->cached_statements[i]);
            if(result != 0) {
                printf("语句执行失败: %s\n", block->cached_statements[i]);
                return -1;
            }
        }
    }
    
    return 1;
}

// 结束控制块
int flow_controller_end_block(FlowController* controller, StatementExecutor executor) {
    FlowBlock* current = get_current_block(controller);
    if(!current) return -1;
    
    int result = 0;
    
    switch(current->type) {
        case FLOW_TYPE_IF:
        case FLOW_TYPE_ELIF:
        case FLOW_TYPE_ELSE:
            // 执行条件块
            result = execute_block(current, executor);
            break;
            
        case FLOW_TYPE_WHILE:
            // WHILE循环：执行并重复
            while(current->condition_result) {
                result = execute_block(current, executor);
                if(result <= 0) break;
                
                // 重新评估条件
                current->condition_result = flow_controller_evaluate_condition(current->condition_expr);
            }
            break;
            
        case FLOW_TYPE_FOR:
            // FOR循环：执行多次
            while((current->step_value > 0 && current->current_value <= current->end_value) ||
                  (current->step_value < 0 && current->current_value >= current->end_value)) {
                
                // 更新循环变量
                char val_str[32];
                snprintf(val_str, sizeof(val_str), "%d", current->current_value);
                set_local_variable(current->var_name, val_str);
                
                // 执行循环体
                result = execute_block(current, executor);
                if(result <= 0) break;
                
                // 更新循环变量值
                current->current_value += current->step_value;
            }
            break;
    }
    
    // 清理当前控制块
    if(current->cached_statements) {
        for(int i = 0; i < current->statement_count; i++) {
            free(current->cached_statements[i]);
        }
        free(current->cached_statements);
    }
    if(current->condition_expr) {
        free(current->condition_expr);
    }
    
    controller->block_count--;
    controller->in_multiline = (controller->block_count > 0);
    
    printf("弹出控制块\n");
    return result;
}

// 检查是否在控制块中
bool flow_controller_is_in_block(FlowController* controller) {
    return controller && controller->block_count > 0;
}

// 检查是否应该缓存语句
bool flow_controller_should_cache(FlowController* controller) {
    return flow_controller_is_in_block(controller);
}

// 获取控制深度
int flow_controller_get_depth(FlowController* controller) {
    return controller ? controller->block_count : 0;
}

// 打印状态
void flow_controller_print_status(FlowController* controller) {
    if(!controller) {
        printf("流程控制器: NULL\n");
        return;
    }
    
    printf("流程控制器状态: 深度=%d, 多行=%s\n", 
           controller->block_count, 
           controller->in_multiline ? "是" : "否");
    
    for(int i = 0; i < controller->block_count; i++) {
        FlowBlock* block = &controller->blocks[i];
        const char* type_name = "";
        switch(block->type) {
            case FLOW_TYPE_IF: type_name = "IF"; break;
            case FLOW_TYPE_ELIF: type_name = "ELIF"; break;
            case FLOW_TYPE_ELSE: type_name = "ELSE"; break;
            case FLOW_TYPE_WHILE: type_name = "WHILE"; break;
            case FLOW_TYPE_FOR: type_name = "FOR"; break;
        }
        
        printf("  [%d] %s: 条件=%s, 语句数=%d\n", 
               i, type_name, 
               block->condition_result ? "真" : "假", 
               block->statement_count);
    }
}

// BREAK和CONTINUE（简化实现）
int flow_controller_break(FlowController* controller) {
    printf("BREAK命令（简化处理）\n");
    return 0;
}

int flow_controller_continue(FlowController* controller) {
    printf("CONTINUE命令（简化处理）\n");
    return 0;
} 