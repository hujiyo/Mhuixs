/*
#版权所有 (c) Mhuixs-team 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2024.11
Email:hj18914255909@outlook.com
*/
/*
execute.c —— Mhuixs 命令执行层

回应 2024.11 最初的设想（Mhuixs.c 里的注释 + 空的 execute.c）：
    命令一行一条，空格分隔，第一个词是命令，其余是参数。
    用 run_queue 承接命令、ret_queue 承接返回，主循环消费队列。

本层刻意不做的事（这是「命令」与「语言」的分界线）：
    - 没有变量、没有 if / while / for、没有函数定义、没有作用域
    - 不做表达式求值（发 "1+2" 不会得到 3）
    理由：这些属于调用方的职责。Mhuixs 只负责「对数据下命令」；
    需要循环和判断的客户端，用 C / Python / 或 AI 现场生成的代码去写。
    这样 Mhuixs 不必与 Lua / Python 竞争，也不必实现图灵完备 —— 
    命令是可白名单、可审计、可静态检查的，对 AI 调用方尤其重要。

命令一览：
    create <list|bitmap> <name>   创建数据结构并挂钩
    drop <name>                   摘除钩子
    hooks                         列出所有钩子
    type <name>                   查看钩子指向的类型

    rpush <name> <v> [<v>...]     LIST 右侧插入
    lpush <name> <v> [<v>...]     LIST 左侧插入
    lpop  <name>                  LIST 左侧弹出
    rpop  <name>                  LIST 右侧弹出
    llen  <name>                  LIST 长度
    lget  <name> <i>              LIST 读取下标 i
    lset  <name> <i> <v>          LIST 改写下标 i

    bset   <name> <off> <0|1>     BITMAP 设置某位
    bget   <name> <off>           BITMAP 读取某位
    bcount <name> <st> <ed>       BITMAP 统计区间内 1 的个数
    bsize  <name>                 BITMAP 位数

值语法：
    123       -> NUMBER
    "hello"   -> STRING（引号强制）
    hello     -> STRING（非数字时自动回退）
*/

#include "execute.h"
#include "hook.h"
#include "registry.h"
#include "bignum.h"
#include "list.h"
#include "bitmap.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define MAX_TOKENS    48
#define MAX_TOKEN_LEN 256

/* ---------------- 输出缓冲 ---------------- */

static void append(char *out, size_t outlen, const char *fmt, ...)
{
    size_t used = strlen(out);
    if (used >= outlen - 1) return;

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(out + used, outlen - used, fmt, ap);
    va_end(ap);
}

/* ---------------- 分词 ---------------- */

typedef struct {
    char text[MAX_TOKEN_LEN];
    int  quoted;   /* 被引号包裹 -> 强制当字符串 */
} token_t;

static int tokenize(const char *line, token_t *toks, int max_tokens)
{
    int n = 0;
    const char *p = line;

    while (*p && n < max_tokens) {
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
        if (!*p || *p == '#') break;   /* 空行或注释 */

        int quoted = 0;
        int len = 0;

        if (*p == '"' || *p == '\'') {
            char q = *p++;
            quoted = 1;
            while (*p && *p != q && len < MAX_TOKEN_LEN - 1)
                toks[n].text[len++] = *p++;
            if (*p == q) p++;          /* 跳过收尾引号 */
        } else {
            while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n'
                   && len < MAX_TOKEN_LEN - 1)
                toks[n].text[len++] = *p++;
        }

        toks[n].text[len] = '\0';
        toks[n].quoted = quoted;
        n++;
    }
    return n;
}

/* ---------------- 取对象 ---------------- */

static const char *type_name(int t)
{
    switch (t) {
        case BIGNUM_TYPE_NUMBER: return "number";
        case BIGNUM_TYPE_STRING: return "string";
        case BIGNUM_TYPE_BITMAP: return "bitmap";
        case BIGNUM_TYPE_LIST:   return "list";
        case BIGNUM_TYPE_TABLE:  return "table";
        case BIGNUM_TYPE_KVALOT: return "kvalot";
        default:                 return "unknown";
    }
}

/* 取出钩子指向的 BHS；expect_type < 0 表示不校验类型 */
static BHS *get_obj(const char *name, int expect_type, char *out, size_t outlen)
{
    HOOK *h = reg_find_hook(name);
    if (!h) {
        snprintf(out, outlen, "ERR no such hook: %s", name);
        return NULL;
    }
    if (!h->obj) {
        snprintf(out, outlen, "ERR hook '%s' holds no object", name);
        return NULL;
    }
    if (expect_type >= 0 && h->obj->type != expect_type) {
        snprintf(out, outlen, "ERR hook '%s' is %s, not %s",
                 name, type_name(h->obj->type), type_name(expect_type));
        return NULL;
    }
    return h->obj;
}

/* 字面量 -> BHS：带引号强制字符串，否则先试数字、失败再退化为字符串 */
static BHS *parse_value(const token_t *tok)
{
    if (tok->quoted)
        return bignum_from_raw_string(tok->text);

    BHS *num = bignum_from_string(tok->text);
    if (num) return num;

    return bignum_from_raw_string(tok->text);
}

/* LIST 的取值函数在越界时返回 (BHS*)-1，需要单独记 */
#define IS_MERR_OBJ(p) ((p) == NULL || (p) == (Obj)(intptr_t)-1)

/* ---------------- 命令实现 ---------------- */

static int cmd_create(token_t *t, int argc, char *out, size_t outlen)
{
    if (argc != 3) {
        snprintf(out, outlen, "ERR usage: create <list|bitmap> <name>");
        return -1;
    }
    const char *type = t[1].text;
    const char *name = t[2].text;

    BHS *obj = NULL;
    if      (!strcmp(type, "list"))   obj = bignum_create_list();
    else if (!strcmp(type, "bitmap")) obj = bitmap_create();
    else {
        /* 这里就是 HOOK 承诺还没兑现的地方 */
        snprintf(out, outlen,
                 "ERR type '%s' is not bridged to BHS yet (available: list / bitmap)",
                 type);
        return -1;
    }
    if (!obj) {
        snprintf(out, outlen, "ERR out of memory");
        return -1;
    }

    /* 先注册钩子，再把对象挂上去 */
    HOOK *hook = NULL;
    int r = reg_register_hook(0, name, &hook);
    if (r != 0) {
        bignum_destroy(obj);
        snprintf(out, outlen, "ERR register hook failed (ret=%d, 1 = name taken)", r);
        return -1;
    }

    /* hook_set_bhs 内部做深拷贝，本地对象用完即释放 */
    int sr = hook_set_bhs(hook, 0, obj);
    bignum_destroy(obj);
    if (sr != 0) {
        reg_unregister_hook(name);
        snprintf(out, outlen, "ERR attach object failed (ret=%d)", sr);
        return -1;
    }

    snprintf(out, outlen, "OK created %s '%s'", type, name);
    return 0;
}

static int cmd_drop(token_t *t, int argc, char *out, size_t outlen)
{
    if (argc != 2) { snprintf(out, outlen, "ERR usage: drop <name>"); return -1; }
    if (!reg_find_hook(t[1].text)) {
        /* 幂等：删除不存在的钩子视为成功，方便脚本与演示重复执行 */
        snprintf(out, outlen, "OK '%s' not found (nothing to drop)", t[1].text);
        return 0;
    }
    reg_unregister_hook(t[1].text);
    snprintf(out, outlen, "OK dropped '%s'", t[1].text);
    return 0;
}

static int cmd_type(token_t *t, int argc, char *out, size_t outlen)
{
    if (argc != 2) { snprintf(out, outlen, "ERR usage: type <name>"); return -1; }
    BHS *obj = get_obj(t[1].text, -1, out, outlen);
    if (!obj) return -1;
    snprintf(out, outlen, "OK %s -> %s", t[1].text, type_name(obj->type));
    return 0;
}

/* hooks 命令的输出累加器 */
struct hook_list_acc {
    char  *out;
    size_t outlen;
    int    count;
};

static int collect_hooks(const char *key, void *value, void *user_data)
{
    struct hook_list_acc *acc = (struct hook_list_acc *)user_data;
    BHS *obj = ((HOOK *)value)->obj;
    append(acc->out, acc->outlen, "%-16s %s\n",
           key, obj ? type_name(obj->type) : "(empty)");
    acc->count++;
    return 0;
}

static int cmd_hooks(token_t *t, int argc, char *out, size_t outlen)
{
    (void)t;
    if (argc != 1) { snprintf(out, outlen, "ERR usage: hooks"); return -1; }

    out[0] = '\0';
    append(out, outlen, "OK %d hook(s):\n", reg_get_hook_count());

    struct hook_list_acc acc;
    acc.out = out; acc.outlen = outlen; acc.count = 0;
    hash_foreach(Reg.hook_map, collect_hooks, &acc);
    return 0;
}

/* ---------- LIST ---------- */

static int cmd_push(token_t *t, int argc, char *out, size_t outlen, int left)
{
    if (argc < 3) {
        snprintf(out, outlen, "ERR usage: %s <name> <value> [<value>...]",
                 left ? "lpush" : "rpush");
        return -1;
    }
    BHS *obj = get_obj(t[1].text, BIGNUM_TYPE_LIST, out, outlen);
    if (!obj) return -1;

    LIST *lst = bignum_get_list(obj);
    if (!lst) { snprintf(out, outlen, "ERR not a list"); return -1; }

    int ok = 0;
    for (int i = 2; i < argc; i++) {
        BHS *v = parse_value(&t[i]);
        if (!v) continue;
        int r = left ? list_lpush(lst, v) : list_rpush(lst, v);
        /* 元素所有权移交给 LIST（注意：LIST 目前没有元素析构器，
         * 因此这里不能释放 v，代价是 drop 时元素内存不会回收 —— 既有设计问题） */
        if (r != 0) bignum_destroy(v);
        else ok++;
    }

    obj->length = list_size(lst);   /* 让 BHS 的 length 与 LIST 同步 */
    snprintf(out, outlen, "OK %s %s, len=%zu", left ? "lpush" : "rpush",
             t[1].text, list_size(lst));
    return ok > 0 ? 0 : -1;
}

static int cmd_pop(token_t *t, int argc, char *out, size_t outlen, int left)
{
    if (argc != 2) {
        snprintf(out, outlen, "ERR usage: %s <name>", left ? "lpop" : "rpop");
        return -1;
    }
    BHS *obj = get_obj(t[1].text, BIGNUM_TYPE_LIST, out, outlen);
    if (!obj) return -1;

    LIST *lst = bignum_get_list(obj);
    if (!lst) { snprintf(out, outlen, "ERR not a list"); return -1; }

    Obj v = left ? list_lpop(lst) : list_rpop(lst);
    if (IS_MERR_OBJ(v)) {
        snprintf(out, outlen, "ERR list '%s' is empty", t[1].text);
        return -1;
    }

    char buf[512];
    buf[0] = '\0';
    bignum_to_string(v, buf, sizeof(buf), 0);
    bignum_destroy(v);

    obj->length = list_size(lst);
    snprintf(out, outlen, "OK %s", buf);
    return 0;
}

static int cmd_llen(token_t *t, int argc, char *out, size_t outlen)
{
    if (argc != 2) { snprintf(out, outlen, "ERR usage: llen <name>"); return -1; }
    BHS *obj = get_obj(t[1].text, BIGNUM_TYPE_LIST, out, outlen);
    if (!obj) return -1;
    snprintf(out, outlen, "OK %zu", list_size(bignum_get_list(obj)));
    return 0;
}

static int cmd_lget(token_t *t, int argc, char *out, size_t outlen)
{
    if (argc != 3) { snprintf(out, outlen, "ERR usage: lget <name> <i>"); return -1; }
    BHS *obj = get_obj(t[1].text, BIGNUM_TYPE_LIST, out, outlen);
    if (!obj) return -1;

    long i = strtol(t[2].text, NULL, 10);
    Obj v = list_get_index(bignum_get_list(obj), (size_t)i);
    if (IS_MERR_OBJ(v)) {
        snprintf(out, outlen, "ERR index %ld out of range", i);
        return -1;
    }
    char buf[512];
    buf[0] = '\0';
    bignum_to_string(v, buf, sizeof(buf), 0);
    snprintf(out, outlen, "OK %s", buf);
    return 0;
}

static int cmd_lset(token_t *t, int argc, char *out, size_t outlen)
{
    if (argc != 4) { snprintf(out, outlen, "ERR usage: lset <name> <i> <v>"); return -1; }
    BHS *obj = get_obj(t[1].text, BIGNUM_TYPE_LIST, out, outlen);
    if (!obj) return -1;

    long i = strtol(t[2].text, NULL, 10);
    BHS *v = parse_value(&t[3]);
    if (!v) { snprintf(out, outlen, "ERR bad value"); return -1; }

    int r = list_set_index(bignum_get_list(obj), (size_t)i, v);
    if (r != 0) {
        bignum_destroy(v);
        snprintf(out, outlen, "ERR index %ld out of range", i);
        return -1;
    }
    snprintf(out, outlen, "OK lset %s[%ld]", t[1].text, i);
    return 0;
}

/* ---------- BITMAP ---------- */

static int cmd_bset(token_t *t, int argc, char *out, size_t outlen)
{
    if (argc != 4) { snprintf(out, outlen, "ERR usage: bset <name> <off> <0|1>"); return -1; }
    BHS *obj = get_obj(t[1].text, BIGNUM_TYPE_BITMAP, out, outlen);
    if (!obj) return -1;

    unsigned long off = strtoul(t[2].text, NULL, 10);
    int val = (int)strtol(t[3].text, NULL, 10);
    if (bitmap_set(obj, off, (uint8_t)(val ? 1 : 0)) != 0) {
        snprintf(out, outlen, "ERR bset failed");
        return -1;
    }
    snprintf(out, outlen, "OK set bit %lu = %d", off, val ? 1 : 0);
    return 0;
}

static int cmd_bget(token_t *t, int argc, char *out, size_t outlen)
{
    if (argc != 3) { snprintf(out, outlen, "ERR usage: bget <name> <off>"); return -1; }
    BHS *obj = get_obj(t[1].text, BIGNUM_TYPE_BITMAP, out, outlen);
    if (!obj) return -1;
    unsigned long off = strtoul(t[2].text, NULL, 10);

    uint64_t len = bitmap_size(obj);
    if (off >= len) {
        snprintf(out, outlen,
                 "ERR offset %lu out of bounds (bitmap has %llu bits)",
                 off, (unsigned long long)len);
        return -1;
    }
    snprintf(out, outlen, "OK %d", bitmap_get(obj, off));
    return 0;
}

static int cmd_bcount(token_t *t, int argc, char *out, size_t outlen)
{
    if (argc != 4) { snprintf(out, outlen, "ERR usage: bcount <name> <st> <ed>"); return -1; }
    BHS *obj = get_obj(t[1].text, BIGNUM_TYPE_BITMAP, out, outlen);
    if (!obj) return -1;
    unsigned long st = strtoul(t[2].text, NULL, 10);
    unsigned long ed = strtoul(t[3].text, NULL, 10);

    /* 区间语义是闭区间 [st, ed]，合法范围 ed <= length-1。
     * 注意 bitmap_count 在越界时返回 (uint64_t)-1，
     * 那会显示成一个巨大的合法数字，所以这里必须自己先挡住。 */
    uint64_t len = bitmap_size(obj);
    if (st > ed || ed >= len) {
        snprintf(out, outlen,
                 "ERR range [%lu,%lu] out of bounds (bitmap has %llu bits, max index %llu)",
                 st, ed, (unsigned long long)len,
                 len ? (unsigned long long)(len - 1) : 0ULL);
        return -1;
    }
    snprintf(out, outlen, "OK %llu", (unsigned long long)bitmap_count(obj, st, ed));
    return 0;
}

static int cmd_bsize(token_t *t, int argc, char *out, size_t outlen)
{
    if (argc != 2) { snprintf(out, outlen, "ERR usage: bsize <name>"); return -1; }
    BHS *obj = get_obj(t[1].text, BIGNUM_TYPE_BITMAP, out, outlen);
    if (!obj) return -1;
    snprintf(out, outlen, "OK %llu", (unsigned long long)bitmap_size(obj));
    return 0;
}

/* ---------------- 分发 ---------------- */

int mhx_execute(const char *line, char *out, size_t outlen)
{
    if (!line || !out || outlen < 2) return -1;
    out[0] = '\0';

    token_t toks[MAX_TOKENS];
    int argc = tokenize(line, toks, MAX_TOKENS);
    if (argc == 0) return 0;   /* 空行 / 纯注释 */

    const char *cmd = toks[0].text;

    if (!strcmp(cmd, "create")) return cmd_create(toks, argc, out, outlen);
    if (!strcmp(cmd, "drop"))   return cmd_drop (toks, argc, out, outlen);
    if (!strcmp(cmd, "hooks"))  return cmd_hooks(toks, argc, out, outlen);
    if (!strcmp(cmd, "type"))   return cmd_type (toks, argc, out, outlen);

    if (!strcmp(cmd, "rpush"))  return cmd_push (toks, argc, out, outlen, 0);
    if (!strcmp(cmd, "lpush"))  return cmd_push (toks, argc, out, outlen, 1);
    if (!strcmp(cmd, "lpop"))   return cmd_pop  (toks, argc, out, outlen, 1);
    if (!strcmp(cmd, "rpop"))   return cmd_pop  (toks, argc, out, outlen, 0);
    if (!strcmp(cmd, "llen"))   return cmd_llen (toks, argc, out, outlen);
    if (!strcmp(cmd, "lget"))   return cmd_lget (toks, argc, out, outlen);
    if (!strcmp(cmd, "lset"))   return cmd_lset (toks, argc, out, outlen);

    if (!strcmp(cmd, "bset"))   return cmd_bset  (toks, argc, out, outlen);
    if (!strcmp(cmd, "bget"))   return cmd_bget  (toks, argc, out, outlen);
    if (!strcmp(cmd, "bcount")) return cmd_bcount(toks, argc, out, outlen);
    if (!strcmp(cmd, "bsize"))  return cmd_bsize (toks, argc, out, outlen);

    snprintf(out, outlen, "ERR unknown command: %s", cmd);
    return -1;
}
