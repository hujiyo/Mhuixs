/*
#版权所有 (c) Mhuixs-team 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
Email:hj18914255909@outlook.com
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <readline/readline.h>
#include <readline/history.h>

#include "lexer.h"
#include "logo.h"

#define PORT 18482
#define DEFAULT_SERVER_IP "127.0.0.1"
#define BUFFER_SIZE 4096
#define MAX_QUERY_LENGTH 8192

// 全局变量
static SSL_CTX *ssl_ctx = NULL;
static SSL *ssl_conn = NULL;
static int sock_fd = -1;
static char server_ip[16] = DEFAULT_SERVER_IP;
static int server_port = PORT;
static int connected = 0;
static int verbose = 0;

// 函数声明
void init_openssl();
void cleanup_openssl();
SSL_CTX *create_ssl_context();
int connect_to_server(const char *ip, int port);
void disconnect_from_server();
int send_query(const char *query);
char* receive_response();
void print_help();
void print_version();
void signal_handler(int signum);
int parse_command_line(int argc, char *argv[]);
void interactive_mode();
void batch_mode(const char *filename);
void print_welcome();

// 初始化OpenSSL
void init_openssl() {
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
    SSL_library_init();
}

// 清理OpenSSL
void cleanup_openssl() {
    if (ssl_conn) {
        SSL_shutdown(ssl_conn);
        SSL_free(ssl_conn);
        ssl_conn = NULL;
    }
    if (ssl_ctx) {
        SSL_CTX_free(ssl_ctx);
        ssl_ctx = NULL;
    }
    if (sock_fd >= 0) {
        close(sock_fd);
        sock_fd = -1;
    }
    EVP_cleanup();
    ERR_free_strings();
}

// 创建SSL上下文
SSL_CTX *create_ssl_context() {
    const SSL_METHOD *method;
    SSL_CTX *ctx;

    method = SSLv23_client_method();
    ctx = SSL_CTX_new(method);
    if (!ctx) {
        fprintf(stderr, "错误: 无法创建SSL上下文\n");
        ERR_print_errors_fp(stderr);
        return NULL;
    }

    // 设置验证模式为不验证服务器证书（开发环境）
    SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);

    return ctx;
}

// 连接到服务器
int connect_to_server(const char *ip, int port) {
    struct sockaddr_in serv_addr;
    
    if (connected) {
        printf("已经连接到服务器\n");
        return 0;
    }

    // 创建套接字
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("socket creation error");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0) {
        fprintf(stderr, "错误: 无效的IP地址 %s\n", ip);
        close(sock_fd);
        sock_fd = -1;
        return -1;
    }

    printf("正在连接到 %s:%d...\n", ip, port);
    
    if (connect(sock_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("连接失败");
        close(sock_fd);
        sock_fd = -1;
        return -1;
    }

    // 创建SSL连接
    ssl_conn = SSL_new(ssl_ctx);
    if (!ssl_conn) {
        fprintf(stderr, "错误: 无法创建SSL连接\n");
        close(sock_fd);
        sock_fd = -1;
        return -1;
    }

    SSL_set_fd(ssl_conn, sock_fd);

    if (SSL_connect(ssl_conn) <= 0) {
        fprintf(stderr, "错误: SSL连接失败\n");
        ERR_print_errors_fp(stderr);
        SSL_free(ssl_conn);
        ssl_conn = NULL;
        close(sock_fd);
        sock_fd = -1;
        return -1;
    }

    connected = 1;
    printf("✓ 已成功连接到 Mhuixs 服务器\n");
    return 0;
}

// 断开与服务器的连接
void disconnect_from_server() {
    if (!connected) return;
    
    printf("正在断开连接...\n");
    connected = 0;
    
    if (ssl_conn) {
        SSL_shutdown(ssl_conn);
        SSL_free(ssl_conn);
        ssl_conn = NULL;
    }
    
    if (sock_fd >= 0) {
        close(sock_fd);
        sock_fd = -1;
    }
    
    printf("✓ 已断开连接\n");
}

// 发送查询到服务器
int send_query(const char *query) {
    if (!connected || !ssl_conn) {
        fprintf(stderr, "错误: 未连接到服务器\n");
        return -1;
    }

    if (verbose) {
        printf("发送查询: %s\n", query);
    }

    int len = strlen(query);
    int sent = SSL_write(ssl_conn, query, len);
    
    if (sent <= 0) {
        fprintf(stderr, "错误: 发送查询失败\n");
        ERR_print_errors_fp(stderr);
        return -1;
    }

    return sent;
}

// 接收服务器响应
char* receive_response() {
    if (!connected || !ssl_conn) {
        fprintf(stderr, "错误: 未连接到服务器\n");
        return NULL;
    }

    char *buffer = malloc(BUFFER_SIZE);
    if (!buffer) {
        fprintf(stderr, "错误: 内存分配失败\n");
        return NULL;
    }

    int received = SSL_read(ssl_conn, buffer, BUFFER_SIZE - 1);
    if (received <= 0) {
        fprintf(stderr, "错误: 接收响应失败\n");
        free(buffer);
        return NULL;
    }

    buffer[received] = '\0';
    
    if (verbose) {
        printf("接收响应: %s\n", buffer);
    }

    return buffer;
}

// 打印帮助信息
void print_help() {
    printf("Mhuixs 客户端 - 用法:\n");
    printf("  mhuixs-client [选项] [命令]\n\n");
    printf("选项:\n");
    printf("  -h, --help              显示此帮助信息\n");
    printf("  -v, --version           显示版本信息\n");
    printf("  -s, --server <IP>       指定服务器IP地址 (默认: %s)\n", DEFAULT_SERVER_IP);
    printf("  -p, --port <端口>       指定服务器端口 (默认: %d)\n", PORT);
    printf("  -f, --file <文件>       从文件批量执行查询\n");
    printf("  -V, --verbose           详细模式\n");
    printf("  -q, --quiet             静默模式\n\n");
    printf("交互式命令:\n");
    printf("  \\q, \\quit              退出客户端\n");
    printf("  \\h, \\help              显示帮助信息\n");
    printf("  \\c, \\connect           连接到服务器\n");
    printf("  \\d, \\disconnect        断开连接\n");
    printf("  \\s, \\status            显示连接状态\n");
    printf("  \\v, \\verbose           切换详细模式\n\n");
    printf("示例:\n");
    printf("  mhuixs-client                    # 启动交互模式\n");
    printf("  mhuixs-client -s 192.168.1.100   # 连接到指定服务器\n");
    printf("  mhuixs-client -f queries.naql    # 批量执行查询\n");
}

// 打印版本信息
void print_version() {
    printf("Mhuixs 客户端 v1.0.0\n");
    printf("基于内存的高性能数据库客户端\n");
    printf("版权所有 (c) Mhuixs-team 2024\n");
}

// 信号处理函数
void signal_handler(int signum) {
    printf("\n\n接收到中断信号，正在清理资源...\n");
    disconnect_from_server();
    cleanup_openssl();
    exit(0);
}

// 解析命令行参数
int parse_command_line(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_help();
            return 1;
        }
        else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            print_version();
            return 1;
        }
        else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--server") == 0) {
            if (i + 1 < argc) {
                strcpy(server_ip, argv[++i]);
            } else {
                fprintf(stderr, "错误: 缺少服务器IP地址\n");
                return -1;
            }
        }
        else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--port") == 0) {
            if (i + 1 < argc) {
                server_port = atoi(argv[++i]);
                if (server_port <= 0 || server_port > 65535) {
                    fprintf(stderr, "错误: 无效的端口号\n");
                    return -1;
                }
            } else {
                fprintf(stderr, "错误: 缺少端口号\n");
                return -1;
            }
        }
        else if (strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--file") == 0) {
            if (i + 1 < argc) {
                batch_mode(argv[++i]);
                return 1;
            } else {
                fprintf(stderr, "错误: 缺少文件名\n");
                return -1;
            }
        }
        else if (strcmp(argv[i], "-V") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = 1;
        }
        else if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--quiet") == 0) {
            verbose = 0;
        }
        else {
            fprintf(stderr, "错误: 未知选项 %s\n", argv[i]);
            return -1;
        }
    }
    return 0;
}

// 打印欢迎信息
void print_welcome() {
    print_mhuixs_logo();
    printf("\n");
    printf("欢迎使用 Mhuixs 数据库客户端!\n");
    printf("服务器地址: %s:%d\n", server_ip, server_port);
    printf("输入 \\h 获取帮助信息，输入 \\q 退出\n");
    printf("----------------------------------------\n");
}

// 交互模式
void interactive_mode() {
    char *input;
    char *response;
    
    print_welcome();
    
    // 自动连接到服务器
    if (connect_to_server(server_ip, server_port) != 0) {
        printf("警告: 无法连接到服务器，请使用 \\c 命令手动连接\n");
    }
    
    while (1) {
        // 设置提示符
        char prompt[64];
        if (connected) {
            snprintf(prompt, sizeof(prompt), "mhuixs> ");
        } else {
            snprintf(prompt, sizeof(prompt), "mhuixs(断开)> ");
        }
        
        input = readline(prompt);
        
        if (!input) {
            printf("\n再见!\n");
            break;
        }
        
        // 跳过空行
        if (strlen(input) == 0) {
            free(input);
            continue;
        }
        
        // 添加到历史记录
        add_history(input);
        
        // 处理内置命令
        if (strncmp(input, "\\q", 2) == 0 || strncmp(input, "\\quit", 5) == 0) {
            free(input);
            break;
        }
        else if (strncmp(input, "\\h", 2) == 0 || strncmp(input, "\\help", 5) == 0) {
            print_help();
        }
        else if (strncmp(input, "\\c", 2) == 0 || strncmp(input, "\\connect", 8) == 0) {
            if (connect_to_server(server_ip, server_port) != 0) {
                printf("连接失败\n");
            }
        }
        else if (strncmp(input, "\\d", 2) == 0 || strncmp(input, "\\disconnect", 11) == 0) {
            disconnect_from_server();
        }
        else if (strncmp(input, "\\s", 2) == 0 || strncmp(input, "\\status", 7) == 0) {
            printf("连接状态: %s\n", connected ? "已连接" : "未连接");
            if (connected) {
                printf("服务器: %s:%d\n", server_ip, server_port);
            }
        }
        else if (strncmp(input, "\\v", 2) == 0 || strncmp(input, "\\verbose", 8) == 0) {
            verbose = !verbose;
            printf("详细模式: %s\n", verbose ? "开启" : "关闭");
        }
        else {
            // 处理NAQL查询
            if (connected) {
                if (send_query(input) > 0) {
                    response = receive_response();
                    if (response) {
                        printf("%s\n", response);
                        free(response);
                    }
                }
            } else {
                printf("错误: 未连接到服务器，请使用 \\c 命令连接\n");
            }
        }
        
        free(input);
    }
}

// 批处理模式
void batch_mode(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "错误: 无法打开文件 %s\n", filename);
        return;
    }
    
    printf("正在执行批处理文件: %s\n", filename);
    
    if (connect_to_server(server_ip, server_port) != 0) {
        fprintf(stderr, "错误: 无法连接到服务器\n");
        fclose(file);
        return;
    }
    
    char line[MAX_QUERY_LENGTH];
    int line_number = 0;
    
    while (fgets(line, sizeof(line), file)) {
        line_number++;
        
        // 去除换行符
        char *newline = strchr(line, '\n');
        if (newline) *newline = '\0';
        
        // 跳过空行和注释
        if (strlen(line) == 0 || line[0] == '#') {
            continue;
        }
        
        printf("执行第 %d 行: %s\n", line_number, line);
        
        if (send_query(line) > 0) {
            char *response = receive_response();
            if (response) {
                printf("结果: %s\n", response);
                free(response);
            }
        }
    }
    
    fclose(file);
    printf("批处理完成\n");
}

// 主函数
int main(int argc, char *argv[]) {
    // 设置信号处理
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // 初始化OpenSSL
    init_openssl();
    
    // 创建SSL上下文
    ssl_ctx = create_ssl_context();
    if (!ssl_ctx) {
        cleanup_openssl();
        return 1;
    }
    
    // 解析命令行参数
    int parse_result = parse_command_line(argc, argv);
    if (parse_result != 0) {
        cleanup_openssl();
        return parse_result == 1 ? 0 : 1;
    }
    
    // 启动交互模式
    interactive_mode();
    
    // 清理资源
    disconnect_from_server();
    cleanup_openssl();
    
    return 0;
}
