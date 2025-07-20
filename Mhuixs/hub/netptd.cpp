#include "manager.h"
#include "unistd.h"
#include "errno.h"
#include "stdlib.h"
using namespace std;

// 响应回复线程池管理
#define RESPONSE_POOL_SIZE 16

response_t* g_pthread_task[RESPONSE_POOL_SIZE] ={};//回复主线程给每个线程安排的任务，子线程只可读、接管指针所有权、获得回复进度
pthread_mutex_t g_pthread_mutex[RESPONSE_POOL_SIZE];
pthread_cond_t g_pthread_cond[RESPONSE_POOL_SIZE];
int pthread_state[RESPONSE_POOL_SIZE] = {};//每个子线程都对应一位，子线程只能将自己的位从DEALING改为IDLE，回复主线程只有权将IDLE改为DEALING

#define DEALING 0 //正在处理reponse
#define IDLE    1 //空闲，可以安排任务
#define SHUTDOWN 2//请求关闭线程

// 响应状态定义
#define RESP_PENDING 0//等待发送
#define RESP_SENDING 1//正在发送
#define RESP_COMPLETED 2//发送完成
#define RESP_FAILED 3//发送失败

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

// "连接响应"-线程函数 监听新连接、接收数据、发送数据、处理连接关闭和异常
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
                            // 连接关闭或错误，标记会话为空闲
                            session->state = SESS_IDLE;
                            epoll_ctl(manager->epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                        }
                    }

                    if (event->events & (EPOLLHUP | EPOLLERR)) {
                        // 连接关闭或错误，标记会话为空闲
                        session->state = SESS_IDLE;
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
                //..暂时不写
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

static void* child_reponse_thread_(void* arg) {
    int id = *((int*)arg);
    pthread_state[id] = IDLE;

    while (true) {
        pthread_mutex_lock(&g_pthread_mutex[id]);//阻塞锁
        while (pthread_state[id]!=DEALING || pthread_state[id] == SHUTDOWN) {
            pthread_cond_wait(&g_pthread_cond[id],&g_pthread_mutex[id]);
        }
        /*
        if (!g_pthread_task[id] || !g_pthread_task[id]->session) {
            pthread_state[id] = IDLE;
            continue;
        }
        */
        if (pthread_state[id] ==SHUTDOWN) {
            break;//关闭线程
        }
        int ret = send_all(g_pthread_task[id]);
        if (ret) {
            if (ret == -1) {
                //...
            }
            else if (ret == -2) {
                //...
            }
        }

        // 释放资源
        if (g_pthread_task[id]->response_len > 48) {
            free(g_pthread_task[id]->data);
        }
        free(g_pthread_task[id]);// 接管指针内存并释放

        // 标记线程为空闲
        pthread_state[id] = IDLE;//释放所有权
    }

    return NULL;
}
#define INLINE_DATA_THRESHOLD 48

// 临时线程处理函数
static void* temp_response_handler(void* arg) {
    response_t* resp = (response_t*)arg;
    send_all(resp);
    if (resp->response_len > INLINE_DATA_THRESHOLD) {
        free(resp->data);
    }
    free(resp);
    return NULL;
}

static int thread_ids[RESPONSE_POOL_SIZE];
// "回复池管理"-线程函数
static void* response_thread_func_(void* arg) {
    network_manager_t* manager = (network_manager_t*)arg;
    if (!manager) return (void*)1;

    response_manager_thread_running_flag = 1;

    for (int i = 0; i < RESPONSE_POOL_SIZE; i++) {
        pthread_state[i] = DEALING;
    }

    //启动线程
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    pthread_t respon[RESPONSE_POOL_SIZE];
    for (int i=0;i<RESPONSE_POOL_SIZE;i++) {
        thread_ids[i] = i;
        int ret = 0;
        ret += pthread_create(&respon[i], &attr, child_reponse_thread_, &thread_ids[i]);
        ret += pthread_mutex_init(&g_pthread_mutex[i], NULL);
        ret += pthread_cond_init(&g_pthread_cond[i], NULL);
        if (ret) {
            response_manager_thread_running_flag = -1;
            printf("response_thread_func_ :system error!\n");
            system("read -p '按回车键继续...'");
            exit(1);
        }
    }

    //等待网络模块开始工作
    while (running_flag == UN_START) {
        usleep(1000);//1ms
    }

    while (running_flag == RUN || running_flag == KILL) {
        response_t* response = NULL;
        response_queue.wait_dequeue(response);//获得一个新待发送回复指针

        if (!response || !response->session || response->response_len == 0) {
            if (response) {
                if (response->response_len > INLINE_DATA_THRESHOLD) {
                    free(response->data);
                }
                free(response);
            }
            continue;
        }

        uint8_t* data_ptr = response->response_len > INLINE_DATA_THRESHOLD ?
                           response->data : response->inline_data;

        // 小响应直接发送，大响应交给线程池处理
        if (response->response_len <= 4096) {
            for (int i = 0; i < 3; i++) { //尝试发送三次
                uint32_t remaining = response->response_len - response->sent_len;
                ssize_t bytes_sent = send(response->session->socket_fd,
                                          data_ptr + response->sent_len,
                                          remaining,
                                          MSG_NOSIGNAL);
                if (bytes_sent < 0) {
                    // 发送失败，记录错误并退出重试
                    break;
                } else if (bytes_sent == 0) {
                    // 连接关闭
                    break;
                } else {
                    response->sent_len += bytes_sent;
                    if (response->sent_len == response->response_len) {
                        break;
                    }
                }
            }

            // 释放资源
            if (response->response_len > INLINE_DATA_THRESHOLD) {
                free(response->data);
            }
            free(response);

            // 发送成功，处理下一个
            // 发送失败也继续处理下一个
            continue;
        }

        // 大响应，分配给空闲线程
        bool assigned = false;
        for (int i = 0; i < RESPONSE_POOL_SIZE; i++) {
            if (pthread_state[i] == IDLE) {
                g_pthread_task[i] = response;
                assigned = true;
                pthread_state[i] = DEALING;
                pthread_cond_signal(&g_pthread_cond[i]);//唤醒等待线程
                break;
            }
        }

        if (!assigned) {
            // 没有空闲线程，创建一次性分离型线程
            pthread_t temp_thread;
            pthread_create(&temp_thread, &attr, temp_response_handler, response);
        }
    }

    if (running_flag == CLOSE) {
        for (int i = 0; i < RESPONSE_POOL_SIZE; i++) {
            if (pthread_state[i] == DEALING) {
                pthread_cancel(respon[i]);
                pthread_join(respon[i], NULL);
            }
        }
        response_manager_thread_running_flag = 0;
    }
    pthread_attr_destroy(&attr);
    return NULL;
}


