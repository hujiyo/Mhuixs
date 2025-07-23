#include "netplug.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// 全局变量
netplug_t* g_netplug = nullptr;
ConcurrentQueue<command_t*> command_queue;
BlockingReaderWriterQueue<response_t*> response_queue;

// 内部函数声明
static void on_connection(uv_stream_t* server, int status);
static void on_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf);
static void on_write(uv_write_t* req, int status);
static void on_close(uv_handle_t* handle);
static void on_cleanup_timer(uv_timer_t* timer);
static void alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf);
static void worker_thread(void* arg);
static void response_thread(void* arg);
static session_t* alloc_session();
static void free_session(session_t* session);
static void process_buffer(session_t* session);
static int send_data(session_t* session, const uint8_t* data, uint32_t len);

// 初始化网络插件
int netplug_init(uint16_t port) {
    g_netplug = (netplug_t*)calloc(1, sizeof(netplug_t));
    if (!g_netplug) return -1;
    
    g_netplug->max_sessions = MAX_SESSIONS;
    g_netplug->running = true;
    
    // 初始化会话池
    g_netplug->sessions = (session_t*)calloc(MAX_SESSIONS, sizeof(session_t));
    g_netplug->idle_sessions = (uint32_t*)calloc(MAX_SESSIONS, sizeof(uint32_t));
    if (!g_netplug->sessions || !g_netplug->idle_sessions) {
        free(g_netplug);
        return -1;
    }
    
    // 初始化空闲会话索引
    for (uint32_t i = 0; i < MAX_SESSIONS; i++) {
        g_netplug->idle_sessions[i] = i;
        g_netplug->sessions[i].state = SESS_IDLE;
        g_netplug->sessions[i].pool_index = i;
        g_netplug->sessions[i].recv_buffer.data = (uint8_t*)malloc(BUFFER_SIZE);
        g_netplug->sessions[i].recv_buffer.capacity = BUFFER_SIZE;
    }
    g_netplug->idle_count = MAX_SESSIONS;
    
    uv_mutex_init(&g_netplug->pool_mutex);
    
    // 初始化libuv
    g_netplug->loop = uv_default_loop();
    uv_tcp_init(g_netplug->loop, &g_netplug->server);
    
    struct sockaddr_in addr;
    uv_ip4_addr("0.0.0.0", port, &addr);
    uv_tcp_bind(&g_netplug->server, (const struct sockaddr*)&addr, 0);
    
    // 启动清理定时器
    uv_timer_init(g_netplug->loop, &g_netplug->cleanup_timer);
    uv_timer_start(&g_netplug->cleanup_timer, on_cleanup_timer, CLEANUP_INTERVAL_MS, CLEANUP_INTERVAL_MS);
    
    // 启动工作线程
    for (int i = 0; i < 4; i++) {
        uv_thread_create(&g_netplug->worker_threads[i], worker_thread, nullptr);
    }
    
    return 0;
}

// 启动服务器
int netplug_start() {
    if (!g_netplug) return -1;
    
    int ret = uv_listen((uv_stream_t*)&g_netplug->server, 128, on_connection);
    if (ret) return ret;
    
    printf("Server listening on port\n");
    return uv_run(g_netplug->loop, UV_RUN_DEFAULT);
}

// 关闭服务器
void netplug_shutdown() {
    if (!g_netplug) return;
    
    g_netplug->running = false;
    
    // 关闭所有会话
    uv_mutex_lock(&g_netplug->pool_mutex);
    for (uint32_t i = 0; i < MAX_SESSIONS; i++) {
        if (g_netplug->sessions[i].state != SESS_IDLE) {
            uv_close((uv_handle_t*)&g_netplug->sessions[i].tcp_handle, on_close);
        }
        free(g_netplug->sessions[i].recv_buffer.data);
    }
    uv_mutex_unlock(&g_netplug->pool_mutex);
    
    uv_close((uv_handle_t*)&g_netplug->server, nullptr);
    uv_timer_stop(&g_netplug->cleanup_timer);
    uv_close((uv_handle_t*)&g_netplug->cleanup_timer, nullptr);
    
    // 等待工作线程结束
    for (int i = 0; i < 4; i++) {
        uv_thread_join(&g_netplug->worker_threads[i]);
    }
    
    uv_mutex_destroy(&g_netplug->pool_mutex);
    free(g_netplug->sessions);
    free(g_netplug->idle_sessions);
    free(g_netplug);
    g_netplug = nullptr;
}

// 新连接回调
static void on_connection(uv_stream_t* server, int status) {
    if (status < 0) return;
    
    session_t* session = alloc_session();
    if (!session) return;
    
    uv_tcp_init(g_netplug->loop, &session->tcp_handle);
    session->tcp_handle.data = session;
    
    if (uv_accept(server, (uv_stream_t*)&session->tcp_handle) == 0) {
        session->session_id = Idalloc.get_sid();
        session->user_id = 65536;
        session->state = SESS_ALIVE;
        session->last_activity = uv_now(g_netplug->loop);
        
        uv_read_start((uv_stream_t*)&session->tcp_handle, alloc_buffer, on_read);
    } else {
        free_session(session);
    }
}

// 分配缓冲区回调
static void alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
    session_t* session = (session_t*)handle->data;
    if (!session) {
        buf->base = nullptr;
        buf->len = 0;
        return;
    }
    
    buffer_t* buffer = &session->recv_buffer;
    uint32_t available = buffer->capacity - buffer->size;
    
    if (available < 1024) {
        // 扩容缓冲区
        uint32_t new_capacity = buffer->capacity * 2;
        uint8_t* new_data = (uint8_t*)realloc(buffer->data, new_capacity);
        if (new_data) {
            buffer->data = new_data;
            buffer->capacity = new_capacity;
            available = new_capacity - buffer->size;
        }
    }
    
    buf->base = (char*)(buffer->data + buffer->size);
    buf->len = available;
}

// 读取数据回调
static void on_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
    session_t* session = (session_t*)stream->data;
    if (!session) return;
    
    if (nread < 0) {
        uv_close((uv_handle_t*)stream, on_close);
        return;
    }
    
    if (nread > 0) {
        session->recv_buffer.size += nread;
        session->last_activity = uv_now(g_netplug->loop);
        process_buffer(session);
    }
}

// 写入完成回调
static void on_write(uv_write_t* req, int status) {
    response_t* resp = (response_t*)req->data;
    if (resp) {
        if (resp->response_len > 48) {
            free(resp->data);
        }
        free(resp);
    }
    free(req);
}

// 连接关闭回调
static void on_close(uv_handle_t* handle) {
    session_t* session = (session_t*)handle->data;
    if (session) {
        free_session(session);
    }
}

// 清理定时器回调
static void on_cleanup_timer(uv_timer_t* timer) {
    if (!g_netplug) return;
    
    uint64_t now = uv_now(g_netplug->loop);
    uv_mutex_lock(&g_netplug->pool_mutex);
    
    for (uint32_t i = 0; i < MAX_SESSIONS; i++) {
        session_t* session = &g_netplug->sessions[i];
        if (session->state == SESS_ALIVE && 
            (now - session->last_activity) > TIMEOUT_MS) {
            uv_close((uv_handle_t*)&session->tcp_handle, on_close);
        }
    }
    
    uv_mutex_unlock(&g_netplug->pool_mutex);
}

// 分配会话
static session_t* alloc_session() {
    uv_mutex_lock(&g_netplug->pool_mutex);
    
    if (g_netplug->idle_count == 0) {
        uv_mutex_unlock(&g_netplug->pool_mutex);
        return nullptr;
    }
    
    uint32_t index = g_netplug->idle_sessions[--g_netplug->idle_count];
    session_t* session = &g_netplug->sessions[index];
    
    uv_mutex_unlock(&g_netplug->pool_mutex);
    
    // 重置会话
    session->recv_buffer.size = 0;
    session->recv_buffer.read_pos = 0;
    session->state = SESS_USING;
    
    return session;
}

// 释放会话
static void free_session(session_t* session) {
    if (!session) return;
    
    uv_mutex_lock(&g_netplug->pool_mutex);
    
    session->state = SESS_IDLE;
    session->session_id = 0;
    session->user_id = 0;
    session->recv_buffer.size = 0;
    session->recv_buffer.read_pos = 0;
    
    g_netplug->idle_sessions[g_netplug->idle_count++] = session->pool_index;
    
    uv_mutex_unlock(&g_netplug->pool_mutex);
}

// 处理接收缓冲区
static void process_buffer(session_t* session) {
    buffer_t* buffer = &session->recv_buffer;
    
    while (buffer->size - buffer->read_pos >= 8) { // 假设包头至少8字节
        uint32_t start_index, packet_size;
        if (find_packet_boundary(buffer->data + buffer->read_pos, 
                                buffer->size - buffer->read_pos, 
                                &start_index, &packet_size)) {
            
            // 解包
            str unpacked = unpacking(buffer->data + buffer->read_pos + start_index, packet_size);
            if (unpacked.string && unpacked.len >= 8) {
                uint32_t command_id;
                memcpy(&command_id, unpacked.string, 4);
                
                command_t* cmd = (command_t*)calloc(1, sizeof(command_t));
                if (cmd) {
                    cmd->session = session;
                    cmd->command_id = (CommandNumber)ntohl(command_id);
                    command_queue.enqueue(cmd);
                }
                
                free(unpacked.string);
            }
            
            buffer->read_pos += start_index + packet_size;
        } else {
            break;
        }
    }
    
    // 压缩缓冲区
    if (buffer->read_pos > 0) {
        memmove(buffer->data, buffer->data + buffer->read_pos, 
                buffer->size - buffer->read_pos);
        buffer->size -= buffer->read_pos;
        buffer->read_pos = 0;
    }
}

// 工作线程
static void worker_thread(void* arg) {
    while (g_netplug->running) {
        command_t* cmd = nullptr;
        if (command_queue.try_dequeue(cmd)) {
            if (cmd) {
                // 处理命令
                free(cmd);
            }
        } else {
            uv_sleep(1);
        }
    }
}

// 响应线程
static void response_thread(void* arg) {
    while (g_netplug->running) {
        response_t* resp = nullptr;
        if (response_queue.try_dequeue(resp)) {
            if (resp && resp->session) {
                uint8_t* data = resp->response_len > 48 ? resp->data : resp->inline_data;
                send_data(resp->session, data, resp->response_len);
            }
        } else {
            uv_sleep(1);
        }
    }
}

// 发送数据
static int send_data(session_t* session, const uint8_t* data, uint32_t len) {
    if (!session || session->state != SESS_ALIVE) return -1;
    
    uv_write_t* req = (uv_write_t*)malloc(sizeof(uv_write_t));
    if (!req) return -1;
    
    uv_buf_t buf = uv_buf_init((char*)data, len);
    req->data = nullptr;
    
    return uv_write(req, (uv_stream_t*)&session->tcp_handle, &buf, 1, on_write);
}

// 认证会话
int auth_session(SID session_id, UID uid) {
    uv_mutex_lock(&g_netplug->pool_mutex);
    
    for (uint32_t i = 0; i < MAX_SESSIONS; i++) {
        session_t* session = &g_netplug->sessions[i];
        if (session->session_id == session_id && session->state == SESS_ALIVE) {
            session->user_id = uid;
            uv_mutex_unlock(&g_netplug->pool_mutex);
            return 0;
        }
    }
    
    uv_mutex_unlock(&g_netplug->pool_mutex);
    return -1;
}

// 发送响应
int send_response(session_t *session, uint8_t* response_data, uint32_t response_len) {
    if (!session || !response_data || response_len == 0) return -1;
    
    response_t* response = (response_t*)calloc(1, sizeof(response_t));
    if (!response) return -1;
    
    response->session = session;
    response->response_len = response_len;
    response->sent_len = 0;
    
    if (response_len > 48) {
        response->data = response_data;
    } else {
        memcpy(response->inline_data, response_data, response_len);
        free(response_data);
    }
    
    response_queue.enqueue(response);
    return 0;
}