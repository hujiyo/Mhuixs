/*
sock.c —— 跨平台 socket 薄封装

这一层刻意做得薄：只把两个平台不一致的地方收口，不做缓冲、不做超时、
不做事件循环。上层拿着它写最朴素的阻塞式收发即可。
*/

#include "sock.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
  #include <ws2tcpip.h>     /* inet_pton */
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <netinet/tcp.h>  /* TCP_NODELAY */
  #include <arpa/inet.h>    /* inet_pton */
  #include <unistd.h>       /* close */
  #include <errno.h>
  #include <signal.h>       /* SIGPIPE */
#endif

int sock_init(void)
{
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return -1;
#else
    /* 见 sock.h：不忽略 SIGPIPE 的话，一次断线就能把服务进程杀掉 */
    signal(SIGPIPE, SIG_IGN);
#endif
    return 0;
}

void sock_cleanup(void)
{
#ifdef _WIN32
    WSACleanup();
#endif
}

void sock_close(mhx_sock_t s)
{
    if (s == MHX_SOCK_INVALID) return;
#ifdef _WIN32
    closesocket(s);
#else
    close(s);
#endif
}

int sock_last_error(void)
{
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

static int was_interrupted(void)
{
#ifdef _WIN32
    return WSAGetLastError() == WSAEINTR;
#else
    return errno == EINTR;
#endif
}

int sock_last_was_interrupted(void)
{
    return was_interrupted();
}

/* 关掉 Nagle：命令都是小包，攒着发只会平白多出几十毫秒延迟 */
static void set_nodelay(mhx_sock_t s)
{
    int yes = 1;
#ifdef _WIN32
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char*)&yes, sizeof(yes));
#else
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));
#endif
}

static int clamp_len(size_t len)
{
    /* 两个平台的收发都吃 int 长度 */
    return len > 0x7FFFFFFFu ? 0x7FFFFFFF : (int)len;
}

long sock_recv(mhx_sock_t s, void *buf, size_t len)
{
    if (len == 0) return 0;

    int n = recv(s, (char*)buf, clamp_len(len), 0);
    if (n > 0)  return (long)n;
    if (n == 0) return 0;                 /* 对端有序关闭 */
    if (was_interrupted()) return SOCK_RETRY;
    return SOCK_ERR;
}

long sock_send_all(mhx_sock_t s, const void *buf, size_t len)
{
    const char *p = (const char*)buf;
    size_t sent = 0;

    while (sent < len) {
        int n = send(s, p + sent, clamp_len(len - sent), 0);
        if (n > 0) { sent += (size_t)n; continue; }
        if (n < 0 && was_interrupted()) continue;
        return SOCK_ERR;
    }
    return (long)sent;
}

mhx_sock_t sock_listen(const char *bind_addr, int port)
{
    /* port 传 0 表示"端口交给系统挑"，实际值事后用 sock_local_port 问 */
    if (port < 0 || port > 65535) return MHX_SOCK_INVALID;

    mhx_sock_t fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == MHX_SOCK_INVALID) return MHX_SOCK_INVALID;

#ifdef _WIN32
    /* Windows 上 SO_REUSEADDR 的语义是"允许抢占别人正在监听的端口"，
     * 那是个安全隐患。这里要的是"独占"，用 SO_EXCLUSIVEADDRUSE。 */
    {
        BOOL excl = TRUE;
        setsockopt(fd, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char*)&excl, sizeof(excl));
    }
#else
    /* POSIX 上 SO_REUSEADDR 的语义正相反：只为绕开 TIME_WAIT 导致的
     * "地址已占用"，不会抢别人的端口。 */
    {
        int yes = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    }
#endif

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((unsigned short)port);

    const char *ip = bind_addr ? bind_addr : "127.0.0.1";
    if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1) {
        sock_close(fd);
        return MHX_SOCK_INVALID;
    }

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        sock_close(fd);
        return MHX_SOCK_INVALID;
    }
    if (listen(fd, 16) != 0) {   /* backlog：第二个客户端会在这里排队 */
        sock_close(fd);
        return MHX_SOCK_INVALID;
    }
    return fd;
}

mhx_sock_t sock_accept(mhx_sock_t listener)
{
    mhx_sock_t c = accept(listener, NULL, NULL);
    if (c == MHX_SOCK_INVALID) return MHX_SOCK_INVALID;
    set_nodelay(c);
    return c;
}

mhx_sock_t sock_connect(const char *host, int port)
{
    mhx_sock_t fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == MHX_SOCK_INVALID) return MHX_SOCK_INVALID;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((unsigned short)port);
    if (inet_pton(AF_INET, host ? host : "127.0.0.1", &addr.sin_addr) != 1) {
        sock_close(fd);
        return MHX_SOCK_INVALID;
    }

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        sock_close(fd);
        return MHX_SOCK_INVALID;
    }
    set_nodelay(fd);
    return fd;
}

int sock_peer_addr(mhx_sock_t s, char *buf, size_t buflen)
{
    struct sockaddr_in addr;
#ifdef _WIN32
    int alen = (int)sizeof(addr);
#else
    socklen_t alen = sizeof(addr);
#endif
    memset(&addr, 0, sizeof(addr));

    if (getpeername(s, (struct sockaddr*)&addr, &alen) != 0) return 0;

    char ip[64];
    if (!inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip))) return 0;

    snprintf(buf, buflen, "%s:%u", ip, (unsigned)ntohs(addr.sin_port));
    return 1;
}

int sock_local_port(mhx_sock_t s, int *out)
{
    if (!out) return 0;

    struct sockaddr_in addr;
#ifdef _WIN32
    int alen = (int)sizeof(addr);
#else
    socklen_t alen = sizeof(addr);
#endif
    memset(&addr, 0, sizeof(addr));

    if (getsockname(s, (struct sockaddr*)&addr, &alen) != 0) return 0;
    *out = (int)ntohs(addr.sin_port);
    return 1;
}
