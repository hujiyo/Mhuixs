#include "session.h"

/*
 * Mhuixs 数据库会话管理模块实现 (纯C版本)
 * 
 * 版权所有 (c) Mhuixs-team 2025
 * Email: hj18914255909@outlook.com
 */

// 全局变量
static network_manager_t* g_network_manager = NULL;//网络管理器
static session_pool_t* g_session_pool = NULL;//会话池
static pthread_mutex_t g_global_mutex = PTHREAD_MUTEX_INITIALIZER;//全局互斥锁

//=============================================================================
// 命令队列实现
//=============================================================================

command_queue_t* create_command_queue(uint32_t max_size) {
    command_queue_t* queue = calloc(1,sizeof(command_queue_t));
    if (!queue) return NULL;
    queue->max_size = max_size;
    
    if (pthread_mutex_init(&queue->mutex, NULL) != 0) {
        free(queue);
        return NULL;
    }
    
    if (pthread_cond_init(&queue->cond, NULL) != 0) {
        pthread_mutex_destroy(&queue->mutex);
        free(queue);
        return NULL;
    }
    return queue;
}

void destroy_command_queue(command_queue_t* queue) {
    /*
     *确保：
     *没有线程会再使用这个队列
     *没有线程正阻塞在条件变量 `queue->cond`
     *
     *所以销毁前必须先调用command_queue_shutdown函数
     */
    if (!queue) return;
    //先清空队列
    pthread_mutex_lock(&queue->mutex);
    command_t* current = queue->head;
    while (current) {
        command_t* next = current->next;
        command_destroy(current);
        current = next;
    }
    queue->head = NULL;
    queue->tail = NULL;
    queue->count = 0;
    pthread_mutex_unlock(&queue->mutex);
    pthread_mutex_destroy(&queue->mutex);//删除锁之前先解锁
    pthread_cond_destroy(&queue->cond);
    free(queue);
}

int command_queue_push(command_queue_t* queue, command_t* cmd) {
    if (!queue || !cmd) return SESS_INVALID;
    
    pthread_mutex_lock(&queue->mutex);
    
    if (queue->shutdown) {
        pthread_mutex_unlock(&queue->mutex);
        return SESS_ERROR;
    }
    
    if (queue->count >= queue->max_size) {
        pthread_mutex_unlock(&queue->mutex);
        return SESS_FULL;
    }
    
    // 按优先级插入（简化实现：优先级高的插在前面）
    if (!queue->head || cmd->priority > queue->head->priority) {
        cmd->next = queue->head;
        queue->head = cmd;
        if (!queue->tail) queue->tail = cmd;
    } else {
        command_t* current = queue->head;
        while (current->next && current->next->priority >= cmd->priority) {
            current = current->next;
        }
        cmd->next = current->next;
        current->next = cmd;
        if (!cmd->next) queue->tail = cmd;
    }
    
    queue->count++;
    pthread_cond_signal(&queue->cond);
    pthread_mutex_unlock(&queue->mutex);
    
    return SESS_OK;
}
command_t* command_queue_pop(command_queue_t* queue, uint32_t timeout_ms) {
    if (!queue) return NULL;
    
    pthread_mutex_lock(&queue->mutex);
    
    struct timespec ts;
    if (timeout_ms > 0) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        ts.tv_sec = tv.tv_sec + timeout_ms / 1000;
        ts.tv_nsec = (tv.tv_usec + (timeout_ms % 1000) * 1000) * 1000;
        if (ts.tv_nsec >= 1000000000) {
            ts.tv_sec++;
            ts.tv_nsec -= 1000000000;
        }
    }
    
    while (!queue->head && !queue->shutdown) {
        if (timeout_ms > 0) {
            if (pthread_cond_timedwait(&queue->cond, &queue->mutex, &ts) != 0) {
                break; // 超时
            }
        } else {
            pthread_cond_wait(&queue->cond, &queue->mutex);
        }
    }
    
    command_t* cmd = NULL;
    if (queue->head) {
        cmd = queue->head;
        queue->head = cmd->next;
        if (!queue->head) queue->tail = NULL;
        cmd->next = NULL;
        queue->count--;
    }
    
    pthread_mutex_unlock(&queue->mutex);
    return cmd;
}
command_t* command_queue_try_pop(command_queue_t* queue) {
    if (!queue) return NULL;
    
    pthread_mutex_lock(&queue->mutex);
    
    command_t* cmd = NULL;
    if (queue->head) {
        cmd = queue->head;
        queue->head = cmd->next;
        if (!queue->head) queue->tail = NULL;
        cmd->next = NULL;
        queue->count--;
    }
    
    pthread_mutex_unlock(&queue->mutex);
    return cmd;
}



void command_queue_shutdown(command_queue_t* queue) {
    if (!queue) return;
    
    pthread_mutex_lock(&queue->mutex);
    queue->shutdown = 1;
    pthread_cond_broadcast(&queue->cond);
    pthread_mutex_unlock(&queue->mutex);
}

uint32_t command_queue_size(command_queue_t* queue) {
    if (!queue) return 0;
    
    pthread_mutex_lock(&queue->mutex);
    uint32_t size = queue->count;
    pthread_mutex_unlock(&queue->mutex);
    
    return size;
}

//=============================================================================
// 命令操作实现
//=============================================================================

command_t* command_create(const CommandNumber cmd_id,const uint32_t seq_num,const uint32_t priority,
                         const uint8_t* data,const uint32_t data_len) {
    command_t* cmd = malloc(sizeof(command_t));
    if (!cmd) return NULL;
    
    memset(cmd, 0, sizeof(command_t));
    cmd->command_id = cmd_id;
    cmd->sequence_num = seq_num;
    cmd->priority = priority;
    cmd->data_len = data_len;
    cmd->timestamp = time(NULL);
    
    if (data_len > 0 && data) {
        cmd->data = (uint8_t*)malloc(data_len);
        if (!cmd->data) {
            free(cmd);
            return NULL;
        }
        memcpy(cmd->data, data, data_len);
    }
    
    return cmd;
}

void command_destroy(command_t* cmd) {
    if (!cmd) return;
    
    if (cmd->data) {
        free(cmd->data);
    }
    free(cmd);
}

//=============================================================================
// 网络缓冲区实现
//=============================================================================

buffer_struct* create_network_buffer(uint32_t initial_capacity) {
    buffer_struct* buffer = malloc(sizeof(buffer_struct));
    if (!buffer) return NULL;
    
    buffer->buffer = (uint8_t*)malloc(initial_capacity);
    if (!buffer->buffer) {
        free(buffer);
        return NULL;
    }
    
    buffer->capacity = initial_capacity;
    buffer->read_pos = 0;
    buffer->write_pos = 0;
    
    if (pthread_mutex_init(&buffer->mutex, NULL) != 0) {
        free(buffer->buffer);
        free(buffer);
        return NULL;
    }
    
    return buffer;
}

void destroy_network_buffer(buffer_struct* buffer) {
    if (!buffer) return;
    
    pthread_mutex_destroy(&buffer->mutex);
    if (buffer->buffer) {
        free(buffer->buffer);
    }
    free(buffer);
}

int buffer_write(buffer_struct* buffer, const uint8_t* data, uint32_t len) {
    if (!buffer || !data || len == 0) return SESS_INVALID;
    
    pthread_mutex_lock(&buffer->mutex);
    
    // 检查是否需要扩容
    if (buffer->write_pos + len > buffer->capacity) {
        if (buffer->read_pos > 0) {
            // 先尝试压缩
            uint32_t data_size = buffer->write_pos - buffer->read_pos;
            if (data_size > 0) {
                memmove(buffer->buffer, buffer->buffer + buffer->read_pos, data_size);
            }
            buffer->write_pos = data_size;
            buffer->read_pos = 0;
        }
        
        if (buffer->write_pos + len > buffer->capacity) {
            // 仍然不够，需要扩容
            uint32_t new_capacity = (buffer->capacity * 2 > buffer->write_pos + len) ? 
                                   buffer->capacity * 2 : buffer->write_pos + len;
            uint8_t* new_buffer = (uint8_t*)realloc(buffer->buffer, new_capacity);
            if (!new_buffer) {
                pthread_mutex_unlock(&buffer->mutex);
                return SESS_ERROR;
            }
            buffer->buffer = new_buffer;
            buffer->capacity = new_capacity;
        }
    }
    
    memcpy(buffer->buffer + buffer->write_pos, data, len);
    buffer->write_pos += len;
    
    pthread_mutex_unlock(&buffer->mutex);
    return SESS_OK;
}

uint32_t buffer_read(buffer_struct* buffer, uint8_t* data, uint32_t max_len) {
    if (!buffer || !data || max_len == 0) return 0;
    
    pthread_mutex_lock(&buffer->mutex);
    
    uint32_t available_data = buffer->write_pos - buffer->read_pos;
    uint32_t to_read = (max_len < available_data) ? max_len : available_data;
    
    if (to_read > 0) {
        memcpy(data, buffer->buffer + buffer->read_pos, to_read);
        buffer->read_pos += to_read;
    }
    
    pthread_mutex_unlock(&buffer->mutex);
    return to_read;
}

uint32_t buffer_peek(buffer_struct* buffer, uint8_t* data, uint32_t max_len) {
    if (!buffer || !data || max_len == 0) return 0;
    
    pthread_mutex_lock(&buffer->mutex);
    
    uint32_t available_data = buffer->write_pos - buffer->read_pos;
    uint32_t to_peek = (max_len < available_data) ? max_len : available_data;
    
    if (to_peek > 0) {
        memcpy(data, buffer->buffer + buffer->read_pos, to_peek);
    }
    
    pthread_mutex_unlock(&buffer->mutex);
    return to_peek;
}

uint32_t get_buffer_available_size(buffer_struct* buffer) {
    if (!buffer) return 0;
    
    pthread_mutex_lock(&buffer->mutex);
    uint32_t available = buffer->write_pos - buffer->read_pos;
    pthread_mutex_unlock(&buffer->mutex);
    
    return available;
}

void clear_buffer(buffer_struct* buffer) {
    if (!buffer) return;
    
    pthread_mutex_lock(&buffer->mutex);
    buffer->read_pos = 0;
    buffer->write_pos = 0;
    pthread_mutex_unlock(&buffer->mutex);
}

void compact_buffer(buffer_struct* buffer) {
    if (!buffer) return;
    
    pthread_mutex_lock(&buffer->mutex);
    
    if (buffer->read_pos > 0) {
        uint32_t data_size = buffer->write_pos - buffer->read_pos;
        if (data_size > 0) {
            memmove(buffer->buffer, buffer->buffer + buffer->read_pos, data_size);
        }
        buffer->write_pos = data_size;
        buffer->read_pos = 0;
    }
    
    pthread_mutex_unlock(&buffer->mutex);
}

//=============================================================================
// 会话操作实现
//=============================================================================

session_t* create_session(SID sid,int if_default,uint32_t max_buffer_size,
                            uint32_t max_command_queue_size,int if_disable_cpr) {
    session_t* session = malloc(sizeof(session_t));
    if (!session) return NULL;
    
    memset(session, 0, sizeof(session_t));
    session->session_id = sid;
    session->socket_fd = -1;
    session->state = SESS_IDLE;
    session->priority = PRIORITY_NORMAL;
    session->last_activity = time(NULL);

    // 复制配置
    if (if_default) {
        session->max_buffer_size = max_buffer_size;
        session->max_command_queue_size = max_command_queue_size;
        session->if_disable_cpr = if_disable_cpr;
    } else { // 默认配置
        session->max_buffer_size = 64 * 1024;
        session->max_command_queue_size = 1000;
        session->if_disable_cpr = 1;//默认不禁用自动压缩
    }
    
    // 初始化互斥锁
    if (pthread_mutex_init(&session->session_mutex, NULL) != 0) {
        free(session);
        return NULL;
    }
    
    // 创建命令队列
    session->command_queue = create_command_queue(session->max_command_queue_size);
    if (!session->command_queue) {
        pthread_mutex_destroy(&session->session_mutex);
        free(session);
        return NULL;
    }
    
    // 创建网络缓冲区
    session->recv_buffer = create_network_buffer(session->config.max_buffer_size / 2);
    session->send_buffer = create_network_buffer(session->config.max_buffer_size / 2);
    
    if (!session->recv_buffer || !session->send_buffer) {
        if (session->recv_buffer) destroy_network_buffer(session->recv_buffer);
        if (session->send_buffer) destroy_network_buffer(session->send_buffer);
        command_queue_destroy(session->command_queue);
        pthread_mutex_destroy(&session->session_mutex);
        free(session);
        return NULL;
    }
    
    // 初始化认证信息
    session->auth_info.is_guest = 1;
    session->auth_info.is_authenticated = 0;
    strcpy(session->auth_info.username, "guest");
    
    return session;
}

void destroy_session(session_t* session) {
    if (!session) return;
    
    session_shutdown(session);
    
    if (session->command_queue) {
        command_queue_destroy(session->command_queue);
    }
    if (session->recv_buffer) {
        destroy_network_buffer(session->recv_buffer);
    }
    if (session->send_buffer) {
        destroy_network_buffer(session->send_buffer);
    }
    
    pthread_mutex_destroy(&session->session_mutex);
    free(session);
}

int session_initialize(session_t* session, int socket_fd, const struct sockaddr* client_addr, UID uid) {
    if (!session) return SESS_INVALID;
    
    pthread_mutex_lock(&session->session_mutex);
    
    if (session->state != SESS_IDLE) {
        pthread_mutex_unlock(&session->session_mutex);
        return SESS_ERROR;
    }
    
    session->socket_fd = socket_fd;
    
    // 复制客户端地址
    if (client_addr) {
        if (client_addr->sa_family == AF_INET) {
            memcpy(&session->client_addr, client_addr, sizeof(struct sockaddr_in));
        } else if (client_addr->sa_family == AF_INET6) {
            memcpy(&session->client_addr, client_addr, sizeof(struct sockaddr_in6));
        }
    }
    
    // 设置套接字为非阻塞
    if (set_socket_nonblocking(socket_fd) != 0) {
        pthread_mutex_unlock(&session->session_mutex);
        return SESS_ERROR;
    }
    
    // 重置统计信息
    memset(&session->stats, 0, sizeof(session_stats_t));
    session->stats.created_time = time(NULL);
    session->stats.last_activity = session->stats.created_time;
    
    // 清理命令队列和缓冲区
    command_queue_clear(session->command_queue);
    clear_buffer(session->recv_buffer);
    clear_buffer(session->send_buffer);
    
    // 初始化认证信息
    session->auth_info.user_id = uid;
    if (uid > 0) {
        session->auth_info.is_guest = 0;
    }
    
    // 更新状态
    session->state = SESS_ALIVE;
    session->priority = PRIORITY_NORMAL;
    session->revisit_count = 0;
    
    session->created_time = time(NULL);
    session->last_activity = session->created_time;
    
    pthread_mutex_unlock(&session->session_mutex);
    return SESS_OK;
}

void session_shutdown(session_t* session) {
    if (!session) return;
    
    pthread_mutex_lock(&session->session_mutex);
    
    if (session->state == SESS_IDLE) {
        pthread_mutex_unlock(&session->session_mutex);
        return;
    }
    
    session->state = SESS_CLOSING;
    
    // 关闭命令队列
    if (session->command_queue) {
        command_queue_shutdown(session->command_queue);
    }
    
    // 发送缓冲区中的剩余数据
    session_send_data(session);
    
    // 关闭套接字
    if (session->socket_fd >= 0) {
        close(session->socket_fd);
        session->socket_fd = -1;
    }
    
    // 清空地址
    memset(&session->client_addr, 0, sizeof(session->client_addr));
    
    // 清空加密密钥
    memset(session->aes_key, 0, sizeof(session->aes_key));
    
    session->state = SESS_IDLE;
    
    pthread_mutex_unlock(&session->session_mutex);
}

void session_reset(session_t* session) {
    if (!session) return;
    
    session_shutdown(session);
    
    pthread_mutex_lock(&session->session_mutex);
    
    // 重置状态
    session->state = SESS_IDLE;
    session->priority = PRIORITY_NORMAL;
    session->revisit_count = 0;
    
    // 重置认证信息
    memset(&session->auth_info, 0, sizeof(user_auth_info_t));
    session->auth_info.is_guest = 1;
    strcpy(session->auth_info.username, "guest");
    
    // 清理缓冲区和队列
    if (session->command_queue) {
        command_queue_clear(session->command_queue);
    }
    if (session->recv_buffer) {
        clear_buffer(session->recv_buffer);
    }
    if (session->send_buffer) {
        clear_buffer(session->send_buffer);
    }
    
    // 重置统计信息
    memset(&session->stats, 0, sizeof(session_stats_t));
    
    pthread_mutex_unlock(&session->session_mutex);
}

//=============================================================================
// 工具函数实现
//=============================================================================

int set_socket_nonblocking(int sockfd) {
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1) {
        return SESS_ERROR;
    }
    
    if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {
        return SESS_ERROR;
    }
    
    return SESS_OK;
}

const char* session_state_to_string(session_state_t state) {
    switch (state) {
        case SESS_IDLE: return "IDLE";
        case SESS_ALIVE: return "ALIVE";
        case SESS_SLEEPING: return "SLEEPING";
        case SESS_BUSY: return "BUSY";
        case SESS_SUPER: return "SUPER";
        case SESS_CLOSING: return "CLOSING";
        case SESS_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

const char* session_priority_to_string(session_priority_t priority) {
    switch (priority) {
        case PRIORITY_LOWEST: return "LOWEST";
        case PRIORITY_LOW: return "LOW";
        case PRIORITY_NORMAL: return "NORMAL";
        case PRIORITY_HIGH: return "HIGH";
        case PRIORITY_HIGHEST: return "HIGHEST";
        case PRIORITY_SUPER: return "SUPER";
        default: return "UNKNOWN";
    }
} 