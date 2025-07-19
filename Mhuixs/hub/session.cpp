#include "session.h"
#include "manager.h"

// 全局变量
extern network_manager_t* g_network_manager;//网络管理器
extern volatile int running_flag;//网络系统运行标志

// 响应线程管理结构
typedef struct response_thread_info {
    pthread_t thread_id;         // 子线程ID
    response_t* response;        // 关联的响应
    volatile int active;         // 线程是否活跃
    volatile int shutdown;       // 关闭标志
} response_thread_info_t;


extern Id_alloctor Idalloc;

////本地函数////
SIIP alloc_with_socket_(int socket_fd, const sockaddr* client_addr);//分配会话 epoll线程使用
void destroy_session_on_(session_t* session);//销毁会话
int create_session_on_(SIIP siip, int socket_fd, const sockaddr* client_addr);//初始化会话
//_create_session_on执行失败后，会话仍然是IDLE状态，不需要处理会话释放问题，但别忘释放套接字
void shutdown_session_on_(session_t* session);//关闭会话
int try_get_session_ownership_(SIIP siid);//按会话SIID尝试获得所有权
int release_session_ownership_(SIIP siid);//按会话SIID释放所有权
void compact_buffer_(buffer_struct* buffer);//压缩网络缓冲区
void clear_buffer_(buffer_struct* buffer);//清空网络缓冲区
uint32_t get_buffer_available_size(buffer_struct* buffer);//获取网络缓冲区可用空间
// 网络缓冲区操作
int buffer_write_(buffer_struct buffer, const uint8_t* data, uint32_t len);//将数据写入网络缓冲区
uint32_t buffer_read_(buffer_struct* buffer, uint8_t* data, uint32_t max_len);//从网络缓冲区中读取数据
uint32_t buffer_peek_(buffer_struct* buffer, uint8_t* data, uint32_t max_len);//查看网络缓冲区数据
// 会话网络操作
int session_receive_data_(session_t* session);//接收数据
int session_send_data_(session_t* session);//发送数据
int session_process_incoming_packets_(session_t* session);//处理入站数据包

//缓冲区操作函数
int buffer_write_(buffer_struct buffer, const uint8_t* data, uint32_t len) {
    if (!data || len == 0) return SESS_INVALID;
    
    pthread_mutex_lock(&buffer.mutex);
    
    // 检查是否需要扩容
    if (buffer.write_pos + len > buffer.capacity) {
        if (buffer.read_pos > 0) {
            // 先尝试压缩
            uint32_t data_size = buffer.write_pos - buffer.read_pos;
            if (data_size > 0) {
                memmove(buffer.buffer, buffer.buffer + buffer.read_pos, data_size);
            }
            buffer.write_pos = data_size;
            buffer.read_pos = 0;
        }
        
        if (buffer.write_pos + len > buffer.capacity) {
            // 仍然不够，需要扩容
            uint32_t new_capacity = (buffer.capacity * 2 > buffer.write_pos + len) ?
                                   buffer.capacity * 2 : buffer.write_pos + len;
            uint8_t* new_buffer = (uint8_t*)realloc(buffer.buffer, new_capacity);
            if (!new_buffer) {
                pthread_mutex_unlock(&buffer.mutex);
                return SESS_ERR;
            }
            buffer.buffer = new_buffer;
            buffer.capacity = new_capacity;
        }
    }
    
    memcpy(buffer.buffer + buffer.write_pos, data, len);
    buffer.write_pos += len;
    
    pthread_mutex_unlock(&buffer.mutex);
    return SESS_OK;
}
uint32_t buffer_read_(buffer_struct* buffer, uint8_t* data, uint32_t max_len) {
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
uint32_t buffer_peek_(buffer_struct* buffer, uint8_t* data, uint32_t max_len) {
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
void clear_buffer_(buffer_struct* buffer) {
    if (!buffer) return;
    
    pthread_mutex_lock(&buffer->mutex);
    buffer->read_pos = 0;
    buffer->write_pos = 0;
    pthread_mutex_unlock(&buffer->mutex);
}

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
void compact_buffer_(buffer_struct* buffer) {
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
//会话相关函数

void destroy_session_on_(session_t* session) {
    //必须保证地址的有效性
    shutdown_session_on_(session);
    //清空接收缓冲区
    pthread_mutex_destroy(&session->recv_buffer.mutex);
    if (session->recv_buffer.buffer) {
        free(session->recv_buffer.buffer);
    }
    pthread_mutex_destroy(&session->session_mutex);
}
void shutdown_session_on_(session_t* session) {
    pthread_mutex_lock(&session->session_mutex);
    if (session->state == SESS_IDLE) {
        pthread_mutex_unlock(&session->session_mutex);
        return;
    }

    // 关闭套接字 客户端断开连接
    if (session->socket_fd >= 0) {
        close(session->socket_fd);
        session->socket_fd = -1;
    }

    session->state = SESS_IDLE;//标记为空闲，内存清理的事情交给下一个使用者
    pthread_mutex_unlock(&session->session_mutex);
}


int create_session_on_(SIIP siip, int socket_fd, const sockaddr* client_addr) {
    if (siip>=g_network_manager->pool_capacity) return -1;
    session_t* session = &g_network_manager->sesspool[siip];

    pthread_mutex_lock(&session->session_mutex);
    if (session->state != SESS_IDLE) { //检查是否是空的
        pthread_mutex_unlock(&session->session_mutex);
        return SESS_ERR;
    }

    session->socket_fd = socket_fd;

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
        pthread_mutex_unlock(&session->session_mutex);
        return SESS_ERR;
    }

    // 清理命令队列和缓冲区
    clear_buffer_(&session->recv_buffer);

    // 初始化认证信息
    session->user_id = 65536;
    // 更新状态
    session->last_activity = time(NULL);
    session->state = SESS_ALIVE;

    pthread_mutex_unlock(&session->session_mutex);
    return SESS_OK;
}


SIIP alloc_with_socket_(int socket_fd, const sockaddr* client_addr) {
    network_manager_t* pool = g_network_manager;
    if (!pool )return SIZE_MAX;

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

    // 更新统计
    pool->active_sessions++;
    pool->idle_sessions--;

    pthread_mutex_unlock(&pool->pool_mutex);

    if (create_session_on_(session_id, socket_fd, client_addr) != SESS_OK) {
        return SIZE_MAX;
    }
    return session_id;
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



int session_receive_data_(session_t* session) {
    if (!session || session->socket_fd < 0) return SESS_INVALID;

    uint8_t buffer[8192];
    const ssize_t bytes_received = recv(session->socket_fd, buffer, sizeof(buffer), MSG_DONTWAIT);

    if (bytes_received > 0) {
        // 写入接收缓冲区
        if (buffer_write_(session->recv_buffer, buffer, bytes_received) == SESS_OK) {
            session->last_activity = time(NULL);
            return bytes_received;
        }
        return SESS_ERROR;
    }
    else if (bytes_received == 0) {
        // 连接关闭
        return SESS_ERROR;
    }
    else {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0; // 暂时无数据
        }
        return SESS_ERROR;
    }
}

int session_send_data_(session_t* session) {
    if (!session || session->socket_fd < 0) return SESS_INVALID;

    uint32_t available = get_buffer_available_size(&session->send_buffer);
    if (available == 0) return 0;

    uint8_t buffer[8192];
    uint32_t to_send = (available > sizeof(buffer)) ? sizeof(buffer) : available;

    uint32_t bytes_read = buffer_read_(&session->send_buffer, buffer, to_send);
    if (bytes_read == 0) return 0;

    ssize_t bytes_sent = send(session->socket_fd, buffer, bytes_read, MSG_DONTWAIT);

    if (bytes_sent > 0) {
        session->last_activity = time(NULL);

        // 如果没有发送完全，需要将剩余数据放回缓冲区
        if (bytes_sent < bytes_read) {
            // 简化处理：重新写入未发送的数据
            buffer_write_(session->send_buffer, buffer + bytes_sent, bytes_read - bytes_sent);
        }

        return bytes_sent;
    }
    if (bytes_sent == 0) {
        return 0;
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // 重新写入数据
        buffer_write_(session->send_buffer, buffer, bytes_read);
        return 0;
    }
    return SESS_ERROR;
}

int session_process_incoming_packets_(session_t* session) {
    if (!session) return SESS_INVALID;

    // 使用新的pkg库处理数据包
    uint32_t available = get_buffer_available_size(&session->recv_buffer);
    if (available < PACKET_HEADER_SIZE) return 0;

    uint8_t buffer[8192];
    uint32_t bytes_read = buffer_peek_(&session->recv_buffer, buffer, sizeof(buffer));

    // 查找完整的数据包
    uint32_t start_index, packet_size;
    if (find_packet_boundary(buffer, bytes_read, &start_index, &packet_size)) {
        // 解包
        str unpacked_data = unpacking(buffer + start_index, packet_size);
        
        if (unpacked_data.string && unpacked_data.len > 0) {
            // 从解包后的数据中提取命令信息
            // 假设数据格式为：command_id(4字节) + sequence(4字节) + 用户数据
            if (unpacked_data.len >= 8) {
                uint32_t command_id, sequence;
                memcpy(&command_id, unpacked_data.string, 4);
                memcpy(&sequence, unpacked_data.string + 4, 4);
                
                // 转换为主机字节序
                command_id = ntohl(command_id);
                sequence = ntohl(sequence);
                
                // 创建命令
                command_t* cmd = create_command(
                    session->user_id,
                    (CommandNumber)command_id,
                    sequence,
                    1, // 默认优先级
                    unpacked_data.string + 8,
                    unpacked_data.len - 8
                );

                if (cmd) {
                    // 将命令加入队列
                    if (push_command(cmd) == SESS_OK) {
                        // 从缓冲区移除已处理的数据
                        uint8_t temp_buffer[8192];
                        buffer_read_(&session->recv_buffer, temp_buffer, start_index + packet_size);
                        
                        // 释放解包后的数据
                        free(unpacked_data.string);
                        return 1;
                    } else {
                        destroy_command(cmd);
                    }
                }
            }
            
            // 释放解包后的数据
            free(unpacked_data.string);
        }
    }

    return 0;
}

int send_response_to_session(SID session_id, const uint8_t* response_data, uint32_t response_len) {
    // 通过session_id查找socket_fd
    session_t* session = get_session(session_id);
    if (!session || !response_data || response_len == 0) return SESS_INVALID;

    // 创建响应结构体
    response_t* response = (response_t*)calloc(1, sizeof(response_t));
    if (!response) return SESS_ERROR;

    response->socket_fd = session->socket_fd;
    response->session = session_id;
    response->response_len = response_len;
    response->sent_len = 0;
    response->retry_count = 0;
    response->status = RESP_PENDING;

    // 复制数据
    if (response_len < 48) {
        memcpy(response->inline_data, response_data, response_len);
    } else {
        response->data = (uint8_t*)malloc(response_len);
        if (!response->data) {
            free(response);
            return SESS_ERROR;
        }
        memcpy(response->data, response_data, response_len);
    }

    // 加入响应队列
    response_queue.enqueue(response);

    return SESS_OK;
}



int auth_session(SID session_id, UID uid) {
    //UID成员只由初始化阶段和回收阶段的epoll线程和执行线程通过本函数修改，且二者时间线上岔开，不用考虑竞争
    session_t* sesspool = g_network_manager->sesspool;
    pthread_mutex_lock(&g_network_manager->pool_mutex);
    for (size_t i = 0;i<g_network_manager->active_num;i++) {
        session_t* session = &sesspool[g_network_manager->active_sessions[i]];
        if (session->session_id == session_id &&
            session->state != SESS_IDLE) {
            pthread_mutex_unlock(&g_network_manager->pool_mutex);
            if (pthread_mutex_trylock(&session->session_mutex) == EBUSY) {
                return SESS_ERR;
            }
            pthread_mutex_unlock(&session->session_mutex);
            session->user_id = uid;//认证成功
            return SESS_OK;
        }
    }
    return SESS_INVALID;
}

