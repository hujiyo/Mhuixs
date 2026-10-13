/*
session.c —— 会话层实现

元命令与数据命令的唯一入口。三个前端（交互 / 脚本 / 网络）都从这里过。
*/

#include "session.h"
#include "registry.h"
#include "env.h"
#include "merr.h"
#include "mstring.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 切词后存放参数文本的缓冲：参数个数 × 单参数上限 */
#define LINE_STORAGE ((size_t)MHX_MAX_ARGS * (MHX_MAX_ARG_BYTES + 1))

/*
 * 单连接串行的唯一屏障。
 *
 * v0.2.0 只有一路会话在跑命令，所以这里是空的。将来支持多连接时，
 * 只把这两个函数换成一把全局互斥锁即可 —— 所有命令执行都已经收敛到
 * run_command()，不必去碰任何命令实现。
 *
 * 换成真锁时必须遵守：
 *   - 锁序 exec -> 注册表锁，绝不反向（registry.c 的取锁路径已经固定）
 *   - 持锁期间严禁调用 HOOK_logout：它内部会再取注册表锁，
 *     POSIX 的普通互斥锁不可重入，会自死锁
 */
static void mhx_exec_lock(void)   { }
static void mhx_exec_unlock(void) { }

void mhx_session_init(mhx_session *s, int is_server)
{
    if (!s) return;
    s->json_mode     = is_server ? 1 : 0;
    s->authenticated = is_server ? 0 : 1;   /* 本地前端没有认证这回事 */
    s->is_server     = is_server;
}

/* ---------------------------------------------------------------- */
/* 注册表路径与落盘（原先在 Mhuixs.c 里，网络层也要用，所以挪进来）      */
/* ---------------------------------------------------------------- */

char *mhx_registry_path(void)
{
    mstring home = Env.MhuixsHomePath;
    mstring sep  = mstr(
#ifdef _WIN32
        (char*)"\\"
#else
        (char*)"/"
#endif
    );
    mstring fname = mstr((char*)"registry.dat");
    mstring tmp   = mstr_concat(home, sep);
    mstring path  = mstr_concat(tmp, fname);
    char   *cstr  = mstr_to_cstr(path);

    mstr_free(sep);
    mstr_free(fname);
    mstr_free(tmp);
    mstr_free(path);

    return cstr;
}

int mhx_save_registry(void)
{
    char *path = mhx_registry_path();
    int rc = reg_save_to_disk(path);
    free(path);
    if (rc != 0) return -1;
    return reg_get_hook_count();
}

/* ---------------------------------------------------------------- */
/* 元命令                                                            */
/* ---------------------------------------------------------------- */

/* 参数与字面量整体比较（参数不带结尾 NUL，不能直接用 strcmp） */
static int arg_eq(const struct mhx_arg *a, const char *lit)
{
    size_t n = strlen(lit);
    if (a->len != n) return 0;
    return n == 0 || memcmp(a->p, lit, n) == 0;
}

/*
 * 把结果写进 out。两种会话的 out 含义不同：
 *   本地会话 —— 给前端自己排版用的原文本（前端各有各的包装）
 *   网络会话 —— 恒为一个 JSON 文档（协议只认这一种响应体）
 * 用一条规则管住差别，前端就不必各自判断了。
 */
static void put_text(mhx_session *s, char *out, size_t outlen,
                     int ok, const char *code, const char *text)
{
    if (s->is_server) mhx_json_text(out, outlen, ok, code, text);
    else              snprintf(out, outlen, "%s", text);
}

static mhx_sess_status feed_meta(mhx_session *s, int argc,
                                 const struct mhx_arg *args, const char *raw,
                                 char *out, size_t outlen)
{
    const struct mhx_arg *m = &args[0];

    if (arg_eq(m, ":quit") || arg_eq(m, ":q") || arg_eq(m, ":exit"))
        return MHX_SESS_QUIT;

    if (arg_eq(m, ":help") || arg_eq(m, ":?")) {
        put_text(s, out, outlen, 1, "ok", mhx_help_text());
        return MHX_SESS_HELP;
    }

    if (arg_eq(m, ":save")) {
        int k = mhx_save_registry();
        if (k < 0) {
            put_text(s, out, outlen, 0, "failed", "保存失败");
            return MHX_SESS_SAVE_FAIL;
        }
        char msg[64];
        snprintf(msg, sizeof(msg), "已保存 %d 个 HOOK", k);
        put_text(s, out, outlen, 1, "ok", msg);
        return MHX_SESS_SAVE_OK;
    }

    if (arg_eq(m, ":format")) {
        if (argc > 1 && arg_eq(&args[1], "json")) {
            s->json_mode = 1;
        } else if (argc > 1 && arg_eq(&args[1], "text")) {
            s->json_mode = 0;
        } else {
            put_text(s, out, outlen, 0, "bad_usage", "用法：:format text|json");
            return MHX_SESS_FORMAT_BAD;
        }

        if (s->is_server) {
            /* 线格式固定是 JSON，客户端改不了 —— 但也不报错，
             * 这样同一份脚本既能本地跑也能过网络跑。 */
            put_text(s, out, outlen, 1, "ok",
                     "网络会话固定 JSON 输出，本设置只影响本地呈现");
        } else {
            put_text(s, out, outlen, 1, "ok", s->json_mode ? "json" : "text");
        }
        return MHX_SESS_FORMAT_OK;
    }

    /* :shutdown 只对网络会话有意义：本地跑完就退出了，用不着停机指令 */
    if (s->is_server && arg_eq(m, ":shutdown"))
        return MHX_SESS_SHUTDOWN;

    put_text(s, out, outlen, 0, "unknown_command", raw ? raw : "");
    return MHX_SESS_UNKNOWN_META;
}

/* ---------------------------------------------------------------- */
/* 入口                                                              */
/* ---------------------------------------------------------------- */

static mhx_sess_status run_command(mhx_session *s, int argc,
                                   const struct mhx_arg *args,
                                   char *out, size_t outlen)
{
    mhx_exec_lock();

    /* 网络会话恒用 JSON，不看 json_mode ——
     * 否则客户端发一句 ":format text" 就能把线格式改坏。 */
    int json = s->is_server ? 1 : s->json_mode;
    int rc = mhx_execute_argv(argc, args, out, outlen, json);

    mhx_exec_unlock();

    return rc == 0 ? MHX_SESS_OK : MHX_SESS_ERR;
}

mhx_sess_status mhx_session_feed(mhx_session *s, int argc,
                                 const struct mhx_arg *args, const char *meta_raw,
                                 char *out, size_t outlen)
{
    if (!s || !out || outlen < 2) return MHX_SESS_OK;
    out[0] = '\0';

    if (argc <= 0) return MHX_SESS_OK;   /* 空行 */

    if (args[0].len > 0 && args[0].p[0] == ':')
        return feed_meta(s, argc, args, meta_raw, out, outlen);

    return run_command(s, argc, args, out, outlen);
}

mhx_sess_status mhx_session_feed_line(mhx_session *s, const char *line,
                                      char *out, size_t outlen)
{
    if (!s || !line || !out || outlen < 2) return MHX_SESS_OK;
    out[0] = '\0';

    /* 元命令的判定看**行首字符**，与历史行为一致：前导空格会让它落到
     * 命令层（回 unknown command）。这是有意保留的，不是漏了 trim。
     * 网络前端拿到的是已切好的参数、没有前导空白，所以那边按第一个参数判。 */
    const int is_meta_line = (line[0] == ':');

    char storage[LINE_STORAGE];
    struct mhx_arg args[MHX_MAX_ARGS];
    int argc = mhx_split_line(line, storage, sizeof(storage), args, MHX_MAX_ARGS);

    if (argc <= 0) return MHX_SESS_OK;

    if (is_meta_line) return feed_meta(s, argc, args, line, out, outlen);

    /* 这里不能转手给 mhx_session_feed：它会按 args[0] 再判一次元命令，
     * 于是 "  :quit" 又会被当成 :quit —— 判定口径就白分了。 */
    return run_command(s, argc, args, out, outlen);
}
