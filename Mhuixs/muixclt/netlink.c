#include "netlink.h"

int connected = 0;//连接状态

// 全局变量
static SSL_CTX *ssl_ctx = NULL;
static SSL *ssl_conn = NULL;//SSL连接的指针
static int sock_fd = -1;//套接字的文件描述符
char server_ip[16] = DEFAULT_SERVER_IP;//服务器IP地址
int server_port = PORT;//服务器端口

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







