#ifndef SOCK_H
#define SOCK_H

#include <stddef.h>

/*
sock.h —— 跨平台 socket 薄封装

只包住两个平台真正不一样的地方：初始化、关闭、错误码、收发签名。
上层（server.c）因此可以只写一份逻辑，不必到处 #ifdef。

注意包含顺序：本头文件在 Windows 上会拉进 <winsock2.h>，而它**必须**
排在 <windows.h> 之前。目前没有任何头文件包含 windows.h，所以照常包含
本文件即可；将来若要引入 windows.h，务必放在本文件之后。
*/

#ifdef _WIN32
  #include <winsock2.h>
  typedef SOCKET mhx_sock_t;
  #define MHX_SOCK_INVALID INVALID_SOCKET
#else
  typedef int mhx_sock_t;
  #define MHX_SOCK_INVALID (-1)
#endif

/* 收发函数的失败返回值。用负数是因为字节数恒为非负，不会撞车。 */
#define SOCK_ERR   (-1L)   /* 真错误，连接应关闭 */
#define SOCK_RETRY (-2L)   /* 被信号打断，重试即可 */

/* 进程级初始化 / 收尾。Windows 要 WSAStartup，POSIX 是空操作。
 *
 * 另外这个函数会忽略 SIGPIPE：客户端在响应写到一半断开时，POSIX 的默认
 * 行为是**直接终止整个进程** —— 一次断线就把常驻服务打掉。忽略之后
 * send 返回错误，由上层按普通错误处理。 */
int  sock_init(void);
void sock_cleanup(void);

void sock_close(mhx_sock_t s);

/* 上次失败的错误码（POSIX 是 errno，Windows 是 WSAGetLastError） */
int sock_last_error(void);

/*
 * 收一次。返回值：
 *   > 0  实际收到的字节数
 *   0    对端有序关闭
 *   SOCK_RETRY / SOCK_ERR
 * 只收一次，不收满 —— 半包由调用方的缓冲自己攒。
 */
long sock_recv(mhx_sock_t s, void *buf, size_t len);

/*
 * 发完整块，内部循环补齐部分写。返回已发送字节数（等于 len），
 * 或 SOCK_ERR。上层不必关心 TCP 分片。
 */
long sock_send_all(mhx_sock_t s, const void *buf, size_t len);

/* 监听。bind_addr 传 NULL 时用 127.0.0.1。失败返回 MHX_SOCK_INVALID。 */
mhx_sock_t sock_listen(const char *bind_addr, int port);

/* 接受一个连接。失败返回 MHX_SOCK_INVALID（可查 sock_last_error 决定是否重试）。 */
mhx_sock_t sock_accept(mhx_sock_t listener);

/* 连接出去。测试和将来的客户端要用。 */
mhx_sock_t sock_connect(const char *host, int port);

/* 把对端地址写成 "ip:port" 便于日志。失败返回 0。 */
int sock_peer_addr(mhx_sock_t s, char *buf, size_t buflen);

#endif /* SOCK_H */
