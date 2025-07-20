#include "manager.h"
#include "env.hpp"
#include "fcntl.h"
#include "unistd.h"
#include "errno.h"
#include "string.h"
#include "stdlib.h"

int set_socket_nonblocking_(const int sockfd) {
    const int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1) {
        return SESS_ERR;
    }
    if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {
        return SESS_ERR;
    }
    return SESS_OK;
}
int try_get_session_ownership_(SIIP siid) {
    //获得非空闲对话的所有权
    if (siid >= Env.max_sessions ) return SESS_ERR;
    session_t* session = &g_network_manager->sesspool[siid];
    if (session->state == SESS_IDLE ||
        pthread_mutex_trylock(&session->session_mutex) == EBUSY
        )return SESS_ERR;
    return 0;
}
int release_session_ownership_(SIIP siid) {
    //释放对话的所有权
    if (siid >= Env.max_sessions) return SESS_INVALID;
    session_t* session = &g_network_manager->sesspool[siid];
    pthread_mutex_unlock(&session->session_mutex);// 释放会话锁
    return 0;
}
SIIP alloc_with_socket_(int socket_fd, const sockaddr* client_addr) {
    network_manager_t* pool = g_network_manager;
    if (!pool) return SIZE_MAX;

    pthread_mutex_lock(&pool->pool_mutex);
    if (pool->idle_num == 0) {
        pthread_mutex_unlock(&pool->pool_mutex);
        return SIZE_MAX; // 没有空闲会话
    }

    // 从空闲列表中取出一个会话
    const SIIP session_id = pool->idle_sessions[pool->idle_num - 1];
    pool->idle_num--;

    // 添加到活跃列表
    pool->active_sessions[pool->active_num] = session_id;
    pool->active_num++;

    pthread_mutex_unlock(&pool->pool_mutex);

    if (session_id >= g_network_manager->pool_capacity) return SIZE_MAX;

    session_t* session = &g_network_manager->sesspool[session_id];

    pthread_mutex_lock(&session->session_mutex);
    if (session->state != SESS_IDLE) {
        pthread_mutex_unlock(&session->session_mutex);
        return SIZE_MAX;
    }

    session->socket_fd = socket_fd;

    // 分配会话ID
    session->session_id = Idalloc.get_sid();
    if (session->session_id == merr) {
        pthread_mutex_unlock(&session->session_mutex);
        return SIZE_MAX;
    }

    // 复制客户端地址
    if (client_addr) {
        if (client_addr->sa_family == AF_INET) {
            memcpy(&session->client_addr, client_addr, sizeof(sockaddr_in));
        } else if (client_addr->sa_family == AF_INET6) {
            memcpy(&session->client_addr, client_addr, sizeof(sockaddr_in6));
        }
    }

    // 设置套接字为非阻塞
    if (set_socket_nonblocking_(socket_fd) != 0) {
        Idalloc.del_sid(session->session_id);
        pthread_mutex_unlock(&session->session_mutex);
        return SIZE_MAX;
    }

    // 初始化认证信息
    session->user_id = 65536;

    // 更新状态
    session->last_activity = time(NULL);
    session->state = SESS_ALIVE;

    pthread_mutex_unlock(&session->session_mutex);
    return session_id;
}
void init_sesspool_(network_manager_t* manager) {
    /* 初始化会话池，包括所有会话的状态、锁和缓冲区
     * 函数需要将会话标记为空闲（1）
     * 初始化会话锁和接收缓冲区锁（2）
     * 初始化缓冲区内存为默认大小（3）
     * 会话池的每一个空位都只要被执行一次 */
    for (size_t i = 0; i < manager->pool_capacity; i++) {
        session_t* session = &manager->sesspool[i];

        session->state = SESS_IDLE;// 标记为空会话
        session->session_id = 0; // 初始会话ID为0
        session->socket_fd = -1; // 初始套接字为-1
        
        // 初始化会话互斥锁
        if (pthread_mutex_init(&session->session_mutex, NULL) != 0) goto err;
        // 初始化接收缓冲区锁
        if (pthread_mutex_init(&session->recv_buffer.mutex, NULL) != 0) goto err;

        // 创建默认网络缓冲区
        session->recv_buffer.buffer = (uint8_t*)calloc(1, DEFAULT_BUFFER_SIZE);
        session->recv_buffer.capacity = DEFAULT_BUFFER_SIZE;
        session->recv_buffer.read_pos = 0;
        session->recv_buffer.write_pos = 0;
        if (!session->recv_buffer.buffer) goto err;

        // 设置空闲会话索引
        manager->idle_sessions[i] = i;
    }
    manager->idle_num = manager->pool_capacity;
    manager->active_num = 0;
    return;

    err:
    printf("init_sesspool_ failed!\n");
    system("read -p '按回车键继续...'");
    exit(1);
}
