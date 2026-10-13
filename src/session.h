#ifndef SESSION_H
#define SESSION_H

#include <stddef.h>
#include "execute.h"   /* struct mhx_arg */

/*
session.h —— 会话层

一行输入进去，一段结果出来。交互模式、脚本模式、网络会话三个前端共用这一层。

为什么要有它：元命令（:help / :save / :format / :quit）原先在 repl() 和
run_script() 里各写了一遍，再加一路网络前端就是第三遍。更要紧的是，
输出格式原先是一个**进程级全局开关**，一个连接切换会串到所有连接 ——
现在它是会话对象的一部分，每个会话各有一份。

分工：本层负责"这条输入是什么、该改什么状态、核心结果是什么"，
具体怎么呈现（写到哪个流、加什么前缀、失败要不要中止）留给前端。
三个前端的排版要求本来就不同，硬合并会变成一堆 if。

返回码只表示"这条输入的类别"，不表示数据命令本身成功与否 ——
数据命令失败返回 MHX_SESS_ERR，前端决定继续还是中止。
*/

typedef enum {
    MHX_SESS_OK = 0,        /* 正常结束；out 是输出（可能为空） */
    MHX_SESS_ERR,           /* 数据命令执行失败；out 是给用户看的错误文本 */
    MHX_SESS_QUIT,          /* :quit —— 结束本次会话 */
    MHX_SESS_HELP,          /* out = 帮助全文，前端原样输出（末尾已有换行） */
    MHX_SESS_SAVE_OK,       /* out = 形如 "已保存 3 个 HOOK" */
    MHX_SESS_SAVE_FAIL,     /* 落盘失败 */
    MHX_SESS_FORMAT_OK,     /* out = 新格式名（"text" / "json"） */
    MHX_SESS_FORMAT_BAD,    /* :format 的参数不认识 */
    MHX_SESS_UNKNOWN_META,  /* out = 那条元命令原文 */
    MHX_SESS_SHUTDOWN       /* 请求停机；只有 is_server 会话会产生 */
} mhx_sess_status;

typedef struct {
    int json_mode;       /* 输出格式。每会话一份 —— 不再是进程级全局 */
    int authenticated;   /* 网络会话用；本地前端恒为 1 */
    int is_server;       /* 是否网络会话，决定 :shutdown 是否可用 */
} mhx_session;

void mhx_session_init(mhx_session *s, int is_server);

/*
 * 文本前端入口：一行文本进出（行尾的 \r\n 由前端先剥掉）。
 * 内部先切词，再判元命令，再交给命令层。
 */
mhx_sess_status mhx_session_feed_line(mhx_session *s, const char *line,
                                      char *out, size_t outlen);

/*
 * 参数已切好的入口（网络协议层用）。
 * meta_raw 用于"未知元命令"时回显：文本前端给整行原文，网络前端给第一个参数。
 * 传 NULL 时回显第一个参数。
 */
mhx_sess_status mhx_session_feed(mhx_session *s, int argc,
                                 const struct mhx_arg *args,
                                 const char *meta_raw,
                                 char *out, size_t outlen);

/* <MhuixsHomePath>/registry.dat 的完整路径，调用方负责 free */
char *mhx_registry_path(void);

/* 注册表落盘；成功返回保存的 HOOK 数，失败返回 -1 */
int mhx_save_registry(void);

#endif /* SESSION_H */
