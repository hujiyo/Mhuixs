/*
* Mhuixs 会话管理模块
* Copyright (c) HuJi 2025
* Email: hj18914255909@outlook.com
*/
#ifndef MANAGER_H
#define MANAGER_H


#include <stdint.h>
#include <time.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
//#include <arpa/inet.h>
//#include <unistd.h>
//#include <fcntl.h>
//#include <errno.h>
#include <sys/epoll.h>
//#include <stdlib.h>
//#include <string.h>
//#include <stdio.h>
#include <sys/time.h>
#include <signal.h>

// 项目内部依赖
#include "getid.hpp"
//#include "usergroup.hpp"
//#include "pkg.h"
#include "funseq.h"
//#include "mtype.hpp"
#include "dependence/concurrentqueue.h"
#include "dependence/readerwriterqueue.h"
using namespace std;
using namespace moodycamel;

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
#define ENABLE_IPV6 0  // 默认不启用IPv6


#ifdef __cplusplus
extern "C" {
#endif

typedef size_t Session_Index_in_Pool,SIIP;

extern Id_alloctor Idalloc;

#define UN_START 7
#define RUN      1
#define CLOSE    0  //全面关闭
#define KILL     3  //请求关闭

// 错误码\状态码定义
#define SESS_OK           0
#define SESS_ERR         -1
#define SESS_INVALID     -2
#define SESS_FULL        -3
#define SESS_IDLE         1     //会话为空
#define SESS_ALIVE        2     //会话存活
#define SESS_USING        3     //会话处于中间状态，正在被操作

typedef struct buffer_struct {
    uint8_t* buffer;//缓冲区
    uint32_t capacity;//缓冲区容量
    uint32_t read_pos;//读头
    uint32_t write_pos;//写头
    pthread_mutex_t mutex; // 互斥锁
}buffer_struct;

typedef struct session_t {
    SID session_id;               // 会话ID
    int socket_fd;                // 套接字文件描述符
    sockaddr_storage client_addr; // 客户端地址
    volatile UID user_id;         // 用户认证ID信息（通过执行模块接口修改）
    buffer_struct recv_buffer;      // 接收缓冲区
    int state;                      //会话状态
    time_t last_activity;          // 最后活动时间
    pthread_mutex_t session_mutex; // 会话互斥锁
} session_t;//会话描述符

// 命令结构体
typedef struct command {
    session_t *session;              // 会话描述符指针
    CommandNumber command_id;   // 命令ID
    void* k0;
    void* k1;
    void* k2;
    void* k3;
} command_t;

// 响应结构体
typedef struct response {
    session_t *session;                 // 回复会话对象
    uint32_t response_len;         // 响应长度:[<57]->inline_data | [>=57]->data
    union {
        uint8_t* data;//需要对方释放
        uint8_t inline_data[56];
    }; // 响应数据
} response_t;

// 网络管理器结构体
typedef struct {
    session_t* sesspool;                // 会话池
    SIIP* idle_sessions;                // 空闲会话ID数组
    SIIP* active_sessions;              // 活跃会话ID数组
    volatile uint32_t pool_capacity;    // 会话池容量
    volatile uint32_t idle_num;         // 空闲会话数
    volatile uint32_t active_num;       // 活跃会话数
    pthread_mutex_t pool_mutex;         // 池互斥锁

    int listen_fd;                      // 监听套接字
    int epoll_fd;                       // epoll文件描述符:通知

    pthread_t* network_threads;         // 网络线程数组
    pthread_t* worker_threads;          // 工作线程数组
    pthread_t response_thread;          // 发送线程
    pthread_t cleanup_thread;           // 清理线程

    pthread_mutex_t manager_mutex;      // 管理器锁
}network_manager_t;



// 认证管理接口(由用户/组权限管理模块进行统一操作)
int auth_session(SID session_id, UID uid);//认证会话后用户管理模块调用本函数对session信息进行修改
// 执行线程调用本函数将数据推入回复队列
int send_response(session_t *session, uint8_t* response_data, uint32_t response_len);

// 全局变量
extern network_manager_t* g_network_manager;//网络管理器
extern volatile int running_flag;//网络系统运行标志
extern volatile int cleanup_thread_running_flag;//清理线程运行标志
extern volatile int response_manager_thread_running_flag;//回复线程运行标志
extern volatile atomic_int network_thread_running_flag;//网络线程运行标志
extern volatile atomic<int> worker_thread_running_flag;//解包工作线程线程运行标志

// 全局响应队列和线程管理
extern ReaderWriterQueue<response_t*> response_queue;//全局回复队列
extern ConcurrentQueue<command_t*> command_queue;//全局执行队列


void network_server_system_init();//初始化会话系统,由Mhuixs主线程启动阶段调用
void network_server_system_start();//启动会话系统,由Mhuixs主线程启动阶段调用
void network_server_system_shutdown();//关闭会话系统,由Mhuixs主线程关闭阶段调用


//内部辅助函数声明(netbase.cpp)
int set_socket_nonblocking_(int sockfd);//设置对话为非阻塞
SIIP alloc_with_socket_(int socket_fd, const sockaddr* client_addr);//分配会话 epoll线程使用
int try_get_session_ownership_(SIIP siid);//按会话SIID尝试获得所有权
int release_session_ownership_(SIIP siid);//按会话SIID释放所有权
static void init_sesspool_(network_manager_t* manager);//线程池初始化

#ifdef __cplusplus
}
#endif

#endif