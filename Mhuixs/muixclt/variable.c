#include "variable.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 全局变量存储
static Variable* local_vars = NULL;
static int var_count = 0;
static int var_capacity = 0;

// 控制流状态
static ControlState control_state = {0, 0, {0}, {0}};

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
    
    // 重置控制状态
    control_state_init();
}

// 检测变量类型
VariableType detect_variable_type(const char* value) {
    if(value == NULL) return VAR_TYPE_STRING;
    
    // 检查是否是数字
    char* endptr;
    strtod(value, &endptr);
    if(*endptr == '\0' && *value != '\0') {
        return VAR_TYPE_NUMBER;
    }
    
    // 检查是否是布尔值
    if(strcmp(value, "true") == 0 || strcmp(value, "false") == 0) {
        return VAR_TYPE_BOOLEAN;
    }
    
    // 默认为字符串
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
                    case VAR_TYPE_NUMBER: type_name = "数字"; break;
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
    control_state.condition_depth = 0;
    control_state.loop_depth = 0;
    memset(control_state.if_stack, 0, sizeof(control_state.if_stack));
    memset(control_state.loop_stack, 0, sizeof(control_state.loop_stack));
}

int push_if_condition(int condition_result) {
    if(control_state.condition_depth >= 32) return -1;
    control_state.if_stack[control_state.condition_depth] = condition_result;
    control_state.condition_depth++;
    return 0;
}

int pop_if_condition(void) {
    if(control_state.condition_depth <= 0) return -1;
    control_state.condition_depth--;
    return control_state.if_stack[control_state.condition_depth];
}

int get_current_if_state(void) {
    if(control_state.condition_depth <= 0) return 1; // 默认为真
    return control_state.if_stack[control_state.condition_depth - 1];
}

int push_loop_state(void) {
    if(control_state.loop_depth >= 32) return -1;
    control_state.loop_stack[control_state.loop_depth] = 1;
    control_state.loop_depth++;
    return 0;
}

int pop_loop_state(void) {
    if(control_state.loop_depth <= 0) return -1;
    control_state.loop_depth--;
    return control_state.loop_stack[control_state.loop_depth];
}

int get_current_loop_state(void) {
    if(control_state.loop_depth <= 0) return 0; // 不在循环中
    return control_state.loop_stack[control_state.loop_depth - 1];
}

// 从GET命令结果设置变量
int set_variable_from_command_result(const char* name, const char* result) {
    // 这里可以解析GET命令的结果，支持不同格式
    // 简单实现：直接将结果作为字符串赋值
    return set_local_variable(name, result);
} 