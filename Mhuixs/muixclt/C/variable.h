#ifndef VARIABLE_H
#define VARIABLE_H

#include "stdstr.h"

#ifdef __cplusplus
extern "C" {
#endif

// 变量类型枚举
typedef enum {
    VAR_TYPE_STRING,
    VAR_TYPE_NUMBER,
    VAR_TYPE_LIST,
    VAR_TYPE_BOOLEAN
} VariableType;

// 变量结构体
typedef struct {
    str name;
    str value;
    VariableType type;
    int is_list;
    str* list_items;
    int list_count;
} Variable;

// 控制流状态管理
typedef struct {
    int condition_depth;
    int loop_depth;
    int if_stack[32];     // IF语句栈
    int loop_stack[32];   // 循环语句栈
} ControlState;

// 变量系统初始化和清理
void variable_system_init(void);
void variable_system_cleanup(void);

// 变量类型检测
VariableType detect_variable_type(const char* value);

// 基本变量操作
int set_local_variable(const char* name, const char* value);
int set_local_variable_with_type(const char* name, const char* value, VariableType type);
const char* get_local_variable(const char* name);
int delete_local_variable(const char* name);

// 列表变量操作
int set_local_list_variable(const char* name, const char** values, int count);
int add_to_list_variable(const char* name, const char* value);
const char* get_list_variable_item(const char* name, int index);
int get_list_variable_count(const char* name);

// 变量信息获取
int get_variable_count(void);
void print_all_variables(void);
void print_variable_info(const char* name);

// 控制流管理
void control_state_init(void);
int push_if_condition(int condition_result);
int pop_if_condition(void);
int get_current_if_state(void);
int push_loop_state(void);
int pop_loop_state(void);
int get_current_loop_state(void);

// 从GET命令结果设置变量
int set_variable_from_command_result(const char* name, const char* result);

#ifdef __cplusplus
}
#endif

#endif // VARIABLE_H 