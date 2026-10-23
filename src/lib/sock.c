/*
sock.c —— 跨平台 socket 薄封装

这一层只把两个平台不一致的地方收口：初始化、非阻塞、错误码、收发签名、
"等谁就绪"。它不做缓冲 —— 攒半包、排队输出都是服务端自己的事。

等待用 select（Windows）和 poll（POSIX）实现。没用 WSAPoll，是因为它在
Windows 上会漏报/误报就绪状态（libevent 就为此退回 select）；select 慢一点，
但行为确定，而这里的连接数本来就有上限。
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
  #include <fcntl.h>        /* O_NONBLOCK */
  #include <poll.h>         /* poll */
  #include <errno.h>
  #include <signal.h>       /* SIGPIPE */
#endif

/* select 的 fd_set 是定长数组。Makefile 在 Windows 上把 FD_SETSIZE 抬到 512；
 * 漏了这一步会变成"第 65 个连接开始 select 失败"，很难查，所以宁可编不过。 */
#if defined(_WIN32) && FD_SETSIZE < SOCK_MAX_EVENTS
#error "FD_SETSIZE 太小：Windows 构建需要在 CFLAGS 里加 -DFD_SETSIZE=512"
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

static int was_would_block(void)
{
#ifdef _WIN32
    return WSAGetLastError() == WSAEWOULDBLOCK;
#else
    return errno == EAGAIN || errno == EWOULDBLOCK;
#endif
}

int sock_last_would_block(void)
{
    return was_would_block();
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
    if (was_would_block()) return SOCK_AGAIN;
    return SOCK_ERR;
}

long sock_send_some(mhx_sock_t s, const void *buf, size_t len)
{
    if (len == 0) return 0;

    int n = send(s, (const char*)buf, clamp_len(len), 0);
    if (n > 0) return (long)n;
    if (n == 0) return 0;
    if (was_interrupted()) return SOCK_RETRY;
    if (was_would_block()) return SOCK_AGAIN;
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

int sock_set_nonblocking(mhx_sock_t s, int on)
{
#ifdef _WIN32
    u_long nb = on ? 1UL : 0UL;
    return ioctlsocket(s, FIONBIO, &nb) == 0 ? 0 : -1;
#else
    int fl = fcntl(s, F_GETFL, 0);
    if (fl < 0) return -1;
    if (on) fl |= O_NONBLOCK;
    else    fl &= ~O_NONBLOCK;
    return fcntl(s, F_SETFL, fl) == 0 ? 0 : -1;
#endif
}

int sock_wait(sock_event *evs, int n, int timeout_ms)
{
    if (!evs || n <= 0 || n > SOCK_MAX_EVENTS) return SOCK_ERR;

    for (int i = 0; i < n; i++) evs[i].can_read = evs[i].can_write = 0;

#ifdef _WIN32
    fd_set rf, wf;
    FD_ZERO(&rf);
    FD_ZERO(&wf);
    for (int i = 0; i < n; i++) {
        if (evs[i].want_read)  FD_SET(evs[i].fd, &rf);
        if (evs[i].want_write) FD_SET(evs[i].fd, &wf);
    }

    struct timeval tv;
    struct timeval *ptv = NULL;
    if (timeout_ms >= 0) {
        tv.tv_sec  = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        ptv = &tv;
    }

    int rc = select(0, &rf, &wf, NULL, ptv);
    if (rc == SOCKET_ERROR) return was_interrupted() ? 0 : SOCK_ERR;

    for (int i = 0; i < n; i++) {
        if (evs[i].want_read  && FD_ISSET(evs[i].fd, &rf)) evs[i].can_read  = 1;
        if (evs[i].want_write && FD_ISSET(evs[i].fd, &wf)) evs[i].can_write = 1;
    }
    return rc;
#else
    struct pollfd pf[SOCK_MAX_EVENTS];
    for (int i = 0; i < n; i++) {
        pf[i].fd      = evs[i].fd;
        pf[i].events  = 0;
        if (evs[i].want_read)  pf[i].events |= POLLIN;
        if (evs[i].want_write) pf[i].events |= POLLOUT;
        pf[i].revents = 0;
    }

    int rc = poll(pf, (nfds_t)n, timeout_ms);
    if (rc < 0) return errno == EINTR ? 0 : SOCK_ERR;

    for (int i = 0; i < n; i++) {
        /* POLLERR / POLLHUP / POLLNVAL 不看有没有申请都会回来，
         * 一律当作"可读"处理：让调用方去做一次 recv，从错误码上分辨。 */
        short bad = (short)(pf[i].revents & (POLLERR | POLLHUP | POLLNVAL));
        if (evs[i].want_read  && (pf[i].revents & POLLIN))  evs[i].can_read  = 1;
        if (evs[i].want_write && (pf[i].revents & POLLOUT)) evs[i].can_write = 1;
        if (bad) { evs[i].can_read = 1; evs[i].can_write = 1; }
    }
    return rc;
#endif
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
    if (listen(fd, 128) != 0) {  /* backlog：accept 一时跟不上时，新连接在这里等 */
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
