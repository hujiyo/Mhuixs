#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "Mhudef.h"

/*
#版权所有 (c) Mhuixs-team 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2025.1
Email:hj18914255909@outlook.com
*/

#include <unistd.h> //unix标准符号定义头文件
#include <sys/socket.h>//socket函数库头文件
#include <sys/types.h>//基本系统数据类型
#include <netinet/in.h>//Internet地址族
#include <arpa/inet.h>//提供IP地址转换函数
#include <fcntl.h> //文件控制定义头文件
#include <errno.h> //错误号定义头文件
/*
Mhuixs通过会话来管理客户端连接
*/
int set_nonblocking(int sockfd) 
{
   /*
   设置套接字为非阻塞模式
   成功-返回0  失败-返回err
   */
   int flags, s;
   // 获取当前的文件描述符标志
   flags = fcntl(sockfd, F_GETFL, 0);
   if (flags == -1) {
      return err; // 错误，无法获取文件描述符标志
   }
   // 添加非阻塞标志
   flags |= O_NONBLOCK;
   // 设置修改后的文件描述符标志
   s = fcntl(sockfd, F_SETFL, flags);
   if (s == -1) {
      return err; // 错误，无法设置文件描述符标志
   }

   return 0; // 成功
}

#define PORT 18482                  //端口号，1848.2.22:《共产党宣言》发表
#define BUFFER_SIZE 1024            // 缓冲区大小（1KB/8KB/16KB/64KB）
#define MAX_SESSIONS 64             // 最大会话数量
#define SESSION_backlog 8           //连接等待队列最大长度
#define heartbeat 10                //心跳包间隔时间（单位:s）
#define err -1                      //错误代码

uint32_t _SESSIONS_CURRENT_NUM_=0; // 全局变量:当前会话总数量

typedef struct SESSION{
   int sessocket; // 通信套接字文件描述符
   struct sockaddr client_addr; // 客户端地址

   userid_t user_id; // 客户端身份id
   RANK rank; // 客户端权限等级

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
   uint32_t pos; // 数据读取头偏移量   
} SESSION;

int set_session_ip(SESSION* session,const char* ip,int af){
   /*
   功能：设置会话ip地址

   session:会话指针
   ip:ip地址
   af:地址族 AF_INET:IPv4协议族，AF_INET6:IPv6协议族

   成功-返回0  失败-返回err
   */
   if(session==NULL) return err;
   if(af!=AF_INET&&af!=AF_INET6) return err;
   if(af==AF_INET){
      session->client_addr.sa_family=AF_INET;
      struct sockaddr_in *sockaddr_in = (struct sockaddr_in *)&session->client_addr;
      inet_pton(AF_INET, ip, &(sockaddr_in->sin_addr));//将字符串形式的IPv4地址转换为网络字节序
   }
   else if(af==AF_INET6){
      session->client_addr.sa_family=AF_INET6;
      struct sockaddr_in6 *sockaddr_in6 = (struct sockaddr_in6 *)&session->client_addr;
      inet_pton(AF_INET6, ip, &(sockaddr_in6->sin6_addr));//将字符串形式的IPv6地址转换为网络字节序 
   }
   return 0;
}
int free_session(SESSION* session)// 释放某会话资源
{
   if(session==NULL) {
      return err;
   }
   goto l;
   /*
   功能：释放、重置会话资源
   */   
   session->sessocket=0;//通信套接字文件描述符
   memset(&session->client_addr,0,sizeof(session->client_addr));//清空客户端地址
   session->user_id=0; // 客户端身份id RANK_guest=0
   session->rank=0; // 客户端权限等级 RANK_guest=0
   session->time=0; // 会话建立时间
   session->revisit_sum=0; // 回访次数归零
   session->status=SESS_idle; // 会话状态归0
   session->priority=0; // 会话优先级0
   free(session->buffer);
   session->buffer=(uint8_t*)calloc(BUFFER_SIZE,sizeof(uint8_t));
   session->buffer_size=BUFFER_SIZE;//唯一不是0的成员
   session->datlen=0; // 已缓存的数据量归零
   session->pos=0; // 数据读取头偏移量归零
   return 0;
   l:
   free(session->buffer);
   memset(session,0,sizeof(SESSION));
   session->buffer=(uint8_t*)calloc(BUFFER_SIZE,sizeof(uint8_t));
   if(session->buffer==NULL) {
      return err;
   }
   session->buffer_size=BUFFER_SIZE;//唯一不是0的成员
   return 0;
}
int kill_session(SESSION* session)
{
   /*
   这个函数是软关闭会话，释放会话资源，关闭通信套接字
   后期还要考虑很多安全结束会话的问题
   */
   //先关闭通信套接字
   close(session->sessocket);
   //重置会话资源
   free_session(session);
   return 0;
}
int start_session_server(uint16_t port,int af,uint32_t backlog,SESSION* SESSPOOL,uint32_t sessionums,uint32_t buffer_size)// 启动会话服务器
{
   /*
   功能：启动会话服务器

   port:端口号
   af:地址族 AF_INET:IPv4协议族，AF_INET6:IPv6协议族
   sessionums:最大会话数量
   backlog:连接等待队列最大长度
   buffer_size:会话缓冲区默认大小（最开始分配1KB，可以增加）

   成功-返回socket文件描述符  失败-返回err
   */
   if(af!=AF_INET&&af!=AF_INET6) {
      return err;
   }
   // 创建socket文件描述符
   int server_fd;
   if ((server_fd = socket(af, SOCK_STREAM, 0)) < 0) { //返回一个非负整数表示成功
      perror("socket failed");//AF_INET:IPv4协议族，SOCK_STREAM:面向连接的字节流，0:使用默认协议
      return err;
   }
   // 设置为非阻塞模式
   if (set_nonblocking(server_fd) == err) {
      perror("set_nonblocking failed");
      close(server_fd);
      return err;
   }
   // 绑定socket到端口
   struct sockaddr_in address;
   int addrlen = sizeof(struct sockaddr_in);
   address.sin_family = AF_INET;//IPv4协议族
   address.sin_addr.s_addr = INADDR_ANY;//任意IP地址
   address.sin_port = htons(port);//htons:将主机字节序转换为网络字节序
   if ( bind(server_fd, (struct sockaddr *)&address, sizeof(struct sockaddr_in)) < 0) {
      perror("bind failed");
      close(server_fd);
      return err;
   }
   // 设置为监听，设置最大待处理数
   if (listen(server_fd, backlog) < 0) {
      perror("listen failed");
      close(server_fd);
      return err;
   }
   //初始化会话池
   SESSPOOL=(SESSION*)calloc(sessionums,sizeof(SESSION));
   for(int i=0;i<sessionums;i++){
      SESSPOOL[i].buffer=(uint8_t*)calloc(buffer_size,sizeof(uint8_t));
      SESSPOOL[i].buffer_size=buffer_size;
   }
   return server_fd;
}
int end_session_server(int server_fd,SESSION* SESSPOOL,uint32_t sessionums)// 关闭会话服务器
{
   //关闭socket服务模块
   for(int i=0;i<sessionums;i++){ // 遍历所有会话
      kill_session(&SESSPOOL[i]); // 软关闭会话
      free(SESSPOOL[i].buffer);//释放会话缓冲区
   }
   free(SESSPOOL);//释放整个会话池内存
   close(server_fd);
   return 0;
}
void flash_accept(int server_fd,SESSION* SESSPOOL, uint16_t sessionums,uint16_t backlog)
{
   /*
   功能：刷新accept连接，将新连接加入会话池
   */
   for(int i=0;i<backlog;i++){
      if(_SESSIONS_CURRENT_NUM_>=sessionums) {
         break;// 会话池已满，退出循环
      }
      //接受新连接
      struct sockaddr new_client_addr;//即将存储待连接的客户端地址
      int addrlen = sizeof(struct sockaddr_in);
      int new_socket=accept(server_fd, &new_client_addr, (socklen_t*)&addrlen);//获得通信套接字文件描述符
      if(new_socket<0){
         if(errno==EAGAIN||errno==EWOULDBLOCK) {// 没有新连接
            break;
         }
         continue;
      }
      _SESSIONS_CURRENT_NUM_++;//当前会话数量+1
      //找到一个空闲的会话
      for(int j=0;j<sessionums;j++){
         if(SESSPOOL[j].status==SESS_idle){
            SESSPOOL[j].sessocket=new_socket;//初始化通信套接字文件描述符
            SESSPOOL[j].client_addr=new_client_addr;//初始化客户端ip地址
            SESSPOOL[j].time=time(NULL);//初始化会话建立时间
            SESSPOOL[j].status=SESS_alive;//初始化会话状态
            SESSPOOL[j].priority=1;//初始化会话优先级
            SESSPOOL[j].user_id = 
         }  
      }
   }
  
}





int my_main() {
    SESSION* SESSPOOL=NULL;
    int server_fd = start_session_server(PORT,SESSION_backlog,SESSPOOL,MAX_SESSIONS,BUFFER_SIZE);
    if(server_fd == err) return err;



   int new_socket;//通信套接字文件描述符   

    // 接受客户端连接
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