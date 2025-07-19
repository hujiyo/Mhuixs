/*
 * Mhuixs 会话管理模块
 * Copyright (c) HuJi 2025
 * Email: hj18914255909@outlook.com
 */
#ifndef SESSION_H
#define SESSION_H

#include <stdint.h>
#include <time.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/epoll.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/time.h>
#include <signal.h>

// 项目内部依赖
#include "getid.hpp"
#include "usergroup.hpp"
#include "pkg.h"
#include "funseq.h"
#include "mtype.hpp"
#include "dependence/concurrentqueue.h"

using namespace moodycamel;
using namespace std;

#define TIMEOUT 300 // 默认超时时间(s)
#define CLEANUP_INTERVAL 60 //默认会话池清理间隔（s）

#define MAX_BUFFER_SIZE 256*1024 //默认最大缓冲区为256KB
#define DEFAULT_BUFFER_SIZE 4*1024 //默认缓冲区大小为4KB

#define EPOLL_MAX_EVENTS 1024 //epoll单批次返回最大事件数
#define EPOLL_TIMEOUT 100 //epoll超时时间(毫秒)

#define PORT 18482
#define LISTEN_BACKLOG 1024

#define NETWORK_THREADS 1  // 单网络线程 + epoll
#define WORKER_THREADS 4   // 根据CPU核心数调整
#define max_response_threads 10 // 最大响应线程数

#define ADDRESS_FAMILY AF_INET //只支持ipv4

#ifdef __cplusplus
extern "C" {
#endif

typedef size_t Session_Index_in_Pool,SIIP;

// 的响应结构体
typedef struct response {
    int socket_fd;                // 套接字文件描述符
    uint32_t response_len;         // 响应长度:[<56]->inline_data | [>=56]->data
    union {
        uint8_t* data;
        uint8_t inline_data[56];
    }; // 响应数据
} response_t;


// 错误码\状态码定义
#define SESS_OK           0
#define SESS_ERR         -1
#define SESS_INVALID     -2
#define SESS_FULL        -3



// 命令结构体
typedef struct command {
    SID session;                // 会话唯一ID
    UID caller;                 // 呼叫的用户
    uint32_t command_id;        // 命令ID
    uint32_t sequence_num;      // 序列号
    uint32_t data_len;          // 数据长度
    uint8_t* data;              // 命令数据
} command_t;



typedef struct buffer_struct {
    uint8_t* buffer;//缓冲区
    uint32_t capacity;//缓冲区容量
    uint32_t read_pos;//读头
    uint32_t write_pos;//写头
    pthread_mutex_t mutex; // 互斥锁
}buffer_struct;

typedef struct session {
    SID session_id;               // 会话ID
    int socket_fd;                // 套接字文件描述符
    sockaddr_storage client_addr; // 客户端地址
    volatile UID user_id;         // 用户认证ID信息（通过执行模块接口修改）
    buffer_struct recv_buffer;      // 接收缓冲区
    int state;                      //会话状态
    time_t last_activity;          // 最后活动时间
    pthread_mutex_t session_mutex; // 会话互斥锁
} session_t;//会话描述符

int set_socket_nonblocking_(int sockfd);

// 认证管理接口(由用户/组权限管理模块进行统一操作)
int auth_session(SID session_id, UID uid);//认证会话后用户管理模块调用本函数对session信息进行修改



#ifdef __cplusplus
}
#endif

#endif 