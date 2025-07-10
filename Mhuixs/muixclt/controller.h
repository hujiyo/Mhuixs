#ifndef FLOW_CONTROLLER_H
#define FLOW_CONTROLLER_H

#include <stdint.h>
#include <stdbool.h>
#include "stdstr.h"

// 控制类型枚举
typedef enum {
    FLOW_TYPE_IF,
    FLOW_TYPE_ELIF,
    FLOW_TYPE_ELSE,
    FLOW_TYPE_WHILE,
    FLOW_TYPE_FOR
} FlowType;

// 控制块状态
typedef struct {
    FlowType type;
    bool condition_result;
    bool has_executed;      // 用于IF/ELIF链
    bool is_active;
    char** cached_statements;
    int statement_count;
    char* condition_expr;   // 用于WHILE循环重新评估
    
    // FOR循环特有数据
    char var_name[64];
    int start_value;
    int end_value;
    int step_value;
    int current_value;
} FlowBlock;

// 流程控制器状态
typedef struct {
    FlowBlock* blocks;      // 控制块栈
    int block_count;        // 当前控制块数量
    int max_blocks;         // 最大控制块数量
    bool in_multiline;      // 是否在多行模式中
} FlowController;

// 语句执行回调函数类型
typedef int (*StatementExecutor)(const char* statement);

// 流程控制器接口
FlowController* flow_controller_create(void);
void flow_controller_destroy(FlowController* controller);

// 控制块管理
int flow_controller_push_if(FlowController* controller, const char* condition);
int flow_controller_push_elif(FlowController* controller, const char* condition);
int flow_controller_push_else(FlowController* controller);
int flow_controller_push_while(FlowController* controller, const char* condition);
int flow_controller_push_for(FlowController* controller, const char* var_name, 
                             int start, int end, int step);
int flow_controller_end_block(FlowController* controller, StatementExecutor executor);

// 语句缓存
int flow_controller_cache_statement(FlowController* controller, const char* statement);
bool flow_controller_is_in_block(FlowController* controller);
bool flow_controller_should_cache(FlowController* controller);

// 条件评估
int flow_controller_evaluate_condition(const char* condition);

// 循环控制
int flow_controller_break(FlowController* controller);
int flow_controller_continue(FlowController* controller);

// 调试和状态查询
void flow_controller_print_status(FlowController* controller);
int flow_controller_get_depth(FlowController* controller);

#endif // FLOW_CONTROLLER_H 