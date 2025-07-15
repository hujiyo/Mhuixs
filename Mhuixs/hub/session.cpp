#include "session.hpp"

// 全局变量
extern Id_alloctor Idalloc;
static network_manager_t* g_network_manager = NULL;//网络管理器

////本地函数////
int _init_session_on(session_t* session);//初始化会话
void _destroy_session_on(session_t* session);//销毁会话
int _create_session_on(session_t* session, int socket_fd, const sockaddr* client_addr);//初始化会话
void _shutdown_session_on(session_t* session);//关闭会话
session_t* _alloc_session(network_manager_t* pool);//从空闲池中分配新会话
int _network_manager_init(network_manager_t* manager);//初始化网络管理器
session_t* _try_get_session(SIIP siid);//按会话SIID查找会话
int _set_socket_nonblocking(int sockfd);//设置套接字非阻塞


int _set_socket_nonblocking(int sockfd) {
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1) {
        return SESS_ERROR;
    }

    if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {
        return SESS_ERROR;
    }

    return SESS_OK;
}
//命令队列相关函数
command_queue_t* create_command_queue(uint32_t max_size) {
    command_queue_t* queue =(command_queue_t*)calloc(1,sizeof(command_queue_t));
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
    
    timespec ts;
    if (timeout_ms > 0) {
        timeval tv;
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
void shutdown_command_queue(command_queue_t* queue) {
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
command_t* command_create(const CommandNumber cmd_id,const uint32_t seq_num,const uint32_t priority,
                         const uint8_t* data,const uint32_t data_len) {
    command_t* cmd = (command_t*)malloc(sizeof(command_t));
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
int _init_session_on(session_t* session) {
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
void _destroy_session_on(session_t* session) {
    //必须保证地址的有效性
    _shutdown_session_on(session);
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
int _create_session_on(session_t* session, int socket_fd, const sockaddr* client_addr) {
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
    if (_set_socket_nonblocking(socket_fd) != 0) {
        pthread_mutex_unlock(&session->session_mutex);
        return SESS_ERROR;
    }

    // 重置统计信息
    session->bytes_received =0;
    session->bytes_sent =0;
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
void _shutdown_session_on(session_t* session) {
    //必须保证地址的有效性
    pthread_mutex_lock(&session->session_mutex);
    if (session->state == SESS_IDLE) {
        pthread_mutex_unlock(&session->session_mutex);
        return;
    }
    session->state = SESS_CLOSING;

    session_send_data(session);// 发送缓冲区中的剩余数据 //////////
    ////////////////////////////////缓冲区清理
    
    // 关闭套接字 客户端断开连接
    if (session->socket_fd >= 0) {
        close(session->socket_fd);
        session->socket_fd = -1;
    }
    
    session->state = SESS_IDLE;//标记为空闲，内存清理的事情交给下一个使用者
    pthread_mutex_unlock(&session->session_mutex);
}

// “死会话清理”-线程函数
static void* cleanup_thread(void* arg) {
    network_manager_t* manager = (network_manager_t*)arg;
    while (!manager->shutdown_requested) {
        sleep(CLEANUP_INTERVAL);
        cleanup_dead_sessions(manager);
    }
    return NULL;
}
// “健康检查”-线程函数
static void* session_pool_health_check_thread(void* arg) {
    network_manager_t* pool =(network_manager_t*)arg;
    while (!pool->shutdown_requested) {
        sleep(HEALTH_CHECK_INTERVAL);
        check_all_sessions(pool);
    }
    return NULL;
}

session_t* _alloc_session(network_manager_t* pool) {
    pthread_mutex_lock(&pool->pool_mutex);
    if (pool->idle_count == 0) {
        pthread_mutex_unlock(&pool->pool_mutex);
        return NULL; // 没有空闲会话
    }

    // 从空闲列表中取出一个会话
    const uint32_t session_id = pool->idle_sessions[pool->idle_count - 1];
    pool->idle_count--;

    // 添加到活跃列表
    pool->active_sessions[pool->active_count] = session_id;
    pool->active_count++;

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
    session_t* session = _alloc_session(pool);
    if (!session) return NULL;

    if (_create_session_on(session, socket_fd, client_addr) != SESS_OK) {
        recycle_session(pool, session->session_id);
        return NULL;
    }
    return session;
}

int recycle_session(network_manager_t* pool,const SID session_id) {
    if (!pool || session_id >= Env.max_sessions) return SESS_INVALID;

    pthread_mutex_lock(&pool->pool_mutex);

    // 从活跃列表中移除
    int found = 0;
    for (uint32_t i = 0; i < pool->active_count; i++) {
        if (pool->active_sessions[i] == session_id) {
            // 移动后面的元素向前
            for (uint32_t j = i; j < pool->active_count - 1; j++) {
                pool->active_sessions[j] = pool->active_sessions[j + 1];
            }
            pool->active_count--;
            found = 1;
            break;
        }
    }

    if (!found) {
        // 可能在错误列表中
        for (uint32_t i = 0; i < pool->error_count; i++) {
            if (pool->error_sessions[i] == session_id) {
                for (uint32_t j = i; j < pool->error_count - 1; j++) {
                    pool->error_sessions[j] = pool->error_sessions[j + 1];
                }
                pool->error_count--;
                found = 1;
                break;
            }
        }
    }

    if (found) {
        // 重置会话
        _shutdown_session_on(&pool->sesspool[session_id]);

        // 添加到空闲列表
        pool->idle_sessions[pool->idle_count] = session_id;
        pool->idle_count++;

        // 更新统计
        pool->active_sessions--;
        pool->idle_sessions++;
    }

    pthread_mutex_unlock(&pool->pool_mutex);
    return found ? SESS_OK : SESS_NOT_FOUND;
}

session_t* _try_get_session(SIIP siid) {
    session_t* session = &g_network_manager->sesspool[siid];
    if (session->state == SESS_IDLE ||
        siid >= Env.max_sessions ||
        pthread_mutex_trylock(&session->session_mutex) == EBUSY
        )return NULL;
    return session;
}
int _return_session(SIIP siid) {

}

session_t* pool_find_session_by_uid(network_manager_t* pool, UID user_id) {
    if (!pool) return NULL;

    pthread_mutex_lock(&pool->pool_mutex);

    session_t* found_session = NULL;

    // 搜索活跃会话
    for (uint32_t i = 0; i < pool->active_count; i++) {
        uint32_t session_id = pool->active_sessions[i];
        session_t* session = &pool->sesspool[session_id];

        if (session->user_id == user_id) {
            found_session = session;
            break;
        }
    }

    pthread_mutex_unlock(&pool->pool_mutex);
    return found_session;
}

void cleanup_dead_sessions(network_manager_t* pool) {
    if (!pool) return;

    pthread_mutex_lock(&pool->pool_mutex);

    const time_t current_time = time(NULL);
    uint32_t cleaned_count = 0;

    // 检查活跃会话
    for (uint32_t i = 0; i < pool->active_count; ) {
        const uint32_t session_id = pool->active_sessions[i];
        session_t* session = &pool->sesspool[session_id];

        if (!_session_is_healthy(session) ||                 //不健康会话
            current_time - session->last_activity > TIMEOUT ) //会话已经超时
        {
            // 移动到错误列表
            pool->error_sessions[pool->error_count] = session_id;
            pool->error_count++;

            // 从活跃列表中移除
            for (uint32_t j = i; j < pool->active_count - 1; j++) {
                pool->active_sessions[j] = pool->active_sessions[j + 1];
            }
            pool->active_count--;
            cleaned_count++;
            // 不增加i，因为当前位置现在是下一个元素
        } else {
            i++;
        }
    }

    // 清理错误会话
    for (uint32_t i = 0; i < pool->error_count; i++) {
        uint32_t session_id = pool->error_sessions[i];
        _shutdown_session_on(&pool->sesspool[session_id]);//关闭会话

        // 添加到空闲列表
        pool->idle_sessions[pool->idle_count] = session_id;
        pool->idle_count++;
    }

    pool->error_count = 0;

    pthread_mutex_unlock(&pool->pool_mutex);
}
void check_all_sessions(network_manager_t* pool)
{
    if (!pool) return;

    pthread_mutex_lock(&pool->pool_mutex);

    // 检查所有活跃会话的健康状态
    for (uint32_t i = 0; i < pool->active_count; i++) {
        uint32_t session_id = pool->active_sessions[i];
        session_t* session = &pool->sesspool[session_id];

        if (!_session_is_healthy(session)) {
            // 标记为错误状态
            session->state = SESS_ERROR;
        }
    }

    pthread_mutex_unlock(&pool->pool_mutex);
}

// 网络线程函数
static void* network_thread_func(void* arg) {
    network_manager_t* manager = (network_manager_t*)arg;

    epoll_event events[EPOLL_MAX_EVENTS];

    while (manager->running) {
        int nfds = epoll_wait(manager->epoll_fd, events, EPOLL_MAX_EVENTS,
                             EPOLL_TIMEOUT);

        if (nfds == -1) {
            if (errno == EINTR) continue;
            break;
        }

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
                        _set_socket_nonblocking(client_fd);
                        // 添加到epoll
                        epoll_event ev;
                        ev.events = EPOLLIN | EPOLLET;
                        ev.data.fd = client_fd;

                        if (epoll_ctl(manager->epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) == 0) {
                            __sync_fetch_and_add(&manager->total_connections, 1);
                            __sync_fetch_and_add(&manager->active_connections, 1);
                        } else {
                            recycle_session(manager, session->session_id);
                            close(client_fd);
                        }
                    } else {
                        close(client_fd);
                        __sync_fetch_and_add(&manager->failed_connections, 1);
                    }
                }
            } else {
                // 数据事件
                int client_fd = event->data.fd;

                // 通过文件描述符查找会话
                session_t* session = NULL;
                pthread_mutex_lock(&manager->pool_mutex);

                for (uint32_t j = 0; j < manager->active_count; j++) {
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
                            __sync_fetch_and_sub(&manager->active_connections, 1);
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
                        __sync_fetch_and_sub(&manager->active_connections, 1);
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
        // 获取有待处理命令的会话
        SID session_ids[100];
        const uint32_t count = pool_get_sessions_with_commands(manager, session_ids, 100);

        if (count == 0) {
            usleep(1000); // 1ms
            continue;
        }

        // 处理会话中的数据包
        for (uint32_t i = 0; i < count; i++) {
            session_t* session = find_session(manager, session_ids[i]);
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
        if (_init_session_on(&manager->sesspool[i]) == SESS_ERR) {
            pthread_mutex_unlock(&manager->pool_mutex);
            return SESS_ERR;
        }
        manager->idle_sessions[i] = i;
    }
    manager->idle_count = Env.max_sessions;
    manager->active_count = 0;
    manager->error_count = 0;
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
    for (uint32_t i = 0; i < manager->active_count; i++) {
        const uint32_t session_id = manager->active_sessions[i];
        _shutdown_session_on(&manager->sesspool[session_id]);
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
            session->bytes_received += bytes_received;
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
        session->bytes_sent += bytes_sent;
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

    // 简化的数据包处理
    uint32_t available = get_buffer_available_size(&session->recv_buffer);
    if (available < sizeof(PKG)) return 0;

    uint8_t buffer[8192];
    uint32_t bytes_read = buffer_peek(&session->recv_buffer, buffer, sizeof(buffer));

    if (bytes_read >= sizeof(PKG)) {
        PKG* pkg = (PKG*)buffer;

        // 检查数据包完整性
        if (bytes_read >= sizeof(PKG) + pkg->size) {
            // 创建命令
            command_t* cmd = command_create(
                pkg->command,
                pkg->sequence,
                PRIORITY_NORMAL,
                buffer + sizeof(PKG),
                pkg->size
            );

            if (cmd) {
                // 将命令加入队列
                if (session_enqueue_command(session, cmd) == SESS_OK) {
                    // 从缓冲区移除已处理的数据
                    buffer_read(session->recv_buffer, buffer, sizeof(PKG) + pkg->size);

                    return 1;
                } else {
                    command_destroy(cmd);
                }
            }
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

int _session_is_healthy(const session_t* session) {
    if (session->socket_fd < 0||
    session->state == SESS_ERROR ||
    session->state == SESS_IDLE  )return 0;
    return 1;
}

session_t* get_session(SID session_id) {
    if (!g_network_manager) return NULL;

    return find_session(g_network_manager, session_id);
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

UID get_session_user_id(SID session_id) {
    session_t* session = get_session(session_id);
    if (!session) return 0;

    pthread_mutex_lock(&session->session_mutex);
    UID uid = session->user_id;
    pthread_mutex_unlock(&session->session_mutex);

    return uid;
}

int _network_manager_init(network_manager_t* manager) {
    if (_init_sesspool(manager))

    // 启动“死会话清理”-线程
    if (pthread_create(&manager->cleanup_thread, NULL, cleanup_thread, manager) != 0) {
        return SESS_ERROR;
    }

    // 启动“健康检查”-线程
    if (pthread_create(&manager->health_check_thread, NULL, session_pool_health_check_thread, manager) != 0) {
        manager->shutdown_requested = 1;
        pthread_join(manager->cleanup_thread, NULL);
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
    _set_socket_nonblocking(manager->listen_fd);

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

void _destroy_network_manager(network_manager_t* manager) {
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
    for (uint32_t i = 0; i < manager->active_count; i++) {
        const uint32_t session_id = manager->active_sessions[i];
        _shutdown_session_on(&manager->sesspool[session_id]);
    }
    pthread_mutex_unlock(&manager->pool_mutex);

    // 销毁所有会话
    for (uint32_t i = 0; i < Env.max_sessions; i++) {
        _destroy_session_on(&manager->sesspool[i]);
    }

    free(manager->sesspool);
    free(manager->idle_sessions);
    free(manager->active_sessions);
    free(manager->error_sessions);

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
    manager->idle_sessions = (uint32_t*)malloc(Env.max_sessions * sizeof(uint32_t));
    manager->active_sessions = (uint32_t*)malloc(Env.max_sessions * sizeof(uint32_t));
    manager->error_sessions = (uint32_t*)malloc(Env.max_sessions * sizeof(uint32_t));

    if (!manager->idle_sessions || !manager->active_sessions|| !manager->error_sessions  ||!manager->sesspool) {
        free(manager->idle_sessions);
        free(manager->active_sessions);
        free(manager->error_sessions);
        free(manager->sesspool);
        free(manager);
        return SESS_ERROR;
    }

    manager->listen_fd = -1;
    manager->epoll_fd = -1;

    g_network_manager = manager;

    ///// 初始化网络管理器 /////
    if (_network_manager_init(g_network_manager) != SESS_OK) {
        _destroy_network_manager(g_network_manager);
        g_network_manager = NULL;
        return SESS_ERROR;
    }

    if (network_manager_start(g_network_manager) != SESS_OK) {
        _destroy_network_manager(g_network_manager);
        g_network_manager = NULL;
        return SESS_ERROR;
    }

    return SESS_OK;
}

void shutdown_session_system() {
    if (g_network_manager) {
        _destroy_network_manager(g_network_manager);
        g_network_manager = NULL;
    }
    g_network_manager = NULL;
}
