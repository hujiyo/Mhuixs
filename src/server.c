/*
server.c —— 网络前端

读字节 -> proto 切出参数 -> 会话层执行 -> 回一个 JSON 文档。
就这三步，没有更多。

并发模型：单线程事件循环。所有连接由 sock_wait() 一起盯着，谁就绪处理谁；
**命令执行仍然是串行的**（和 Redis 一样），所以这里没有任何锁，也不存在
两条命令同时改注册表的可能。一个空闲或收得慢的客户端只影响它自己：
应答先堆在它自己的输出缓冲里，等下次可写再送。
*/

#include "server.h"
#include "session.h"
#include "proto.h"
#include "merr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 同时在线的连接上限。Windows 的 select 受 FD_SETSIZE 限制（Makefile 里
 * 抬到 512），这里取 256 留余量。超出的连接直接关掉并记日志：宁可明确
 * 拒绝，也不让 fd_set 悄悄溢出。 */
#define MHX_MAX_CONNS 256

/* 读缓冲的起始容量。按需翻倍，上限是 proto 定的单条命令上限。 */
#define RB_MIN 4096

/* 一轮事件里单个连接最多读这么多字节，读完把机会让给别的连接。 */
#define READ_BUDGET ((size_t)256 * 1024)

/*
 * 单次会话的输出缓冲。JSON 是文本的超集、转义后还会膨胀，按 execute.h
 * 的建议给到 128KB。放在堆上：这条路径每个连接都会走，128KB 的栈帧不划算。
 */
#define SESSION_OUT 131072

/*
 * 单连接待发数据的高水位。到了就不再读这个连接、也不再解析它的输入，
 * 等可写事件把缓冲排空再继续 —— 客户端只发不收时，内存不会无限涨。
 */
#define OUT_HIGH_WATER ((size_t)1 << 20)

/* conn_process 的返回值 */
#define PROC_OK           0
#define PROC_QUIT         1
#define PROC_STOP         2
#define PROC_FAIL         3
#define PROC_BACKPRESSURE 4   /* 输出到高水位提前收手；缓冲排空后接着算 */

typedef struct {
    mhx_sock_t  fd;
    mhx_session sess;

    char  *in;          /* 输入缓冲：in[in_start..in_end) 还没处理 */
    size_t in_cap;
    size_t in_start;
    size_t in_end;

    char  *out;         /* 输出缓冲：out[out_sent..out_len) 还没发出 */
    size_t out_cap;
    size_t out_len;
    size_t out_sent;

    int    closing;     /* 不再读也不再解析；out 排空后关闭 */
} conn;

static void conn_reset(conn *c)
{
    memset(c, 0, sizeof(*c));
    c->fd = MHX_SOCK_INVALID;
}

static void conn_release(conn *c)
{
    if (c->fd != MHX_SOCK_INVALID) sock_close(c->fd);
    free(c->in);
    free(c->out);
    conn_reset(c);
}

/* ---------------------------------------------------------------- */
/* 输出缓冲                                                          */
/* ---------------------------------------------------------------- */

static int out_push(conn *c, const char *p, size_t n)
{
    if (n == 0) return 0;

    /* 先把已经发出的头部挪掉，再决定要不要扩容 */
    if (c->out_sent > 0) {
        if (c->out_len > c->out_sent)
            memmove(c->out, c->out + c->out_sent, c->out_len - c->out_sent);
        c->out_len -= c->out_sent;
        c->out_sent = 0;
    }

    if (c->out_len + n > c->out_cap) {
        size_t ncap = c->out_cap ? c->out_cap : 8192;
        while (ncap < c->out_len + n) ncap *= 2;
        char *nb = (char*)realloc(c->out, ncap);
        if (!nb) return -1;
        c->out     = nb;
        c->out_cap = ncap;
    }

    memcpy(c->out + c->out_len, p, n);
    c->out_len += n;
    return 0;
}

static int push_err(conn *c, const char *tag, const char *msg)
{
    char buf[512];
    size_t n = proto_encode_err(buf, sizeof(buf), tag, msg);
    if (n == 0) return -1;
    return out_push(c, buf, n);
}

static int push_ok(conn *c)
{
    char buf[8];
    size_t n = proto_encode_ok(buf, sizeof(buf));
    if (n == 0) return -1;
    return out_push(c, buf, n);
}

/*
 * 发一段 JSON 文档。去掉 render_json / mhx_json_text 末尾那一个换行 ——
 * 载荷就是干净的 JSON 文档本身。
 */
static int push_json(conn *c, char *scratch, size_t scratch_len, const char *json)
{
    size_t n = strlen(json);
    if (n > 0 && json[n - 1] == '\n') n--;

    size_t wn = proto_encode_bulk(scratch, scratch_len, json, n);
    if (wn == 0) return -1;   /* scratch 不够大，属于内部错误 */
    return out_push(c, scratch, wn);
}

/* 发一条"只带一句话"的成功应答 */
static int push_note(conn *c, char *scratch, size_t scratch_len, const char *text)
{
    char tmp[512];
    mhx_json_text(tmp, sizeof(tmp), 1, "ok", text);
    return push_json(c, scratch, scratch_len, tmp);
}

/* 参数与字面量做大小写不敏感比较（命令名用） */
static int arg_eq_ci(const proto_arg *a, const char *lit)
{
    size_t n = strlen(lit);
    if (a->len != n) return 0;

    for (size_t i = 0; i < n; i++) {
        char c = a->p[i];
        char d = lit[i];
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        if (d >= 'a' && d <= 'z') d = (char)(d - 'a' + 'A');
        if (c != d) return 0;
    }
    return 1;
}

/*
 * 令牌比对。先比长度、再逐字节累积异或 —— 这是**卫生习惯**（别让比较
 * 提前返回），不是密码学保证：本协议明文传输，真要防的是网络窃听，
 * 那得靠 TLS，不在当前范围内。
 */
static int token_ok(const proto_arg *a, const char *token)
{
    size_t tl = strlen(token);
    if (a->len != tl) return 0;

    unsigned char diff = 0;
    for (size_t i = 0; i < tl; i++)
        diff |= (unsigned char)(a->p[i] ^ token[i]);
    return diff == 0;
}

/* ---------------------------------------------------------------- */
/* 单个连接：读 -> 执行 -> 发                                        */
/* ---------------------------------------------------------------- */

/*
 * 非阻塞读，直到内核暂时没数据为止。
 * 返回 1 = 读到了（可能还有），0 = 暂时没有更多，-1 = 对端关闭 / 出错。
 * 缓冲已经到单条命令上限还凑不出完整命令时，置 *too_large 并返回 -1。
 */
static int conn_read(conn *c, int *too_large)
{
    if (too_large) *too_large = 0;

    size_t total = 0;
    for (;;) {
        if (c->in_end == c->in_cap) {
            if (c->in_start > 0) {
                memmove(c->in, c->in + c->in_start, c->in_end - c->in_start);
                c->in_end  -= c->in_start;
                c->in_start = 0;
            }
            if (c->in_end == c->in_cap) {
                if (c->in_cap >= PROTO_MAX_CMD_BYTES) {
                    if (too_large) *too_large = 1;
                    return -1;
                }
                size_t ncap = c->in_cap * 2;
                if (ncap > PROTO_MAX_CMD_BYTES) ncap = PROTO_MAX_CMD_BYTES;
                char *nb = (char*)realloc(c->in, ncap);
                if (!nb) return -1;
                c->in     = nb;
                c->in_cap = ncap;
            }
        }

        long got = sock_recv(c->fd, c->in + c->in_end, c->in_cap - c->in_end);
        if (got > 0) {
            c->in_end += (size_t)got;
            total     += (size_t)got;
            if (total >= READ_BUDGET) return 1;
            continue;
        }
        if (got == SOCK_RETRY) continue;
        if (got == SOCK_AGAIN) return 0;
        return -1;                     /* 0 = 对端有序关闭；SOCK_ERR = 出错 */
    }
}

/*
 * 把输入缓冲里能凑齐的命令全部执行掉，结果堆进输出缓冲。
 * 返回 PROC_*；PROC_FAIL / PROC_QUIT / PROC_STOP 都会顺手置 closing。
 */
static int conn_process(conn *c, const char *token, char *out,
                        char *scratch, size_t scratch_len)
{
    while (!c->closing) {
        /* 背压：发不出去就先别接着算，否则缓冲没有上限。
         * 注意要返回一个区别于"输入处理完了"的原因 —— 缓冲排空后
         * 还得回来接着算，否则命令会永远躺在输入缓冲里等一个不会来的事件。 */
        if (c->out_len - c->out_sent >= OUT_HIGH_WATER) return PROC_BACKPRESSURE;
        if (c->in_start >= c->in_end) { c->in_start = c->in_end = 0; return PROC_OK; }

        proto_cmd cmd;
        proto_result pr = proto_parse(c->in + c->in_start, c->in_end - c->in_start, &cmd);

        if (pr == PROTO_INCOMPLETE) {
            /* 头部挪回去，给后续读入腾地方 */
            if (c->in_start > 0) {
                memmove(c->in, c->in + c->in_start, c->in_end - c->in_start);
                c->in_end  -= c->in_start;
                c->in_start = 0;
            }
            return PROC_OK;
        }
        if (pr == PROTO_ERR) {
            push_err(c, "PROTO", cmd.err ? cmd.err : "malformed request");
            c->closing = 1;
            return PROC_FAIL;
        }

        c->in_start += cmd.consumed;

        if (!c->sess.authenticated) {
            if (!(cmd.argc == 2 && arg_eq_ci(&cmd.args[0], "AUTH"))) {
                push_err(c, "NOAUTH", "authenticate first");
                c->closing = 1;
                return PROC_FAIL;
            }
            if (!token || !token[0] || !token_ok(&cmd.args[1], token)) {
                push_err(c, "NOAUTH", "invalid token");
                c->closing = 1;
                return PROC_FAIL;
            }
            c->sess.authenticated = 1;
            push_ok(c);
            continue;
        }

        mhx_sess_status st = mhx_session_feed(&c->sess, cmd.argc, cmd.args, NULL,
                                              out, SESSION_OUT);

        /* 这两条要先把应答排出去再收摊，否则客户端只看到连接被掐 */
        if (st == MHX_SESS_QUIT) {
            push_note(c, scratch, scratch_len, "再见");
            c->closing = 1;
            return PROC_QUIT;
        }
        if (st == MHX_SESS_SHUTDOWN) {
            push_note(c, scratch, scratch_len, "服务正在停机");
            c->closing = 1;
            return PROC_STOP;
        }

        if (push_json(c, scratch, scratch_len, out) != 0) {
            c->closing = 1;
            return PROC_FAIL;
        }
    }
    return PROC_OK;
}

/*
 * 尽量把输出缓冲发出去。返回 1 = 发完，0 = 暂时发不出去（等可写事件），
 * -1 = 出错，连接应当关闭。
 */
static int conn_flush(conn *c)
{
    while (c->out_sent < c->out_len) {
        long n = sock_send_some(c->fd, c->out + c->out_sent, c->out_len - c->out_sent);
        if (n > 0) { c->out_sent += (size_t)n; continue; }
        if (n == SOCK_RETRY) continue;
        if (n == SOCK_AGAIN) return 0;
        return -1;
    }
    c->out_sent = c->out_len = 0;
    return 1;
}

/* 停机时尽力把还排着队的应答送出去，最多等两秒，然后一律关闭 */
static void conn_finish(conn *c)
{
    if (c->fd == MHX_SOCK_INVALID) return;

    if (c->out_len > c->out_sent) {
        int waited = 0;
        while (waited < 2000) {
            if (conn_flush(c) != 0) break;
            sock_event ev;
            ev.fd = c->fd;
            ev.want_read = 0;
            ev.want_write = 1;
            ev.can_read = ev.can_write = 0;
            if (sock_wait(&ev, 1, 200) < 0) break;
            waited += 200;
        }
    }
    conn_release(c);
}

/* ---------------------------------------------------------------- */
/* 监听与事件循环                                                    */
/* ---------------------------------------------------------------- */

int mhx_server_run_listener(mhx_sock_t lfd, const char *token)
{
    conn  conns[MHX_MAX_CONNS];
    for (int i = 0; i < MHX_MAX_CONNS; i++) conn_reset(&conns[i]);

    /* 会话输出与编码暂存各一份就够：命令执行是串行的，一份轮着用 */
    char *out     = (char*)malloc(SESSION_OUT);
    char *scratch = (char*)malloc(SESSION_OUT + 64);
    if (!out || !scratch) {
        free(out);
        free(scratch);
        return -1;
    }

    if (sock_set_nonblocking(lfd, 1) != 0) {
        fprintf(stderr, "mhuixs: 监听套接字切不到非阻塞模式\n");
        free(out);
        free(scratch);
        return -1;
    }

    int stop = 0;
    int rc   = 0;

    sock_event evs[MHX_MAX_CONNS + 1];
    int        slot[MHX_MAX_CONNS + 1];   /* evs[i] 对应哪个连接 */

    while (!stop) {
        int n = 0;

        evs[n].fd         = lfd;
        evs[n].want_read  = 1;
        evs[n].want_write = 0;
        slot[n] = -1;
        n++;

        for (int i = 0; i < MHX_MAX_CONNS; i++) {
            conn *c = &conns[i];
            if (c->fd == MHX_SOCK_INVALID) continue;

            evs[n].fd         = c->fd;
            evs[n].want_read  = !c->closing &&
                                (c->out_len - c->out_sent) < OUT_HIGH_WATER;
            evs[n].want_write = (c->out_len > c->out_sent);
            slot[n] = i;
            n++;
        }

        int ready = sock_wait(evs, n, -1);
        if (ready < 0) {
            fprintf(stderr, "mhuixs: 等待连接出错（错误码 %d），停止监听\n",
                    sock_last_error());
            rc = -1;
            break;
        }
        if (ready == 0) continue;      /* 超时（理论上不会）/ 被信号打断 */

        /* 1) 新连接：非阻塞 accept，直到没有为止 */
        if (evs[0].can_read) {
            for (;;) {
                mhx_sock_t cfd = sock_accept(lfd);
                if (cfd == MHX_SOCK_INVALID) {
                    if (!sock_last_would_block() && !sock_last_was_interrupted())
                        logger("accept failed");
                    break;
                }

                int free_slot = -1;
                for (int i = 0; i < MHX_MAX_CONNS; i++) {
                    if (conns[i].fd == MHX_SOCK_INVALID) { free_slot = i; break; }
                }
                if (free_slot < 0) {
                    fprintf(stderr, "mhuixs: 连接数已达上限 %d，拒绝新连接\n",
                            MHX_MAX_CONNS);
                    sock_close(cfd);
                    continue;
                }

                if (sock_set_nonblocking(cfd, 1) != 0) { sock_close(cfd); continue; }

                conn *c = &conns[free_slot];
                conn_reset(c);
                c->fd = cfd;
                c->in = (char*)malloc(RB_MIN);
                if (!c->in) { conn_release(c); continue; }
                c->in_cap = RB_MIN;
                mhx_session_init(&c->sess, 1);

                char peer[64];
                if (sock_peer_addr(cfd, peer, sizeof(peer))) {
                    char line[128];
                    snprintf(line, sizeof(line), "connection from %s", peer);
                    logger(line);
                }
            }
        }

        /* 2) 已有连接：读 -> 执行 -> 发。即使这一轮没有可读事件，
         *    conn_process 也可能有上一轮背压留下的输入要接着处理。 */
        for (int i = 1; i < n; i++) {
            conn *c = &conns[slot[i]];
            if (c->fd == MHX_SOCK_INVALID) continue;

            if (evs[i].can_read) {
                int too_large = 0;
                int rr = conn_read(c, &too_large);
                if (too_large) {
                    push_err(c, "PROTO", "command too large");
                    c->closing = 1;
                } else if (rr < 0) {
                    conn_release(c);
                    continue;
                }
            }

            /* 执行 -> 发送 -> （缓冲排空且还有被背压挡住输入）接着执行 */
            for (;;) {
                int proc = conn_process(c, token, out, scratch, SESSION_OUT + 64);
                if (proc == PROC_STOP) stop = 1;

                if (c->out_len > c->out_sent) {
                    int fr = conn_flush(c);
                    if (fr < 0) { conn_release(c); break; }
                    if (fr == 0) break;          /* 发不出去，等可写事件 */
                }

                if (c->closing) {
                    if (c->out_len == c->out_sent) conn_release(c);
                    break;
                }

                if (proc == PROC_BACKPRESSURE) continue;
                break;
            }
        }
    }

    /* 停机：把排着队的应答尽量送完，再全部关闭 */
    for (int i = 0; i < MHX_MAX_CONNS; i++) conn_finish(&conns[i]);

    free(scratch);
    free(out);
    return rc;
}

int mhx_server_run(const char *bind_addr, int port, const char *token)
{
    /* 没配令牌就拒绝启动：宁可开不起来，也不要裸奔在网上 */
    if (!token || !token[0]) {
        fprintf(stderr, "mhuixs: 未配置访问令牌，拒绝启动服务端\n");
        fprintf(stderr, "        在配置里设 authtoken，或设环境变量 MHUIXS_TOKEN\n");
        return -1;
    }

    const char *ip = bind_addr ? bind_addr : "127.0.0.1";

    mhx_sock_t lfd = sock_listen(ip, port);
    if (lfd == MHX_SOCK_INVALID) {
        fprintf(stderr, "mhuixs: 无法监听 %s:%d（错误码 %d）\n",
                ip, port, sock_last_error());
        return -1;
    }

    fprintf(stderr, "  服务已监听 %s:%d（多连接并行接入，命令串行执行）\n", ip, port);
    fprintf(stderr, "  最多同时在线 %d 个连接\n", MHX_MAX_CONNS);

    int rc = mhx_server_run_listener(lfd, token);
    sock_close(lfd);
    return rc;
}

int mhx_server_serve_one(mhx_sock_t s, const char *token)
{
    conn c;
    conn_reset(&c);
    c.fd = s;

    char *out     = (char*)malloc(SESSION_OUT);
    char *scratch = (char*)malloc(SESSION_OUT + 64);
    if (!out || !scratch) {
        free(out);
        free(scratch);
        conn_reset(&c);
        return SERVE_FAIL;
    }

    c.in = (char*)malloc(RB_MIN);
    if (!c.in) {
        free(out);
        free(scratch);
        conn_reset(&c);
        return SERVE_FAIL;
    }
    c.in_cap = RB_MIN;
    mhx_session_init(&c.sess, 1);

    if (sock_set_nonblocking(s, 1) != 0) {
        free(out);
        free(scratch);
        free(c.in);
        conn_reset(&c);
        return SERVE_FAIL;
    }

    int rc = SERVE_OK;
    for (;;) {
        if (c.closing && c.out_len == c.out_sent) break;

        sock_event ev;
        ev.fd         = c.fd;
        ev.want_read  = !c.closing && (c.out_len - c.out_sent) < OUT_HIGH_WATER;
        ev.want_write = (c.out_len > c.out_sent);
        ev.can_read = ev.can_write = 0;

        int ready = sock_wait(&ev, 1, -1);
        if (ready < 0) { rc = SERVE_FAIL; break; }

        if (ev.can_read) {
            int too_large = 0;
            int rr = conn_read(&c, &too_large);
            if (too_large) {
                push_err(&c, "PROTO", "command too large");
                c.closing = 1;
                rc = SERVE_FAIL;
            } else if (rr < 0) {
                break;                 /* 对端关闭 */
            }
        }

        int leave = 0;
        for (;;) {
            int proc = conn_process(&c, token, out, scratch, SESSION_OUT + 64);
            if (proc == PROC_STOP) rc = SERVE_STOP;
            else if (proc == PROC_FAIL) rc = SERVE_FAIL;

            if (c.out_len > c.out_sent) {
                int fr = conn_flush(&c);
                if (fr < 0) { rc = SERVE_FAIL; leave = 1; break; }
                if (fr == 0) break;              /* 等可写 */
            }

            if (c.closing) { leave = 1; break; }
            if (proc == PROC_BACKPRESSURE) continue;
            break;
        }
        if (leave) break;
    }

    /* 收尾：把最后一条应答尽量送出去再关 */
    if (c.out_len > c.out_sent && c.fd != MHX_SOCK_INVALID) {
        int waited = 0;
        while (waited < 2000) {
            if (conn_flush(&c) != 0) break;
            sock_event ev;
            ev.fd = c.fd;
            ev.want_read = 0;
            ev.want_write = 1;
            ev.can_read = ev.can_write = 0;
            if (sock_wait(&ev, 1, 200) < 0) break;
            waited += 200;
        }
    }

    free(c.in);
    free(c.out);
    free(out);
    free(scratch);
    conn_reset(&c);      /* 连接由调用方关闭，这里不碰 fd */

    return rc;
}
