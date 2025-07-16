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
#include "comdq.h"


#define DEFAULT_HEARTBEAT_INTERVAL 10 // 默认心跳间隔(s)
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

#define ADDRESS_FAMILY AF_INET //只支持ipv4

#ifdef __cplusplus
extern "C" {
#endif

typedef size_t Session_Index_in_Pool,SIIP;

typedef enum {
    SESS_IDLE     = 0,  // 空闲：已回收，可重新分配
    SESS_ALIVE    = 1,  // 活跃：等待数据，正常工作
    SESS_SLEEPING = 2,  // 休眠：通过心跳包维持连接
    SESS_BUSY     = 3,  // 繁忙：正在处理数据传输
    SESS_SUPER    = 4,  // 超级：管理员会话，优先级最高
    SESS_CLOSING  = 5,  // 关闭中：正在安全关闭
    SESS_ERROR    = 6,  // 错误：异常状态，需要强制回收
} session_state_t;// 会话状态枚举

// 错误码定义
#define SESS_OK           0
#define SESS_ERR         -1
#define SESS_INVALID     -2
#define SESS_FULL        -3

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
    volatile session_state_t state;// 会话状态
    buffer_struct recv_buffer;// 接收缓冲区
    buffer_struct send_buffer;// 发送缓冲区
    time_t last_activity;          // 最后活动时间
    pthread_mutex_t session_mutex; // 会话互斥锁
} session_t;//会话描述符

// 网络管理器结构体
struct network_manager_t{
    session_t* sesspool;            // 会话池
    SIIP* idle_sessions;        // 空闲会话ID数组
    SIIP* active_sessions;      // 活跃会话ID数组
    volatile uint32_t pool_capacity;  // 会话池容量
    volatile uint32_t idle_num;   // 空闲会话数
    volatile uint32_t active_num; // 活跃会话数
    pthread_mutex_t pool_mutex;     // 池互斥锁

    int listen_fd;                  // 监听套接字
    int epoll_fd;                   // epoll文件描述符:通知
    pthread_t* network_threads;     // 网络线程数组
    pthread_t* worker_threads;      // 工作线程数组
    pthread_t cleanup_thread;       // 清理线程
    pthread_t health_check_thread;  // 健康检查线程
    volatile int shutdown_requested;// 关闭请求标志
    pthread_mutex_t manager_mutex;  // 管理器锁
    
    volatile int initialized;       // 初始化标志
    volatile int running;           // 运行标志
};

// 网络缓冲区操作
int buffer_write(buffer_struct buffer, const uint8_t* data, uint32_t len);//将数据写入网络缓冲区
uint32_t buffer_read(buffer_struct* buffer, uint8_t* data, uint32_t max_len);//从网络缓冲区中读取数据
uint32_t buffer_peek(buffer_struct* buffer, uint8_t* data, uint32_t max_len);//查看网络缓冲区数据
uint32_t get_buffer_available_size(buffer_struct* buffer);//获取网络缓冲区可用空间
void clear_buffer(buffer_struct* buffer);//清空网络缓冲区
void compact_buffer(buffer_struct* buffer);//压缩网络缓冲区

// 会话网络操作
int session_receive_data(session_t* session);//接收数据
int session_send_data(session_t* session);//发送数据
int session_process_incoming_packets(session_t* session);//处理入站数据包

// 会话状态管理
void session_set_state(session_t* session, session_state_t new_state);//设置会话状态

session_t* alloc_with_socket(network_manager_t* pool, int socket_fd,const sockaddr* client_addr);//分配会话
int recycle_session(network_manager_t* pool, SID session_id);//回收会话

// 统计和维护
void cleanup_dead_sessions(network_manager_t* pool);//清理死亡会话

// 网络管理器操作
int network_manager_start(network_manager_t* manager);//启动网络管理器
void network_manager_stop(network_manager_t* manager);//停止网络管理器
void network_manager_shutdown(network_manager_t* manager);//关闭网络管理器

// 执行模块接口 - 供执行模块调用的会话管理函数
int send_response_to_session(SID session_id, const uint8_t* response, uint32_t response_len);//向会话发送响应

// 认证管理接口(由用户/组权限管理模块进行统一操作)
int auth_session(SID session_id, UID uid);//认证会话后用户管理模块调用本函数对session信息进行修改
void invalidate_session_auth(SID session_id);//使会话认证失效

int session_system_init();//初始化会话系统,由Mhuixs主线程启动阶段调用
void shutdown_session_system();//关闭会话系统,由Mhuixs主线程启动阶段调用

#ifdef __cplusplus
}
#endif

#endif 