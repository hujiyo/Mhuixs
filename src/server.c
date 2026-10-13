/*
server.c —— 网络前端

读字节 -> proto 切出参数 -> 会话层执行 -> 回一个 JSON 文档。
就这三步，没有更多。

关于并发：v0.2.0 是单连接串行，所以这里没有任何锁。将来要支持多连接，
正确的做法是在 session.c 的 mhx_exec_lock() 处加一把全局锁并给每个连接
起一个线程 —— 那时本文件的结构不用动，因为所有命令执行都已经收敛
到会话层了。
*/

#include "server.h"
#include "session.h"
#include "proto.h"
#include "merr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 读缓冲的起始容量。按需翻倍，上限是 proto 定的单条命令上限。 */
#define RB_MIN 4096

/*
 * 单次会话的输出缓冲。JSON 是文本的超集、转义后还会膨胀，按 execute.h
 * 的建议给到 128KB。放在堆上：这条路径每个连接都会走，128KB 的栈帧不划算。
 */
#define SESSION_OUT 131072

static int send_raw(mhx_sock_t c, const char *buf, size_t n)
{
    if (n == 0) return 0;
    return sock_send_all(c, buf, n) == (long)n ? 0 : -1;
}

static int send_err(mhx_sock_t c, const char *tag, const char *msg)
{
    char buf[512];
    size_t n = proto_encode_err(buf, sizeof(buf), tag, msg);
    if (n == 0) return -1;
    return send_raw(c, buf, n);
}

/*
 * 发一段 JSON 文档。去掉 render_json / mhx_json_text 末尾那一个换行 ——
 * 载荷就是干净的 JSON 文档本身。
 */
static int send_json(mhx_sock_t c, char *scratch, size_t scratch_len, const char *json)
{
    size_t n = strlen(json);
    if (n > 0 && json[n - 1] == '\n') n--;

    size_t wn = proto_encode_bulk(scratch, scratch_len, json, n);
    if (wn == 0) return -1;   /* scratch 不够大，属于内部错误 */
    return send_raw(c, scratch, wn);
}

/* 发一条"只带一句话"的成功应答 */
static int send_note(mhx_sock_t c, char *scratch, size_t scratch_len, const char *text)
{
    char tmp[512];
    mhx_json_text(tmp, sizeof(tmp), 1, "ok", text);
    return send_json(c, scratch, scratch_len, tmp);
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
 * 那得靠 TLS，不在 v0.2.0 范围内。
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

int mhx_server_serve_one(mhx_sock_t conn, const char *token)
{
    mhx_session sess;
    mhx_session_init(&sess, 1);

    char *out    = (char*)malloc(SESSION_OUT);
    char *wbuf   = (char*)malloc(SESSION_OUT + 64);
    char *rb     = (char*)malloc(RB_MIN);
    if (!out || !wbuf || !rb) {
        free(out); free(wbuf); free(rb);
        return SERVE_FAIL;
    }

    size_t cap = RB_MIN;
    size_t start = 0;   /* 已消费；rb[start..end) 是还没处理的字节 */
    size_t end   = 0;
    int rc = SERVE_OK;

    for (;;) {
        /* 1) 把缓冲里能凑齐的命令全部处理掉 —— 流水线在这里天然成立 */
        while (end > start) {
            proto_cmd cmd;
            proto_result pr = proto_parse(rb + start, end - start, &cmd);

            if (pr == PROTO_INCOMPLETE) break;
            if (pr == PROTO_ERR) {
                send_err(conn, "PROTO", cmd.err ? cmd.err : "malformed request");
                rc = SERVE_FAIL;
                goto done;
            }

            start += cmd.consumed;

            if (!sess.authenticated) {
                if (!(cmd.argc == 2 && arg_eq_ci(&cmd.args[0], "AUTH"))) {
                    send_err(conn, "NOAUTH", "authenticate first");
                    rc = SERVE_FAIL;
                    goto done;
                }
                if (!token || !token[0] || !token_ok(&cmd.args[1], token)) {
                    send_err(conn, "NOAUTH", "invalid token");
                    rc = SERVE_FAIL;
                    goto done;
                }
                sess.authenticated = 1;

                char ok[8];
                size_t on = proto_encode_ok(ok, sizeof(ok));
                if (send_raw(conn, ok, on) != 0) { rc = SERVE_FAIL; goto done; }
                continue;
            }

            mhx_sess_status st = mhx_session_feed(&sess, cmd.argc, cmd.args, NULL,
                                                  out, SESSION_OUT);

            /* 这两条要先把应答发出去再收摊，否则客户端只看到连接被掐 */
            if (st == MHX_SESS_QUIT) {
                if (send_note(conn, wbuf, SESSION_OUT + 64, "再见") != 0)
                    rc = SERVE_FAIL;
                goto done;                       /* 只关这条连接，服务继续 */
            }
            if (st == MHX_SESS_SHUTDOWN) {
                send_note(conn, wbuf, SESSION_OUT + 64, "服务正在停机");
                rc = SERVE_STOP;
                goto done;
            }

            if (send_json(conn, wbuf, SESSION_OUT + 64, out) != 0) {
                rc = SERVE_FAIL;
                goto done;
            }
        }

        /* 2) 读更多。先一次性把已消费的头部挪掉（按读次数摊，不是按命令数） */
        if (start > 0) {
            if (end > start) memmove(rb, rb + start, end - start);
            end -= start;
            start = 0;
        }

        if (end == cap) {
            if (cap >= PROTO_MAX_CMD_BYTES) {
                send_err(conn, "PROTO", "command too large");
                rc = SERVE_FAIL;
                goto done;
            }
            size_t ncap = cap * 2;
            if (ncap > PROTO_MAX_CMD_BYTES) ncap = PROTO_MAX_CMD_BYTES;
            char *nb = (char*)realloc(rb, ncap);
            if (!nb) { rc = SERVE_FAIL; goto done; }
            rb = nb;
            cap = ncap;
        }

        long got = sock_recv(conn, rb + end, cap - end);
        if (got == SOCK_RETRY) continue;      /* 被信号打断，重来 */
        if (got <= 0) goto done;              /* 0 = 对端有序关闭；SOCK_ERR = 出错 */
        end += (size_t)got;
    }

done:
    free(rb);
    free(wbuf);
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

    fprintf(stderr, "  服务已监听 %s:%d（单连接串行）\n", ip, port);
    fprintf(stderr, "  第二个客户端会排队等待，直到当前连接结束\n");

    int rc = 0;

    for (;;) {
        mhx_sock_t conn = sock_accept(lfd);
        if (conn == MHX_SOCK_INVALID) {
            if (sock_last_was_interrupted()) continue;
            fprintf(stderr, "mhuixs: accept 失败（错误码 %d），停止监听\n", sock_last_error());
            rc = -1;
            break;
        }

        char peer[64];
        if (sock_peer_addr(conn, peer, sizeof(peer))) {
            char line[128];
            snprintf(line, sizeof(line), "connection from %s", peer);
            logger(line);
        }

        int st = mhx_server_serve_one(conn, token);
        sock_close(conn);

        if (st == SERVE_STOP) {
            fprintf(stderr, "  收到停机指令，停止监听\n");
            break;
        }
    }

    sock_close(lfd);
    return rc;
}
