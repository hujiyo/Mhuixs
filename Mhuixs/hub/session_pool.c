#include "session.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>

/*
 * 会话池管理实现
 */

// 清理线程函数
static void* session_pool_cleanup_thread(void* arg) {
    session_pool_t* pool = (session_pool_t*)arg;
    
    while (!pool->shutdown_requested) {
        sleep(pool->config.cleanup_interval);
        
        if (pool->config.enable_auto_cleanup) {
            session_pool_cleanup_dead_sessions(pool);
        }
    }
    
    return NULL;
}

// 健康检查线程函数
static void* session_pool_health_check_thread(void* arg) {
    session_pool_t* pool = (session_pool_t*)arg;
    
    while (!pool->shutdown_requested) {
        sleep(pool->config.health_check_interval);
        session_pool_health_check_all_sessions(pool);
    }
    
    return NULL;
}

//=============================================================================
// 会话池操作实现
//=============================================================================

session_pool_t* session_pool_create(const session_pool_config_t* config) {
    session_pool_t* pool = (session_pool_t*)malloc(sizeof(session_pool_t));
    if (!pool) return NULL;
    
    memset(pool, 0, sizeof(session_pool_t));
    
    // 复制配置
    if (config) {
        pool->config = *config;
    } else {
        // 默认配置
        pool->config.max_sessions = 10000;
        pool->config.initial_pool_size = 100;
        pool->config.cleanup_interval = 60;
        pool->config.health_check_interval = 30;
        pool->config.max_idle_sessions = 1000;
        pool->config.enable_auto_cleanup = 1;
        pool->config.enable_session_reuse = 1;
    }
    
    // 分配会话数组
    pool->sessions = (session_t*)malloc(pool->config.max_sessions * sizeof(session_t));
    if (!pool->sessions) {
        free(pool);
        return NULL;
    }
    
    // 分配索引数组
    pool->idle_sessions = (uint32_t*)malloc(pool->config.max_sessions * sizeof(uint32_t));
    pool->active_sessions = (uint32_t*)malloc(pool->config.max_sessions * sizeof(uint32_t));
    pool->error_sessions = (uint32_t*)malloc(pool->config.max_sessions * sizeof(uint32_t));
    
    if (!pool->idle_sessions || !pool->active_sessions || !pool->error_sessions) {
        if (pool->idle_sessions) free(pool->idle_sessions);
        if (pool->active_sessions) free(pool->active_sessions);
        if (pool->error_sessions) free(pool->error_sessions);
        free(pool->sessions);
        free(pool);
        return NULL;
    }
    
    // 初始化互斥锁
    if (pthread_mutex_init(&pool->pool_mutex, NULL) != 0) {
        free(pool->idle_sessions);
        free(pool->active_sessions);
        free(pool->error_sessions);
        free(pool->sessions);
        free(pool);
        return NULL;
    }
    
    return pool;
}

void session_pool_destroy(session_pool_t* pool) {
    if (!pool) return;
    
    session_pool_shutdown(pool);
    
    // 销毁所有会话
    for (uint32_t i = 0; i < pool->config.max_sessions; i++) {
        destroy_session(&pool->sessions[i]);
    }
    
    if (pool->sessions) free(pool->sessions);
    if (pool->idle_sessions) free(pool->idle_sessions);
    if (pool->active_sessions) free(pool->active_sessions);
    if (pool->error_sessions) free(pool->error_sessions);
    
    pthread_mutex_destroy(&pool->pool_mutex);
    free(pool);
}

int session_pool_initialize(session_pool_t* pool) {
    if (!pool) return SESS_INVALID;
    
    pthread_mutex_lock(&pool->pool_mutex);
    
    // 初始化所有会话为空闲状态
    for (uint32_t i = 0; i < pool->config.max_sessions; i++) {
        session_config_t session_config = {
            .max_buffer_size = 64 * 1024,
            .max_command_queue_size = 1000,
            .idle_timeout = 300,
            .heartbeat_interval = 30,
            .max_errors = 10,
            .enable_encryption = 1,
            .enable_compression = 0
        };
        
        session_t* session = create_session(i, &session_config);
        if (!session) {
            pthread_mutex_unlock(&pool->pool_mutex);
            return SESS_ERROR;
        }
        
        pool->sessions[i] = *session;
        pool->idle_sessions[i] = i;
        free(session); // 只要内容，不要指针
    }
    
    pool->idle_count = pool->config.max_sessions;
    pool->active_count = 0;
    pool->error_count = 0;
    
    // 启动清理线程
    if (pthread_create(&pool->cleanup_thread, NULL, session_pool_cleanup_thread, pool) != 0) {
        pthread_mutex_unlock(&pool->pool_mutex);
        return SESS_ERROR;
    }
    
    // 启动健康检查线程
    if (pthread_create(&pool->health_check_thread, NULL, session_pool_health_check_thread, pool) != 0) {
        pool->shutdown_requested = 1;
        pthread_join(pool->cleanup_thread, NULL);
        pthread_mutex_unlock(&pool->pool_mutex);
        return SESS_ERROR;
    }
    
    pthread_mutex_unlock(&pool->pool_mutex);
    return SESS_OK;
}

void session_pool_shutdown(session_pool_t* pool) {
    if (!pool) return;
    
    pthread_mutex_lock(&pool->pool_mutex);
    pool->shutdown_requested = 1;
    pthread_mutex_unlock(&pool->pool_mutex);
    
    // 等待线程结束
    pthread_join(pool->cleanup_thread, NULL);
    pthread_join(pool->health_check_thread, NULL);
    
    // 关闭所有活跃会话
    pthread_mutex_lock(&pool->pool_mutex);
    for (uint32_t i = 0; i < pool->active_count; i++) {
        uint32_t session_id = pool->active_sessions[i];
        session_shutdown(&pool->sessions[session_id]);
    }
    pthread_mutex_unlock(&pool->pool_mutex);
}

session_t* session_pool_allocate_session(session_pool_t* pool) {
    if (!pool) return NULL;
    
    pthread_mutex_lock(&pool->pool_mutex);
    
    if (pool->idle_count == 0) {
        pthread_mutex_unlock(&pool->pool_mutex);
        return NULL; // 没有空闲会话
    }
    
    // 从空闲列表中取出一个会话
    uint32_t session_id = pool->idle_sessions[pool->idle_count - 1];
    pool->idle_count--;
    
    // 添加到活跃列表
    pool->active_sessions[pool->active_count] = session_id;
    pool->active_count++;
    
    // 更新统计
    pool->stats.active_sessions++;
    pool->stats.idle_sessions--;
    pool->stats.allocated_count++;
    
    session_t* session = &pool->sessions[session_id];
    
    pthread_mutex_unlock(&pool->pool_mutex);
    return session;
}

session_t* session_pool_allocate_with_socket(session_pool_t* pool, int socket_fd, 
                                           const struct sockaddr* client_addr, UID uid) {
    session_t* session = session_pool_allocate_session(pool);
    if (!session) return NULL;
    
    if (session_initialize(session, socket_fd, client_addr, uid) != SESS_OK) {
        session_pool_recycle_session(pool, session->session_id);
        return NULL;
    }
    
    return session;
}

int session_pool_recycle_session(session_pool_t* pool, SID session_id) {
    if (!pool || session_id >= pool->config.max_sessions) return SESS_INVALID;
    
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
        session_reset(&pool->sessions[session_id]);
        
        // 添加到空闲列表
        pool->idle_sessions[pool->idle_count] = session_id;
        pool->idle_count++;
        
        // 更新统计
        pool->stats.active_sessions--;
        pool->stats.idle_sessions++;
        pool->stats.recycled_count++;
    }
    
    pthread_mutex_unlock(&pool->pool_mutex);
    return found ? SESS_OK : SESS_NOT_FOUND;
}

session_t* session_pool_find_session(session_pool_t* pool, SID session_id) {
    if (!pool || session_id >= pool->config.max_sessions) return NULL;
    
    pthread_mutex_lock(&pool->pool_mutex);
    
    session_t* session = &pool->sessions[session_id];
    if (session->state == SESS_IDLE) {
        session = NULL;
    }
    
    pthread_mutex_unlock(&pool->pool_mutex);
    return session;
}

session_t* session_pool_find_session_by_user(session_pool_t* pool, UID user_id) {
    if (!pool) return NULL;
    
    pthread_mutex_lock(&pool->pool_mutex);
    
    session_t* found_session = NULL;
    
    // 搜索活跃会话
    for (uint32_t i = 0; i < pool->active_count; i++) {
        uint32_t session_id = pool->active_sessions[i];
        session_t* session = &pool->sessions[session_id];
        
        if (session->auth_info.user_id == user_id && session->auth_info.is_authenticated) {
            found_session = session;
            break;
        }
    }
    
    pthread_mutex_unlock(&pool->pool_mutex);
    return found_session;
}

uint32_t session_pool_get_executable_sessions(session_pool_t* pool, SID* session_ids, uint32_t max_count) {
    if (!pool || !session_ids || max_count == 0) return 0;
    
    pthread_mutex_lock(&pool->pool_mutex);
    
    uint32_t count = 0;
    
    // 遍历活跃会话，找出有待处理命令的会话
    for (uint32_t i = 0; i < pool->active_count && count < max_count; i++) {
        uint32_t session_id = pool->active_sessions[i];
        session_t* session = &pool->sessions[session_id];
        
        if (session->state == SESS_ALIVE || session->state == SESS_BUSY) {
            if (command_queue_size(session->command_queue) > 0) {
                session_ids[count] = session_id;
                count++;
            }
        }
    }
    
    pthread_mutex_unlock(&pool->pool_mutex);
    return count;
}

uint32_t session_pool_get_sessions_with_commands(session_pool_t* pool, SID* session_ids, uint32_t max_count) {
    return session_pool_get_executable_sessions(pool, session_ids, max_count);
}

void session_pool_cleanup_dead_sessions(session_pool_t* pool) {
    if (!pool) return;
    
    pthread_mutex_lock(&pool->pool_mutex);
    
    time_t current_time = time(NULL);
    uint32_t cleaned_count = 0;
    
    // 检查活跃会话
    for (uint32_t i = 0; i < pool->active_count; ) {
        uint32_t session_id = pool->active_sessions[i];
        session_t* session = &pool->sessions[session_id];
        
        if (session_should_close(session) || 
            (current_time - session->last_activity > session->config.idle_timeout)) {
            
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
        session_reset(&pool->sessions[session_id]);
        
        // 添加到空闲列表
        pool->idle_sessions[pool->idle_count] = session_id;
        pool->idle_count++;
    }
    
    pool->error_count = 0;
    pool->stats.cleanup_count += cleaned_count;
    
    pthread_mutex_unlock(&pool->pool_mutex);
}

void session_pool_health_check_all_sessions(session_pool_t* pool) {
    if (!pool) return;
    
    pthread_mutex_lock(&pool->pool_mutex);
    
    // 检查所有活跃会话的健康状态
    for (uint32_t i = 0; i < pool->active_count; i++) {
        uint32_t session_id = pool->active_sessions[i];
        session_t* session = &pool->sessions[session_id];
        
        if (!session_is_healthy(session)) {
            // 标记为错误状态
            session->state = SESS_ERROR;
        }
    }
    
    pthread_mutex_unlock(&pool->pool_mutex);
} 