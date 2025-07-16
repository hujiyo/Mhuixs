#include "session.h"

// 全局变量
extern Id_alloctor Idalloc;
static network_manager_t* g_network_manager = NULL;//网络管理器

////本地函数////
int init_session_on_(session_t* session);//初始化会话
void destroy_session_on_(session_t* session);//销毁会话
int create_session_on_(session_t* session, int socket_fd, const sockaddr* client_addr);//初始化会话
//_create_session_on执行失败后，会话仍然是IDLE状态，不需要处理会话释放问题，但别忘释放套接字
void shutdown_session_on_(session_t* session);//关闭会话
session_t* alloc_session_(network_manager_t* pool);//从空闲池中分配新会话
int network_manager_init_(network_manager_t* manager);//初始化网络管理器
session_t* try_get_session_ownership_(SIIP siid);//按会话SIID尝试获得所有权
int release_session_ownership_(SIIP siid);//按会话SIID释放所有权
int set_socket_nonblocking_(int sockfd);//设置套接字非阻塞
void destroy_network_manager_(network_manager_t* manager);//销毁网络管理模块

int set_socket_nonblocking_(int sockfd) {
    const int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1) {
        return SESS_ERROR;
    }

    if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {
        return SESS_ERROR;
    }

    return SESS_OK;
}
//缓冲区操作函数
int buffer_write(buffer_struct buffer, const uint8_t* data, uint32_t len) {
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
                return SESS_ERROR;
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
//会话相关函数
int init_session_on_(session_t* session) {
    //必须保证地址的有效性
    session->session_id = Idalloc.get_sid();
    session->socket_fd = -1;
    session->state = SESS_IDLE;
    session->last_activity = time(NULL);
    
    // 初始化互斥锁
    if (pthread_mutex_init(&session->session_mutex, NULL) != 0) {
        free(session);
        return SESS_ERR;
    }
    
    // 创建网络缓冲区
    session->recv_buffer.buffer = (uint8_t*)calloc(1,DEFAULT_BUFFER_SIZE);
    session->send_buffer.buffer = (uint8_t*)calloc(1,DEFAULT_BUFFER_SIZE);
    if (!session->recv_buffer.buffer ||
        pthread_mutex_init(&session->recv_buffer.mutex, NULL) != 0)   {
        free(session->send_buffer.buffer);
        pthread_mutex_destroy(&session->session_mutex);
        free(session);
        return SESS_ERR;
    }
    if (!session->send_buffer.buffer||
        pthread_mutex_init(&session->send_buffer.mutex, NULL) != 0)   {
        free(session->recv_buffer.buffer);
        pthread_mutex_destroy(&session->session_mutex);
        free(session);
        return SESS_ERR;
    }
    session->recv_buffer.capacity = DEFAULT_BUFFER_SIZE;
    session->send_buffer.capacity = DEFAULT_BUFFER_SIZE;
    
    // 初始化认证信息
    session->user_id = 65536;
    return SESS_OK;
}
void destroy_session_on_(session_t* session) {
    //必须保证地址的有效性
    shutdown_session_on_(session);
    //清空接收缓冲区
    pthread_mutex_destroy(&session->recv_buffer.mutex);
    if (session->recv_buffer.buffer) {
        free(session->recv_buffer.buffer);
    }
    //清空发送缓冲区
    pthread_mutex_destroy(&session->send_buffer.mutex);
    if (session->send_buffer.buffer) {
        free(session->send_buffer.buffer);
    }
    pthread_mutex_destroy(&session->session_mutex);
}
int create_session_on_(session_t* session, int socket_fd, const sockaddr* client_addr) {
    pthread_mutex_lock(&session->session_mutex);
    
    if (session->state != SESS_IDLE) { //检查是否是空的
        pthread_mutex_unlock(&session->session_mutex);
        return SESS_ERROR;
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
        return SESS_ERROR;
    }

    // 重置统计信息
    session->last_activity = time(NULL);
    
    // 清理命令队列和缓冲区
    clear_buffer(&session->recv_buffer);
    clear_buffer(&session->send_buffer);
    
    // 初始化认证信息
    session->user_id = 65536;
    // 更新状态
    session->state = SESS_ALIVE;
    session->last_activity = time(NULL);
    
    pthread_mutex_unlock(&session->session_mutex);
    return SESS_OK;
}
void shutdown_session_on_(session_t* session) {
    //必须保证地址的有效性
    pthread_mutex_lock(&session->session_mutex);
    if (session->state == SESS_IDLE) {
        pthread_mutex_unlock(&session->session_mutex);
        return;
    }
    session->state = SESS_CLOSING;

    session_send_data(session);// 发送缓冲区中的剩余数据 //////////
    ////////////////////////////////缓冲区重置
    
    // 关闭套接字 客户端断开连接
    if (session->socket_fd >= 0) {
        close(session->socket_fd);
        session->socket_fd = -1;
    }
    
    session->state = SESS_IDLE;//标记为空闲，内存清理的事情交给下一个使用者
    pthread_mutex_unlock(&session->session_mutex);
}

// “会话清理”-线程函数
static void* cleanup_thread(void* arg) {
    network_manager_t* manager = (network_manager_t*)arg;
    if (!manager) return NULL;
    while (!manager->shutdown_requested) {
        sleep(CLEANUP_INTERVAL);
        const time_t current_time = time(NULL);

        pthread_mutex_lock(&manager->pool_mutex);
        // 检查活跃会话
        for (SIIP i = 0; i < manager->active_num; ) {
            const SIIP session_idx = manager->active_sessions[i];
            session_t* session_to_check = &manager->sesspool[session_idx];

            if (session_to_check->socket_fd < 0||        //套接字异常
                session_to_check->state == SESS_ERROR || //会话出错
                session_to_check->state == SESS_IDLE ||  //会话被主动标记为空闲
                current_time - session_to_check->last_activity > TIMEOUT  //会话已经超时
            )   {
                shutdown_session_on_(&manager->sesspool[session_idx]);//关闭会话

                // 添加到空闲列表
                manager->idle_sessions[manager->idle_num] = session_idx;
                manager->idle_num++;

                // 从活跃列表中移除
                for (uint32_t j = i; j < manager->active_num - 1; j++) {
                    manager->active_sessions[j] = manager->active_sessions[j + 1];
                }
                manager->active_num--;// 不增加i，因为当前位置现在是下一个元素
                continue;
            }
            i++;
        }
        pthread_mutex_unlock(&manager->pool_mutex);
    }
    return NULL;
}

session_t* alloc_session_(network_manager_t* pool) {
    pthread_mutex_lock(&pool->pool_mutex);
    if (pool->idle_num == 0) {
        pthread_mutex_unlock(&pool->pool_mutex);
        return NULL; // 没有空闲会话
    }

    // 从空闲列表中取出一个会话
    const uint32_t session_id = pool->idle_sessions[pool->idle_num - 1];
    pool->idle_num--;

    // 添加到活跃列表
    pool->active_sessions[pool->active_num] = session_id;
    pool->active_num++;

    // 更新统计
    pool->active_sessions++;
    pool->idle_sessions--;

    session_t* session = &pool->sesspool[session_id];
    pthread_mutex_unlock(&pool->pool_mutex);
    return session;
}

session_t* alloc_with_socket(network_manager_t* pool, int socket_fd,
                                           const sockaddr* client_addr) {
    if (!pool)return NULL;
    session_t* session = alloc_session_(pool);
    if (!session) return NULL;

    if (create_session_on_(session, socket_fd, client_addr) != SESS_OK) {
        return NULL;
    }
    return session;
}

session_t* try_get_session_ownership_(SIIP siid) {
    //获得非空闲对话的所有权
    if (siid >= Env.max_sessions )return NULL;
    session_t* session = &g_network_manager->sesspool[siid];
    if (session->state == SESS_IDLE ||
        pthread_mutex_trylock(&session->session_mutex) == EBUSY
        )return NULL;
    return session;
}
int release_session_ownership_(SIIP siid) {
    //释放对话的所有权
    if (siid >= Env.max_sessions) return SESS_INVALID;
    session_t* session = &g_network_manager->sesspool[siid];
    pthread_mutex_unlock(&session->session_mutex);// 释放会话锁
    return 0;
}

// 网络线程函数
//监听新连接、接收数据、发送数据、处理连接关闭和异常
static void* network_thread_func(void* arg) {
    network_manager_t* manager = (network_manager_t*)arg;

    epoll_event events[EPOLL_MAX_EVENTS];

    while (manager->running) {
        int nfds = epoll_wait(manager->epoll_fd, events, EPOLL_MAX_EVENTS,
                             EPOLL_TIMEOUT);
        if (nfds == -1) {
            if (errno == EINTR) continue;
            break;
        }__gthread_once()

        for (int i = 0; i < nfds; i++) {
            epoll_event* event = &events[i];

            if (event->data.fd == manager->listen_fd) {
                // 新连接
                sockaddr_storage client_addr;
                socklen_t addr_len = sizeof(client_addr);
                int client_fd = accept(manager->listen_fd, (sockaddr*)&client_addr, &addr_len);

                if (client_fd >= 0) {
                    // 检查连接限制
                    char client_ip[INET6_ADDRSTRLEN];
                    if (client_addr.ss_family == AF_INET) {
                        sockaddr_in* addr_in = (sockaddr_in*)&client_addr;
                        inet_ntop(AF_INET, &addr_in->sin_addr, client_ip, INET_ADDRSTRLEN);
                    } else {
                        sockaddr_in6* addr_in6 = (sockaddr_in6*)&client_addr;
                        inet_ntop(AF_INET6, &addr_in6->sin6_addr, client_ip, INET6_ADDRSTRLEN);
                    }

                    // 分配会话
                    session_t* session = alloc_with_socket(manager,client_fd,(struct sockaddr*)&client_addr);

                    if (session) {
                        // 设置为非阻塞
                        set_socket_nonblocking_(client_fd);
                        // 添加到epoll
                        epoll_event ev;
                        ev.events = EPOLLIN | EPOLLET;
                        ev.data.fd = client_fd;

                        if (epoll_ctl(manager->epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) != 0) {
                            recycle_session(manager, session->session_id);
                            close(client_fd);
                        }
                    } else {
                        close(client_fd);
                    }
                }
            } else {
                // 数据事件
                int client_fd = event->data.fd;

                // 通过文件描述符查找会话
                session_t* session = NULL;
                pthread_mutex_lock(&manager->pool_mutex);

                for (uint32_t j = 0; j < manager->active_num; j++) {
                    uint32_t session_id = manager->active_sessions[j];
                    session_t* s = &manager->sesspool[session_id];
                    if (s->socket_fd == client_fd) {
                        session = s;
                        break;
                    }
                }

                pthread_mutex_unlock(&manager->pool_mutex);

                if (session) {
                    if (event->events & EPOLLIN) {
                        // 接收数据
                        if (session_receive_data(session) < 0) {
                            // 连接关闭或错误
                            epoll_ctl(manager->epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                            recycle_session(manager, session->session_id);
                        }
                    }

                    if (event->events & EPOLLOUT) {
                        // 发送数据
                        session_send_data(session);
                    }

                    if (event->events & (EPOLLHUP | EPOLLERR)) {
                        // 连接关闭或错误
                        epoll_ctl(manager->epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                        recycle_session(manager, session->session_id);
                    }
                }
            }
        }
    }
    return NULL;
}

// 工作线程函数
static void* worker_thread_func(void* arg) {
    network_manager_t* manager =(network_manager_t*)arg;

    while (manager->running) {
        // 使用epoll多路复用机制获取有待处理命令的会话
       // SIIP;

       // if (count == 0) {
       //     usleep(1000); // 1ms
       //     continue;
       // }network_thread_func

        // 处理会话中的数据包
        for (uint32_t i = 0; i < count; i++) {
            session_t* session = try_get_session_ownership_(session_idx[i]);
            if (session) {
                session_process_incoming_packets(session);
            }
        }
    }
    return NULL;
}

static int _init_sesspool(network_manager_t* manager) {
    //附属network_manager_init函数,用来初始化池相关部分的成员
    pthread_mutex_lock(&manager->pool_mutex);
    //  初始化所有会话为空闲状态
    for (uint32_t i = 0; i < Env.max_sessions; i++) {
        if (init_session_on_(&manager->sesspool[i]) == SESS_ERR) {
            pthread_mutex_unlock(&manager->pool_mutex);
            return SESS_ERR;
        }
        manager->idle_sessions[i] = i;
    }
    manager->idle_num = Env.max_sessions;
    manager->active_num = 0;
    pthread_mutex_unlock(&manager->pool_mutex);
    return SESS_OK;
}

int network_manager_start(network_manager_t* manager) {
    if (!manager || !manager->initialized) return SESS_INVALID;

    manager->running = 1;

    // 启动网络线程
    for (uint32_t i = 0; i < NETWORK_THREADS; i++) {
        if (pthread_create(&manager->network_threads[i], NULL, network_thread_func, manager) != 0) {
            manager->running = 0;
            return SESS_ERROR;
        }
    }

    // 启动工作线程
    for (uint32_t i = 0; i < WORKER_THREADS; i++) {
        if (pthread_create(&manager->worker_threads[i], NULL, worker_thread_func, manager) != 0) {
            manager->running = 0;
            return SESS_ERROR;
        }
    }

    return SESS_OK;
}

void network_manager_stop(network_manager_t* manager) {
    if (!manager) return;

    pthread_mutex_lock(&manager->pool_mutex);
    manager->shutdown_requested = 1;
    pthread_mutex_unlock(&manager->pool_mutex);

    // 等待线程结束
    pthread_join(manager->cleanup_thread, NULL);
    pthread_join(manager->health_check_thread, NULL);

    // 关闭所有活跃会话
    pthread_mutex_lock(&manager->pool_mutex);
    for (uint32_t i = 0; i < manager->active_num; i++) {
        const uint32_t session_id = manager->active_sessions[i];
        shutdown_session_on_(&manager->sesspool[session_id]);
    }
    pthread_mutex_unlock(&manager->pool_mutex);

    manager->running = 0;

    // 等待所有线程结束
    for (uint32_t i = 0; i < NETWORK_THREADS; i++) {
        pthread_join(manager->network_threads[i], NULL);
    }

    for (uint32_t i = 0; i < WORKER_THREADS; i++) {
        pthread_join(manager->worker_threads[i], NULL);
    }
}

int session_receive_data(session_t* session) {
    if (!session || session->socket_fd < 0) return SESS_INVALID;

    uint8_t buffer[8192];
    const ssize_t bytes_received = recv(session->socket_fd, buffer, sizeof(buffer), MSG_DONTWAIT);

    if (bytes_received > 0) {
        // 写入接收缓冲区
        if (buffer_write(session->recv_buffer, buffer, bytes_received) == SESS_OK) {
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

int session_send_data(session_t* session) {
    if (!session || session->socket_fd < 0) return SESS_INVALID;

    uint32_t available = get_buffer_available_size(&session->send_buffer);
    if (available == 0) return 0;

    uint8_t buffer[8192];
    uint32_t to_send = (available > sizeof(buffer)) ? sizeof(buffer) : available;

    uint32_t bytes_read = buffer_read(&session->send_buffer, buffer, to_send);
    if (bytes_read == 0) return 0;

    ssize_t bytes_sent = send(session->socket_fd, buffer, bytes_read, MSG_DONTWAIT);

    if (bytes_sent > 0) {
        session->last_activity = time(NULL);

        // 如果没有发送完全，需要将剩余数据放回缓冲区
        if (bytes_sent < bytes_read) {
            // 简化处理：重新写入未发送的数据
            buffer_write(session->send_buffer, buffer + bytes_sent, bytes_read - bytes_sent);
        }

        return bytes_sent;
    }
    if (bytes_sent == 0) {
        return 0;
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // 重新写入数据
        buffer_write(session->send_buffer, buffer, bytes_read);
        return 0;
    }
    return SESS_ERROR;
}

int session_process_incoming_packets(session_t* session) {
    if (!session) return SESS_INVALID;

    // 使用新的pkg库处理数据包
    uint32_t available = get_buffer_available_size(&session->recv_buffer);
    if (available < PACKET_HEADER_SIZE) return 0;

    uint8_t buffer[8192];
    uint32_t bytes_read = buffer_peek(&session->recv_buffer, buffer, sizeof(buffer));

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
                command_t* cmd = command_create(
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
                        buffer_read(&session->recv_buffer, temp_buffer, start_index + packet_size);
                        
                        // 释放解包后的数据
                        free(unpacked_data.string);
                        return 1;
                    } else {
                        command_destroy(cmd);
                    }
                }
            }
            
            // 释放解包后的数据
            free(unpacked_data.string);
        }
    }

    return 0;
}

int push_command(command_t* cmd) {
    if (!cmd) return SESS_INVALID;
    return command_queue_push(command_queue, cmd);
}

command_t* pop_command() {
    return command_queue_try_pop(command_queue);
}

int has_pending_commands(session_t* session) {
    if (!session) return 0;

    return command_queue_size(command_queue) > 0;
}

void session_set_state(session_t* session, session_state_t new_state) {
    if (!session) return;

    pthread_mutex_lock(&session->session_mutex);
    session->state = new_state;
    session->last_activity = time(NULL);
    pthread_mutex_unlock(&session->session_mutex);
}

int send_response_to_session(SID session_id, const uint8_t* response, uint32_t response_len) {
    session_t* session = get_session(session_id);
    if (!session || !response || response_len == 0) return SESS_INVALID;

    return buffer_write(session->send_buffer, response, response_len);
}

int auth_session(SID session_id, UID uid) {
    session_t* session = get_session(session_id);
    if (!session) return SESS_INVALID;

    pthread_mutex_lock(&session->session_mutex);

    session->user_id = uid;

    // 根据用户权限设置会话优先级
    if (uid == 1) { // 假设uid=1是管理员
        session->state = SESS_SUPER;
    }

    pthread_mutex_unlock(&session->session_mutex);

    return SESS_OK;
}

void invalidate_session_auth(const SID session_id) {
    session_t* session = get_session(session_id);
    if (!session) return;

    pthread_mutex_lock(&session->session_mutex);

    session->user_id = 65536;

    if (session->state == SESS_SUPER) {
        session->state = SESS_ALIVE;
    }

    pthread_mutex_unlock(&session->session_mutex);
}

int network_manager_init_(network_manager_t* manager) {
    if (_init_sesspool(manager))

    // 启动“死会话清理”-线程
    if (pthread_create(&manager->cleanup_thread, NULL, cleanup_thread, manager) != 0) {
        return SESS_ERROR;
    }

    // 创建监听套接字
    manager->listen_fd = socket(ADDRESS_FAMILY, SOCK_STREAM, 0);
    if (manager->listen_fd < 0) {
        return SESS_ERROR;
    }

    // 设置套接字选项
    int opt = 1;
    setsockopt(manager->listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 绑定地址
    sockaddr_storage bind_addr = {};

    if (ADDRESS_FAMILY == AF_INET) {
        sockaddr_in* addr_in = (sockaddr_in*)&bind_addr;
        addr_in->sin_family = AF_INET;
        addr_in->sin_addr.s_addr = INADDR_ANY;
        addr_in->sin_port = htons(PORT);

        if (bind(manager->listen_fd, (sockaddr*)&bind_addr, sizeof(sockaddr_in)) < 0) {
            close(manager->listen_fd);
            return SESS_ERROR;
        }
    }
    else {
        /*//目前不可到达的代码
        sockaddr_in6* addr_in6 = (sockaddr_in6*)&bind_addr;
        addr_in6->sin6_family = AF_INET6;
        addr_in6->sin6_addr = in6addr_any;
        addr_in6->sin6_port = htons(PORT);

        if (bind(manager->listen_fd, (sockaddr*)&bind_addr, sizeof(sockaddr_in6)) < 0) {
            close(manager->listen_fd);
            return SESS_ERROR;
        }
        */
    }

    // 开始监听
    if (listen(manager->listen_fd, LISTEN_BACKLOG) < 0) {
        close(manager->listen_fd);
        return SESS_ERROR;
    }

    // 设置为非阻塞
    set_socket_nonblocking_(manager->listen_fd);

    // 创建epoll
    manager->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (manager->epoll_fd < 0) {
        close(manager->listen_fd);
        return SESS_ERROR;
    }

    // 添加监听套接字到epoll
    epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = manager->listen_fd;

    if (epoll_ctl(manager->epoll_fd, EPOLL_CTL_ADD, manager->listen_fd, &ev) < 0) {
        close(manager->epoll_fd);
        close(manager->listen_fd);
        return SESS_ERROR;
    }

    // 分配线程数组
    manager->network_threads = (pthread_t*)malloc(NETWORK_THREADS * sizeof(pthread_t));
    manager->worker_threads = (pthread_t*)malloc(WORKER_THREADS * sizeof(pthread_t));

    if (!manager->network_threads || !manager->worker_threads) {
        if (manager->network_threads) free(manager->network_threads);
        if (manager->worker_threads) free(manager->worker_threads);
        close(manager->epoll_fd);
        close(manager->listen_fd);
        return SESS_ERROR;
    }

    manager->initialized = 1;

    return SESS_OK;
}

void destroy_network_manager_(network_manager_t* manager) {
    network_manager_stop(manager);

    if (manager->epoll_fd >= 0) {
        close(manager->epoll_fd);
        manager->epoll_fd = -1;
    }

    if (manager->listen_fd >= 0) {
        close(manager->listen_fd);
        manager->listen_fd = -1;
    }

    manager->initialized = 0;

    pthread_mutex_lock(&manager->pool_mutex);
    manager->shutdown_requested = 1;
    pthread_mutex_unlock(&manager->pool_mutex);

    // 等待线程结束
    pthread_join(manager->cleanup_thread, NULL);
    pthread_join(manager->health_check_thread, NULL);

    // 关闭所有活跃会话
    pthread_mutex_lock(&manager->pool_mutex);
    for (uint32_t i = 0; i < manager->active_num; i++) {
        const uint32_t session_id = manager->active_sessions[i];
        shutdown_session_on_(&manager->sesspool[session_id]);
    }
    pthread_mutex_unlock(&manager->pool_mutex);

    // 销毁所有会话
    for (uint32_t i = 0; i < Env.max_sessions; i++) {
        destroy_session_on_(&manager->sesspool[i]);
    }

    free(manager->sesspool);
    free(manager->idle_sessions);
    free(manager->active_sessions);

    network_manager_shutdown(manager);

    if (manager->network_threads) {
        free(manager->network_threads);
    }

    if (manager->worker_threads) {
        free(manager->worker_threads);
    }

    pthread_mutex_destroy(&manager->pool_mutex);
    free(manager);
}

int session_system_init() {
    /////资源分配+配置信息缓存/////
    if (g_network_manager) return SESS_ERROR;//已经存在网络管理器模块
    network_manager_t* manager = (network_manager_t*)calloc(1,sizeof(network_manager_t));
    if (!manager) return SESS_ERROR;

    // 初始化会话池互斥锁
    if (pthread_mutex_init(&manager->pool_mutex, NULL) != 0) {
        free(manager);
        return SESS_ERROR;
    }

    // 分配会话池和相关索引池数组
    manager->sesspool = (session_t*)malloc(Env.max_sessions * sizeof(session_t));
    manager->idle_sessions = (SIIP*)malloc(Env.max_sessions * sizeof(uint32_t));
    manager->active_sessions = (SIIP*)malloc(Env.max_sessions * sizeof(uint32_t));

    if (!manager->idle_sessions || !manager->active_sessions ||!manager->sesspool) {
        free(manager->idle_sessions);
        free(manager->active_sessions);
        free(manager->sesspool);
        free(manager);
        return SESS_ERROR;
    }

    manager->listen_fd = -1;
    manager->epoll_fd = -1;

    g_network_manager = manager;

    ///// 初始化网络管理器 /////
    if (network_manager_init_(g_network_manager) != SESS_OK) {
        destroy_network_manager_(g_network_manager);
        g_network_manager = NULL;
        return SESS_ERROR;
    }

    if (network_manager_start(g_network_manager) != SESS_OK) {
        destroy_network_manager_(g_network_manager);
        g_network_manager = NULL;
        return SESS_ERROR;
    }

    return SESS_OK;
}

void shutdown_session_system() {
    if (g_network_manager) {
        destroy_network_manager_(g_network_manager);
        g_network_manager = NULL;
    }
    g_network_manager = NULL;
}
