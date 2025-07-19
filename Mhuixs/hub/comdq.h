#ifndef COMDQ_H
#define COMDQ_H

// 命令队列结构体（基于moodycamel::ConcurrentQueue）
#include <sys/time.h>
#include <unistd.h>
#include <stdatomic.h>
#include <cstring>
#include "dependence/concurrentqueue.h"
using namespace moodycamel;

#include "getid.hpp"
#include "funseq.h"

// 命令结构体
typedef struct command {
    UID caller;                 // 呼叫的用户
    uint32_t command_id;        // 命令ID
    uint32_t sequence_num;      // 序列号
    uint32_t data_len;          // 数据长度
    uint8_t* data;              // 命令数据
} command_t;

struct command_queue_t {
    ConcurrentQueue<command_t*> command_queue; // 全局命令队列，存储指针以避免拷贝
    volatile int shutdown;  // 关闭标志
};

// 命令队列操作
command_queue_t* create_command_queue(uint32_t max_size);//生成全局命令队列
void destroy_command_queue(command_queue_t* queue);//销毁命令队列
size_t command_queue_push(command_queue_t* queue,const command_t* cmd,size_t num);//将命令推入命令队列
command_t* command_queue_pop(command_queue_t* queue, uint32_t timeout_ms,size_t* num_back);//从命令队列中取出命令

// 队列命令操作
command_t* create_command(UID caller,CommandNumber cmd_id,uint32_t seq_num,uint32_t priority,
                         const uint8_t* data,uint32_t data_len);//创建命令
void destroy_command(command_t* cmd);//销毁命令
int push_command(command_t* cmd);//将命令推入会话队列

#endif

