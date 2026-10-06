/* 回归测试：命令层的机器可读输出（JSON）。
 *
 * 为什么值得单测：这一层是给程序 / AI 消费的契约，重构时最容易悄悄改坏。
 * 两条不变量：
 *   1. 文本模式（mhx_execute）的输出与历史完全一致 —— 老调用方靠它
 *   2. JSON 的 ok / code / kind / value 稳定 —— 新调用方靠它
 * 所以这里断言的是【结构化字段】，不是文案。文案改了不该挂测试，
 * 字段没了、错了必须挂。
 */
#include <stdio.h>
#include <string.h>

#include "execute.h"
#include "registry.h"

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("  FAIL: %s\n", msg); fails++; } } while (0)

/* 只断言子串，不断言整个文档 —— 字段顺序不是契约的一部分 */
static int has(const char *s, const char *needle)
{
    return strstr(s, needle) != NULL;
}

/* 跑一条命令，要求返回码与 JSON 内容都对 */
static void jexpect(const char *cmd, int want_rc, const char *needle, const char *desc)
{
    char out[8192];
    int rc = mhx_execute_json(cmd, out, sizeof(out));

    if (rc != want_rc) {
        printf("  FAIL: [%s] rc=%d 期望 %d (%s)\n", cmd, rc, want_rc, desc);
        fails++;
        return;
    }
    if (!has(out, needle)) {
        printf("  FAIL: [%s] %s\n        期望含: %s\n        实际:   %s\n",
               cmd, desc, needle, out);
        fails++;
    }
}

/* 文本模式的行为必须逐字不变 */
static void texpect(const char *cmd, int want_rc, const char *want_text, const char *desc)
{
    char out[8192];
    int rc = mhx_execute(cmd, out, sizeof(out));

    if (rc != want_rc || strcmp(out, want_text) != 0) {
        printf("  FAIL: [%s] %s\n        期望 rc=%d \"%s\"\n        实际 rc=%d \"%s\"\n",
               cmd, desc, want_rc, want_text, rc, out);
        fails++;
    }
}

int main(void)
{
    logger_init(".");
    if (reg_init() != 0) { printf("reg_init failed\n"); return 1; }

    {
        char out[8192];
        mhx_execute("create list fruits", out, sizeof(out));
        mhx_execute("rpush fruits apple 42 banana", out, sizeof(out));

        mhx_execute("create bitmap flags", out, sizeof(out));
        mhx_execute("bset flags 3 1", out, sizeof(out));

        mhx_execute("create kvalot cache", out, sizeof(out));
        mhx_execute("kset cache k v", out, sizeof(out));

        mhx_execute("create table users id:int name:str", out, sizeof(out));
        mhx_execute("tadd users 1 alice", out, sizeof(out));
        mhx_execute("tadd users 2", out, sizeof(out));   /* 只给一列 -> name 为空单元格 */
        mhx_execute("create list emptylist", out, sizeof(out));
    }

    printf("文本模式：输出与历史逐字节一致\n");
    texpect("llen fruits",        0, "OK 3",                        "int 型成功");
    texpect("lget fruits 0",      0, "OK \"apple\"",                "字符串带引号");
    texpect("lget fruits 1",      0, "OK 42",                       "数字");
    texpect("llen nosuch",       -1, "ERR no such hook: nosuch",    "钩子不存在");
    texpect("llen flags",        -1, "ERR hook 'flags' is bitmap, not list", "类型不符");
    texpect("lget fruits 99",    -1, "ERR index 99 out of range",   "越界");
    texpect("llen",              -1, "ERR usage: llen <name>",      "参数不足");
    texpect("nonsense",          -1, "ERR unknown command: nonsense", "命令不认识");
    texpect("",                   0, "",                            "空行无输出");

    printf("JSON：成功时的结构化返回值\n");
    jexpect("llen fruits",  0, "\"ok\":true,\"code\":\"ok\",\"kind\":\"int\",\"value\":3", "int 计数");
    jexpect("lget fruits 0",0, "\"kind\":\"str\",\"value\":\"apple\"", "str 取值不带引号");
    jexpect("lget fruits 1",0, "\"kind\":\"num\",\"value\":42",        "任意精度数字按字面量输出");
    jexpect("kexists cache k", 0, "\"kind\":\"bool\",\"value\":true",  "键存在 -> true");
    jexpect("kexists cache nope", 0, "\"kind\":\"bool\",\"value\":false", "键不存在仍是成功，值为 false");
    jexpect("bget flags 3",  0, "\"kind\":\"bool\",\"value\":true",    "某一位");
    jexpect("bsize flags",   0, "\"kind\":\"int\",\"value\":4",        "位图位数");
    jexpect("klen cache",    0, "\"kind\":\"int\",\"value\":1",        "键数量");
    jexpect("trows users",   0, "\"kind\":\"int\",\"value\":2",        "行数");
    jexpect("type users",    0, "\"kind\":\"str\",\"value\":\"table\"", "类型名");
    jexpect("tfields users", 0, "\"kind\":\"list\",\"value\":[\"id\",\"name\"]", "字段列表");
    jexpect("tget users 0 name", 0, "\"kind\":\"str\",\"value\":\"alice\"", "单元格取值");
    jexpect("tget users 1 name", 0, "\"kind\":\"null\"",                "空单元格 -> null");
    jexpect("create list x0", 0, "\"ok\":true,\"code\":\"ok\"",         "写命令只有 code+text");

    printf("JSON：text 字段就是文本模式的输出（含转义）\n");
    jexpect("llen fruits",   0, "\"text\":\"OK 3\"",                    "text 与文本模式同串");
    jexpect("lget fruits 0", 0, "\"text\":\"OK \\\"apple\\\"\"",        "text 里的引号已转义");
    jexpect("lget fruits 99",-1, "\"text\":\"ERR index 99 out of range\"", "失败时也有 text");

    printf("JSON：失败码分类\n");
    jexpect("llen nosuch",      -1, "\"ok\":false,\"code\":\"no_hook\"",        "钩子不存在");
    jexpect("llen flags",       -1, "\"code\":\"type_mismatch\"",               "类型不符");
    jexpect("lget fruits 99",   -1, "\"code\":\"out_of_range\"",                "下标越界");
    jexpect("llen",             -1, "\"code\":\"bad_usage\"",                   "参数不足");
    jexpect("nonsense",         -1, "\"code\":\"unknown_command\"",             "命令不认识");
    jexpect("kget cache nope",  -1, "\"code\":\"no_key\"",                      "键不存在");
    jexpect("tget users 0 nope",-1, "\"code\":\"no_field\"",                    "列不存在");
    jexpect("lpop emptylist",   -1, "\"code\":\"empty\"",                       "空列表弹出");
    jexpect("create list fruits",-1,"\"code\":\"name_taken\"",                  "重名钩子");

    printf("JSON：空行与选项\n");
    jexpect("", 0, "\"ok\":true,\"code\":\"ok\",\"text\":\"\"", "空行 -> ok 且 text 为空");

    {
        /* 缓冲区不足时必须安全返回，不能越界写 */
        char tiny[1];
        int rc = mhx_execute_json("llen fruits", tiny, sizeof(tiny));
        CHECK(rc == -1, "outlen < 2 应当拒绝");
        CHECK(tiny[0] == '\0', "被拒绝时不应写入缓冲区");

        char one[2];
        rc = mhx_execute(NULL, one, sizeof(one));
        CHECK(rc == -1, "line == NULL 应当拒绝");
    }

    reg_destroy();

    if (fails) { printf("\n命令层输出测试：%d 项失败\n", fails); return 1; }
    printf("\n命令层输出测试：全部通过\n");
    return 0;
}
