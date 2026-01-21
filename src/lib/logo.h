/*
#版权所有 (c) HuJi 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2024.11
Email:hj18914255909@outlook.com
*/
#ifndef LOGO_H
#define LOGO_H

#ifdef __cplusplus
extern "C" {
#endif

#define DEFAULT_SERVER_IP "127.0.0.1"
#define PORT 18482

void print_help();//打印帮助信息 (Client && Server)
void print_version();//打印版本信息 (Client && Server)
void print_welcome(const char* server_ip, int server_port);//打印欢迎信息 (Client && Server)

#ifdef __cplusplus
}
#endif

#endif