/* 集成测试：网络前端（真 socket，不是打桩）。
 *
 * 为什么值得这么做：现有几套测试全是同进程直调 API，跑不到真实启动路径。
 * 这一套用一个线程跑服务端、主线程当客户端，走的是**真的 socket、
 * 真的分帧、真的阻塞语义** —— 半包、粘包、断线这些只有在真连接上
 * 才会暴露的问题才有机会被逮到。
 *
 * 编排：每一轮新建一个监听套接字（端口交给系统挑），起一个服务线程，
 * 主线程连上去把要说的话一次说完，再读到对端关闭为止，最后断言收到的
 * 整段文本。用"读到关闭"而不是逐条解析，是因为这样客户端侧不需要再写
 * 一遍分帧逻辑 —— 拿被测对象去测它自己没意义。
 *
 * 请求一律用 build_req() 拼：长度由程序算。手写 "$5\r\ntype" 这种
 * 很容易数错（type 是 4 字节），而数错的后果是服务端回一句
 * "missing CRLF after argument body"，跟被测逻辑毫无关系。
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#include "server.h"
#include "session.h"
#include "registry.h"
#include "env.h"
#include "mstring.h"
#include "merr.h"

#include <pthread.h>

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("  FAIL: %s\n", msg); fails++; } } while (0)

#define TOKEN "s3cr3t-token"

static mhx_sock_t g_listener = MHX_SOCK_INVALID;
static int        g_serve_rc = -999;

static void *serve_thread(void *arg)
{
    (void)arg;
    mhx_sock_t conn = sock_accept(g_listener);
    if (conn == MHX_SOCK_INVALID) { g_serve_rc = -998; return NULL; }

    g_serve_rc = mhx_server_serve_one(conn, TOKEN);
    sock_close(conn);
    return NULL;
}

/* 起一轮：监听 -> 服务线程 -> 客户端连上。返回客户端句柄。
 * 连不上时在这里就把失败记下来 —— 否则调用方的断言全写在 if 里，
 * 连不上会变成"一项都没失败"的假通过。 */
static mhx_sock_t open_round(pthread_t *th)
{
    g_listener = sock_listen("127.0.0.1", 0);   /* 0 = 端口交给系统挑 */
    if (g_listener == MHX_SOCK_INVALID) {
        printf("  FAIL: 监听失败\n");
        fails++;
        return MHX_SOCK_INVALID;
    }

    int port = 0;
    if (!sock_local_port(g_listener, &port)) {
        printf("  FAIL: 取不到监听端口\n");
        fails++;
        sock_close(g_listener);
        g_listener = MHX_SOCK_INVALID;
        return MHX_SOCK_INVALID;
    }

    g_serve_rc = -999;
    if (pthread_create(th, NULL, serve_thread, NULL) != 0) {
        printf("  FAIL: 起不了服务线程\n");
        fails++;
        sock_close(g_listener);
        g_listener = MHX_SOCK_INVALID;
        return MHX_SOCK_INVALID;
    }

    mhx_sock_t cli = sock_connect("127.0.0.1", port);
    if (cli == MHX_SOCK_INVALID) {
        printf("  FAIL: 连不上 127.0.0.1:%d\n", port);
        fails++;
    }
    return cli;
}

static void close_round(pthread_t th, mhx_sock_t cli)
{
    sock_close(cli);
    pthread_join(th, NULL);
    sock_close(g_listener);
    g_listener = MHX_SOCK_INVALID;
}

/*
 * 拼一条请求：*n 后面跟若干 $<长度> 的参数。
 * 参数是可变参数表，以 NULL 结尾；长度由程序算。
 */
static void build_req(char *dst, size_t dstlen, ...)
{
    va_list ap;
    int n = 0;

    va_start(ap, dstlen);
    while (va_arg(ap, const char *) != NULL) n++;
    va_end(ap);

    size_t used = (size_t)snprintf(dst, dstlen, "*%d\r\n", n);

    va_start(ap, dstlen);
    for (const char *s = va_arg(ap, const char *); s; s = va_arg(ap, const char *)) {
        if (used >= dstlen) break;
        used += (size_t)snprintf(dst + used, dstlen - used, "$%zu\r\n%s\r\n", strlen(s), s);
    }
    va_end(ap);
}

static int send_all(mhx_sock_t c, const char *s)
{
    return sock_send_all(c, s, strlen(s)) == (long)strlen(s) ? 0 : -1;
}

/* 发一条命令（参数以 NULL 结尾），返回 sock_send_all 的结果 */
static int send_cmd(mhx_sock_t c, ...)
{
    char buf[1024];
    va_list ap;
    int n = 0;

    va_start(ap, c);
    while (va_arg(ap, const char *) != NULL) n++;
    va_end(ap);

    size_t used = (size_t)snprintf(buf, sizeof(buf), "*%d\r\n", n);

    va_start(ap, c);
    for (const char *s = va_arg(ap, const char *); s; s = va_arg(ap, const char *)) {
        if (used >= sizeof(buf)) break;
        used += (size_t)snprintf(buf + used, sizeof(buf) - used, "$%zu\r\n%s\r\n", strlen(s), s);
    }
    va_end(ap);

    return send_all(c, buf);
}

static int send_auth(mhx_sock_t c) { return send_cmd(c, "AUTH", TOKEN, NULL); }

/* 读到对端关闭为止，全都攒进 buf（调用方负责足够大） */
static size_t drain(mhx_sock_t c, char *buf, size_t buflen)
{
    size_t n = 0;
    for (;;) {
        if (n + 1 >= buflen) break;
        long got = sock_recv(c, buf + n, buflen - n - 1);
        if (got > 0) { n += (size_t)got; continue; }
        if (got == SOCK_RETRY) continue;
        break;   /* 0 = 对端有序关闭；SOCK_ERR 也收摊 */
    }
    buf[n] = '\0';
    return n;
}

static int has(const char *hay, const char *needle) { return strstr(hay, needle) != NULL; }

int main(void)
{
    static char rx[65536];
    pthread_t th;

    if (sock_init() != 0) { printf("sock_init 失败\n"); return 1; }
    logger_init(".");
    if (reg_init() != 0) { printf("reg_init failed\n"); return 1; }

    /* 万一走到 :save 会往这里写；指到当前目录，跑完自己删 */
    Env.MhuixsHomePath = mstr((char*)".");
    remove("registry.dat");

    printf("未认证就发命令：拒绝并断开\n");
    {
        mhx_sock_t cli = open_round(&th);
        if (cli != MHX_SOCK_INVALID) {
            send_cmd(cli, "llen", "todo", NULL);
            drain(cli, rx, sizeof(rx));
            CHECK(has(rx, "-NOAUTH"), "回的是 NOAUTH");
            CHECK(has(rx, "authenticate first"), "说明了要先认证");
            close_round(th, cli);
        }
    }

    printf("令牌不对：拒绝并断开\n");
    {
        mhx_sock_t cli = open_round(&th);
        if (cli != MHX_SOCK_INVALID) {
            send_cmd(cli, "AUTH", "wrong", NULL);
            drain(cli, rx, sizeof(rx));
            CHECK(has(rx, "-NOAUTH invalid token"), "回的是 invalid token");
            close_round(th, cli);
        }
    }

    printf("认证 + 数据命令 + :quit\n");
    {
        mhx_sock_t cli = open_round(&th);
        if (cli != MHX_SOCK_INVALID) {
            send_auth(cli);
            send_cmd(cli, "create", "list", "netlist", NULL);
            send_cmd(cli, "rpush", "netlist", "a", "b", NULL);
            send_cmd(cli, "llen", "netlist", NULL);
            send_cmd(cli, ":quit", NULL);

            drain(cli, rx, sizeof(rx));
            CHECK(has(rx, "+OK\r\n"), "认证成功回 +OK");
            CHECK(has(rx, "\"ok\":true,\"code\":\"ok\"") , "数据命令回的是 JSON 文档");
            CHECK(has(rx, "\"value\":2"), "llen 的值是 2");
            close_round(th, cli);
        }
    }

    printf("半包：命令逐字节发送也能正确执行\n");
    {
        mhx_sock_t cli = open_round(&th);
        if (cli != MHX_SOCK_INVALID) {
            send_auth(cli);
            send_cmd(cli, "create", "list", "slowlist", NULL);

            /* 逐字节喂过去：每次 send 只给服务端一个字节 */
            char req[256];
            build_req(req, sizeof(req), "llen", "slowlist", NULL);
            for (size_t i = 0; i < strlen(req); i++)
                sock_send_all(cli, req + i, 1);

            send_cmd(cli, ":quit", NULL);
            drain(cli, rx, sizeof(rx));
            CHECK(has(rx, "\"value\":0"), "逐字节发来的命令也被正确执行（空列表 -> 0）");
            close_round(th, cli);
        }
    }

    printf("粘包：一次发两条，两条响应按序回来\n");
    {
        mhx_sock_t cli = open_round(&th);
        if (cli != MHX_SOCK_INVALID) {
            send_auth(cli);

            /* 两条命令拼在**一次 send** 里 */
            char a[256], b[256], both[512];
            build_req(a, sizeof(a), "create", "list", "pipe_a", NULL);
            build_req(b, sizeof(b), "type", "pipe_a", NULL);
            snprintf(both, sizeof(both), "%s%s", a, b);
            send_all(cli, both);

            send_cmd(cli, ":quit", NULL);
            drain(cli, rx, sizeof(rx));
            {
                const char *first  = strstr(rx, "OK created list");
                const char *second = strstr(rx, "\"value\":\"list\"");
                if (!(first && second && first < second)) {
                    printf("  FAIL: 两条响应按发送顺序回来\n");
                    printf("        实际收到: %.*s\n", 400, rx);
                    fails++;
                }
            }
            close_round(th, cli);
        }
    }

    printf("二进制安全：值里带空格与 CRLF，存取一致\n");
    {
        mhx_sock_t cli = open_round(&th);
        if (cli != MHX_SOCK_INVALID) {
            send_auth(cli);
            send_cmd(cli, "create", "kvalot", "bin", NULL);
            send_cmd(cli, "kset", "bin", "k", "a\r\nb", NULL);
            send_cmd(cli, "kget", "bin", "k", NULL);
            send_cmd(cli, ":quit", NULL);

            drain(cli, rx, sizeof(rx));
            /* 值 "a<CR><LF>b" 存进去、取出来仍是同样 4 个字节，
             * 在 JSON 里呈现为转义后的 a\r\nb */
            CHECK(has(rx, "a\\r\\nb"), "含 CRLF 的值原样存回");
            close_round(th, cli);
        }
    }

    printf(":shutdown 让服务端停机\n");
    {
        mhx_sock_t cli = open_round(&th);
        if (cli != MHX_SOCK_INVALID) {
            send_auth(cli);
            send_cmd(cli, ":shutdown", NULL);
            drain(cli, rx, sizeof(rx));
            CHECK(has(rx, "停机"), "回了一条停机应答");
            close_round(th, cli);
            CHECK(g_serve_rc == SERVE_STOP, "serve_one 返回 SERVE_STOP");
        }
    }

    printf("协议错：非法请求被拒绝\n");
    {
        mhx_sock_t cli = open_round(&th);
        if (cli != MHX_SOCK_INVALID) {
            send_auth(cli);
            send_all(cli, "*0\r\n");          /* 参数个数为 0 */
            drain(cli, rx, sizeof(rx));
            CHECK(has(rx, "-PROTO"), "回的是 PROTO 错误");
            close_round(th, cli);
            CHECK(g_serve_rc == SERVE_FAIL, "serve_one 返回 SERVE_FAIL");
        }
    }

    remove("registry.dat");
    remove("registry.dat.tmp");
    remove("log.inf");
    mstr_free(Env.MhuixsHomePath);
    Env.MhuixsHomePath = NULL;

    reg_destroy();
    sock_cleanup();

    if (fails) { printf("\n网络集成测试：%d 项失败\n", fails); return 1; }
    printf("\n网络集成测试：全部通过\n");
    return 0;
}
