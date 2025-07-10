/*
 * Mhuixs 会话管理模块
 *
 * 设计理念：
 * - 会话类比为操作系统中的进程
 * - SESSION结构体类比为早期进程描述符(PCB)
 * - 执行模块类比为CPU，单线程执行
 * - 会话池由多线程管理，维护待执行命令队列
 * - 支持优先级调度和异步命令执行
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

#define DEFAULT_HEARTBEAT_INTERVAL 10 // 默认心跳间隔(秒)
#define TIMEOUT 300 // 默认超时时间(秒)

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SESS_IDLE     = 0,  // 空闲：已回收，可重新分配
    SESS_ALIVE    = 1,  // 活跃：等待数据，正常工作
    SESS_SLEEPING = 2,  // 休眠：通过心跳包维持连接
    SESS_BUSY     = 3,  // 繁忙：正在处理数据传输
    SESS_SUPER    = 4,  // 超级：管理员会话，优先级最高
    SESS_CLOSING  = 5,  // 关闭中：正在安全关闭
    SESS_ERROR    = 6   // 错误：异常状态，需要强制回收
} session_state_t;// 会话状态枚举

typedef enum {
    PRIORITY_LOWEST  = 0,
    PRIORITY_LOW     = 1,
    PRIORITY_NORMAL  = 2,
    PRIORITY_HIGH    = 3,
    PRIORITY_HIGHEST = 4,
    PRIORITY_SUPER   = 5   // 管理员专用
} session_priority_t;// 会话优先级枚举

// 错误码定义
#define SESS_OK           0
#define SESS_ERROR       -1
#define SESS_INVALID     -2
#define SESS_FULL        -3
#define SESS_NOT_FOUND   -4
#define SESS_PERMISSION  -5

// 命令结构体
typedef struct command {
    uint32_t command_id;        // 命令ID
    uint32_t sequence_num;      // 序列号
    uint32_t priority;          // 命令优先级
    uint32_t data_len;          // 数据长度
    uint8_t* data;              // 命令数据
    time_t timestamp;           // 时间戳
    struct command* next;       // 链表指针
} command_t;

// 命令队列结构体（优先级队列）
typedef struct {
    command_t* head;            // 队列头
    command_t* tail;            // 队列尾
    uint32_t count;             // 命令数量
    uint32_t max_size;          // 最大队列长度
    pthread_mutex_t mutex;      // 互斥锁
    pthread_cond_t cond;        // 条件变量
    int shutdown;               // 关闭标志
} command_queue_t;

//=============================================================================
// 会话结构体 (类比进程描述符PCB)
//=============================================================================

typedef struct buffer_struct {
    uint8_t* buffer;//缓冲区
    uint32_t capacity;//缓冲区容量
    uint32_t read_pos;//读头
    uint32_t write_pos;//写头
    pthread_mutex_t mutex; // 互斥锁
}buffer_struct;


// 会话结构体 - 核心会话对象
typedef struct session {
    SID session_id;                 // 会话ID
    int socket_fd;                  // 套接字文件描述符
    struct sockaddr_storage client_addr; // 客户端地址
        
    volatile UID user_id;                // 用户认证ID信息（通过执行模块接口修改）
    
    // 状态管理
    volatile session_state_t state; // 会话状态
    volatile session_priority_t priority; // 会话优先级
    volatile uint32_t revisit_count; // 重连次数
    
    command_queue_t* command_queue; // 命令队列
    
    buffer_struct recv_buffer;// 接收缓冲区
    buffer_struct send_buffer;// 发送缓冲区

    // 线程安全
    pthread_mutex_t session_mutex;  // 会话互斥锁
    
    // 时间戳
    time_t last_activity;           // 最后活动时间
 
    // 会话统计信息
    uint64_t bytes_received;    // 接收字节数
    uint64_t bytes_sent;        // 发送字节数
    uint64_t commands_processed; // 已处理命令数
    uint64_t commands_queued;   // 已入队命令数
    
    // 配置信息 
    uint32_t max_buffer_size;       // 最大缓冲区大小
    uint32_t max_command_queue_size; // 最大命令队列长度
    int if_disable_cpr;            // 是否禁用压缩
} session_t;

//=============================================================================
// 会话池管理
//=============================================================================

// 会话池结构体
typedef struct {
    session_t* sessions;            // 会话数组
    uint32_t* idle_sessions;        // 空闲会话ID数组
    uint32_t* active_sessions;      // 活跃会话ID数组
    uint32_t* error_sessions;       // 错误会话ID数组
    
    uint32_t idle_count;            // 空闲会话数
    uint32_t active_count;          // 活跃会话数
    uint32_t error_count;           // 错误会话数
    
    // 会话池配置
    uint32_t max_sessions;          // 最大会话数
    uint32_t initial_pool_size;     // 初始池大小
    uint32_t cleanup_interval;      // 清理间隔(秒)
    uint32_t health_check_interval; // 健康检查间隔(秒)
    uint32_t max_idle_sessions;     // 最大空闲会话数
    int enable_auto_cleanup;        // 是否启用自动清理
    int enable_session_reuse;       // 是否启用会话重用

    // 会话池统计信息
    volatile uint32_t total_sessions_num;    // 总会话数
    volatile uint32_t active_sessions_num;   // 活跃会话数
    volatile uint32_t idle_sessions_num;     // 空闲会话数
    volatile uint32_t error_sessions_num;    // 错误会话数
    volatile uint32_t allocated_counter;   // 已分配次数
    volatile uint32_t recycled_counter;    // 已回收次数
    
    pthread_mutex_t pool_mutex;     // 池互斥锁
    pthread_t cleanup_thread;       // 清理线程
    pthread_t health_check_thread;  // 健康检查线程
    
    volatile int shutdown_requested; // 关闭请求标志
} session_pool_t;


// 网络管理器配置
typedef struct {
    uint16_t listen_port;           // 监听端口
    int address_family;             // 地址族
    uint32_t listen_backlog;        // 监听队列长度
    int enable_ipv6;                // 是否启用IPv6
    
    uint32_t network_threads;       // 网络线程数
    uint32_t worker_threads;        // 工作线程数
    uint32_t epoll_max_events;      // epoll最大事件数
    uint32_t epoll_timeout;         // epoll超时时间(毫秒)
    
    int enable_rate_limiting;       // 启用速率限制
    uint32_t max_connections_per_ip; // 每IP最大连接数
    uint32_t connection_timeout;    // 连接超时(秒)
    
    // 会话池配置
    uint32_t max_sessions;          // 最大会话数
    uint32_t initial_pool_size;     // 初始池大小
    uint32_t cleanup_interval;      // 清理间隔(秒)
    uint32_t health_check_interval; // 健康检查间隔(秒)
    uint32_t max_idle_sessions;     // 最大空闲会话数
    int enable_auto_cleanup;        // 是否启用自动清理
    int enable_session_reuse;       // 是否启用会话重用
} network_manager_config_t;

// 网络事件类型
typedef enum {
    NET_EVENT_NEW_CONNECTION = 1,//新连接事件
    NET_EVENT_DATA_AVAILABLE = 2,//数据可用事件
    NET_EVENT_CONNECTION_CLOSED = 3,//连接关闭事件
    NET_EVENT_ERROR_OCCURRED = 4//错误事件
} network_event_type_t;

// 网络事件结构
typedef struct {
    network_event_type_t type; // 事件类型
    SID session_id; // 会话ID
    int socket_fd; // 套接字文件描述符
    uint32_t data_len; // 数据长度
    uint8_t* data; // 数据
} network_event_t;

// 网络管理器结构体
typedef struct {
    network_manager_config_t config; // 网络管理器配置
    session_pool_t* session_pool; // 会话池
    
    int listen_fd;                  // 监听套接字
    int epoll_fd;                   // epoll文件描述符:通知
    
    pthread_t* network_threads;     // 网络线程数组
    pthread_t* worker_threads;      // 工作线程数组
    
    volatile int initialized;       // 初始化标志
    volatile int running;           // 运行标志
      
    // 统计信息
    volatile uint64_t total_connections; // 总连接数
    volatile uint64_t active_connections; // 活跃连接数
    volatile uint64_t failed_connections; // 失败连接数
    volatile uint64_t bytes_processed; // 处理字节数
    volatile uint64_t commands_processed; // 处理命令数
    time_t start_time; // 启动时间
} network_manager_t;


// 命令队列操作
command_queue_t* create_command_queue(uint32_t max_size);//创建命令队列
void destroy_command_queue(command_queue_t* queue);//销毁命令队列
int command_queue_push(command_queue_t* queue, command_t* cmd);//将命令推入命令队列
command_t* command_queue_pop(command_queue_t* queue, uint32_t timeout_ms);//从命令队列中取出命令
command_t* command_queue_try_pop(command_queue_t* queue);//尝试从命令队列中取出命令
void command_queue_shutdown(command_queue_t* queue);//关闭命令队列
uint32_t command_queue_size(command_queue_t* queue);//获取命令队列大小（线程安全）

// 命令操作
command_t* command_create(CommandNumber cmd_id,uint32_t seq_num,uint32_t priority,
                         const uint8_t* data,uint32_t data_len);//创建命令
void command_destroy(command_t* cmd);//销毁命令

// 网络缓冲区操作
buffer_struct* create_network_buffer(uint32_t initial_capacity);//创建网络缓冲区
void destroy_network_buffer(buffer_struct* buffer);//销毁网络缓冲区
int buffer_write(buffer_struct* buffer, const uint8_t* data, uint32_t len);//将数据写入网络缓冲区
uint32_t buffer_read(buffer_struct* buffer, uint8_t* data, uint32_t max_len);//从网络缓冲区中读取数据
uint32_t buffer_peek(buffer_struct* buffer, uint8_t* data, uint32_t max_len);//查看网络缓冲区数据
uint32_t get_buffer_available_size(buffer_struct* buffer);//获取网络缓冲区可用空间
void clear_buffer(buffer_struct* buffer);//清空网络缓冲区
void compact_buffer(buffer_struct* buffer);//压缩网络缓冲区

// 会话操作
    /*
    会话池配置
    uint32_t max_sessions;          // 最大会话数
    uint32_t initial_pool_size;     // 初始池大小
    uint32_t cleanup_interval;      // 清理间隔(秒)
    uint32_t health_check_interval; // 健康检查间隔(秒)
    uint32_t max_idle_sessions;     // 最大空闲会话数
    int enable_auto_cleanup;        // 是否启用自动清理
    int enable_session_reuse;       // 是否启用会话重用
     */
session_t* create_session(SID sid,int if_default,//是否启动默认配置
                            uint32_t max_buffer_size,// 最大缓冲区大小
                            uint32_t max_command_queue_size,// 最大命令队列长度
                            int if_disable_cpr);// 是否禁用压缩
                            //创建会话
void destroy_session(session_t* session);//销毁会话
int session_initialize(session_t* session, int socket_fd, const struct sockaddr* client_addr, UID uid);//初始化会话
void session_shutdown(session_t* session);//关闭会话
void session_reset(session_t* session);//重置会话

// 会话网络操作
int session_receive_data(session_t* session);//接收数据
int session_send_data(session_t* session);//发送数据
int session_process_incoming_packets(session_t* session);//处理入站数据包

// 会话命令操作
int session_enqueue_command(session_t* session, command_t* cmd);//将命令推入会话队列
command_t* session_dequeue_command(session_t* session);//从会话队列中取出命令
int session_has_pending_commands(session_t* session);//检查会话是否有待处理命令

// 会话状态管理
void session_set_state(session_t* session, session_state_t new_state);//设置会话状态
void session_set_priority(session_t* session, session_priority_t new_priority);//设置会话优先级
void session_update_activity(session_t* session);//更新会话活动时间

// 会话健康检查
int session_is_healthy(session_t* session);//检查会话是否健康
int session_is_idle_timeout(session_t* session);//检查会话是否超时
int session_should_close(session_t* session);//检查会话是否需要关闭

// 会话池操作
session_pool_t* session_pool_create(const session_pool_config_t* config);//创建会话池
void session_pool_destroy(session_pool_t* pool);//销毁会话池
int session_pool_initialize(session_pool_t* pool);//初始化会话池
void session_pool_shutdown(session_pool_t* pool);//关闭会话池

session_t* session_pool_allocate_session(session_pool_t* pool);//分配会话
session_t* session_pool_allocate_with_socket(session_pool_t* pool, int socket_fd, 
                                            const struct sockaddr* client_addr, UID uid);//分配会话
int session_pool_recycle_session(session_pool_t* pool, SID session_id);//回收会话

session_t* session_pool_find_session(session_pool_t* pool, SID session_id);//按会话ID查找会话
session_t* session_pool_find_session_by_user(session_pool_t* pool, UID user_id);//按用户ID查找会话

// 获取可执行会话列表
uint32_t session_pool_get_executable_sessions(session_pool_t* pool, SID* session_ids, uint32_t max_count);//获取可执行会话列表
uint32_t session_pool_get_sessions_with_commands(session_pool_t* pool, SID* session_ids, uint32_t max_count);//获取有待处理命令的会话列表

// 会话池统计和维护
void session_pool_cleanup_dead_sessions(session_pool_t* pool);//清理死亡会话
void session_pool_health_check_all_sessions(session_pool_t* pool);//检查所有会话的健康状态

// 网络管理器操作
network_manager_t* network_manager_create(const network_manager_config_t* config);//创建网络管理器
void network_manager_destroy(network_manager_t* manager);//销毁网络管理器
int network_manager_initialize(network_manager_t* manager);//初始化网络管理器
int network_manager_start(network_manager_t* manager);//启动网络管理器
void network_manager_stop(network_manager_t* manager);//停止网络管理器
void network_manager_shutdown(network_manager_t* manager);//关闭网络管理器

// 执行模块接口 - 供执行模块调用的会话管理函数
session_t* get_session(SID session_id);//按会话ID获取会话
uint32_t get_sessions_with_commands(SID* session_ids, uint32_t max_count);//获取有待处理命令的会话列表
command_t* get_next_command_from_session(SID session_id);//从会话中获取下一个命令
int send_response_to_session(SID session_id, const uint8_t* response, uint32_t response_len);//向会话发送响应

//=============================================================================
// 执行模块友元接口 - 认证和权限管理
//=============================================================================

// 认证管理接口
int authenticate_session(SID session_id, UID uid, GID gid, 
                        const char* username, const char* auth_token);//认证会话
int promote_guest_session(SID session_id, UID uid, const char* username);//提升游客会话
void invalidate_session_auth(SID session_id);//使会话认证失效

// 权限管理接口
int add_session_to_group(SID session_id, GID group_id);//将会话添加到组
int remove_session_from_group(SID session_id, GID group_id);//将会话从组中移除
int update_session_permissions(SID session_id, const GID* groups, uint32_t group_count);//更新会话权限

// 权限检查接口
int check_session_permission(SID session_id, const char* object_name, Mode_type mode);//检查会话权限
int check_session_group_permission(SID session_id, GID group_id, Mode_type mode);//检查会话组权限
int is_session_object_owner(SID session_id, const char* object_name);//检查会话是否是对象所有者

// 会话状态查询接口
user_auth_info_t get_session_auth_info(SID session_id);//获取会话认证信息
int is_session_authenticated(SID session_id);//检查会话是否已认证
UID get_session_user_id(SID session_id);//获取会话用户ID
GID get_session_group_id(SID session_id);//获取会话组ID

//=============================================================================
// 全局接口
//=============================================================================

// 全局初始化和清理
int session_system_init(const network_manager_config_t* config);//初始化会话系统
void session_system_shutdown(void);//关闭会话系统

// 获取全局实例
network_manager_t* get_global_network_manager(void);//获取全局网络管理器
session_pool_t* get_global_session_pool(void);//获取全局会话池

// 工具函数
int set_socket_nonblocking(int sockfd);//设置套接字非阻塞
const char* session_state_to_string(session_state_t state);//将会话状态转换为字符串
const char* session_priority_to_string(session_priority_t priority);//将会话优先级转换为字符串

#ifdef __cplusplus
}
#endif

#endif 