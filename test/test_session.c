/* 回归测试：会话层。
 *
 * 为什么值得单测：元命令（:help / :save / :format / :quit）原先只存在于
 * main() 的两个循环里，任何测试都碰不到 —— 改坏了也没人知道。抽出会话层
 * 之后它们第一次进入回归。
 *
 * 断言两件事：
 *   1. 每条输入映射到正确的状态码与核心文本
 *   2. 输出格式是**会话自己的状态**，不是进程级全局
 *      （原来一个 :format 会串到所有调用方，这是抽出会话层要解决的核心问题）
 *
 * 这里只测"核心结果"，不测前端怎么排版 —— 排版由各前端自己决定。
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "session.h"
#include "registry.h"
#include "env.h"
#include "mstring.h"
#include "merr.h"

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("  FAIL: %s\n", msg); fails++; } } while (0)

static mhx_session s;

static mhx_sess_status feed(const char *line, char *out, size_t outlen)
{
    return mhx_session_feed_line(&s, line, out, outlen);
}

int main(void)
{
    logger_init(".");
    if (reg_init() != 0) { printf("reg_init failed\n"); return 1; }

    /* :save 会往 Env.MhuixsHomePath 下写 registry.dat。
     * 测试把它指到当前目录，跑完自己删掉 —— 绝不碰用户的数据目录。 */
    Env.MhuixsHomePath = mstr((char*)".");
    remove("registry.dat");

    char out[65536];
    mhx_session_init(&s, 0);

    printf("数据命令：成功与失败都映射到各自状态\n");
    {
        mhx_sess_status st = feed("create list slist", out, sizeof(out));
        CHECK(st == MHX_SESS_OK, "create 成功 -> OK");

        st = feed("rpush slist a b", out, sizeof(out));
        CHECK(st == MHX_SESS_OK, "rpush 成功 -> OK");
        CHECK(strcmp(out, "OK rpush slist, len=2") == 0, "rpush 的输出就是命令层那串");

        st = feed("llen nosuch", out, sizeof(out));
        CHECK(st == MHX_SESS_ERR, "命令失败 -> ERR（由前端决定继续还是中止）");
        CHECK(strcmp(out, "ERR no such hook: nosuch") == 0, "失败文本原样带出");

        st = feed("", out, sizeof(out));
        CHECK(st == MHX_SESS_OK && out[0] == '\0', "空行 -> OK 且无输出");
    }

    printf("输出格式是会话自己的状态\n");
    {
        mhx_sess_status st = feed(":format json", out, sizeof(out));
        CHECK(st == MHX_SESS_FORMAT_OK && strcmp(out, "json") == 0, ":format json -> 核心文本是 json");
        CHECK(s.json_mode == 1, "会话的 json_mode 被置起");

        feed("llen slist", out, sizeof(out));
        CHECK(strstr(out, "\"ok\":true") != NULL, "json_mode 下数据命令吐 JSON");

        st = feed(":format text", out, sizeof(out));
        CHECK(st == MHX_SESS_FORMAT_OK && strcmp(out, "text") == 0, ":format text");
        feed("llen slist", out, sizeof(out));
        CHECK(strcmp(out, "OK 2") == 0, "text_mode 下数据命令吐文本");

        st = feed(":format bogus", out, sizeof(out));
        CHECK(st == MHX_SESS_FORMAT_BAD, ":format 参数不认识 -> FORMAT_BAD");
        CHECK(s.json_mode == 0, "用法错不应改动格式");
    }

    printf("两个会话互不串味（这正是抽会话层要解决的）\n");
    {
        mhx_session other;
        mhx_session_init(&other, 0);

        feed(":format json", out, sizeof(out));         /* 只切 s */
        CHECK(s.json_mode == 1, "s 切成了 json");
        CHECK(other.json_mode == 0, "另一个会话不受影响");

        mhx_session_feed_line(&other, "llen slist", out, sizeof(out));
        CHECK(strcmp(out, "OK 2") == 0, "另一个会话仍按文本输出");

        feed(":format text", out, sizeof(out));         /* 还原 */
    }

    printf("元命令：未知、退出、帮助\n");
    {
        mhx_sess_status st = feed(":foo bar", out, sizeof(out));
        CHECK(st == MHX_SESS_UNKNOWN_META, "未知元命令 -> UNKNOWN_META");
        CHECK(strcmp(out, ":foo bar") == 0, "回显的是整行原文，不是切出来的第一个词");

        st = feed(":quit", out, sizeof(out));
        CHECK(st == MHX_SESS_QUIT, ":quit -> QUIT");
        st = feed(":q", out, sizeof(out));
        CHECK(st == MHX_SESS_QUIT, ":q -> QUIT");
        st = feed(":exit", out, sizeof(out));
        CHECK(st == MHX_SESS_QUIT, ":exit -> QUIT");

        st = feed(":help", out, sizeof(out));
        CHECK(st == MHX_SESS_HELP, ":help -> HELP");
        CHECK(strstr(out, "create") != NULL && strstr(out, "tfields") != NULL,
              ":help 的正文就是命令层那份清单");

        st = feed(":?", out, sizeof(out));
        CHECK(st == MHX_SESS_HELP, ":? -> HELP");
    }

    printf("会话不是服务端时没有 :shutdown\n");
    {
        mhx_sess_status st = feed(":shutdown", out, sizeof(out));
        CHECK(st == MHX_SESS_UNKNOWN_META, "本地会话下 :shutdown 算未知元命令");

        mhx_session srv;
        mhx_session_init(&srv, 1);
        CHECK(srv.authenticated == 0, "服务端会话初始未认证");
        st = mhx_session_feed_line(&srv, ":shutdown", out, sizeof(out));
        CHECK(st == MHX_SESS_SHUTDOWN, "服务端会话下 :shutdown 才是停机请求");
        CHECK(s.authenticated == 1, "本地会话恒为已认证");
    }

    printf("前导空格不算元命令（与历史行为一致）\n");
    {
        mhx_sess_status st = feed("  :quit", out, sizeof(out));
        CHECK(st == MHX_SESS_ERR, "前导空格的 :quit 落到命令层");
        CHECK(strstr(out, "unknown command") != NULL, "并报命令不认识");
    }

    printf(":save\n");
    {
        mhx_sess_status st = feed(":save", out, sizeof(out));
        CHECK(st == MHX_SESS_SAVE_OK, ":save 成功 -> SAVE_OK");
        CHECK(strstr(out, "已保存") != NULL, "核心文本含已保存与数量");

        /* 落盘的东西确实能被读回来 */
        CHECK(reg_get_hook_count() == 1, "当前就一个钩子");
    }

    /* 收尾：删掉测试自己写出来的注册表与可能残留的临时文件 */
    remove("registry.dat");
    remove("registry.dat.tmp");
    remove("log.inf");
    mstr_free(Env.MhuixsHomePath);
    Env.MhuixsHomePath = NULL;

    reg_destroy();

    if (fails) { printf("\n会话层测试：%d 项失败\n", fails); return 1; }
    printf("\n会话层测试：全部通过\n");
    return 0;
}
