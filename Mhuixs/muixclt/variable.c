#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "variable.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 全局变量存储
static Variable* local_vars = NULL;
static int var_count = 0;
static int var_capacity = 0;

// 控制流状态
static FlowControlState flow_control = {0};

// 执行语句的函数指针（由外部设置）
extern int (*execute_statement_function)(const char* statement);

// 变量系统初始化
void variable_system_init(void) {
    local_vars = NULL;
    var_count = 0;
    var_capacity = 0;
    control_state_init();
}

// 变量系统清理
void variable_system_cleanup(void) {
    for(int i = 0; i < var_count; i++) {
        // 清理列表数据
        if(local_vars[i].is_list && local_vars[i].list_items) {
            for(int j = 0; j < local_vars[i].list_count; j++) {
                str_free(&local_vars[i].list_items[j]);
            }
            free(local_vars[i].list_items);
        }
        
        str_free(&local_vars[i].name);
        str_free(&local_vars[i].value);
    }
    free(local_vars);
    local_vars = NULL;
    var_count = 0;
    var_capacity = 0;
    
    // 清理FOR循环栈
    for(int i = 0; i < flow_control.for_depth; i++) {
        if(flow_control.for_stack[i].cached_lines) {
            for(int j = 0; j < flow_control.for_stack[i].cached_count; j++) {
                free(flow_control.for_stack[i].cached_lines[j]);
            }
            free(flow_control.for_stack[i].cached_lines);
        }
    }
    
    // 重置控制状态
    control_state_init();
}

// 检测变量类型
VariableType detect_variable_type(const char* value) {
    if(value == NULL) return VAR_TYPE_STRING;
    
    // 检查是否是布尔值
    if(strcmp(value, "true") == 0 || strcmp(value, "false") == 0) {
        return VAR_TYPE_BOOLEAN;
    }
    
    // 所有其他值（包括数字）都视为字符串
    return VAR_TYPE_STRING;
}

// 基本变量操作
int set_local_variable(const char* name, const char* value) {
    return set_local_variable_with_type(name, value, detect_variable_type(value));
}

int set_local_variable_with_type(const char* name, const char* value, VariableType type) {
    // 查找是否已存在
    for(int i = 0; i < var_count; i++) {
        if(strncmp((char*)local_vars[i].name.string, name, local_vars[i].name.len) == 0 && 
           strlen(name) == local_vars[i].name.len) {
            // 清理现有变量的列表数据
            if(local_vars[i].is_list && local_vars[i].list_items) {
                for(int j = 0; j < local_vars[i].list_count; j++) {
                    str_free(&local_vars[i].list_items[j]);
                }
                free(local_vars[i].list_items);
            }
            
            // 更新现有变量
            str_free(&local_vars[i].value);
            str_init(&local_vars[i].value);
            str_append_string(&local_vars[i].value, value);
            local_vars[i].type = type;
            local_vars[i].is_list = 0;
            local_vars[i].list_items = NULL;
            local_vars[i].list_count = 0;
            return 0;
        }
    }
    
    // 添加新变量
    if(var_count >= var_capacity) {
        var_capacity = var_capacity == 0 ? 8 : var_capacity * 2;
        Variable* new_vars = realloc(local_vars, var_capacity * sizeof(Variable));
        if(!new_vars) return -1;
        local_vars = new_vars;
    }
    
    str_init(&local_vars[var_count].name);
    str_init(&local_vars[var_count].value);
    str_append_string(&local_vars[var_count].name, name);
    str_append_string(&local_vars[var_count].value, value);
    local_vars[var_count].type = type;
    local_vars[var_count].is_list = 0;
    local_vars[var_count].list_items = NULL;
    local_vars[var_count].list_count = 0;
    var_count++;
    
    return 0;
}

const char* get_local_variable(const char* name) {
    for(int i = 0; i < var_count; i++) {
        if(strncmp((char*)local_vars[i].name.string, name, local_vars[i].name.len) == 0 && 
           strlen(name) == local_vars[i].name.len) {
            return (char*)local_vars[i].value.string;
        }
    }
    return NULL;
}

int delete_local_variable(const char* name) {
    for(int i = 0; i < var_count; i++) {
        if(strncmp((char*)local_vars[i].name.string, name, local_vars[i].name.len) == 0 && 
           strlen(name) == local_vars[i].name.len) {
            // 清理列表数据
            if(local_vars[i].is_list && local_vars[i].list_items) {
                for(int j = 0; j < local_vars[i].list_count; j++) {
                    str_free(&local_vars[i].list_items[j]);
                }
                free(local_vars[i].list_items);
            }
            
            str_free(&local_vars[i].name);
            str_free(&local_vars[i].value);
            // 移动后面的变量
            for(int j = i; j < var_count - 1; j++) {
                local_vars[j] = local_vars[j + 1];
            }
            var_count--;
            return 0;
        }
    }
    return -1; // 变量不存在
}

// 列表变量操作
int set_local_list_variable(const char* name, const char** values, int count) {
    // 查找是否已存在
    for(int i = 0; i < var_count; i++) {
        if(strncmp((char*)local_vars[i].name.string, name, local_vars[i].name.len) == 0 && 
           strlen(name) == local_vars[i].name.len) {
            // 清理现有数据
            if(local_vars[i].is_list && local_vars[i].list_items) {
                for(int j = 0; j < local_vars[i].list_count; j++) {
                    str_free(&local_vars[i].list_items[j]);
                }
                free(local_vars[i].list_items);
            }
            str_free(&local_vars[i].value);
            
            // 设置为列表
            str_init(&local_vars[i].value);
            str_append_string(&local_vars[i].value, "[list]");
            local_vars[i].type = VAR_TYPE_LIST;
            local_vars[i].is_list = 1;
            local_vars[i].list_count = count;
            
            if(count > 0) {
                local_vars[i].list_items = malloc(count * sizeof(str));
                for(int j = 0; j < count; j++) {
                    str_init(&local_vars[i].list_items[j]);
                    str_append_string(&local_vars[i].list_items[j], values[j]);
                }
            } else {
                local_vars[i].list_items = NULL;
            }
            
            return 0;
        }
    }
    
    // 添加新列表变量
    if(var_count >= var_capacity) {
        var_capacity = var_capacity == 0 ? 8 : var_capacity * 2;
        Variable* new_vars = realloc(local_vars, var_capacity * sizeof(Variable));
        if(!new_vars) return -1;
        local_vars = new_vars;
    }
    
    str_init(&local_vars[var_count].name);
    str_init(&local_vars[var_count].value);
    str_append_string(&local_vars[var_count].name, name);
    str_append_string(&local_vars[var_count].value, "[list]");
    local_vars[var_count].type = VAR_TYPE_LIST;
    local_vars[var_count].is_list = 1;
    local_vars[var_count].list_count = count;
    
    if(count > 0) {
        local_vars[var_count].list_items = malloc(count * sizeof(str));
        for(int j = 0; j < count; j++) {
            str_init(&local_vars[var_count].list_items[j]);
            str_append_string(&local_vars[var_count].list_items[j], values[j]);
        }
    } else {
        local_vars[var_count].list_items = NULL;
    }
    
    var_count++;
    return 0;
}

int add_to_list_variable(const char* name, const char* value) {
    for(int i = 0; i < var_count; i++) {
        if(strncmp((char*)local_vars[i].name.string, name, local_vars[i].name.len) == 0 && 
           strlen(name) == local_vars[i].name.len) {
            if(!local_vars[i].is_list) {
                return -1; // 不是列表变量
            }
            
            // 扩展列表
            str* new_items = realloc(local_vars[i].list_items, (local_vars[i].list_count + 1) * sizeof(str));
            if(!new_items) return -1;
            
            local_vars[i].list_items = new_items;
            str_init(&local_vars[i].list_items[local_vars[i].list_count]);
            str_append_string(&local_vars[i].list_items[local_vars[i].list_count], value);
            local_vars[i].list_count++;
            
            return 0;
        }
    }
    return -1; // 变量不存在
}

const char* get_list_variable_item(const char* name, int index) {
    for(int i = 0; i < var_count; i++) {
        if(strncmp((char*)local_vars[i].name.string, name, local_vars[i].name.len) == 0 && 
           strlen(name) == local_vars[i].name.len) {
            if(!local_vars[i].is_list || index < 0 || index >= local_vars[i].list_count) {
                return NULL;
            }
            return (char*)local_vars[i].list_items[index].string;
        }
    }
    return NULL;
}

int get_list_variable_count(const char* name) {
    for(int i = 0; i < var_count; i++) {
        if(strncmp((char*)local_vars[i].name.string, name, local_vars[i].name.len) == 0 && 
           strlen(name) == local_vars[i].name.len) {
            if(!local_vars[i].is_list) {
                return -1;
            }
            return local_vars[i].list_count;
        }
    }
    return -1; // 变量不存在
}

// 变量信息获取
int get_variable_count(void) {
    return var_count;
}

void print_all_variables(void) {
    printf("=== 本地变量列表 (%d个变量) ===\n", var_count);
    for(int i = 0; i < var_count; i++) {
        printf("%d. ", i + 1);
        print_variable_info((char*)local_vars[i].name.string);
    }
}

void print_variable_info(const char* name) {
    for(int i = 0; i < var_count; i++) {
        if(strncmp((char*)local_vars[i].name.string, name, local_vars[i].name.len) == 0 && 
           strlen(name) == local_vars[i].name.len) {
            if(local_vars[i].is_list) {
                printf("变量 %s (列表类型，%d个元素):\n", name, local_vars[i].list_count);
                for(int j = 0; j < local_vars[i].list_count; j++) {
                    char* item_cstr = str_to_cstr(&local_vars[i].list_items[j]);
                    printf("  [%d] %s\n", j, item_cstr ? item_cstr : "");
                    free(item_cstr);
                }
            } else {
                const char* type_name = "";
                switch(local_vars[i].type) {
                    case VAR_TYPE_STRING: type_name = "字符串"; break;
                    case VAR_TYPE_BOOLEAN: type_name = "布尔"; break;
                    case VAR_TYPE_LIST: type_name = "列表"; break;
                }
                char* value_cstr = str_to_cstr(&local_vars[i].value);
                printf("变量 %s (%s): %s\n", name, type_name, value_cstr ? value_cstr : "");
                free(value_cstr);
            }
            return;
        }
    }
    printf("变量 %s 不存在\n", name);
}

// 控制流管理
void control_state_init(void) {
    flow_control.for_depth = 0;
    flow_control.control_depth = 0;
    flow_control.condition_depth = 0;
    flow_control.loop_depth = 0;
    memset(flow_control.for_stack, 0, sizeof(flow_control.for_stack));
    memset(flow_control.control_stack, 0, sizeof(flow_control.control_stack));
    memset(flow_control.if_stack, 0, sizeof(flow_control.if_stack));
    memset(flow_control.loop_stack, 0, sizeof(flow_control.loop_stack));
}

// 新的条件控制管理函数
int push_control_state(ControlType type, int condition_result, const char* condition_expr) {
    if(flow_control.control_depth >= 32) return -1;
    
    ControlState* state = &flow_control.control_stack[flow_control.control_depth];
    state->type = type;
    state->condition_result = condition_result;
    state->has_executed = 0;
    state->is_active = 1;
    state->cached_lines = NULL;
    state->cached_count = 0;
    state->condition_expr = condition_expr ? strdup(condition_expr) : NULL;
    
    // 初始化FOR循环特有数据
    if(type == CONTROL_TYPE_FOR) {
        memset(state->var_name, 0, sizeof(state->var_name));
        state->start_value = 0;
        state->end_value = 0;
        state->step_value = 1;
        state->current_value = 0;
    }
    
    flow_control.control_depth++;
    printf("推入%s控制块，条件结果: %d\n", 
           type == CONTROL_TYPE_IF ? "IF" :
           type == CONTROL_TYPE_ELIF ? "ELIF" :
           type == CONTROL_TYPE_ELSE ? "ELSE" :
           type == CONTROL_TYPE_WHILE ? "WHILE" :
           type == CONTROL_TYPE_FOR ? "FOR" : "UNKNOWN",
           condition_result);
    return 0;
}

int pop_control_state(void) {
    if(flow_control.control_depth <= 0) return -1;
    
    ControlState* state = &flow_control.control_stack[flow_control.control_depth - 1];
    
    // 清理缓存的语句
    if(state->cached_lines) {
        for(int i = 0; i < state->cached_count; i++) {
            free(state->cached_lines[i]);
        }
        free(state->cached_lines);
    }
    
    // 清理条件表达式
    if(state->condition_expr) {
        free(state->condition_expr);
    }
    
    printf("弹出%s控制块\n", 
           state->type == CONTROL_TYPE_IF ? "IF" :
           state->type == CONTROL_TYPE_ELIF ? "ELIF" :
           state->type == CONTROL_TYPE_ELSE ? "ELSE" :
           state->type == CONTROL_TYPE_WHILE ? "WHILE" :
           state->type == CONTROL_TYPE_FOR ? "FOR" : "UNKNOWN");
    
    flow_control.control_depth--;
    return 0;
}

ControlState* get_current_control_state(void) {
    if(flow_control.control_depth <= 0) return NULL;
    return &flow_control.control_stack[flow_control.control_depth - 1];
}

int is_in_control_block(void) {
    return flow_control.control_depth > 0;
}

int should_execute_current_block(void) {
    ControlState* state = get_current_control_state();
    if(!state) return 1; // 不在控制块中，正常执行
    
    switch(state->type) {
        case CONTROL_TYPE_IF:
        case CONTROL_TYPE_ELIF:
            return state->condition_result && !state->has_executed;
        case CONTROL_TYPE_ELSE:
            // ELSE块只有在前面的IF/ELIF都没有执行时才执行
            return !state->has_executed;
        case CONTROL_TYPE_WHILE:
        case CONTROL_TYPE_FOR:
            return state->condition_result;
        default:
            return 1;
    }
}

int add_control_statement(const char* statement) {
    ControlState* state = get_current_control_state();
    if(!state) return -1;
    
    // 扩展缓存数组
    char** new_lines = realloc(state->cached_lines, 
                              (state->cached_count + 1) * sizeof(char*));
    if(!new_lines) return -1;
    
    state->cached_lines = new_lines;
    state->cached_lines[state->cached_count] = strdup(statement);
    state->cached_count++;
    
    printf("缓存控制块语句: %s\n", statement);
    return 0;
}

int execute_control_block(void) {
    ControlState* state = get_current_control_state();
    if(!state) return -1;
    
    if(!should_execute_current_block()) {
        printf("跳过控制块执行\n");
        return 0;
    }
    
    printf("执行控制块，包含 %d 条语句\n", state->cached_count);
    
    // 标记为已执行（用于IF/ELIF链）
    state->has_executed = 1;
    
    // 执行缓存的语句
    for(int i = 0; i < state->cached_count; i++) {
        printf("  执行: %s\n", state->cached_lines[i]);
        
        // 如果有外部执行函数，调用它
        if(execute_statement_function) {
            int result = execute_statement_function(state->cached_lines[i]);
            if(result != 0) {
                printf("语句执行失败: %s\n", state->cached_lines[i]);
                return -1;
            }
        }
    }
    
    return 1;
}

// 条件评估函数
int evaluate_condition(const char* condition_expr) {
    if(!condition_expr) return 0;
    
    // 简单的条件评估（调试用）
    if(strcmp(condition_expr, "0") == 0) {
        printf("评估条件: %s -> 0\n", condition_expr);
        return 0;
    } else if(strcmp(condition_expr, "1") == 0) {
        printf("评估条件: %s -> 1\n", condition_expr);
        return 1;
    } else if(condition_expr[0] == '$') {
        // 变量条件：检查变量值
        const char* var_value = get_local_variable(condition_expr);
        if(var_value) {
            int val = atoi(var_value);
            printf("评估条件: %s (值=%s) -> %d\n", condition_expr, var_value, val ? 1 : 0);
            return val ? 1 : 0;
        } else {
            printf("评估条件: %s (未定义) -> 0\n", condition_expr);
            return 0;
        }
    } else {
        // 其他表达式，暂时返回1（调试）
        printf("评估条件: %s -> 1 (默认)\n", condition_expr);
        return 1;
    }
}

int evaluate_simple_condition(const char* left, const char* op, const char* right) {
    if(!left || !op || !right) return 0;
    
    // 简单比较操作（调试用）
    printf("评估简单条件: %s %s %s\n", left, op, right);
    return 1; // 暂时返回真
}

// WHILE循环管理
int push_while_loop(const char* condition_expr) {
    int condition_result = evaluate_condition(condition_expr);
    return push_control_state(CONTROL_TYPE_WHILE, condition_result, condition_expr);
}

int execute_while_iteration(void) {
    ControlState* state = get_current_control_state();
    if(!state || state->type != CONTROL_TYPE_WHILE) return -1;
    
    // 重新评估条件
    state->condition_result = evaluate_condition(state->condition_expr);
    
    if(!state->condition_result) {
        printf("WHILE循环条件为假，结束循环\n");
        return 0; // 循环结束
    }
    
    // 执行循环体
    execute_control_block();
    return 1; // 继续循环
}

// IF/ELSE/ELIF管理
int push_if_statement(const char* condition_expr) {
    int condition_result = evaluate_condition(condition_expr);
    return push_control_state(CONTROL_TYPE_IF, condition_result, condition_expr);
}

int push_elif_statement(const char* condition_expr) {
    // 检查前面的IF/ELIF是否已经执行过
    ControlState* prev_state = get_current_control_state();
    int has_prev_executed = prev_state ? prev_state->has_executed : 0;
    
    int condition_result = has_prev_executed ? 0 : evaluate_condition(condition_expr);
    ControlState* state = &flow_control.control_stack[flow_control.control_depth - 1];
    
    // 更新当前状态为ELIF
    state->type = CONTROL_TYPE_ELIF;
    state->condition_result = condition_result;
    if(state->condition_expr) free(state->condition_expr);
    state->condition_expr = condition_expr ? strdup(condition_expr) : NULL;
    
    printf("处理ELIF，条件结果: %d\n", condition_result);
    return 0;
}

int push_else_statement(void) {
    ControlState* prev_state = get_current_control_state();
    int has_prev_executed = prev_state ? prev_state->has_executed : 0;
    
    // ELSE只有在前面的IF/ELIF都没有执行时才执行
    ControlState* state = &flow_control.control_stack[flow_control.control_depth - 1];
    state->type = CONTROL_TYPE_ELSE;
    state->condition_result = !has_prev_executed;
    
    printf("处理ELSE，条件结果: %d\n", state->condition_result);
    return 0;
}

int should_skip_to_end(void) {
    ControlState* state = get_current_control_state();
    if(!state) return 0;
    
    // 如果是IF/ELIF且已经有条件成功执行，跳过后续的ELIF/ELSE
    if((state->type == CONTROL_TYPE_IF || state->type == CONTROL_TYPE_ELIF) 
       && state->has_executed) {
        return 1;
    }
    
    return 0;
}

// FOR循环管理函数
int push_for_loop(const char* var_name, int start, int end, int step) {
    if(flow_control.for_depth >= 16) return -1; // 最多支持16层嵌套
    
    ForLoopState* loop = &flow_control.for_stack[flow_control.for_depth];
    strncpy(loop->var_name, var_name, sizeof(loop->var_name) - 1);
    loop->var_name[sizeof(loop->var_name) - 1] = '\0';
    loop->start_value = start;
    loop->end_value = end;
    loop->step_value = step;
    loop->current_value = start;
    loop->loop_start_line = 0;
    loop->cached_lines = NULL;
    loop->cached_count = 0;
    loop->is_active = 1;
    
    // 设置循环变量的初始值
    char val_str[32];
    snprintf(val_str, sizeof(val_str), "%d", start);
    set_local_variable(var_name, val_str);
    
    flow_control.for_depth++;
    printf("开始FOR循环: %s = %d 到 %d，步长 %d\n", var_name, start, end, step);
    return 0;
}

int pop_for_loop(void) {
    if(flow_control.for_depth <= 0) return -1;
    
    ForLoopState* loop = &flow_control.for_stack[flow_control.for_depth - 1];
    
    // 清理缓存的语句
    if(loop->cached_lines) {
        for(int i = 0; i < loop->cached_count; i++) {
            free(loop->cached_lines[i]);
        }
        free(loop->cached_lines);
    }
    
    printf("结束FOR循环: %s\n", loop->var_name);
    flow_control.for_depth--;
    return 0;
}

int get_current_for_depth(void) {
    return flow_control.for_depth;
}

ForLoopState* get_current_for_loop(void) {
    if(flow_control.for_depth <= 0) return NULL;
    return &flow_control.for_stack[flow_control.for_depth - 1];
}

int execute_for_loop_iteration(void) {
    ForLoopState* loop = get_current_for_loop();
    if(!loop || !loop->is_active) return -1;
    
    // 检查循环是否应该继续
    if((loop->step_value > 0 && loop->current_value > loop->end_value) ||
       (loop->step_value < 0 && loop->current_value < loop->end_value)) {
        // 循环结束
        return 0;
    }
    
    // 更新循环变量
    char val_str[32];
    snprintf(val_str, sizeof(val_str), "%d", loop->current_value);
    set_local_variable(loop->var_name, val_str);
    
    // 执行缓存的语句
    for(int i = 0; i < loop->cached_count; i++) {
        printf("执行循环体语句: %s\n", loop->cached_lines[i]);
        // 这里应该调用lexer来执行语句，暂时只打印
    }
    
    // 更新循环变量值
    loop->current_value += loop->step_value;
    
    return 1; // 继续循环
}

int add_for_loop_statement(const char* statement) {
    ForLoopState* loop = get_current_for_loop();
    if(!loop || !loop->is_active) return -1;
    
    // 扩展缓存数组
    char** new_lines = realloc(loop->cached_lines, 
                              (loop->cached_count + 1) * sizeof(char*));
    if(!new_lines) return -1;
    
    loop->cached_lines = new_lines;
    loop->cached_lines[loop->cached_count] = strdup(statement);
    loop->cached_count++;
    
    printf("缓存FOR循环语句: %s\n", statement);
    return 0;
}

int is_in_for_loop(void) {
    return flow_control.for_depth > 0;
}

// 从GET命令结果设置变量
int set_variable_from_command_result(const char* name, const char* result) {
    // 这里可以解析GET命令的结果，支持不同格式
    // 简单实现：直接将结果作为字符串赋值
    return set_local_variable(name, result);
}

// 保持向后兼容性的函数
int push_if_condition(int condition_result) {
    if(flow_control.condition_depth >= 32) return -1;
    flow_control.if_stack[flow_control.condition_depth] = condition_result;
    flow_control.condition_depth++;
    return 0;
}

int pop_if_condition(void) {
    if(flow_control.condition_depth <= 0) return -1;
    flow_control.condition_depth--;
    return flow_control.if_stack[flow_control.condition_depth];
}

int get_current_if_state(void) {
    if(flow_control.condition_depth <= 0) return 1; // 默认为真
    return flow_control.if_stack[flow_control.condition_depth - 1];
}

int push_loop_state(void) {
    if(flow_control.loop_depth >= 32) return -1;
    flow_control.loop_stack[flow_control.loop_depth] = 1;
    flow_control.loop_depth++;
    return 0;
}

int pop_loop_state(void) {
    if(flow_control.loop_depth <= 0) return -1;
    flow_control.loop_depth--;
    return flow_control.loop_stack[flow_control.loop_depth];
}

int get_current_loop_state(void) {
    if(flow_control.loop_depth <= 0) return 0; // 不在循环中
    return flow_control.loop_stack[flow_control.loop_depth - 1];
} 