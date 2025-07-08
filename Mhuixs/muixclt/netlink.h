#ifndef NETLINK_H
#define NETLINK_H

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

#define BUFFER_SIZE 4096
#define MAX_QUERY_LENGTH 8192
#define PORT 18482
#define DEFAULT_SERVER_IP "127.0.0.1" //默认服务端在本地

extern int connected;//连接状态
extern char server_ip[16];//服务器IP地址
extern int server_port;//服务器端口

void init_openssl();//初始化openssl
void cleanup_openssl();//清理openssl
SSL_CTX *create_ssl_context();//创建SSL上下文
int connect_to_server(const char *ip, int port);//连接到服务器
void disconnect_from_server();//断开与服务器的连接
int send_query(const char *query);//发送查询到服务器
char *receive_response();//接收服务器响应

#endif