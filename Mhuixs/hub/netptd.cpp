#include "manager.h"
#include "unistd.h"
#include "errno.h"
#include "string.h"
#include "stdlib.h"
#include "vector"

// 响应线程管理
static std::vector<response_thread_info_t*> response_threads;
static pthread_mutex_t response_threads_mutex = PTHREAD_MUTEX_INITIALIZER;

// 响应状态定义
#define RESP_PENDING 0
#define RESP_SENDING 1
#define RESP_COMPLETED 2
#define RESP_FAILED 3

// 大响应阈值（字节）
#define LARGE_RESPONSE_THRESHOLD 8192

// "会话清理"-线程函数
static void* cleanup_thread_(void *arg) {
    network_manager_t* manager = (network_manager_t*)arg;
    if (!manager) return (void*)1;
    while (running_flag == UN_START) {
        sleep(1);
    }
    while (running_flag == RUN ) {
        sleep(CLEANUP_INTERVAL);
        if (running_flag == CLOSE || running_flag == KILL ) {
            return NULL;
        }
        const time_t current_time = time(NULL);
        //可以提前记录信息
        pthread_mutex_lock(&manager->pool_mutex);
        // 检查活跃会话
        for (SIIP i = 0; i < manager->active_num; ) {
            const SIIP session_idx = manager->active_sessions[i];
            session_t* session_to_check = &manager->sesspool[session_idx];

            if (session_to_check->state == SESS_IDLE ||  //会话被主动标记为空闲:对端关闭连接，异常连接错误
                current_time - session_to_check->last_activity > TIMEOUT  //会话已经超时：网络中断
            )   {
                if (pthread_mutex_trylock((pthread_mutex_t*)&session_to_check->session_mutex) != 0) {
                    i++;continue;//有线程正在使用直接跳过
                }
                session_to_check->state = SESS_IDLE;

                //缓冲区重置  回收线程得负责恢复默认缓冲区大小
                if (session_to_check->recv_buffer.capacity!=DEFAULT_BUFFER_SIZE) {
                    uint8_t* new_ptr = (uint8_t*)realloc(session_to_check->recv_buffer.buffer,DEFAULT_BUFFER_SIZE);
                    if (!new_ptr) {
                        printf("cleanup_thread_ : realloc , make sure the system is normal!");
                    }
                    else {
                        session_to_check->recv_buffer.buffer = new_ptr;
                        session_to_check->recv_buffer.capacity = DEFAULT_BUFFER_SIZE;
                    }
                }
                session_to_check->recv_buffer.read_pos = 0;
                session_to_check->recv_buffer.write_pos = 0;

                //关闭套接字 客户端断开连接
                if (session_to_check->socket_fd >= 0) {
                    close(session_to_check->socket_fd);
                    session_to_check->socket_fd = -1;
                }
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

// "连接响应"-线程函数
//监听新连接、接收数据、发送数据、处理连接关闭和异常
//"连接响应"-线程函数 在程序关闭阶段是第一种关闭的线程种类
static void* network_thread_func_(void* arg) {
    network_manager_t* manager = (network_manager_t*)arg;
    if (!manager) return (void*)1;
    while (running_flag == UN_START) {
        sleep(1);
    }
    ++network_thread_running_flag;
    epoll_event events[EPOLL_MAX_EVENTS];

    while (running_flag == RUN) {
        int nfds = epoll_wait(manager->epoll_fd, events, EPOLL_MAX_EVENTS,EPOLL_TIMEOUT);
        if (nfds == -1) {
            if (errno == EINTR) continue;
            break;
        }  //??__gthread_once()

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
                    SIIP new_session_siip = alloc_with_socket_(client_fd,(struct sockaddr*)&client_addr);

                    if (new_session_siip != SIZE_MAX) {
                        session_t* session = &g_network_manager->sesspool[new_session_siip];
                        // 设置为非阻塞
                        set_socket_nonblocking_(client_fd);
                        // 添加到epoll
                        epoll_event ev;
                        ev.events = EPOLLIN | EPOLLET;
                        ev.data.fd = client_fd;

                        if (epoll_ctl(manager->epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) != 0) {
                            // 分配失败，关闭连接
                            shutdown_session_on_(session);
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
                        if (session_receive_data_(session) < 0) {
                            // 连接关闭或错误，标记会话为空闲
                            session->state = SESS_IDLE;
                            epoll_ctl(manager->epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                        }
                    }

                    if (event->events & EPOLLOUT) {
                        // 发送数据
                        session_send_data_(session);
                    }

                    if (event->events & (EPOLLHUP | EPOLLERR)) {
                        // 连接关闭或错误，标记会话为空闲
                        session->state = SESS_IDLE;
                        epoll_ctl(manager->epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                    }
                }
            }
        }
    }
    if (running_flag == KILL) {
        --network_thread_running_flag;
    }
    return NULL;
}

// 清理已完成的专用线程
static void cleanup_finished_response_threads() {
    pthread_mutex_lock(&response_threads_mutex);
    
    auto it = response_threads.begin();
    while (it != response_threads.end()) {
        response_thread_info_t* thread_info = *it;
        if (!thread_info->active) {
            // 线程已完成，清理资源
            pthread_join(thread_info->thread_id, NULL);
            delete thread_info;
            it = response_threads.erase(it);
        } else {
            ++it;
        }
    }
    
    pthread_mutex_unlock(&response_threads_mutex);
}

// 主响应处理线程
static void* response_thread_func(void* arg) {
    while (running_flag == RUN || running_flag == KILL) {
        response_t* response = NULL;
        if (response_queue.try_dequeue(response)) {
            // 检查是否为大响应或可能阻塞的响应
            if (response->response_len > LARGE_RESPONSE_THRESHOLD || 
                response->retry_count > 0) {
                // 大响应或重试过的响应，立即创建专用线程并放手不管
                create_dedicated_response_thread(response);
            } else {
                // 小响应，快速发送
                int result = send_response_direct(response);
                if (result == SESS_ERR) {
                    // 发送失败，标记为重试并重新加入队列
                    response->retry_count++;
                    response_queue.enqueue(response);
                } else if (result == SESS_OK) {
                    // 发送成功，释放内存
                    destroy_response(response);
                }
            }
        } else {
            usleep(1000); // 1ms
        }
        
        // 定期清理已完成的线程
        static int cleanup_counter = 0;
        if (++cleanup_counter >= 1000) { // 每1000次循环清理一次
            cleanup_finished_response_threads();
            cleanup_counter = 0;
        }
    }
    return NULL;
}

// 专用响应线程（完全自主，处理大响应）
static void* dedicated_response_thread_func(void* arg) {
    response_thread_info_t* thread_info = (response_thread_info_t*)arg;
    response_t* response = thread_info->response;
    session_t* session = response->session;

    // 获取会话所有权
    SIIP session_idx = SIZE_MAX;
    for (uint32_t i = 0; i < g_network_manager->active_num; i++) {
        if (g_network_manager->sesspool[g_network_manager->active_sessions[i]].session_id == session->session_id) {
            session_idx = g_network_manager->active_sessions[i];
            break;
        }
    }

    if (session_idx == SIZE_MAX) {
        // 会话不存在，直接清理并退出
        destroy_response(response);
        thread_info->active = 0;
        return NULL;
    }

    // 尝试获取会话所有权
    if (try_get_session_ownership_(session_idx) != 0) {
        // 无法获取所有权，重新加入队列
        response_queue.enqueue(response);
        thread_info->active = 0;
        return NULL;
    }

    // 开始发送数据
    while (!thread_info->shutdown && response->sent_len < response->response_len) {
        // 分块发送数据
        size_t remaining = response->response_len - response->sent_len;
        size_t chunk_size = (remaining > 8192) ? 8192 : remaining;

        const uint8_t* data_ptr = (response->response_len >= 57) ?
                                 response->data + response->sent_len :
                                 response->inline_data + response->sent_len;

        ssize_t bytes_sent = send(session->socket_fd, data_ptr, chunk_size, MSG_DONTWAIT);

        if (bytes_sent > 0) {
            response->sent_len += bytes_sent;
            // 更新会话活动时间
            session->last_activity = time(NULL);
        } else if (bytes_sent == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(1000); // 等待1ms后重试
                continue;
            } else {
                // 发送错误，标记会话状态
                session->state = SESS_IDLE;
                break;
            }
        } else {
            // 发送返回0，连接可能已关闭
            session->state = SESS_IDLE;
            break;
        }
    }

    // 释放会话所有权
    release_session_ownership_(session_idx);

    // 发送完成或失败，清理资源
    if (response->sent_len >= response->response_len) {
        // 发送成功
        response->status = RESP_COMPLETED;
    } else {
        // 发送失败
        response->status = RESP_FAILED;
    }

    // 销毁响应
    destroy_response(response);

    // 标记线程为非活跃
    thread_info->active = 0;

    return NULL;
}

// 创建专用响应线程
static int create_dedicated_response_thread(response_t* response) {
    pthread_mutex_lock(&response_threads_mutex);

    // 检查是否超过最大线程数
    int active_threads = 0;
    for (auto& thread_info : response_threads) {
        if (thread_info->active) active_threads++;
    }

    if (active_threads >= max_response_threads) {
        // 超过线程数限制，重新加入队列
        pthread_mutex_unlock(&response_threads_mutex);
        response_queue.enqueue(response);
        return SESS_ERR;
    }

    // 创建新线程信息
    response_thread_info_t* thread_info = new response_thread_info_t();
    thread_info->response = response;
    thread_info->active = 1;
    thread_info->shutdown = 0;
    response->sent_len = 0;
    response->status = RESP_SENDING;

    // 创建线程
    if (pthread_create(&thread_info->thread_id, NULL,
                      dedicated_response_thread_func, thread_info) != 0) {
        delete thread_info;
        pthread_mutex_unlock(&response_threads_mutex);
        // 创建线程失败，重新加入队列
        response_queue.enqueue(response);
        return SESS_ERR;
    }

    response_threads.push_back(thread_info);
    pthread_mutex_unlock(&response_threads_mutex);

    return SESS_OK;
}

// 直接发送响应（用于小响应）
static int send_response_direct(response_t* response) {
    if (!response || !response->session || response->session->socket_fd < 0) return SESS_INVALID;

    const uint8_t* data_ptr = (response->response_len < 57) ?
                             response->inline_data : response->data;

    ssize_t bytes_sent = send(response->session->socket_fd, data_ptr, response->response_len, MSG_DONTWAIT);

    if (bytes_sent == response->response_len) {
        return SESS_OK; // 发送成功
    } else if (bytes_sent > 0) {
        // 部分发送，需要重试
        return SESS_ERR;
    } else if (bytes_sent == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return SESS_ERR; // 需要重试
        } else {
            return SESS_ERR; // 发送错误
        }
    }
    return SESS_ERR;
}

// 销毁响应
static void destroy_response(response_t* response) {
    if (!response) return;
    
    if (response->response_len >= 57 && response->data) {
        free(response->data);
    }
    free(response);
}

// "解析工作"-线程函数
static void* worker_thread_func_(void* arg) {
    network_manager_t* manager = (network_manager_t*)arg;
    if (!manager) return (void*)1;
    
    while (running_flag == UN_START) {
        sleep(1);
    }
    ++worker_thread_running_flag;

    while (running_flag == RUN || running_flag == KILL) {
        // 从命令队列获取命令
        command_t* cmd = NULL;
        if (command_queue.try_dequeue(cmd)) {
            if (cmd && cmd->session) {
                // 处理会话中的数据包
                session_process_incoming_packets_(cmd->session);
                
                // 释放命令
                free(cmd);
            }
        } else {
            usleep(1000); // 1ms
        }
    }
    
    if (running_flag == CLOSE) {
        --worker_thread_running_flag;
    }
    return NULL;
}

// "回复"-线程函数
static void* response_manager_thread_func_(void* arg) {
    network_manager_t* manager = (network_manager_t*)arg;
    if (!manager) return (void*)1;
    
    while (running_flag == UN_START) {
        sleep(1);
    }
    ++response_manager_thread_running_flag;

    // 启动主响应处理线程
    response_thread_func(arg);

    if (running_flag == CLOSE) {
        --response_manager_thread_running_flag;
    }
    return NULL;
}