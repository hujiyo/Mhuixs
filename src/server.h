#ifndef SERVER_H
#define SERVER_H

#include "sock.h"

/*
server.h —— 网络前端

只管"接受连接、读字节、写字节"。命令语义全在会话层，线格式全在 proto，
这一层不掺和它们的事。

v0.2.0 是**单连接串行**：一次只服务一个连接，服务完再接受下一个。
第二个客户端不会收到拒绝，而是排在 listen backlog 里等着 —— 这是有意的，
省掉一条并发路径；README 里要写明，否则会被当成"服务卡住了"。

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
 * 只服务一条已经建立好的连接。
 * 单独暴露是为了让集成测试不必去管 accept 循环。
 */
int mhx_server_serve_one(mhx_sock_t conn, const char *token);

#endif /* SERVER_H */
