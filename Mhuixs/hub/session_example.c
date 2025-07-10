#include "session.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

/*
 * Mhuixs会话管理模块使用示例 (纯C版本)
 * 
 * 演示如何使用会话管理模块：
 * 1. 初始化会话系统
 * 2. 启动网络服务
 * 3. 执行模块如何处理会话命令
 * 4. 权限验证示例
 * 5. 优雅关闭
 */

static volatile int g_running = 1;

// 信号处理函数
void signal_handler(int sig) {
    printf("\n接收到信号 %d，准备关闭服务器...\n", sig);
    g_running = 0;
}

// 模拟执行模块的命令处理函数
void process_command(SID session_id, command_t* cmd) {
    printf("处理会话 %u 的命令：ID=%u, 序列=%u, 优先级=%u, 数据长度=%u\n",
           session_id, cmd->command_id, cmd->sequence_num, cmd->priority, cmd->data_len);
    
    // 模拟命令处理
    switch (cmd->command_id) {
        case 1: // 登录命令
            printf("  -> 处理登录命令\n");
            // 模拟认证成功
            authenticate_session(session_id, 1001, 100, "testuser", "auth_token_123");
            break;
            
        case 2: // 查询命令
            printf("  -> 处理查询命令\n");
            // 检查权限
            if (check_session_permission(session_id, "test_table", MODE_READ)) {
                printf("  -> 权限检查通过\n");
                // 模拟查询结果
                const char* response = "查询结果：记录1, 记录2, 记录3";
                send_response_to_session(session_id, (const uint8_t*)response, strlen(response));
            } else {
                printf("  -> 权限检查失败\n");
                const char* error = "错误：没有读取权限";
                send_response_to_session(session_id, (const uint8_t*)error, strlen(error));
            }
            break;
            
        case 3: // 插入命令
            printf("  -> 处理插入命令\n");
            if (check_session_permission(session_id, "test_table", MODE_WRITE)) {
                printf("  -> 权限检查通过\n");
                const char* response = "插入成功";
                send_response_to_session(session_id, (const uint8_t*)response, strlen(response));
            } else {
                printf("  -> 权限检查失败\n");
                const char* error = "错误：没有写入权限";
                send_response_to_session(session_id, (const uint8_t*)error, strlen(error));
            }
            break;
            
        default:
            printf("  -> 未知命令\n");
            break;
    }
}

// 模拟执行模块的主循环
void* execution_module_thread(void* arg) {
    printf("执行模块启动...\n");
    
    while (g_running) {
        // 获取有待处理命令的会话
        SID session_ids[100];
        uint32_t session_count = get_sessions_with_commands(session_ids, 100);
        
        if (session_count == 0) {
            usleep(10000); // 10ms
            continue;
        }
        
        printf("发现 %u 个会话有待处理命令\n", session_count);
        
        // 处理每个会话的命令
        for (uint32_t i = 0; i < session_count; i++) {
            SID session_id = session_ids[i];
            
            // 获取会话信息
            session_t* session = get_session(session_id);
            if (!session) continue;
            
            printf("处理会话 %u (用户ID: %u, 状态: %s)\n", 
                   session_id, 
                   get_session_user_id(session_id),
                   session_state_to_string(session->state));
            
            // 处理所有待处理命令
            command_t* cmd;
            while ((cmd = get_next_command_from_session(session_id)) != NULL) {
                process_command(session_id, cmd);
                command_destroy(cmd);
            }
        }
    }
    
    printf("执行模块关闭\n");
    return NULL;
}

// 统计信息显示线程
void* stats_thread(void* arg) {
    while (g_running) {
        sleep(10); // 每10秒显示一次统计信息
        
        session_pool_t* pool = get_global_session_pool();
        if (pool) {
            printf("\n=== 会话池统计信息 ===\n");
            printf("总会话数: %u\n", pool->stats.total_sessions);
            printf("活跃会话数: %u\n", pool->stats.active_sessions);
            printf("空闲会话数: %u\n", pool->stats.idle_sessions);
            printf("错误会话数: %u\n", pool->stats.error_sessions);
            printf("已分配次数: %u\n", pool->stats.allocated_count);
            printf("已回收次数: %u\n", pool->stats.recycled_count);
            printf("清理次数: %u\n", pool->stats.cleanup_count);
            printf("======================\n\n");
        }
        
        network_manager_t* manager = get_global_network_manager();
        if (manager) {
            printf("=== 网络管理器统计信息 ===\n");
            printf("总连接数: %lu\n", manager->total_connections);
            printf("活跃连接数: %lu\n", manager->active_connections);
            printf("失败连接数: %lu\n", manager->failed_connections);
            printf("已处理字节数: %lu\n", manager->bytes_processed);
            printf("已处理命令数: %lu\n", manager->commands_processed);
            printf("运行时间: %ld 秒\n", time(NULL) - manager->start_time);
            printf("==========================\n\n");
        }
    }
    
    return NULL;
}

int main(int argc, char* argv[]) {
    printf("Mhuixs 会话管理模块示例 (纯C版本)\n");
    printf("================================\n\n");
    
    // 注册信号处理函数
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // 配置网络管理器
    network_manager_config_t config = {
        .listen_port = 8080,
        .address_family = AF_INET,
        .listen_backlog = 1024,
        .enable_ipv6 = 0,
        .network_threads = 4,
        .worker_threads = 8,
        .epoll_max_events = 1024,
        .epoll_timeout = 100,
        .enable_rate_limiting = 1,
        .max_connections_per_ip = 100,
        .connection_timeout = 300,
        .session_pool_config = {
            .max_sessions = 10000,
            .initial_pool_size = 100,
            .cleanup_interval = 60,
            .health_check_interval = 30,
            .max_idle_sessions = 1000,
            .enable_auto_cleanup = 1,
            .enable_session_reuse = 1
        }
    };
    
    // 初始化会话系统
    printf("初始化会话系统...\n");
    if (session_system_init(&config) != SESS_OK) {
        fprintf(stderr, "会话系统初始化失败\n");
        return 1;
    }
    
    printf("会话系统初始化成功\n");
    printf("监听端口: %d\n", config.listen_port);
    printf("最大会话数: %u\n", config.session_pool_config.max_sessions);
    printf("网络线程数: %u\n", config.network_threads);
    printf("工作线程数: %u\n", config.worker_threads);
    printf("\n");
    
    // 启动执行模块线程
    pthread_t execution_thread;
    if (pthread_create(&execution_thread, NULL, execution_module_thread, NULL) != 0) {
        fprintf(stderr, "执行模块线程创建失败\n");
        session_system_shutdown();
        return 1;
    }
    
    // 启动统计信息显示线程
    pthread_t stats_thread_id;
    if (pthread_create(&stats_thread_id, NULL, stats_thread, NULL) != 0) {
        fprintf(stderr, "统计线程创建失败\n");
        g_running = 0;
        pthread_join(execution_thread, NULL);
        session_system_shutdown();
        return 1;
    }
    
    printf("服务器启动成功！\n");
    printf("可以使用 telnet localhost 8080 连接测试\n");
    printf("按 Ctrl+C 停止服务器\n\n");
    
    // 权限验证示例
    printf("=== 权限验证示例 ===\n");
    
    // 模拟创建一个会话并进行权限验证
    session_t* test_session = session_pool_allocate_session(get_global_session_pool());
    if (test_session) {
        SID test_sid = test_session->session_id;
        
        printf("创建测试会话: %u\n", test_sid);
        
        // 初始状态：游客
        printf("初始状态 - 是否认证: %s\n", is_session_authenticated(test_sid) ? "是" : "否");
        printf("用户ID: %u\n", get_session_user_id(test_sid));
        
        // 认证用户
        authenticate_session(test_sid, 1001, 100, "testuser", "token123");
        printf("认证后 - 是否认证: %s\n", is_session_authenticated(test_sid) ? "是" : "否");
        printf("用户ID: %u\n", get_session_user_id(test_sid));
        printf("组ID: %u\n", get_session_group_id(test_sid));
        
        // 添加到附加组
        add_session_to_group(test_sid, 200);
        add_session_to_group(test_sid, 201);
        
        user_auth_info_t auth_info = get_session_auth_info(test_sid);
        printf("用户名: %s\n", auth_info.username);
        printf("附加组数量: %u\n", auth_info.group_count);
        
        // 回收测试会话
        session_pool_recycle_session(get_global_session_pool(), test_sid);
        printf("测试会话已回收\n");
    }
    
    printf("===================\n\n");
    
    // 主循环
    while (g_running) {
        sleep(1);
    }
    
    printf("正在关闭服务器...\n");
    
    // 等待线程结束
    g_running = 0;
    pthread_join(execution_thread, NULL);
    pthread_join(stats_thread_id, NULL);
    
    // 关闭会话系统
    session_system_shutdown();
    
    printf("服务器已关闭\n");
    return 0;
}