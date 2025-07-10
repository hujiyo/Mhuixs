#include "session.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>
#include <signal.h>

/*
 * 网络管理器实现
 */

// 网络线程函数
static void* network_thread_func(void* arg) {
    network_manager_t* manager = (network_manager_t*)arg;
    
    struct epoll_event events[manager->config.epoll_max_events];
    
    while (manager->running) {
        int nfds = epoll_wait(manager->epoll_fd, events, manager->config.epoll_max_events, 
                             manager->config.epoll_timeout);
        
        if (nfds == -1) {
            if (errno == EINTR) continue;
            break;
        }
        
        for (int i = 0; i < nfds; i++) {
            struct epoll_event* event = &events[i];
            
            if (event->data.fd == manager->listen_fd) {
                // 新连接
                struct sockaddr_storage client_addr;
                socklen_t addr_len = sizeof(client_addr);
                int client_fd = accept(manager->listen_fd, (struct sockaddr*)&client_addr, &addr_len);
                
                if (client_fd >= 0) {
                    // 检查连接限制
                    char client_ip[INET6_ADDRSTRLEN];
                    if (client_addr.ss_family == AF_INET) {
                        struct sockaddr_in* addr_in = (struct sockaddr_in*)&client_addr;
                        inet_ntop(AF_INET, &addr_in->sin_addr, client_ip, INET_ADDRSTRLEN);
                    } else {
                        struct sockaddr_in6* addr_in6 = (struct sockaddr_in6*)&client_addr;
                        inet_ntop(AF_INET6, &addr_in6->sin6_addr, client_ip, INET6_ADDRSTRLEN);
                    }
                    
                    // 简化的连接限制检查
                    int allow_connection = 1;
                    if (manager->config.enable_rate_limiting) {
                        pthread_mutex_lock(&manager->ip_mutex);
                        
                        uint32_t connections_from_ip = 0;
                        for (uint32_t j = 0; j < manager->ip_count; j++) {
                            if (strcmp(manager->connection_ips[j], client_ip) == 0) {
                                connections_from_ip = manager->connection_counts[j];
                                break;
                            }
                        }
                        
                        if (connections_from_ip >= manager->config.max_connections_per_ip) {
                            allow_connection = 0;
                        }
                        
                        pthread_mutex_unlock(&manager->ip_mutex);
                    }
                    
                    if (allow_connection) {
                        // 分配会话
                        session_t* session = session_pool_allocate_with_socket(
                            manager->session_pool, client_fd, (struct sockaddr*)&client_addr, 0);
                        
                        if (session) {
                            // 设置为非阻塞
                            set_socket_nonblocking(client_fd);
                            
                            // 添加到epoll
                            struct epoll_event ev;
                            ev.events = EPOLLIN | EPOLLET;
                            ev.data.fd = client_fd;
                            
                            if (epoll_ctl(manager->epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) == 0) {
                                __sync_fetch_and_add(&manager->total_connections, 1);
                                __sync_fetch_and_add(&manager->active_connections, 1);
                                
                                // 更新IP连接计数
                                if (manager->config.enable_rate_limiting) {
                                    pthread_mutex_lock(&manager->ip_mutex);
                                    
                                    int found = 0;
                                    for (uint32_t j = 0; j < manager->ip_count; j++) {
                                        if (strcmp(manager->connection_ips[j], client_ip) == 0) {
                                            manager->connection_counts[j]++;
                                            found = 1;
                                            break;
                                        }
                                    }
                                    
                                    if (!found && manager->ip_count < 1024) {
                                        strcpy(manager->connection_ips[manager->ip_count], client_ip);
                                        manager->connection_counts[manager->ip_count] = 1;
                                        manager->ip_count++;
                                    }
                                    
                                    pthread_mutex_unlock(&manager->ip_mutex);
                                }
                            } else {
                                session_pool_recycle_session(manager->session_pool, session->session_id);
                                close(client_fd);
                            }
                        } else {
                            close(client_fd);
                            __sync_fetch_and_add(&manager->failed_connections, 1);
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
                pthread_mutex_lock(&manager->session_pool->pool_mutex);
                
                for (uint32_t j = 0; j < manager->session_pool->active_count; j++) {
                    uint32_t session_id = manager->session_pool->active_sessions[j];
                    session_t* s = &manager->session_pool->sessions[session_id];
                    if (s->socket_fd == client_fd) {
                        session = s;
                        break;
                    }
                }
                
                pthread_mutex_unlock(&manager->session_pool->pool_mutex);
                
                if (session) {
                    if (event->events & EPOLLIN) {
                        // 接收数据
                        if (session_receive_data(session) < 0) {
                            // 连接关闭或错误
                            epoll_ctl(manager->epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                            session_pool_recycle_session(manager->session_pool, session->session_id);
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
                        session_pool_recycle_session(manager->session_pool, session->session_id);
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
    network_manager_t* manager = (network_manager_t*)arg;
    
    while (manager->running) {
        // 获取有待处理命令的会话
        SID session_ids[100];
        uint32_t count = session_pool_get_sessions_with_commands(manager->session_pool, session_ids, 100);
        
        if (count == 0) {
            usleep(1000); // 1ms
            continue;
        }
        
        // 处理会话中的数据包
        for (uint32_t i = 0; i < count; i++) {
            session_t* session = session_pool_find_session(manager->session_pool, session_ids[i]);
            if (session) {
                session_process_incoming_packets(session);
            }
        }
    }
    
    return NULL;
}

//=============================================================================
// 网络管理器实现
//=============================================================================

network_manager_t* network_manager_create(const network_manager_config_t* config) {
    network_manager_t* manager = (network_manager_t*)malloc(sizeof(network_manager_t));
    if (!manager) return NULL;
    
    memset(manager, 0, sizeof(network_manager_t));
    
    // 复制配置
    if (config) {
        manager->config = *config;
    } else {
        // 默认配置
        manager->config.listen_port = 8080;
        manager->config.address_family = AF_INET;
        manager->config.listen_backlog = 1024;
        manager->config.enable_ipv6 = 0;
        manager->config.network_threads = 4;
        manager->config.worker_threads = 8;
        manager->config.epoll_max_events = 1024;
        manager->config.epoll_timeout = 100;
        manager->config.enable_rate_limiting = 1;
        manager->config.max_connections_per_ip = 100;
        manager->config.connection_timeout = 300;
        
        // 默认会话池配置
        manager->config.session_pool_config.max_sessions = 10000;
        manager->config.session_pool_config.initial_pool_size = 100;
        manager->config.session_pool_config.cleanup_interval = 60;
        manager->config.session_pool_config.health_check_interval = 30;
        manager->config.session_pool_config.max_idle_sessions = 1000;
        manager->config.session_pool_config.enable_auto_cleanup = 1;
        manager->config.session_pool_config.enable_session_reuse = 1;
    }
    
    manager->listen_fd = -1;
    manager->epoll_fd = -1;
    
    // 初始化IP连接数互斥锁
    if (pthread_mutex_init(&manager->ip_mutex, NULL) != 0) {
        free(manager);
        return NULL;
    }
    
    return manager;
}

void network_manager_destroy(network_manager_t* manager) {
    if (!manager) return;
    
    network_manager_shutdown(manager);
    
    if (manager->session_pool) {
        session_pool_destroy(manager->session_pool);
    }
    
    if (manager->network_threads) {
        free(manager->network_threads);
    }
    
    if (manager->worker_threads) {
        free(manager->worker_threads);
    }
    
    pthread_mutex_destroy(&manager->ip_mutex);
    free(manager);
}

int network_manager_initialize(network_manager_t* manager) {
    if (!manager) return SESS_INVALID;
    
    // 创建会话池
    manager->session_pool = session_pool_create(&manager->config.session_pool_config);
    if (!manager->session_pool) {
        return SESS_ERROR;
    }
    
    if (session_pool_initialize(manager->session_pool) != SESS_OK) {
        session_pool_destroy(manager->session_pool);
        manager->session_pool = NULL;
        return SESS_ERROR;
    }
    
    // 创建监听套接字
    manager->listen_fd = socket(manager->config.address_family, SOCK_STREAM, 0);
    if (manager->listen_fd < 0) {
        return SESS_ERROR;
    }
    
    // 设置套接字选项
    int opt = 1;
    setsockopt(manager->listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // 绑定地址
    struct sockaddr_storage bind_addr;
    memset(&bind_addr, 0, sizeof(bind_addr));
    
    if (manager->config.address_family == AF_INET) {
        struct sockaddr_in* addr_in = (struct sockaddr_in*)&bind_addr;
        addr_in->sin_family = AF_INET;
        addr_in->sin_addr.s_addr = INADDR_ANY;
        addr_in->sin_port = htons(manager->config.listen_port);
        
        if (bind(manager->listen_fd, (struct sockaddr*)&bind_addr, sizeof(struct sockaddr_in)) < 0) {
            close(manager->listen_fd);
            return SESS_ERROR;
        }
    } else {
        struct sockaddr_in6* addr_in6 = (struct sockaddr_in6*)&bind_addr;
        addr_in6->sin6_family = AF_INET6;
        addr_in6->sin6_addr = in6addr_any;
        addr_in6->sin6_port = htons(manager->config.listen_port);
        
        if (bind(manager->listen_fd, (struct sockaddr*)&bind_addr, sizeof(struct sockaddr_in6)) < 0) {
            close(manager->listen_fd);
            return SESS_ERROR;
        }
    }
    
    // 开始监听
    if (listen(manager->listen_fd, manager->config.listen_backlog) < 0) {
        close(manager->listen_fd);
        return SESS_ERROR;
    }
    
    // 设置为非阻塞
    set_socket_nonblocking(manager->listen_fd);
    
    // 创建epoll
    manager->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (manager->epoll_fd < 0) {
        close(manager->listen_fd);
        return SESS_ERROR;
    }
    
    // 添加监听套接字到epoll
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = manager->listen_fd;
    
    if (epoll_ctl(manager->epoll_fd, EPOLL_CTL_ADD, manager->listen_fd, &ev) < 0) {
        close(manager->epoll_fd);
        close(manager->listen_fd);
        return SESS_ERROR;
    }
    
    // 分配线程数组
    manager->network_threads = (pthread_t*)malloc(manager->config.network_threads * sizeof(pthread_t));
    manager->worker_threads = (pthread_t*)malloc(manager->config.worker_threads * sizeof(pthread_t));
    
    if (!manager->network_threads || !manager->worker_threads) {
        if (manager->network_threads) free(manager->network_threads);
        if (manager->worker_threads) free(manager->worker_threads);
        close(manager->epoll_fd);
        close(manager->listen_fd);
        return SESS_ERROR;
    }
    
    manager->initialized = 1;
    manager->start_time = time(NULL);
    
    return SESS_OK;
}

int network_manager_start(network_manager_t* manager) {
    if (!manager || !manager->initialized) return SESS_INVALID;
    
    manager->running = 1;
    
    // 启动网络线程
    for (uint32_t i = 0; i < manager->config.network_threads; i++) {
        if (pthread_create(&manager->network_threads[i], NULL, network_thread_func, manager) != 0) {
            manager->running = 0;
            return SESS_ERROR;
        }
    }
    
    // 启动工作线程
    for (uint32_t i = 0; i < manager->config.worker_threads; i++) {
        if (pthread_create(&manager->worker_threads[i], NULL, worker_thread_func, manager) != 0) {
            manager->running = 0;
            return SESS_ERROR;
        }
    }
    
    return SESS_OK;
}

void network_manager_stop(network_manager_t* manager) {
    if (!manager) return;
    
    manager->running = 0;
    
    // 等待所有线程结束
    for (uint32_t i = 0; i < manager->config.network_threads; i++) {
        pthread_join(manager->network_threads[i], NULL);
    }
    
    for (uint32_t i = 0; i < manager->config.worker_threads; i++) {
        pthread_join(manager->worker_threads[i], NULL);
    }
}

void network_manager_shutdown(network_manager_t* manager) {
    if (!manager) return;
    
    network_manager_stop(manager);
    
    if (manager->session_pool) {
        session_pool_shutdown(manager->session_pool);
    }
    
    if (manager->epoll_fd >= 0) {
        close(manager->epoll_fd);
        manager->epoll_fd = -1;
    }
    
    if (manager->listen_fd >= 0) {
        close(manager->listen_fd);
        manager->listen_fd = -1;
    }
    
    manager->initialized = 0;
}