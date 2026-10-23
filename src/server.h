#ifndef SERVER_H
#define SERVER_H

#include "sock.h"

/*
server.h —— 网络前端

只管"接受连接、读字节、写字节"。命令语义全在会话层，线格式全在 proto，
这一层不掺和它们的事。

并发：**单线程事件循环**。所有连接一起等，谁就绪处理谁；命令执行仍然是
一条一条来的（和 Redis 一样），所以没有锁，也不需要锁。空闲连接、收得慢
的客户端都只影响它自己 —— 未发出的应答留在自己的输出缓冲里，下次可写再送。
同时在线上限见 server.c 的 MHX_MAX_CONNS，超出直接断开。

握手：连接建立后第一条请求必须是 AUTH <token>，不对就断开；未认证前
发别的请求同样断开。令牌为空则**拒绝启动**：宁可起不来，也别裸奔在网上。
*/

/* mhx_server_serve_one 的返回值 */
#define SERVE_OK    0    /* 对端正常断开 */
#define SERVE_STOP  1    /* 收到 :shutdown，调用方应当停止监听 */
#define SERVE_FAIL (-1)  /* 协议错 / 读写错 */

/* 监听并循环服务连接，直到收到 :shutdown 或监听不可用。返回 0 正常停机。 */
int mhx_server_run(const char *bind_addr, int port, const char *token);

/*
 * 在一条已经 listen 好的套接字上跑事件循环，直到 :shutdown 或出错。
 * 返回 0 表示正常停机。**不关闭 lfd**，由调用方负责。
 * 单独暴露是为了让测试能先用端口 0 监听、问出实际端口再启动服务。
 */
int mhx_server_run_listener(mhx_sock_t lfd, const char *token);

/*
 * 只服务一条已经建立好的连接（单连接循环，内部同样走事件机制）。
 * 单连接场景和集成测试用；它不会碰其他连接，也不会关闭调用方给的套接字。
 */
int mhx_server_serve_one(mhx_sock_t conn, const char *token);

#endif /* SERVER_H */
