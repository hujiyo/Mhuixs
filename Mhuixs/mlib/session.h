/*
#include <stdint.h>
#include <time.h>
#ifndef SESSION_H
#define SESSION_H
#include "Mhudef.h"

#ifdef _WIN32
#include "session-win.c"
#else
#include "session-linux.c"
#endif


typedef uint32_t USER_ID;

typedef struct SESSION{
   int sessocket; // 通信套接字文件描述符
   struct sockaddr client_addr; // 客户端地址

   USER_ID user_id; // 客户端身份id
   RANK rank; // 客户端权限等级

   time_t time; // 会话建立时间
   uint32_t life; // 会话生命剩余（单位:s）
   int status; // 会话状态
   int attribute; // 会话属性   

   uint8_t* buffer; // 会话缓冲区
   uint32_t buffer_size; // 会话缓冲区大小（固定）（1KB/8KB/16KB/64KB）
   uint32_t datlen; // 已缓存的数据量
   uint32_t pos; // 数据读取头偏移量   
} SESSION;


void flash_sessions(SESSION* session, uint16_t num); // 刷新所有会话的状态、生命。

int kill_session(SESSION* session); // 强制释放某会话资源

#endif
*/