#ifndef PROTO_H
#define PROTO_H

#include <stddef.h>
#include "execute.h"   /* MHX_MAX_ARGS / MHX_MAX_ARG_BYTES：界限的唯一出处 */

/*
proto.h —— MHP/1 线格式的编解码

**纯函数，不含任何 socket 依赖** —— 收发是 server.c 的事，这里只管
"字节 <-> 参数"。分开的直接好处是这一层能被普通单测干净地链接，
半包、粘包、二进制安全这些最容易出错的路径都可以离线测。

请求（客户端 -> 服务端）：

    *<n>\r\n                        n = 1..MHX_MAX_ARGS
      $<len>\r\n<原始字节>\r\n       自动判定（等价文本模式不加引号）
      +<文本>\r\n                   强制字符串（等价文本模式加引号）
      ...

响应（服务端 -> 客户端）：

    +OK\r\n                         仅认证成功
    -<PROTO|NOAUTH> <消息>\r\n       分帧/认证失败，发完即关连接
    $<len>\r\n<JSON 文档>\r\n        一切走到会话层的请求

为什么参数要带长度而不是拼成一行：值是任意字节，可以含空格、换行、
甚至 NUL。文本没有划界能力，长度前缀才有 —— 这就是二进制安全。
`+` 简单串用来承载文本模式里"引号"的语义（强制当字符串），
代价是它自身不能含 CR/LF。
*/

typedef enum {
    PROTO_OK = 0,      /* 解析出一条完整命令；cmd->consumed 是它占的字节数 */
    PROTO_INCOMPLETE,  /* 数据还不够；什么都不消费，读更多再来 */
    PROTO_ERR          /* 协议错；cmd->err 里有原因，连接应当关闭 */
} proto_result;

/* 一个参数。p 指向调用方缓冲区内部，本层不拥有内存。 */
typedef struct {
    const char *p;
    size_t      len;
    int         quoted;
} proto_arg;

typedef struct {
    proto_arg   args[MHX_MAX_ARGS];
    int         argc;
    size_t      consumed;   /* PROTO_OK 时有效：这条命令占了几个字节 */
    const char *err;        /* PROTO_ERR 时有效：一句静态说明 */
} proto_cmd;

/*
 * 从 buf[0..len) 头部尝试解析一条命令。
 *
 * 解析是无状态的：数据不够就返回 PROTO_INCOMPLETE、一个字节都不消费，
 * 调用方读更多字节后**从头再解析一次**即可。因为有字节上限，重扫的
 * 代价有界，不值得引入可恢复的状态机 —— 那正是半包/粘包最容易写错的地方。
 *
 * 返回 PROTO_OK 后，调用方消费掉 consumed 个字节，可以对剩余部分再次调用，
 * 这就是流水线（一次 recv 里含多条命令）。
 */
proto_result proto_parse(const char *buf, size_t len, proto_cmd *cmd);

/* 单条命令的字节上限。超过还不完整就报错关连接，避免缓冲无限增长。 */
#define PROTO_MAX_CMD_BYTES ((size_t)MHX_MAX_ARGS * (MHX_MAX_ARG_BYTES + 32) + 64)

/* ---------------- 编码：服务端回包 ---------------- */

/* "$<len>\r\n<payload>\r\n"，含结尾 NUL 需要多少字节 */
size_t proto_bulk_size(size_t payload_len);

/*
 * 写 "$<len>\r\n<payload>\r\n"。
 * payload 是 JSON 文档，长度显式给出（里面可能有 \n，不能用 strlen 之外的假设）。
 * 缓冲区不够返回 0，且不写入。
 */
size_t proto_encode_bulk(char *dst, size_t dstlen, const char *payload, size_t payload_len);

/* 写 "+OK\r\n"，返回写入字节数（不含结尾 NUL）。缓冲区不够返回 0。 */
size_t proto_encode_ok(char *dst, size_t dstlen);

/* 写 "-<tag> <msg>\r\n"，返回写入字节数。msg 里的 CR/LF 会被换成空格 ——
 * 否则一条错误消息就能把分帧搞乱。缓冲区不够返回 0。 */
size_t proto_encode_err(char *dst, size_t dstlen, const char *tag, const char *msg);

#endif /* PROTO_H */
