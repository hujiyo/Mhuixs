#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include "Mhudef.hpp"
#include "getid.hpp"//用户ID分配器
#include "iami/maes.hpp"

#include <fcntl.h> // 文件控制定义头文件
#include <errno.h> // 错误号定义头文件
/*
#版权所有 (c) Mhuixs-team 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2025.1
Email:hj18914255909@outlook.com
*/

/*
=====================
session 模块
=====================
负责管理客户端连接、每个会话相当于一个"进程"，执行模块就类似处理器，
每个会话会管理自己的一个队列，执行模块根据优先级在队列中获取任务执行
*/
#ifndef SESSION_H
#define SESSION_H

//默认参数定义
#define PORT 18482                  //Mhuixs默认端口号
#define BUFFER_SIZE 1024            // 缓冲区默认初始大小（1KB/8KB/16KB/64KB）
#define MAX_SESSIONS 64             // 最大会话数量
#define SESSION_backlog 8           //连接等待队列最大长度
#define heartbeat 10                //心跳包间隔时间（单位:s）


#ifdef _WIN32

#include <winsock2.h> // Windows 套接字头文件
#include <ws2tcpip.h> // Windows 套接字函数库头文件

#endif
#ifdef __linux__

#include <unistd.h> //unix标准符号定义头文件
#include <sys/socket.h>//socket函数库头文件
#include <sys/types.h>//基本系统数据类型
#include <netinet/in.h>//Internet地址族
#include <arpa/inet.h>//提供IP地址转换函数
#include <fcntl.h> //文件控制定义头文件
#include <errno.h> //错误号定义头文件

#endif


typedef struct SESSION{//会话
   int sessocket; // 通信套接字文件描述符
   struct sockaddr client_addr; // 客户端地址

   SID sid; // 会话ID
   UID uid; // 客户端身份id

   time_t time; // 会话建立时间
   uint32_t revisit_sum; // 回访次数，接收到数据时清零，否则+1，超过设置时间时视为断开连接
   int status; // 会话状态
   uint8_t priority; // 会话优先级
   #define SESS_idle 0        // 会话死亡：已回收，priority=0
   #define SESS_alive 1       // 会话正常：等待数据，priority=1
   #define SESS_sleep 2       // 会话休眠：通过心跳包维持 priority=1
   #define SESS_busy 3        // 会话繁忙:数据传输，提高优先级 priority=2
   #define SESS_super 4       // 特殊会话:管理人员 priority=2

   uint8_t* buffer; // 会话缓冲区
   uint32_t buffer_size; // 会话缓冲区大小
   uint32_t datlen; // 已缓存的数据量
   uint32_t ofst_ptr; // 数据读取头偏移量

   SKEY skey;//AES加密密钥
} SESSION;

int killandfree_session(SESSION* session);
/*
   这个函数是软关闭会话，重置会话资源，关闭通信套接字
   //后期还要考虑很多安全结束会话的问题

   0-成功
   merr-失败
*/

int start_session_server(uint16_t port,int af,uint32_t backlog,SESSION* SESSPOOL,uint32_t sessionums,uint32_t buffer_size);
/*
   功能：启动会话服务器

   port:端口号
   af:地址族 AF_INET:IPv4协议族，AF_INET6:IPv6协议族
   sessionums:最大会话数量
   backlog:连接等待队列最大长度
   buffer_size:会话缓冲区默认大小（最开始分配1KB，可以增加）

   成功-返回一个非阻塞的socket文件描述符  失败-返回err
*/

int end_session_server(int server_fd,SESSION* SESSPOOL,uint32_t sessionums);
/*
   功能：关闭会话服务器
*/

void flash_accept(int server_fd,SESSION* SESSPOOL, uint16_t sessionums,uint16_t backlog);
/*
   功能：刷新accept连接，将”新增连接“加入”会话池“中
   常常调用这个函数以实时接受客户端新连接

   server_fd:服务器socket文件描述符
   SESSPOOL:会话池
   sessionums:会话池最大容量
   backlog:连接等待队列最大长度，建议和start_session_server中的backlog相同
*/

#endif


/*
int my_main() {
   
   // 启动会话服务器
   SESSION* SESSPOOL=NULL;//会话池
   int server_fd = start_session_server(PORT,SESSION_backlog,SESSPOOL,MAX_SESSIONS,BUFFER_SIZE);
   if(server_fd == merr) return merr;



   

   // 接受客户端连接
   int new_socket;//通信套接字文件描述符
   if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
      perror("accept");
      close(server_fd);
      exit(EXIT_FAILURE);
   }

   printf("Connection accepted\n");

   // 读取客户端消息
   int valread = read(new_socket, buffer, BUFFER_SIZE);
   printf("%s\n", buffer);

   // 发送响应消息给客户端
   const char *hello = "Hello from server";
   send(new_socket, hello, strlen(hello), 0);
   printf("Hello message sent\n");

   // 关闭socket
   close(new_socket);
   close(server_fd);
}
*/