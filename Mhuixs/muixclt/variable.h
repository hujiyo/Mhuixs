#ifndef VARIABLE_H
#define VARIABLE_H

#include "stdstr.h"

#ifdef __cplusplus
extern "C" {
#endif

// 变量类型枚举
typedef enum {
    VAR_TYPE_STRING,
    VAR_TYPE_LIST,
    VAR_TYPE_BOOLEAN
} VariableType;//数字统一视为字符串，涉及加减操作时再转换为数字

// 变量结构体
typedef struct {
    str name;
    str value;
    VariableType type;
    int is_list;
    str* list_items;
    int list_count;
} Variable;

// 条件控制类型枚举
typedef enum {
    CONTROL_TYPE_IF,
    CONTROL_TYPE_ELIF,
    CONTROL_TYPE_ELSE,
    CONTROL_TYPE_WHILE,
    CONTROL_TYPE_FOR
} ControlType;

// 条件控制状态结构
typedef struct {
    ControlType type;        // 控制类型
    int condition_result;    // 条件结果 (0=false, 1=true)
    int has_executed;        // 是否已经执行过 (用于IF/ELIF链)
    int is_active;           // 是否当前激活
    char** cached_lines;     // 缓存的语句
    int cached_count;        // 缓存语句数量
    
    // WHILE循环特有
    char* condition_expr;    // 条件表达式（用于重新评估）
    
    // FOR循环特有数据
    char var_name[64];       // 循环变量名
    int start_value;         // 开始值
    int end_value;           // 结束值
    int step_value;          // 步长
    int current_value;       // 当前值
} ControlState;

// FOR循环状态结构（保持向后兼容）
typedef struct {
    char var_name[64];    // 循环变量名
    int start_value;      // 开始值
    int end_value;        // 结束值
    int step_value;       // 步长
    int current_value;    // 当前值
    int loop_start_line;  // 循环开始行（用于脚本模式）
    char** cached_lines;  // 缓存的循环体语句
    int cached_count;     // 缓存语句数量
    int is_active;        // 是否激活
} ForLoopState;

// 流程控制栈结构
typedef struct {
    ControlState control_stack[32];  // 通用控制栈，支持嵌套IF/WHILE/FOR
    int control_depth;               // 当前控制深度
    
    // 向后兼容的FOR循环栈
    ForLoopState for_stack[16];      // FOR循环栈，最多支持16层嵌套
    int for_depth;                   // 当前FOR循环深度
    
    // 传统的栈（保持兼容性）
    int condition_depth;
    int loop_depth;
    int if_stack[32];                // IF语句栈
    int loop_stack[32];              // 循环语句栈
} FlowControlState;

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

// 新的条件控制管理
int push_control_state(ControlType type, int condition_result, const char* condition_expr);
int pop_control_state(void);
ControlState* get_current_control_state(void);
int is_in_control_block(void);
int should_execute_current_block(void);
int add_control_statement(const char* statement);
int execute_control_block(void);

// 条件评估函数
int evaluate_condition(const char* condition_expr);
int evaluate_simple_condition(const char* left, const char* op, const char* right);

// WHILE循环管理
int push_while_loop(const char* condition_expr);
int execute_while_iteration(void);

// IF/ELSE/ELIF管理
int push_if_statement(const char* condition_expr);
int push_elif_statement(const char* condition_expr);
int push_else_statement(void);
int should_skip_to_end(void);

// FOR循环管理
int push_for_loop(const char* var_name, int start, int end, int step);
int pop_for_loop(void);
int get_current_for_depth(void);
ForLoopState* get_current_for_loop(void);
int execute_for_loop_iteration(void);
int add_for_loop_statement(const char* statement);
int is_in_for_loop(void);

// 从GET命令结果设置变量
int set_variable_from_command_result(const char* name, const char* result);

#ifdef __cplusplus
}
#endif

#endif // VARIABLE_H 