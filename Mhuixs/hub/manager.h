#ifndef MANAGER_H
#define MANAGER_H

#include "session.h"
#include "getid.hpp"
extern Id_alloctor Idalloc;
//#include "merr.h"

using namespace std;

// IPv6支持配置
// 定义 ENABLE_IPV6 宏来启用IPv6支持
// 当启用IPv6时，服务器将同时监听IPv4和IPv6
// 当不启用时，仅支持IPv4（当前行为）
#define ENABLE_IPV6 0  // 默认不启用IPv6



#define SESS_IDLE         1     //会话为空
#define SESS_ALIVE        2     //会话存活
#define SESS_USING        3     //会话处于中间状态，正在被操作
#define SESS_IDLE     4     //请求回收

#define UN_START 7
#define RUN      1
#define CLOSE    0  //全面关闭
#define KILL     3  //请求关闭



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

void network_server_system_init();//初始化会话系统,由Mhuixs主线程启动阶段调用
void network_server_system_start();//启动会话系统,由Mhuixs主线程启动阶段调用
void network_server_system_shutdown();//关闭会话系统,由Mhuixs主线程关闭阶段调用

// 全局变量
extern network_manager_t* g_network_manager;//网络管理器
extern volatile int running_flag;//网络系统运行标志
extern volatile atomic_int network_thread_running_flag;//网络线程运行标志

#endif