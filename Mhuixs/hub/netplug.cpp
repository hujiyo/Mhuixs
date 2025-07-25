#include "netplug.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <sys/resource.h>

// 全局变量
netplug_t* g_netplug = nullptr;
ConcurrentQueue<command_t*> command_queue;
BlockingReaderWriterQueue<response_t*> response_queue;
Id_alloctor Idalloc;

static uv_thread_t response_thread;
static uv_mutex_t shutdown_mutex;
static bool shutdown_flag = false;

// 安全常量
static const size_t MIN_PACKET_SIZE = 8;
static const size_t MAX_BUFFER_GROWTH = 1048576; // 1MB

// 内部函数声明
static void on_connection(uv_stream_t* server, int status);
static void on_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf);
static void on_write(uv_write_t* req, int status);
static void on_close(uv_handle_t* handle);
static void on_cleanup_timer(uv_timer_t* timer);
static void alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf);
static session_t* alloc_session();
static void free_session(session_t* session);
static int process_buffer(session_t* session);
static void cleanup_expired_sessions();
static int validate_session(session_t* session);
static void reset_session(session_t* session);
static int setup_socket_options(uv_tcp_t* handle);
static void response_thread_func(void* arg);
static void send_response(response_t* resp);

// 错误处理宏
#define CHECK_NULL_RET(ptr, ret) do { \
    if (!(ptr)) { \
        fprintf(stderr, "NULL pointer: %s:%d\n", __FILE__, __LINE__); \
        return (ret); \
    } \
} while(0)

#define SAFE_FREE(ptr) do { \
    if (ptr) { \
        free(ptr); \
        (ptr) = nullptr; \
    } \
} while(0)

// 初始化网络插件
int netplug_init(uint16_t port) {
    if (g_netplug) {
        fprintf(stderr, "netplug already initialized\n");
        return -1;
    }

    // 忽略SIGPIPE信号
    signal(SIGPIPE, SIG_IGN);

    // 设置资源限制
    struct rlimit rlim;
    if (getrlimit(RLIMIT_NOFILE, &rlim) == 0) {
        rlim.rlim_cur = rlim.rlim_max;
        setrlimit(RLIMIT_NOFILE, &rlim);
    }

    g_netplug = (netplug_t*)calloc(1, sizeof(netplug_t));
    CHECK_NULL_RET(g_netplug, -1);

    g_netplug->max_sessions = MAX_SESSIONS;

    // 初始化会话池 - 使用对齐内存分配提升性能
    size_t sessions_size = MAX_SESSIONS * sizeof(session_t);
    g_netplug->sessions = (session_t*)aligned_alloc(64, sessions_size);
    g_netplug->idle_sessions = (uint32_t*)calloc(MAX_SESSIONS, sizeof(uint32_t));

    if (!g_netplug->sessions || !g_netplug->idle_sessions) {
        SAFE_FREE(g_netplug->sessions);
        SAFE_FREE(g_netplug->idle_sessions);
        SAFE_FREE(g_netplug);
        return -1;
    }

    memset(g_netplug->sessions, 0, sessions_size);

    // 初始化空闲会话索引和缓冲区
    for (uint32_t i = 0; i < MAX_SESSIONS; i++) {
        g_netplug->idle_sessions[i] = i;
        session_t* session = &g_netplug->sessions[i];

        session->state = SESS_IDLE;
        session->pool_index = i;
        session->recv_buffer.data = (uint8_t*)aligned_alloc(64, BUFFER_SIZE);

        if (!session->recv_buffer.data) {
            // 清理已分配的缓冲区
            for (uint32_t j = 0; j < i; j++) {
                SAFE_FREE(g_netplug->sessions[j].recv_buffer.data);
            }
            SAFE_FREE(g_netplug->sessions);
            SAFE_FREE(g_netplug->idle_sessions);
            SAFE_FREE(g_netplug);
            return -1;
        }

        session->recv_buffer.capacity = BUFFER_SIZE;
        session->recv_buffer.size = 0;
        session->recv_buffer.read_pos = 0;
    }
    g_netplug->idle_count = MAX_SESSIONS;

    // 初始化互斥锁
    int ret = uv_mutex_init(&g_netplug->pool_mutex);
    if (ret != 0) {
        for (uint32_t i = 0; i < MAX_SESSIONS; i++) {
            SAFE_FREE(g_netplug->sessions[i].recv_buffer.data);
        }
        SAFE_FREE(g_netplug->sessions);
        SAFE_FREE(g_netplug->idle_sessions);
        SAFE_FREE(g_netplug);
        return -1;
    }

    // 初始化关闭标志互斥锁
    ret = uv_mutex_init(&shutdown_mutex);
    if (ret != 0) {
        uv_mutex_destroy(&g_netplug->pool_mutex);
        for (uint32_t i = 0; i < MAX_SESSIONS; i++) {
            SAFE_FREE(g_netplug->sessions[i].recv_buffer.data);
        }
        SAFE_FREE(g_netplug->sessions);
        SAFE_FREE(g_netplug->idle_sessions);
        SAFE_FREE(g_netplug);
        return -1;
    }

    // 启动回复处理线程
    ret = uv_thread_create(&response_thread, response_thread_func, nullptr);
    if (ret != 0) {
        uv_mutex_destroy(&shutdown_mutex);
        uv_mutex_destroy(&g_netplug->pool_mutex);
        for (uint32_t i = 0; i < MAX_SESSIONS; i++) {
            SAFE_FREE(g_netplug->sessions[i].recv_buffer.data);
        }
        SAFE_FREE(g_netplug->sessions);
        SAFE_FREE(g_netplug->idle_sessions);
        SAFE_FREE(g_netplug);
        return -1;
    }

    // 初始化libuv
    g_netplug->loop = uv_default_loop();
    if (!g_netplug->loop) {
        uv_mutex_destroy(&shutdown_mutex);
        uv_mutex_destroy(&g_netplug->pool_mutex);
        for (uint32_t i = 0; i < MAX_SESSIONS; i++) {
            SAFE_FREE(g_netplug->sessions[i].recv_buffer.data);
        }
        SAFE_FREE(g_netplug->sessions);
        SAFE_FREE(g_netplug->idle_sessions);
        SAFE_FREE(g_netplug);
        return -1;
    }

    ret = uv_tcp_init(g_netplug->loop, &g_netplug->server);
    if (ret != 0) {
        uv_mutex_destroy(&shutdown_mutex);
        uv_mutex_destroy(&g_netplug->pool_mutex);
        for (uint32_t i = 0; i < MAX_SESSIONS; i++) {
            SAFE_FREE(g_netplug->sessions[i].recv_buffer.data);
        }
        SAFE_FREE(g_netplug->sessions);
        SAFE_FREE(g_netplug->idle_sessions);
        SAFE_FREE(g_netplug);
        return ret;
    }

    // 设置SO_REUSEADDR
    ret = uv_tcp_simultaneous_accepts(&g_netplug->server, 1);
    if (ret != 0) {
        fprintf(stderr, "Warning: failed to set simultaneous accepts: %s\n", uv_strerror(ret));
    }

    struct sockaddr_in addr;
    ret = uv_ip4_addr("0.0.0.0", port, &addr);
    if (ret != 0) {
        uv_mutex_destroy(&shutdown_mutex);
        uv_mutex_destroy(&g_netplug->pool_mutex);
        for (uint32_t i = 0; i < MAX_SESSIONS; i++) {
            SAFE_FREE(g_netplug->sessions[i].recv_buffer.data);
        }
        SAFE_FREE(g_netplug->sessions);
        SAFE_FREE(g_netplug->idle_sessions);
        SAFE_FREE(g_netplug);
        return ret;
    }
    #ifdef UV_TCP_REUSEADDR
        ret = uv_tcp_bind(&g_netplug->server, (const sockaddr*)&addr, UV_TCP_REUSEADDR);
    #else
        ret = uv_tcp_bind(&g_netplug->server, (const sockaddr*)&addr, 0);
    #endif

    if (ret != 0) {
        uv_mutex_destroy(&shutdown_mutex);
        uv_mutex_destroy(&g_netplug->pool_mutex);
        for (uint32_t i = 0; i < MAX_SESSIONS; i++) {
            SAFE_FREE(g_netplug->sessions[i].recv_buffer.data);
        }
        SAFE_FREE(g_netplug->sessions);
        SAFE_FREE(g_netplug->idle_sessions);
        SAFE_FREE(g_netplug);
        return ret;
    }

    // 启动清理定时器
    ret = uv_timer_init(g_netplug->loop, &g_netplug->cleanup_timer);
    if (ret == 0) {
        ret = uv_timer_start(&g_netplug->cleanup_timer, on_cleanup_timer,
                           CLEANUP_INTERVAL_MS, CLEANUP_INTERVAL_MS);
    }

    if (ret != 0) {
        uv_mutex_destroy(&shutdown_mutex);
        uv_mutex_destroy(&g_netplug->pool_mutex);
        for (uint32_t i = 0; i < MAX_SESSIONS; i++) {
            SAFE_FREE(g_netplug->sessions[i].recv_buffer.data);
        }
        SAFE_FREE(g_netplug->sessions);
        SAFE_FREE(g_netplug->idle_sessions);
        SAFE_FREE(g_netplug);
        return ret;
    }

    return 0;
}

// 启动服务器
int netplug_start() {
    CHECK_NULL_RET(g_netplug, -1);

    int ret = uv_listen((uv_stream_t*)&g_netplug->server, 1024, on_connection);
    if (ret != 0) {
        fprintf(stderr, "Listen failed: %s\n", uv_strerror(ret));
        return ret;
    }

    printf("Server listening on port (max connections: %d)\n", MAX_SESSIONS);
    return uv_run(g_netplug->loop, UV_RUN_DEFAULT);
}

// 关闭服务器
void netplug_shutdown() {
    if (!g_netplug) return;

    // 设置关闭标志
    uv_mutex_lock(&shutdown_mutex);
    shutdown_flag = true;
    uv_mutex_unlock(&shutdown_mutex);

    printf("Shutting down server...\n");

    // 等待回复线程结束
    uv_thread_join(&response_thread);
    uv_mutex_destroy(&shutdown_mutex);


    // 停止接受新连接
    uv_close((uv_handle_t*)&g_netplug->server, nullptr);

    // 停止清理定时器
    uv_timer_stop(&g_netplug->cleanup_timer);
    uv_close((uv_handle_t*)&g_netplug->cleanup_timer, nullptr);

    // 关闭所有活跃会话
    uv_mutex_lock(&g_netplug->pool_mutex);
    for (uint32_t i = 0; i < MAX_SESSIONS; i++) {
        session_t* session = &g_netplug->sessions[i];
        if (session->state != SESS_IDLE && session->state != SESS_USING) {
            uv_close((uv_handle_t*)&session->tcp_handle, on_close);
        }
    }
    uv_mutex_unlock(&g_netplug->pool_mutex);

    // 运行事件循环直到所有句柄都关闭
    uv_run(g_netplug->loop, UV_RUN_DEFAULT);

    // 清理资源
    uv_mutex_destroy(&g_netplug->pool_mutex);

    for (uint32_t i = 0; i < MAX_SESSIONS; i++) {
        SAFE_FREE(g_netplug->sessions[i].recv_buffer.data);
    }

    SAFE_FREE(g_netplug->sessions);
    SAFE_FREE(g_netplug->idle_sessions);
    SAFE_FREE(g_netplug);

    printf("Server shutdown complete\n");
}

// 新连接回调
static void on_connection(uv_stream_t* server, int status) {
    if (status < 0) {
        fprintf(stderr, "Connection error: %s\n", uv_strerror(status));
        return;
    }

    session_t* session = alloc_session();
    if (!session) {
        fprintf(stderr, "Warning: Max sessions reached, rejecting connection\n");
        return;
    }

    int ret = uv_tcp_init(g_netplug->loop, &session->tcp_handle);
    if (ret != 0) {
        fprintf(stderr, "TCP init failed: %s\n", uv_strerror(ret));
        free_session(session);
        return;
    }

    session->tcp_handle.data = session;

    ret = uv_accept(server, (uv_stream_t*)&session->tcp_handle);
    if (ret == 0) {
        // 设置socket选项
        if (setup_socket_options(&session->tcp_handle) != 0) {
            fprintf(stderr, "Warning: Failed to set socket options\n");
        }

        session->session_id = Idalloc.get_sid();
        session->user_id = 65536; // 默认未认证用户ID
        session->state = SESS_ALIVE;
        session->last_activity = uv_now(g_netplug->loop);

        ret = uv_read_start((uv_stream_t*)&session->tcp_handle, alloc_buffer, on_read);
        if (ret != 0) {
            fprintf(stderr, "Read start failed: %s\n", uv_strerror(ret));
            uv_close((uv_handle_t*)&session->tcp_handle, on_close);
        }
    } else {
        fprintf(stderr, "Accept failed: %s\n", uv_strerror(ret));
        free_session(session);
    }
}

// 设置socket选项
static int setup_socket_options(uv_tcp_t* handle) {
    int ret = 0;

    // 启用TCP_NODELAY以减少延迟
    ret = uv_tcp_nodelay(handle, 1);
    if (ret != 0) return ret;

    // 设置keepalive
    ret = uv_tcp_keepalive(handle, 1, 60);
    if (ret != 0) return ret;

    return 0;
}

// 分配缓冲区回调 - 优化内存分配策略
static void alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
    session_t* session = (session_t*)handle->data;
    if (!session || validate_session(session) != 0) {
        buf->base = nullptr;
        buf->len = 0;
        return;
    }

    buffer_t* buffer = &session->recv_buffer;
    uint32_t available = buffer->capacity - buffer->size;

    // 如果可用空间不足，尝试压缩缓冲区
    if (available < 1024 && buffer->read_pos > 0) {
        memmove(buffer->data, buffer->data + buffer->read_pos,
                buffer->size - buffer->read_pos);
        buffer->size -= buffer->read_pos;
        buffer->read_pos = 0;
        available = buffer->capacity - buffer->size;
    }

    // 如果仍然空间不足且未达到最大限制，扩容
    if (available < 1024 && buffer->capacity < MAX_BUFFER_GROWTH) {
        uint32_t new_capacity = buffer->capacity * 2;
        if (new_capacity > MAX_BUFFER_GROWTH) {
            new_capacity = MAX_BUFFER_GROWTH;
        }

        uint8_t* new_data = (uint8_t*)realloc(buffer->data, new_capacity);
        if (new_data) {
            buffer->data = new_data;
            buffer->capacity = new_capacity;
            available = new_capacity - buffer->size;
        } else {
            // 内存分配失败，关闭连接
            buf->base = nullptr;
            buf->len = 0;
            return;
        }
    }

    if (available > 0) {
        buf->base = (char*)(buffer->data + buffer->size);
        buf->len = (available > suggested_size) ? suggested_size : available;
    } else {
        buf->base = nullptr;
        buf->len = 0;
    }
}

// 读取数据回调 - 增强错误处理和安全检查
static void on_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
    session_t* session = (session_t*)stream->data;
    if (!session || validate_session(session) != 0) {
        if (nread != UV_EOF) {
            uv_close((uv_handle_t*)stream, on_close);
        }
        return;
    }

    if (nread < 0) {
        if (nread != UV_EOF) {
            fprintf(stderr, "Read error: %s\n", uv_strerror(nread));
        }
        uv_close((uv_handle_t*)stream, on_close);
        return;
    }

    if (nread > 0) {
        // 防止缓冲区溢出
        if (session->recv_buffer.size + nread > session->recv_buffer.capacity) {
            fprintf(stderr, "Buffer overflow detected, closing connection\n");
            uv_close((uv_handle_t*)stream, on_close);
            return;
        }

        session->recv_buffer.size += nread;
        session->last_activity = uv_now(g_netplug->loop);

        if (process_buffer(session) != 0) {
            uv_close((uv_handle_t*)stream, on_close);
        }
    }
}

// 写入完成回调 - 改进内存管理
static void on_write(uv_write_t* req, int status) {
    if (status < 0 ) {
        fprintf(stderr, "Write error: %s\n", uv_strerror(status));
    }
    if ( !req ) {
        return;
    }

    response_t* resp = (response_t*)req->data;
    if (resp) {
        // 如果使用了外部分配的内存，释放它
        if (resp->response_len > sizeof(resp->inline_data)) {
            SAFE_FREE(resp->data);
        }
        SAFE_FREE(resp);
    }
    SAFE_FREE(req);
}

// 连接关闭回调
static void on_close(uv_handle_t* handle) {
    session_t* session = (session_t*)handle->data;
    if (session) {
        free_session(session);
    }
}

// 清理定时器回调 - 优化性能
static void on_cleanup_timer(uv_timer_t* timer) {
    cleanup_expired_sessions();
}

// 清理过期会话 - 独立函数提高可读性
static void cleanup_expired_sessions() {
    if (!g_netplug) return;

    uint64_t now = uv_now(g_netplug->loop);
    uint32_t closed_count = 0;

    uv_mutex_lock(&g_netplug->pool_mutex);

    for (uint32_t i = 0; i < MAX_SESSIONS; i++) {
        session_t* session = &g_netplug->sessions[i];
        if (session->state == SESS_ALIVE &&
            (now - session->last_activity) > TIMEOUT_MS) {
            uv_close((uv_handle_t*)&session->tcp_handle, on_close);
            closed_count++;
        }
    }

    uv_mutex_unlock(&g_netplug->pool_mutex);

    if (closed_count > 0) {
        printf("Cleaned up %u expired sessions\n", closed_count);
    }
}

// 会话验证
static int validate_session(session_t* session) {
    if (!session || !g_netplug) return -1;

    // 检查会话是否在有效范围内
    if (session < g_netplug->sessions ||
        session >= g_netplug->sessions + MAX_SESSIONS) {
        return -1;
    }

    // 检查状态
    if (session->state != SESS_ALIVE && session->state != SESS_USING) {
        return -1;
    }

    return 0;
}

// 重置会话状态
static void reset_session(session_t* session) {
    if (!session) return;

    session->recv_buffer.size = 0;
    session->recv_buffer.read_pos = 0;
    session->session_id = 0;
    session->user_id = 0;
    session->last_activity = 0;
}

// 分配会话 - 增强线程安全
static session_t* alloc_session() {
    if (!g_netplug) return nullptr;

    uv_mutex_lock(&g_netplug->pool_mutex);

    if (g_netplug->idle_count == 0) {
        uv_mutex_unlock(&g_netplug->pool_mutex);
        return nullptr;
    }

    uint32_t index = g_netplug->idle_sessions[--g_netplug->idle_count];
    session_t* session = &g_netplug->sessions[index];

    // 设置状态为使用中
    session->state = SESS_USING;

    uv_mutex_unlock(&g_netplug->pool_mutex);

    // 重置会话数据
    reset_session(session);

    return session;
}

// 释放会话 - 增强安全性
static void free_session(session_t* session) {
    if (!session || !g_netplug) return;

    uv_mutex_lock(&g_netplug->pool_mutex);

    // 验证会话索引
    if (session->pool_index >= MAX_SESSIONS) {
        uv_mutex_unlock(&g_netplug->pool_mutex);
        fprintf(stderr, "Invalid session pool index: %u\n", session->pool_index);
        return;
    }

    reset_session(session);
    session->state = SESS_IDLE;

    // 防止重复释放
    if (g_netplug->idle_count < MAX_SESSIONS) {
        g_netplug->idle_sessions[g_netplug->idle_count++] = session->pool_index;
    }

    uv_mutex_unlock(&g_netplug->pool_mutex);
}

// 处理接收缓冲区 - 增强安全性和错误处理
static int process_buffer(session_t* session) {
    if (!session) return -1;

    buffer_t* buffer = &session->recv_buffer;
    int processed_packets = 0;
    const int MAX_PACKETS_PER_CALL = 10; // 防止单次处理过多包

    while (buffer->size - buffer->read_pos >= MIN_PACKET_SIZE &&
           processed_packets < MAX_PACKETS_PER_CALL) {

        uint32_t remaining = buffer->size - buffer->read_pos;
        uint32_t start_index, packet_size;

        // 查找包边界
        if (!find_packet_boundary(buffer->data + buffer->read_pos,
                                remaining, &start_index, &packet_size)) {
            break;
        }

        // 安全检查
        if (packet_size > MAX_PACKET_SIZE || packet_size < MIN_PACKET_SIZE) {
            fprintf(stderr, "Invalid packet size: %u\n", packet_size);
            return -1;
        }

        if (start_index + packet_size > remaining) {
            break; // 包不完整
        }

        // 解包
        str unpacked = unpacking(buffer->data + buffer->read_pos + start_index, packet_size);
        if (unpacked.string && unpacked.len >= sizeof(uint32_t)) {
            uint32_t command_id;
            memcpy(&command_id, unpacked.string, sizeof(uint32_t));

            command_t* cmd = (command_t*)calloc(1, sizeof(command_t));
            if (cmd) {
                cmd->session = session;
                cmd->command_id = (CommandNumber)ntohl(command_id);

                if (!command_queue.enqueue(cmd)) {
                    SAFE_FREE(cmd);
                    SAFE_FREE(unpacked.string);
                    fprintf(stderr, "Failed to enqueue command\n");
                    return -1;
                }
            } else {
                SAFE_FREE(unpacked.string);
                return -1;
            }

            SAFE_FREE(unpacked.string);
        }

        buffer->read_pos += start_index + packet_size;
        processed_packets++;
    }

    // 压缩缓冲区 - 仅在必要时进行
    if (buffer->read_pos > buffer->capacity / 2) {
        uint32_t remaining = buffer->size - buffer->read_pos;
        if (remaining > 0) {
            memmove(buffer->data, buffer->data + buffer->read_pos, remaining);
        }
        buffer->size = remaining;
        buffer->read_pos = 0;
    }

    return 0;
}

// 认证会话 - 增强安全性
int auth_session(SID session_id, UID uid) {
    if (!g_netplug || session_id == 0) return -1;

    uv_mutex_lock(&g_netplug->pool_mutex);

    for (uint32_t i = 0; i < MAX_SESSIONS; i++) {
        session_t* session = &g_netplug->sessions[i];
        if (session->session_id == session_id && session->state == SESS_ALIVE) {
            session->user_id = uid;
            session->last_activity = uv_now(g_netplug->loop);
            uv_mutex_unlock(&g_netplug->pool_mutex);
            return 0;
        }
    }

    uv_mutex_unlock(&g_netplug->pool_mutex);
    return -1;
}

// 添加回复处理线程函数：
static void response_thread_func(void* arg) {
    response_t* resp;

    while (true) {
        // 检查关闭标志
        uv_mutex_lock(&shutdown_mutex);
        bool should_exit = shutdown_flag;
        uv_mutex_unlock(&shutdown_mutex);

        if (should_exit) break;

        // 阻塞等待回复（BlockingReaderWriterQueue自带条件变量）
        response_queue.wait_dequeue(resp);
        if (resp && resp->session && validate_session(resp->session) == 0) {
            send_response(resp);
        } else {
            // 清理无效回复
            if (resp) {
                if (resp->response_len > sizeof(resp->inline_data)) {
                    SAFE_FREE(resp->data);
                }
                SAFE_FREE(resp);
            }
        }
    }
}

static void send_response(response_t* resp) {
    if (!resp)return;
    uv_write_t* req = (uv_write_t*)malloc(sizeof(uv_write_t));
    if (!req) {
        if (resp->response_len > sizeof(resp->inline_data)) {
            SAFE_FREE(resp->data);
        }
        SAFE_FREE(resp);
        return;
    }

    req->data = resp;

    uv_buf_t buf;
    if (resp->response_len <= sizeof(resp->inline_data)) {
        buf = uv_buf_init((char*)resp->inline_data, resp->response_len);
    } else {
        buf = uv_buf_init((char*)resp->data, resp->response_len);
    }

    int ret = uv_write(req, (uv_stream_t*)&resp->session->tcp_handle, &buf, 1, on_write);
    if (ret != 0) {
        fprintf(stderr, "Write failed: %s\n", uv_strerror(ret));
        if (resp->response_len > sizeof(resp->inline_data)) {
            SAFE_FREE(resp->data);
        }
        SAFE_FREE(resp);
        SAFE_FREE(req);
    }
}