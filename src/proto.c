/*
proto.c —— MHP/1 编解码

全部是纯函数：输入一段字节，输出若干参数。没有 socket、没有全局状态。
*/

#include "proto.h"

#include <stdio.h>
#include <string.h>

/* 长度字段的溢出保护线。真正的上限由调用方按各自的语义检查
 * （参数个数看 MHX_MAX_ARGS，参数长度看 MHX_MAX_ARG_BYTES），
 * 这里只负责不让 v*10+d 悄悄绕回去。 */
#define PROTO_LEN_NOISE 100000000UL

/*
 * 读一个十进制长度。
 * 返回 1 成功、0 数据还没到齐、-1 不是合法数字或溢出。
 * 手写而不是用 atoi：atoi 遇错返回 0，和"长度真的是 0"分不开，
 * 而且没有溢出检查。
 */
static int read_decimal(const char *b, size_t n, size_t *pos, size_t *out)
{
    size_t i = *pos;

    if (i >= n) return 0;                                  /* 一个字节都还没有 */
    if (b[i] < '0' || b[i] > '9') return -1;               /* 该是数字却不是 */

    size_t v = 0;
    while (i < n && b[i] >= '0' && b[i] <= '9') {
        v = v * 10 + (size_t)(b[i] - '0');
        if (v > PROTO_LEN_NOISE) return -1;                /* 溢出/离谱 */
        i++;
    }

    *pos = i;
    *out = v;
    return 1;
}

/* 期待 buf[*pos] 起是 CRLF。返回 1 是、0 还没到齐、-1 不是。 */
static int expect_crlf(const char *b, size_t n, size_t i)
{
    if (i + 1 >= n) return 0;
    if (b[i] != '\r' || b[i + 1] != '\n') return -1;
    return 1;
}

proto_result proto_parse(const char *buf, size_t len, proto_cmd *cmd)
{
    cmd->argc     = 0;
    cmd->consumed = 0;
    cmd->err      = NULL;

    if (!buf || !cmd) { cmd->err = "internal: null buffer"; return PROTO_ERR; }
    if (len == 0) return PROTO_INCOMPLETE;

    if (buf[0] != '*') { cmd->err = "expected '*'"; return PROTO_ERR; }

    size_t i = 1;
    size_t nargs = 0;

    int r = read_decimal(buf, len, &i, &nargs);
    if (r == 0) return PROTO_INCOMPLETE;
    if (r < 0) { cmd->err = "malformed array length"; return PROTO_ERR; }
    if (nargs < 1 || nargs > MHX_MAX_ARGS) { cmd->err = "bad array length"; return PROTO_ERR; }

    r = expect_crlf(buf, len, i);
    if (r == 0) return PROTO_INCOMPLETE;
    if (r < 0) { cmd->err = "expected CRLF after array length"; return PROTO_ERR; }
    i += 2;

    for (size_t a = 0; a < nargs; a++) {
        if (i >= len) return PROTO_INCOMPLETE;   /* 连元素类型都还没到 */

        const char kind = buf[i];

        if (kind == '$') {
            i++;
            size_t alen = 0;
            int rr = read_decimal(buf, len, &i, &alen);
            if (rr == 0) return PROTO_INCOMPLETE;
            if (rr < 0) { cmd->err = "malformed argument length"; return PROTO_ERR; }
            if (alen > MHX_MAX_ARG_BYTES) { cmd->err = "argument too long"; return PROTO_ERR; }

            rr = expect_crlf(buf, len, i);
            if (rr == 0) return PROTO_INCOMPLETE;
            if (rr < 0) { cmd->err = "expected CRLF after argument length"; return PROTO_ERR; }
            i += 2;

            /* 数据体加尾部 CRLF 都要到齐才算完整 */
            if (i + alen + 2 > len) return PROTO_INCOMPLETE;

            cmd->args[a].p      = buf + i;
            cmd->args[a].len    = alen;
            cmd->args[a].quoted = 0;
            i += alen;

            if (buf[i] != '\r' || buf[i + 1] != '\n') {
                cmd->err = "missing CRLF after argument body";
                return PROTO_ERR;
            }
            i += 2;

        } else if (kind == '+') {
            i++;
            /* 简单串：到 CRLF 为止。它不能含 CR/LF，所以不需要长度 ——
             * 这正是它只配用来送"命令名、钩子名"这类短词的原因。 */
            size_t start = i;
            while (i + 1 < len && !(buf[i] == '\r' && buf[i + 1] == '\n')) i++;
            if (i + 1 >= len) return PROTO_INCOMPLETE;

            size_t alen = i - start;
            if (alen > MHX_MAX_ARG_BYTES) { cmd->err = "argument too long"; return PROTO_ERR; }

            cmd->args[a].p      = buf + start;
            cmd->args[a].len    = alen;
            cmd->args[a].quoted = 1;          /* 承载文本模式里的"引号" */
            i += 2;

        } else {
            /* 包括 '*'：不支持嵌套数组 */
            cmd->err = "unsupported element type";
            return PROTO_ERR;
        }
    }

    cmd->argc     = (int)nargs;
    cmd->consumed = i;
    return PROTO_OK;
}

/* ---------------- 编码 ---------------- */

size_t proto_bulk_size(size_t payload_len)
{
    char num[24];
    int nd = snprintf(num, sizeof(num), "%zu", payload_len);
    if (nd < 0) return 0;
    return (size_t)nd + 5 + payload_len;   /* '$' + 数字 + CRLF + 数据 + CRLF */
}

size_t proto_encode_bulk(char *dst, size_t dstlen, const char *payload, size_t payload_len)
{
    if (!dst || !payload) return 0;

    char num[24];
    int nd = snprintf(num, sizeof(num), "%zu", payload_len);
    if (nd < 0) return 0;

    size_t need = (size_t)nd + 5 + payload_len;   /* 不含结尾 NUL */
    if (need + 1 > dstlen) return 0;              /* 装不下就整条不写 */

    size_t o = 0;
    dst[o++] = '$';
    memcpy(dst + o, num, (size_t)nd); o += (size_t)nd;
    dst[o++] = '\r';
    dst[o++] = '\n';
    memcpy(dst + o, payload, payload_len); o += payload_len;
    dst[o++] = '\r';
    dst[o++] = '\n';
    dst[o]   = '\0';
    return o;
}

size_t proto_encode_ok(char *dst, size_t dstlen)
{
    if (!dst || dstlen < 6) return 0;
    memcpy(dst, "+OK\r\n", 5);
    dst[5] = '\0';
    return 5;
}

size_t proto_encode_err(char *dst, size_t dstlen, const char *tag, const char *msg)
{
    if (!dst || dstlen < 4) return 0;

    size_t done = 0;
    dst[done++] = '-';

    if (tag) {
        while (*tag && done + 3 < dstlen) dst[done++] = *tag++;
    }
    if (done + 3 < dstlen) dst[done++] = ' ';

    if (msg) {
        while (*msg && done + 3 < dstlen) {
            char c = *msg++;
            /* 一条错误消息绝不能把分帧搞乱 */
            if (c == '\r' || c == '\n') c = ' ';
            dst[done++] = c;
        }
    }

    if (done + 3 > dstlen) return 0;   /* 连收尾都放不下 */
    dst[done++] = '\r';
    dst[done++] = '\n';
    dst[done]   = '\0';
    return done;
}
