#include "manager.h"
#include "netptd.cpp"
#include "env.hpp"

// 全局变量/统计变量
network_manager_t* g_network_manager;//网络管理器
volatile int running_flag;//网络系统运行标志
volatile int cleanup_thread_running_flag;//清理线程运行标志
volatile int response_manager_thread_running_flag;//回复线程运行标志
volatile atomic<int> network_thread_running_flag;//网络线程运行标志
volatile atomic<int> worker_thread_running_flag;//解包工作线程线程运行标志

// 全局响应队列和线程管理
ReaderWriterQueue<response_t*> response_queue;//全局回复队列
ConcurrentQueue<command_t*> command_queue;//全局执行队列


void network_server_system_init() {
    //提前声明变量
    const int opt = 1;// 设置套接字选项

    sockaddr_storage bind_addr = {};// 绑定地址
    sockaddr_in* addr_in = (sockaddr_in*)&bind_addr;
    sockaddr_in6* addr_in6 = (sockaddr_in6*)&bind_addr;
    epoll_event ev;

    /////初始化资源分配+配置信息缓存
    network_manager_t* manager = (network_manager_t*)calloc(1,sizeof(network_manager_t));
    if (!manager) {
        printf("calloc error\n");
        goto err;
    }
    manager->pool_capacity = Env.max_sessions;
    // 初始化会话池互斥锁
    if (pthread_mutex_init(&manager->pool_mutex, NULL) != 0) {
        printf("mutex error\n");
        goto err;
    }
    // 分配会话池和相关索引池数组
    manager->sesspool = (session_t*)calloc(manager->pool_capacity , sizeof(session_t));
    manager->idle_sessions = (SIIP*)calloc(manager->pool_capacity , sizeof(uint32_t));
    manager->active_sessions = (SIIP*)calloc(manager->pool_capacity , sizeof(uint32_t));

    if (!manager->idle_sessions || !manager->active_sessions ||!manager->sesspool) {
        printf("calloc error\n");
        goto err;
    }

    manager->listen_fd = -1;
    manager->epoll_fd = -1;

    init_sesspool_(manager);//初始化会话池

    ///// 创建监听套接字
#if ENABLE_IPV6
    // IPv6模式：创建IPv6套接字，支持IPv4兼容性
    manager->listen_fd = socket(AF_INET6, SOCK_STREAM, 0);
    if (manager->listen_fd < 0) {
        printf("server listen socket created error\n");
        goto err;
    }

    // 设置IPv6套接字选项
    setsockopt(manager->listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // 禁用IPv6-only模式，允许IPv4连接
    int ipv6only = 0;
    setsockopt(manager->listen_fd, IPPROTO_IPV6, IPV6_V6ONLY, &ipv6only, sizeof(ipv6only));

    // 绑定到IPv6地址
    addr_in6->sin6_family = AF_INET6;
    addr_in6->sin6_addr = in6addr_any;
    addr_in6->sin6_port = htons(PORT);

    if (bind(manager->listen_fd, (sockaddr*)&bind_addr, sizeof(sockaddr_in6)) < 0) {
        printf("IPv6 bind error\n");
        goto err;
    }
    
    printf("服务器启动：IPv6模式（支持IPv4兼容性）\n");
#else
    // IPv4模式：创建IPv4套接字
    manager->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (manager->listen_fd < 0) {
        printf("server listen socket created error\n");
        goto err;
    }

    setsockopt(manager->listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    addr_in->sin_family = AF_INET;
    addr_in->sin_addr.s_addr = INADDR_ANY;
    addr_in->sin_port = htons(PORT);

    if (bind(manager->listen_fd, (sockaddr*)&bind_addr, sizeof(sockaddr_in)) < 0) {
        printf("IPv4 bind error\n");
        goto err;
    }
    
    printf("服务器启动：IPv4模式\n");
#endif

    ///// 创建epoll
    g_network_manager->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (g_network_manager->epoll_fd < 0) {
        printf("epoll create error\n");
        goto err;
    }
    // 添加监听套接字到epoll

    ev.events = EPOLLIN;
    ev.data.fd = manager->listen_fd;

    if (epoll_ctl(manager->epoll_fd, EPOLL_CTL_ADD, manager->listen_fd, &ev) < 0) {
        printf("epoll_ctl error\n");
        goto err;
    }

    /////相关线程启动
    g_network_manager = manager;//初始化网络管理模块指针
    running_flag = UN_START;//待启动

    // 创建“死会话清理”-线程
    if (pthread_create(&manager->cleanup_thread, NULL, cleanup_thread_, manager) != 0) {
        printf("cleanup_thread_ pthread created error\n");
        goto err;
    }

    // 创建“回复管理”-线程
    if (pthread_create(&manager->response_thread,NULL,response_manager_thread_func_,manager)!=0) {
        printf("response_manager_thread_func_ pthread created error\n");
        goto err;
    }

    // 分配线程数组
    g_network_manager->network_threads = (pthread_t*)calloc(NETWORK_THREADS , sizeof(pthread_t));
    g_network_manager->worker_threads = (pthread_t*)calloc(WORKER_THREADS , sizeof(pthread_t));

    if (!g_network_manager->network_threads || !g_network_manager->worker_threads) {
        printf("calloc error 2\n");
        goto err;
    }

    // 启动网络线程
    for (uint32_t i = 0; i < NETWORK_THREADS; i++) {
        if (pthread_create(&manager->network_threads[i], NULL, network_thread_func_, manager) != 0) {
            printf("network_thread_func_ pthread created error\n");
            goto err;
        }
    }

    // 启动工作线程
    for (uint32_t i = 0; i < WORKER_THREADS; i++) {
        if (pthread_create(&manager->worker_threads[i], NULL, worker_thread_func_, manager) != 0) {
            printf("worker_thread_func_ pthread created error\n");
            goto err;
        }
    }

    return;
    err:
    printf("network_server_system_init failed!\n");
    system("read -p '按回车键继续...'");
    exit(1);
}
void network_server_system_start() {
    // 开始监听
    if (listen(g_network_manager->listen_fd, LISTEN_BACKLOG) < 0) {
        printf("listen error\n");
        goto err;
    }
    // 设置为非阻塞
    set_socket_nonblocking_(g_network_manager->listen_fd);

    //启动运行标志——>提示所有线程可以开始工作
    running_flag = RUN;

    return;
    err:
    printf("network_server_system_start failed!\n");
    system("read -p '按回车键继续...'");
    exit(1);
}
void network_server_system_shutdown() {
    network_manager_t* manager = g_network_manager;
    if (!manager) return;
    //先暂停所有新连接
    running_flag = KILL;//请求线程关闭
    usleep(1000);//等待反应
    if (network_thread_running_flag != 0 ) {
        printf("# network_thread_running_flag ！= 0 #\n");
    }
    // 等待所有 “连接响应”-线程函数 线程结束后关闭套接字和epoll描述符
    for (uint32_t i = 0; i < NETWORK_THREADS; i++) {
        pthread_join(manager->network_threads[i], NULL);
    }
    if (manager->epoll_fd >= 0) {
        close(manager->epoll_fd);
        manager->epoll_fd = -1;
    }
    if (manager->listen_fd >= 0) {
        close(manager->listen_fd);
        manager->listen_fd = -1;
    }
    running_flag = CLOSE;//强行关闭


    for (uint32_t i = 0; i < WORKER_THREADS; i++) {
        pthread_join(manager->worker_threads[i], NULL);
    }

    // 等待线程结束
    pthread_join(manager->cleanup_thread, NULL);
    pthread_join(manager->response_thread, NULL);

    //整个网络管理器的资源就不用释放了，这些临时数据也不要了，有价值的数据将会在线程关闭之前被提交给全局执行队列。
}
int auth_session(const SID session_id,const UID uid) {
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
int send_response(session_t *session, uint8_t* response_data, uint32_t response_len) {
    // 本函数会接管response_data的所有权
    if (!session || !response_data || response_len == 0) return SESS_INVALID;

    // 创建响应结构体
    response_t* response = (response_t*)calloc(1, sizeof(response_t));
    if (!response) return SESS_ERR;

    response->session = session;
    response->response_len = response_len;
    if (response_len < 57) {
        memcpy(&response->inline_data,response_data, response_len);
        free(response_data);
    }
    else {
        response->data = response_data;
    }
    // 加入响应队列
    response_queue.enqueue(response);
    return SESS_OK;
}
