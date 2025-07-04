/*
#版权所有 (c) HuJi 2025
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
Email:hj18914255909@outlook.com
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>//Unix标准库
#include <signal.h>//信号处理库
#include <errno.h>//错误处理库
#include <netinet/in.h>//Internet地址族
#include <arpa/inet.h>//IP地址转换函数
#include <openssl/ssl.h>//SSL/TLS库
#include <openssl/err.h>//错误处理库
#include <readline/readline.h>//readline库
#include <readline/history.h>//history库

#include "lexer.h"
#include "logo.h"

#define PORT 18482 //默认服务器端口
#define DEFAULT_SERVER_IP "127.0.0.1"
#define BUFFER_SIZE 4096
#define MAX_QUERY_LENGTH 8192

// 全局变量
static SSL_CTX *ssl_ctx = NULL;
static SSL *ssl_conn = NULL;//SSL连接的指针
static int sock_fd = -1;//套接字的文件描述符
static char server_ip[16] = DEFAULT_SERVER_IP;//服务器IP地址
static int server_port = PORT;//服务器端口
static int connected = 0;//连接状态

void init_openssl() {
    SSL_load_error_strings();// 加载错误字符串
    OpenSSL_add_ssl_algorithms();// 注册SSL算法
    SSL_library_init();// 初始化SSL库
}

void cleanup_openssl() {
    if (ssl_conn) {//如果ssl连接存在，则关闭连接
        SSL_shutdown(ssl_conn);// 关闭SSL连接
        SSL_free(ssl_conn);// 释放SSL连接
        ssl_conn = NULL;// 将ssl连接设置为NULL
    }
    if (ssl_ctx) {//如果ssl上下文存在，则释放上下文
        SSL_CTX_free(ssl_ctx);// 释放SSL上下文
        ssl_ctx = NULL;// 将ssl上下文设置为NULL
    }
    if (sock_fd >= 0) {//如果套接字存在，则关闭套接字
        close(sock_fd);// 关闭套接字
        sock_fd = -1;// 将套接字重置为-1
    }
    EVP_cleanup();// 清理EVP EVP:OpenSSL加密库,清理的意思是释放EVP库占用的内存
    ERR_free_strings();// 清理错误字符串 ERR:OpenSSL错误库,清理的意思是释放ERR库占用的内存
}

/*
TCP/UDP作为基础传输协议，负责"数据传输",不保证安全性和完整性
SSL/TLS依赖TCP/UDP的数据传输，在此之上构建安全传输协议，负责"数据加密"和"端到端认证"

HTTP = 应用层规则（协议） + TCP/UDP
HTTPS = 应用层规则（协议）+ TCP/UDP + SSL/TLS 
*/

// 创建SSL上下文（创建SSL/TLS安全传输协议的背景）
SSL_CTX *create_ssl_context() {
    const SSL_METHOD *method;//SSL方法
    SSL_CTX *ctx;//声明一个SSL上下文指针

    method = TLS_client_method();
    /*
    笔记
    method = TLS_client_method();
    TLS_client_method()函数用于创建一个仅支持TLS协议的客户端方法对象，该对象可用于初始化SSL/TLS上下文，
    使得客户端能够与服务器进行TLS握手并建立安全连接。
    该方法不再支持不安全的SSLv2和SSLv3协议，只支持TLS 1.0及以上版本。
    */
    ctx = SSL_CTX_new(method);//SSL_CTX_new:SSL_CTX_new是OpenSSL库中的一个函数,用于创建一个SSL上下文
    if (!ctx) {//如果ctx为NULL,则打印错误信息
        fprintf(stderr, "错误: 无法创建SSL上下文\n");//打印错误信息
        ERR_print_errors_fp(stderr);//打印错误信息
        return NULL;
    }

    // 设置验证模式为不验证服务器证书（开发环境）
    //==============================================
    SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);
    //==============================================
    return ctx;
}

// 连接到服务器（先用TCP连接到服务器，并建立SSL连接）
int connect_to_server(const char *ip, int port) {
    struct sockaddr_in serv_addr;//声明一个sockaddr_in结构体 用于存储服务器地址
    
    if (connected) {//如果已经连接到服务器，则打印信息，不用再连接
        printf("已经连接到服务器\n");//打印信息
        return 0;
    }

    // 创建套接字（TCP）
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);//socket:创建一个套接字
    if (sock_fd < 0) {
        perror("socket creation error");
        return -1;
    }

    serv_addr.sin_family = AF_INET;//AF_INET:IPv4协议
    serv_addr.sin_port = htons(port);//htons:将端口号转换为网络字节序

    if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0) {//inet_pton:将IP地址转换为网络字节序
        fprintf(stderr, "错误: 无效的IP地址 %s\n", ip);//打印错误信息
        close(sock_fd);//关闭套接字
        sock_fd = -1;//将套接字重置为-1
        return -1;
    }

    printf("正在连接到 %s:%d...\n", ip, port);
    
    if (connect(sock_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("连接失败");
        close(sock_fd);
        sock_fd = -1;
        return -1;
    }
    //至此，已经创建了套接字，并设置了服务器地址，接下来就是连接到服务器（TCP）
    //此时仅仅建立了TCP连接，但数据传输不安全，接下来建立SSL连接，确保数据传输安全

    // 创建SSL连接（SSL/TLS）
    ssl_conn = SSL_new(ssl_ctx);//SSL_new:创建一个SSL连接，ssl_ctx是SSL上下文
    if (!ssl_conn) {
        fprintf(stderr, "错误: 无法创建SSL连接\n");
        close(sock_fd);
        sock_fd = -1;
        return -1;
    }

    SSL_set_fd(ssl_conn, sock_fd);//SSL_set_fd:将套接字设置为SSL连接

    if (SSL_connect(ssl_conn) <= 0) {//SSL_connect:连接到服务器
        fprintf(stderr, "错误: SSL连接失败\n");
        ERR_print_errors_fp(stderr);//打印错误信息
        SSL_free(ssl_conn);//释放SSL连接
        ssl_conn = NULL;//将ssl连接设置为NULL
        close(sock_fd);//关闭套接字
        sock_fd = -1;//将套接字重置为-1
        return -1;
    }
    //此时，普通TCP被增加了SSL/TLS机制

    connected = 1;//将连接状态设置为已连接
    printf("✓ 已成功连接到 Mhuixs 服务器\n");//打印成功信息
    return 0;
}

// 断开与服务器的连接
void disconnect_from_server() {
    if (!connected) return;//如果未连接到服务器，则返回    
    printf("正在断开连接...\n");
    connected = 0;    

    //SSL/TLS连接断开
    if (ssl_conn) {
        SSL_shutdown(ssl_conn);//关闭SSL连接
        SSL_free(ssl_conn);//释放SSL连接
        ssl_conn = NULL;//将ssl连接设置为NULL
    }
    //TCP连接断开
    if (sock_fd >= 0) {
        close(sock_fd);//关闭套接字
        sock_fd = -1;//将套接字重置为-1
    }    
    printf("✓ 已断开连接\n");
}

// 发送查询到服务器
int send_query(const char *query) {
    if (!connected || !ssl_conn) {
        fprintf(stderr, "错误: 未连接到服务器\n");
        return -1;
    }

    int sent = SSL_write(ssl_conn, query, strlen(query));
    
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
    return buffer;
}

// 打印帮助信息
void print_help() {
    printf("Mhuixs-client - 用法:\n");
    printf("  mhuixs-client [选项] [命令]\n\n");
    printf("选项:\n");
    printf("  -h, --help              显示此帮助信息\n");
    printf("  -v, --version           显示版本信息\n");
    printf("  -s, --server <IP>       指定服务器IP地址 (默认: %s)\n", DEFAULT_SERVER_IP);
    printf("  -p, --port <端口>       指定服务器端口 (默认: %d)\n", PORT);
    printf("  -f, --file <文件>       从文件批量执行查询\n");
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
    printf("Mhuixs-client v0.0.1  Mhuixs客户端\n"
           "Copyright (c) HuJi 2024 Email:hj18914255909@outlook.com\n");
}

// 信号处理函数,当接收到中断信号时，断开连接，清理资源，退出程序
void signal_handler(int signum) {
    printf("\n\n接收到中断信号，正在清理资源...\n");
    disconnect_from_server();//断开连接
    cleanup_openssl();//清理资源
    exit(0);//退出程序
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
        printf("警告: auto连接服务器失败，可使用 \\c 命令手动连接\n");
    }
    
    while (1) {
        // 设置提示符
        char prompt[64];//提示符
        if (connected) snprintf(prompt, sizeof(prompt), "Mhuixs> ");
        else snprintf(prompt, sizeof(prompt), "Mhuixs(断开)> ");
        
        input = readline(prompt);
        /*
        readline 是 C 语言库（如 GNU Readline），提供交互式命令行输入支持，包括：
        1.行编辑（光标移动、删除、历史记录等）
        2.自动补全（按 Tab 键）
        3.历史命令管理（上下箭头键浏览）

        特殊返回值：
        NULL：用户按 Ctrl+D 或输入 EOF（通常是 Ctrl+Z）
        */
        
        if (!input) {
            printf("\nUser exit.\n");
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
        else {
            // 处理NAQL查询
            //==============================================
            //未来核心拓展区
            //==============================================
            if (connected) {
                if (send_query(input) > 0) {
                    response = receive_response();
                    if (response) {
                        printf("%s\n", response);
                        free(response);
                    }
                }
            } else {
                printf("error: 未连接到服务器，请使用 \\c 命令连接\n");
            }
            //==============================================
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
        

        // 处理NAQL查询
        //==============================================
        //未来核心拓展区
        //==============================================
        if (send_query(line) > 0) {
            char *response = receive_response();
            if (response) {
                printf("结果: %s\n", response);
                free(response);
            }
        }
        //===============================================
    }
    
    fclose(file);
    printf("批处理完成\n");
}

// 主函数
int main(int argc, char *argv[]) {
    // 设置信号处理
    signal(SIGINT, signal_handler);//Ctrl+C
    signal(SIGTERM, signal_handler);//Ctrl+Z
    
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
