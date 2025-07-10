#include "session.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*
 * 执行模块接口实现
 * 为执行模块提供会话管理和权限验证功能
 */

// 外部全局变量声明
extern network_manager_t* g_network_manager;
extern session_pool_t* g_session_pool;
extern pthread_mutex_t g_global_mutex;

//=============================================================================
// 会话网络操作实现
//=============================================================================

int session_receive_data(session_t* session) {
    if (!session || session->socket_fd < 0) return SESS_INVALID;
    
    uint8_t buffer[8192];
    ssize_t bytes_received = recv(session->socket_fd, buffer, sizeof(buffer), MSG_DONTWAIT);
    
    if (bytes_received > 0) {
        // 写入接收缓冲区
        if (buffer_write(session->recv_buffer, buffer, bytes_received) == SESS_OK) {
            session->stats.bytes_received += bytes_received;
            session_update_activity(session);
            return bytes_received;
        }
        return SESS_ERROR;
    } else if (bytes_received == 0) {
        // 连接关闭
        return SESS_ERROR;
    } else {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0; // 暂时无数据
        }
        return SESS_ERROR;
    }
}

int session_send_data(session_t* session) {
    if (!session || session->socket_fd < 0) return SESS_INVALID;
    
    uint32_t available = get_buffer_available_size(session->send_buffer);
    if (available == 0) return 0;
    
    uint8_t buffer[8192];
    uint32_t to_send = (available > sizeof(buffer)) ? sizeof(buffer) : available;
    
    uint32_t bytes_read = buffer_read(session->send_buffer, buffer, to_send);
    if (bytes_read == 0) return 0;
    
    ssize_t bytes_sent = send(session->socket_fd, buffer, bytes_read, MSG_DONTWAIT);
    
    if (bytes_sent > 0) {
        session->stats.bytes_sent += bytes_sent;
        session_update_activity(session);
        
        // 如果没有发送完全，需要将剩余数据放回缓冲区
        if (bytes_sent < bytes_read) {
            // 简化处理：重新写入未发送的数据
            buffer_write(session->send_buffer, buffer + bytes_sent, bytes_read - bytes_sent);
        }
        
        return bytes_sent;
    } else if (bytes_sent == 0) {
        return 0;
    } else {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // 重新写入数据
            buffer_write(session->send_buffer, buffer, bytes_read);
            return 0;
        }
        return SESS_ERROR;
    }
}

int session_process_incoming_packets(session_t* session) {
    if (!session) return SESS_INVALID;
    
    // 简化的数据包处理
    uint32_t available = get_buffer_available_size(session->recv_buffer);
    if (available < sizeof(PKG)) return 0;
    
    uint8_t buffer[8192];
    uint32_t bytes_read = buffer_peek(session->recv_buffer, buffer, sizeof(buffer));
    
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
                    session->stats.commands_queued++;
                    
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

//=============================================================================
// 会话命令操作实现
//=============================================================================

int session_enqueue_command(session_t* session, command_t* cmd) {
    if (!session || !cmd) return SESS_INVALID;
    
    return command_queue_push(session->command_queue, cmd);
}

command_t* session_dequeue_command(session_t* session) {
    if (!session) return NULL;
    
    return command_queue_try_pop(session->command_queue);
}

int session_has_pending_commands(session_t* session) {
    if (!session) return 0;
    
    return command_queue_size(session->command_queue) > 0;
}

//=============================================================================
// 会话状态管理实现
//=============================================================================

void session_set_state(session_t* session, session_state_t new_state) {
    if (!session) return;
    
    pthread_mutex_lock(&session->session_mutex);
    session->state = new_state;
    session_update_activity(session);
    pthread_mutex_unlock(&session->session_mutex);
}

void session_set_priority(session_t* session, session_priority_t new_priority) {
    if (!session) return;
    
    pthread_mutex_lock(&session->session_mutex);
    session->priority = new_priority;
    pthread_mutex_unlock(&session->session_mutex);
}

void session_update_activity(session_t* session) {
    if (!session) return;
    
    session->last_activity = time(NULL);
    session->stats.last_activity = session->last_activity;
}

//=============================================================================
// 会话健康检查实现
//=============================================================================

int session_is_healthy(session_t* session) {
    if (!session) return 0;
    
    // 检查套接字状态
    if (session->socket_fd < 0) return 0;
    
    // 检查状态
    if (session->state == SESS_ERROR || session->state == SESS_IDLE) return 0;
    
    // 检查错误次数
    if (session->stats.errors_count >= session->config.max_errors) return 0;
    
    return 1;
}

int session_is_idle_timeout(session_t* session) {
    if (!session) return 1;
    
    time_t current_time = time(NULL);
    return (current_time - session->last_activity) > session->config.idle_timeout;
}

int session_should_close(session_t* session) {
    if (!session) return 1;
    
    return !session_is_healthy(session) || session_is_idle_timeout(session);
}

//=============================================================================
// 执行模块接口实现
//=============================================================================

session_t* get_session(SID session_id) {
    if (!g_session_pool) return NULL;
    
    return session_pool_find_session(g_session_pool, session_id);
}

uint32_t get_sessions_with_commands(SID* session_ids, uint32_t max_count) {
    if (!g_session_pool) return 0;
    
    return session_pool_get_sessions_with_commands(g_session_pool, session_ids, max_count);
}

command_t* get_next_command_from_session(SID session_id) {
    session_t* session = get_session(session_id);
    if (!session) return NULL;
    
    command_t* cmd = session_dequeue_command(session);
    if (cmd) {
        session->stats.commands_processed++;
    }
    
    return cmd;
}

int send_response_to_session(SID session_id, const uint8_t* response, uint32_t response_len) {
    session_t* session = get_session(session_id);
    if (!session || !response || response_len == 0) return SESS_INVALID;
    
    return buffer_write(session->send_buffer, response, response_len);
}

//=============================================================================
// 执行模块友元接口实现 - 认证和权限管理
//=============================================================================

int authenticate_session(SID session_id, UID uid, GID gid, 
                        const char* username, const char* auth_token) {
    session_t* session = get_session(session_id);
    if (!session) return SESS_INVALID;
    
    pthread_mutex_lock(&session->session_mutex);
    
    session->auth_info.user_id = uid;
    session->auth_info.group_id = gid;
    session->auth_info.is_authenticated = 1;
    session->auth_info.is_guest = (uid == 0);
    session->auth_info.auth_time = time(NULL);
    
    if (username) {
        strncpy(session->auth_info.username, username, sizeof(session->auth_info.username) - 1);
        session->auth_info.username[sizeof(session->auth_info.username) - 1] = '\0';
    }
    
    if (auth_token) {
        strncpy(session->auth_info.auth_token, auth_token, sizeof(session->auth_info.auth_token) - 1);
        session->auth_info.auth_token[sizeof(session->auth_info.auth_token) - 1] = '\0';
    }
    
    // 根据用户权限设置会话优先级
    if (uid == 1) { // 假设uid=1是管理员
        session->priority = PRIORITY_SUPER;
        session->state = SESS_SUPER;
    } else {
        session->priority = PRIORITY_NORMAL;
    }
    
    pthread_mutex_unlock(&session->session_mutex);
    
    return SESS_OK;
}

int promote_guest_session(SID session_id, UID uid, const char* username) {
    session_t* session = get_session(session_id);
    if (!session) return SESS_INVALID;
    
    pthread_mutex_lock(&session->session_mutex);
    
    if (!session->auth_info.is_guest) {
        pthread_mutex_unlock(&session->session_mutex);
        return SESS_ERROR; // 不是游客会话
    }
    
    session->auth_info.user_id = uid;
    session->auth_info.is_guest = 0;
    session->auth_info.is_authenticated = 1;
    session->auth_info.auth_time = time(NULL);
    
    if (username) {
        strncpy(session->auth_info.username, username, sizeof(session->auth_info.username) - 1);
        session->auth_info.username[sizeof(session->auth_info.username) - 1] = '\0';
    }
    
    pthread_mutex_unlock(&session->session_mutex);
    
    return SESS_OK;
}

void invalidate_session_auth(SID session_id) {
    session_t* session = get_session(session_id);
    if (!session) return;
    
    pthread_mutex_lock(&session->session_mutex);
    
    session->auth_info.is_authenticated = 0;
    session->auth_info.is_guest = 1;
    session->auth_info.user_id = 0;
    session->auth_info.group_id = 0;
    session->auth_info.group_count = 0;
    
    strcpy(session->auth_info.username, "guest");
    memset(session->auth_info.auth_token, 0, sizeof(session->auth_info.auth_token));
    
    session->priority = PRIORITY_NORMAL;
    if (session->state == SESS_SUPER) {
        session->state = SESS_ALIVE;
    }
    
    pthread_mutex_unlock(&session->session_mutex);
}

//=============================================================================
// 权限管理接口实现
//=============================================================================

int add_session_to_group(SID session_id, GID group_id) {
    session_t* session = get_session(session_id);
    if (!session) return SESS_INVALID;
    
    pthread_mutex_lock(&session->session_mutex);
    
    if (session->auth_info.group_count >= 16) {
        pthread_mutex_unlock(&session->session_mutex);
        return SESS_FULL;
    }
    
    // 检查是否已经在组中
    for (uint32_t i = 0; i < session->auth_info.group_count; i++) {
        if (session->auth_info.additional_groups[i] == group_id) {
            pthread_mutex_unlock(&session->session_mutex);
            return SESS_OK; // 已经在组中
        }
    }
    
    session->auth_info.additional_groups[session->auth_info.group_count] = group_id;
    session->auth_info.group_count++;
    
    pthread_mutex_unlock(&session->session_mutex);
    
    return SESS_OK;
}

int remove_session_from_group(SID session_id, GID group_id) {
    session_t* session = get_session(session_id);
    if (!session) return SESS_INVALID;
    
    pthread_mutex_lock(&session->session_mutex);
    
    for (uint32_t i = 0; i < session->auth_info.group_count; i++) {
        if (session->auth_info.additional_groups[i] == group_id) {
            // 移动后面的元素向前
            for (uint32_t j = i; j < session->auth_info.group_count - 1; j++) {
                session->auth_info.additional_groups[j] = session->auth_info.additional_groups[j + 1];
            }
            session->auth_info.group_count--;
            pthread_mutex_unlock(&session->session_mutex);
            return SESS_OK;
        }
    }
    
    pthread_mutex_unlock(&session->session_mutex);
    return SESS_NOT_FOUND;
}

int update_session_permissions(SID session_id, const GID* groups, uint32_t group_count) {
    session_t* session = get_session(session_id);
    if (!session || !groups || group_count > 16) return SESS_INVALID;
    
    pthread_mutex_lock(&session->session_mutex);
    
    session->auth_info.group_count = group_count;
    for (uint32_t i = 0; i < group_count; i++) {
        session->auth_info.additional_groups[i] = groups[i];
    }
    
    pthread_mutex_unlock(&session->session_mutex);
    
    return SESS_OK;
}

//=============================================================================
// 权限检查接口实现
//=============================================================================

int check_session_permission(SID session_id, const char* object_name, Mode_type mode) {
    session_t* session = get_session(session_id);
    if (!session || !object_name) return 0;
    
    pthread_mutex_lock(&session->session_mutex);
    
    if (!session->auth_info.is_authenticated) {
        pthread_mutex_unlock(&session->session_mutex);
        return 0;
    }
    
    UID uid = session->auth_info.user_id;
    GID gid = session->auth_info.group_id;
    
    pthread_mutex_unlock(&session->session_mutex);
    
    // 调用权限系统检查
    return check_permission(uid, gid, object_name, mode);
}

int check_session_group_permission(SID session_id, GID group_id, Mode_type mode) {
    session_t* session = get_session(session_id);
    if (!session) return 0;
    
    pthread_mutex_lock(&session->session_mutex);
    
    if (!session->auth_info.is_authenticated) {
        pthread_mutex_unlock(&session->session_mutex);
        return 0;
    }
    
    // 检查主组
    if (session->auth_info.group_id == group_id) {
        pthread_mutex_unlock(&session->session_mutex);
        return 1;
    }
    
    // 检查附加组
    for (uint32_t i = 0; i < session->auth_info.group_count; i++) {
        if (session->auth_info.additional_groups[i] == group_id) {
            pthread_mutex_unlock(&session->session_mutex);
            return 1;
        }
    }
    
    pthread_mutex_unlock(&session->session_mutex);
    return 0;
}

int is_session_object_owner(SID session_id, const char* object_name) {
    session_t* session = get_session(session_id);
    if (!session || !object_name) return 0;
    
    pthread_mutex_lock(&session->session_mutex);
    
    if (!session->auth_info.is_authenticated) {
        pthread_mutex_unlock(&session->session_mutex);
        return 0;
    }
    
    UID uid = session->auth_info.user_id;
    pthread_mutex_unlock(&session->session_mutex);
    
    // 调用权限系统检查所有者
    return check_owner(uid, object_name);
}

//=============================================================================
// 会话状态查询接口实现
//=============================================================================

user_auth_info_t get_session_auth_info(SID session_id) {
    user_auth_info_t auth_info;
    memset(&auth_info, 0, sizeof(auth_info));
    
    session_t* session = get_session(session_id);
    if (!session) return auth_info;
    
    pthread_mutex_lock(&session->session_mutex);
    auth_info = session->auth_info;
    pthread_mutex_unlock(&session->session_mutex);
    
    return auth_info;
}

int is_session_authenticated(SID session_id) {
    session_t* session = get_session(session_id);
    if (!session) return 0;
    
    pthread_mutex_lock(&session->session_mutex);
    int authenticated = session->auth_info.is_authenticated;
    pthread_mutex_unlock(&session->session_mutex);
    
    return authenticated;
}

UID get_session_user_id(SID session_id) {
    session_t* session = get_session(session_id);
    if (!session) return 0;
    
    pthread_mutex_lock(&session->session_mutex);
    UID uid = session->auth_info.user_id;
    pthread_mutex_unlock(&session->session_mutex);
    
    return uid;
}

GID get_session_group_id(SID session_id) {
    session_t* session = get_session(session_id);
    if (!session) return 0;
    
    pthread_mutex_lock(&session->session_mutex);
    GID gid = session->auth_info.group_id;
    pthread_mutex_unlock(&session->session_mutex);
    
    return gid;
}

//=============================================================================
// 全局接口实现
//=============================================================================

int session_system_init(const network_manager_config_t* config) {
    pthread_mutex_lock(&g_global_mutex);
    
    if (g_network_manager) {
        pthread_mutex_unlock(&g_global_mutex);
        return SESS_ERROR; // 已经初始化
    }
    
    g_network_manager = network_manager_create(config);
    if (!g_network_manager) {
        pthread_mutex_unlock(&g_global_mutex);
        return SESS_ERROR;
    }
    
    if (network_manager_initialize(g_network_manager) != SESS_OK) {
        network_manager_destroy(g_network_manager);
        g_network_manager = NULL;
        pthread_mutex_unlock(&g_global_mutex);
        return SESS_ERROR;
    }
    
    g_session_pool = g_network_manager->session_pool;
    
    if (network_manager_start(g_network_manager) != SESS_OK) {
        network_manager_destroy(g_network_manager);
        g_network_manager = NULL;
        g_session_pool = NULL;
        pthread_mutex_unlock(&g_global_mutex);
        return SESS_ERROR;
    }
    
    pthread_mutex_unlock(&g_global_mutex);
    return SESS_OK;
}

void session_system_shutdown(void) {
    pthread_mutex_lock(&g_global_mutex);
    
    if (g_network_manager) {
        network_manager_destroy(g_network_manager);
        g_network_manager = NULL;
    }
    
    g_session_pool = NULL;
    
    pthread_mutex_unlock(&g_global_mutex);
}

network_manager_t* get_global_network_manager(void) {
    pthread_mutex_lock(&g_global_mutex);
    network_manager_t* manager = g_network_manager;
    pthread_mutex_unlock(&g_global_mutex);
    return manager;
}

session_pool_t* get_global_session_pool(void) {
    pthread_mutex_lock(&g_global_mutex);
    session_pool_t* pool = g_session_pool;
    pthread_mutex_unlock(&g_global_mutex);
    return pool;
}