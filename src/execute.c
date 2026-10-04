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
    create <list|bitmap|kvalot> <name>          创建数据结构并挂钩
    create table <name> <field:type> [...]      创建表并挂钩，如 id:int name:str
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
    bcount <name> <st> <ed>       BITMAP 统计闭区间 [st,ed] 内 1 的个数
    bsize  <name>                 BITMAP 位数

    kset <name> <key> <value>     KVALOT 写入（键已存在则覆盖，SET 语义）
    kget <name> <key>             KVALOT 读取
    kdel <name> <key>             KVALOT 删除
    klen <name>                   KVALOT 键数量
    kexists <name> <key>          KVALOT 键是否存在

    tadd  <name> [<v>...]         TABLE 追加一行
    tget  <name> <row> <col>      TABLE 读取单元格，col 可为列名或下标
    tset  <name> <row> <col> <v>  TABLE 改写单元格
    tdel  <name> <row>            TABLE 删除一行
    trows <name>                  TABLE 行数
    tfields <name>                TABLE 字段列表

值语法：
    123       -> NUMBER
    "hello"   -> STRING（引号强制）
    hello     -> STRING（非数字时自动回退）
KVALOT 的键一律按字符串处理，不做数字回退。
*/

#include "execute.h"
#include "hook.h"
#include "registry.h"
#include "bignum.h"
#include "list.h"
#include "bitmap.h"
#include "tblh.h"
#include "kvalh.h"

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

/* 字段类型名 <-> BHS 类型码。
 * TABLE 自身不关心类型（tblh.h 说"只负责管理数据关系"），
 * 这里的类型只作为元数据，供 tset 时做一次提示性校验。 */
static int field_type_from_name(const char *s)
{
    if (!strcmp(s, "int")  || !strcmp(s, "num") || !strcmp(s, "number")) return BIGNUM_TYPE_NUMBER;
    if (!strcmp(s, "str")  || !strcmp(s, "string"))                     return BIGNUM_TYPE_STRING;
    if (!strcmp(s, "list"))                                             return BIGNUM_TYPE_LIST;
    if (!strcmp(s, "bitmap") || !strcmp(s, "bmp"))                      return BIGNUM_TYPE_BITMAP;
    if (!strcmp(s, "kvalot"))                                           return BIGNUM_TYPE_KVALOT;
    if (!strcmp(s, "table"))                                            return BIGNUM_TYPE_TABLE;
    return BIGNUM_TYPE_NULL;   /* any */
}

/* 键必须是字符串类型，所以不做数字回退 */
static BHS *parse_key(const token_t *tok)
{
    return bignum_from_raw_string(tok->text);
}

/* ---------------- create 的分支 ----------------
 * create <list|bitmap|kvalot> <name>
 * create table <name> <field:type> [<field:type>...]
 */

static int create_simple(const char *type, const char *name, char *out, size_t outlen)
{
    BHS *obj = NULL;

    if (!strcmp(type, "list")) {
        obj = bignum_create_list();
    } else if (!strcmp(type, "bitmap")) {
        obj = bitmap_create();
    } else if (!strcmp(type, "kvalot")) {
        BHS *nb = bignum_from_raw_string(name);
        if (!nb) { snprintf(out, outlen, "ERR out of memory"); return -1; }
        KVALOT *kv = kvalot_create(nb);
        bignum_destroy(nb);            /* kvalot_create 内部已复制名字 */
        if (!kv) { snprintf(out, outlen, "ERR kvalot create failed"); return -1; }
        obj = bignum_from_kvalot(kv);
        kvalot_destroy(kv);            /* 已复制进 BHS */
    } else {
        snprintf(out, outlen, "ERR unknown type: %s", type);
        return -1;
    }

    if (!obj) { snprintf(out, outlen, "ERR out of memory"); return -1; }

    HOOK *hook = NULL;
    int r = reg_register_hook(0, name, &hook);
    if (r != 0) {
        bignum_destroy(obj);
        snprintf(out, outlen, "ERR register hook failed (ret=%d, 1 = name taken)", r);
        return -1;
    }

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

static int create_table_cmd(token_t *t, int argc, char *out, size_t outlen)
{
    const char *name = t[2].text;
    int nf = argc - 3;   /* 字段个数 */

    /* 第一遍：只校验格式，不分配，避免失败时出现半成品需要回收 */
    for (int i = 0; i < nf; i++) {
        const char *spec = t[3 + i].text;
        const char *colon = strchr(spec, ':');
        if (!colon || colon == spec || *(colon + 1) == '\0') {
            snprintf(out, outlen,
                     "ERR bad field spec '%s' (expected <name>:<type>, e.g. id:int)", spec);
            return -1;
        }
    }

    int     *types = (int*)calloc((size_t)nf, sizeof(int));
    mstring *names = (mstring*)calloc((size_t)nf, sizeof(mstring));
    if (!types || !names) {
        free(types); free(names);
        snprintf(out, outlen, "ERR out of memory");
        return -1;
    }

    /* 第二遍：构造字段名与类型 */
    for (int i = 0; i < nf; i++) {
        char spec[128];
        snprintf(spec, sizeof(spec), "%s", t[3 + i].text);
        char *colon = strchr(spec, ':');
        *colon = '\0';
        types[i] = field_type_from_name(colon + 1);
        names[i] = mstr(spec);
        if (!names[i]) {
            for (int j = 0; j < i; j++) mstr_free(names[j]);
            free(types); free(names);
            snprintf(out, outlen, "ERR out of memory");
            return -1;
        }
    }

    mstring tname = mstr((char*)name);
    if (!tname) {
        for (int i = 0; i < nf; i++) mstr_free(names[i]);
        free(types); free(names);
        snprintf(out, outlen, "ERR out of memory");
        return -1;
    }

    /* create_table 接管 names[i] 与 tname 的所有权。
     * 若这里失败（只可能是 OOM），不再手动释放上述 mstring ——
     * create_table 的内部失败分支已经释放了一部分，手动再放会 double free。
     * 宁可在这种极端情况下泄漏，也不能崩溃。 */
    TABLE *tb = create_table(types, names, (size_t)nf, tname);
    free(types);
    free(names);            /* 数组本身归调用方，元素已交给表 */

    if (!tb) { snprintf(out, outlen, "ERR table create failed"); return -1; }

    BHS *obj = bignum_from_table(tb);
    free_table(tb);         /* 已复制进 BHS */

    if (!obj) { snprintf(out, outlen, "ERR out of memory"); return -1; }

    HOOK *hook = NULL;
    int r = reg_register_hook(0, name, &hook);
    if (r != 0) {
        bignum_destroy(obj);
        snprintf(out, outlen, "ERR register hook failed (ret=%d, 1 = name taken)", r);
        return -1;
    }

    int sr = hook_set_bhs(hook, 0, obj);
    bignum_destroy(obj);
    if (sr != 0) {
        reg_unregister_hook(name);
        snprintf(out, outlen, "ERR attach object failed (ret=%d)", sr);
        return -1;
    }

    snprintf(out, outlen, "OK created table '%s' with %d field(s)", name, nf);
    return 0;
}

static int cmd_create(token_t *t, int argc, char *out, size_t outlen)
{
    if (argc < 3) {
        snprintf(out, outlen,
                 "ERR usage: create <list|bitmap|kvalot> <name>\n"
                 "           create table <name> <field:type> [<field:type>...]");
        return -1;
    }
    if (!strcmp(t[1].text, "table")) {
        if (argc < 4) {
            snprintf(out, outlen,
                     "ERR usage: create table <name> <field:type> [<field:type>...]");
            return -1;
        }
        return create_table_cmd(t, argc, out, outlen);
    }
    if (argc != 3) {
        snprintf(out, outlen, "ERR usage: create <list|bitmap|kvalot> <name>");
        return -1;
    }
    return create_simple(t[1].text, t[2].text, out, outlen);
}

/* ---------- KVALOT ---------- */

static int cmd_kset(token_t *t, int argc, char *out, size_t outlen)
{
    if (argc != 4) { snprintf(out, outlen, "ERR usage: kset <name> <key> <value>"); return -1; }
    BHS *obj = get_obj(t[1].text, BIGNUM_TYPE_KVALOT, out, outlen);
    if (!obj) return -1;

    KVALOT *kv = bignum_get_kvalot(obj);
    if (!kv) { snprintf(out, outlen, "ERR not a kvalot"); return -1; }

    BHS *key = parse_key(&t[2]);
    if (!key) { snprintf(out, outlen, "ERR out of memory"); return -1; }

    /* SET 语义：键已存在则先删再加（KVALOT 本身没有更新接口） */
    if (kvalot_exists(kv, key)) {
        kvalot_remove(kv, key);
    }

    BHS *val = parse_value(&t[3]);
    if (!val) { bignum_destroy(key); snprintf(out, outlen, "ERR out of memory"); return -1; }

    int r = kvalot_add(kv, key, val);
    bignum_destroy(key);                 /* 键不被接管，只取字符串副本 */
    if (r != 0) {
        bignum_destroy(val);             /* 失败时值也不被接管 */
        snprintf(out, outlen, "ERR kset failed");
        return -1;
    }

    obj->length = kvalot_size(kv);
    snprintf(out, outlen, "OK kset %s, keys=%u", t[1].text, kvalot_size(kv));
    return 0;
}

static int cmd_kget(token_t *t, int argc, char *out, size_t outlen)
{
    if (argc != 3) { snprintf(out, outlen, "ERR usage: kget <name> <key>"); return -1; }
    BHS *obj = get_obj(t[1].text, BIGNUM_TYPE_KVALOT, out, outlen);
    if (!obj) return -1;

    BHS *key = parse_key(&t[2]);
    if (!key) { snprintf(out, outlen, "ERR out of memory"); return -1; }

    Obj found = kvalot_find(bignum_get_kvalot(obj), key);
    bignum_destroy(key);

    if (!found) { snprintf(out, outlen, "ERR key '%s' not found", t[2].text); return -1; }

    /* kvalot_find 返回借用指针，不要释放 */
    char buf[512];
    buf[0] = '\0';
    bignum_to_string(found, buf, sizeof(buf), 0);
    snprintf(out, outlen, "OK %s", buf);
    return 0;
}

static int cmd_kdel(token_t *t, int argc, char *out, size_t outlen)
{
    if (argc != 3) { snprintf(out, outlen, "ERR usage: kdel <name> <key>"); return -1; }
    BHS *obj = get_obj(t[1].text, BIGNUM_TYPE_KVALOT, out, outlen);
    if (!obj) return -1;

    BHS *key = parse_key(&t[2]);
    if (!key) { snprintf(out, outlen, "ERR out of memory"); return -1; }

    int r = kvalot_remove(bignum_get_kvalot(obj), key);
    bignum_destroy(key);

    if (r != 0) { snprintf(out, outlen, "ERR key '%s' not found", t[2].text); return -1; }

    obj->length = kvalot_size(obj->data.kvalot);
    snprintf(out, outlen, "OK kdel %s, keys=%u", t[1].text, kvalot_size(obj->data.kvalot));
    return 0;
}

static int cmd_klen(token_t *t, int argc, char *out, size_t outlen)
{
    if (argc != 2) { snprintf(out, outlen, "ERR usage: klen <name>"); return -1; }
    BHS *obj = get_obj(t[1].text, BIGNUM_TYPE_KVALOT, out, outlen);
    if (!obj) return -1;
    snprintf(out, outlen, "OK %u", kvalot_size(bignum_get_kvalot(obj)));
    return 0;
}

static int cmd_kexists(token_t *t, int argc, char *out, size_t outlen)
{
    if (argc != 3) { snprintf(out, outlen, "ERR usage: kexists <name> <key>"); return -1; }
    BHS *obj = get_obj(t[1].text, BIGNUM_TYPE_KVALOT, out, outlen);
    if (!obj) return -1;

    BHS *key = parse_key(&t[2]);
    if (!key) { snprintf(out, outlen, "ERR out of memory"); return -1; }

    int e = kvalot_exists(bignum_get_kvalot(obj), key);
    bignum_destroy(key);
    snprintf(out, outlen, "OK %d", e);
    return 0;
}

/* ---------- TABLE ---------- */

/* 把行列参数解析为字段下标；col 可以是数字下标或字段名 */
static int resolve_col(TABLE *tb, const token_t *col, size_t *out_index)
{
    const char *s = col->text;
    int all_digits = (s[0] != '\0');
    for (const char *p = s; *p; p++) {
        if (*p < '0' || *p > '9') { all_digits = 0; break; }
    }

    if (all_digits) {
        *out_index = (size_t)strtoul(s, NULL, 10);
        return 0;
    }

    size_t idx = get_field_index(tb, (char*)s, strlen(s));
    if (idx == FIELD_NOT_FOUND) return -1;
    *out_index = idx;
    return 0;
}

static int cmd_tadd(token_t *t, int argc, char *out, size_t outlen)
{
    if (argc < 2) { snprintf(out, outlen, "ERR usage: tadd <name> [<value>...]"); return -1; }
    BHS *obj = get_obj(t[1].text, BIGNUM_TYPE_TABLE, out, outlen);
    if (!obj) return -1;

    TABLE *tb = bignum_get_table(obj);
    int n = argc - 2;
    if ((size_t)n > get_field_count(tb)) {
        snprintf(out, outlen,
                 "ERR too many values: got %d, table has %zu field(s)",
                 n, get_field_count(tb));
        return -1;
    }

    Obj *values = NULL;
    if (n > 0) {
        values = (Obj*)calloc((size_t)n, sizeof(Obj));
        if (!values) { snprintf(out, outlen, "ERR out of memory"); return -1; }
        for (int i = 0; i < n; i++) {
            values[i] = parse_value(&t[2 + i]);
            if (!values[i]) {
                for (int j = 0; j < i; j++) bignum_destroy(values[j]);
                free(values);
                snprintf(out, outlen, "ERR out of memory");
                return -1;
            }
        }
    }

    /* add_record 接管每个元素的所有权；values 数组本身归调用方 */
    int r = add_record(tb, values, (size_t)n);
    if (r != 0) {
        for (int i = 0; i < n; i++) bignum_destroy(values[i]);
        free(values);
        snprintf(out, outlen, "ERR add_record failed");
        return -1;
    }
    free(values);

    obj->length = get_record_count(tb);
    snprintf(out, outlen, "OK tadd %s, rows=%zu", t[1].text, get_record_count(tb));
    return 0;
}

static int cmd_tget(token_t *t, int argc, char *out, size_t outlen)
{
    if (argc != 4) { snprintf(out, outlen, "ERR usage: tget <name> <row> <col>"); return -1; }
    BHS *obj = get_obj(t[1].text, BIGNUM_TYPE_TABLE, out, outlen);
    if (!obj) return -1;

    TABLE *tb = bignum_get_table(obj);

    long row = strtol(t[2].text, NULL, 10);
    if (row < 0 || (size_t)row >= get_record_count(tb)) {
        snprintf(out, outlen, "ERR row %ld out of range (rows=%zu)", row, get_record_count(tb));
        return -1;
    }

    size_t col = 0;
    if (resolve_col(tb, &t[3], &col) != 0) {
        snprintf(out, outlen, "ERR no such field: %s", t[3].text);
        return -1;
    }

    /* get_value 对"越界"和"空单元格"都返回 NULL，所以先自己判边界 */
    Obj v = get_value(tb, (size_t)row, col);
    if (!v) { snprintf(out, outlen, "OK (empty)"); return 0; }

    char buf[512];
    buf[0] = '\0';
    bignum_to_string(v, buf, sizeof(buf), 0);
    snprintf(out, outlen, "OK %s", buf);
    return 0;
}

static int cmd_tset(token_t *t, int argc, char *out, size_t outlen)
{
    if (argc != 5) { snprintf(out, outlen, "ERR usage: tset <name> <row> <col> <value>"); return -1; }
    BHS *obj = get_obj(t[1].text, BIGNUM_TYPE_TABLE, out, outlen);
    if (!obj) return -1;

    TABLE *tb = bignum_get_table(obj);

    long row = strtol(t[2].text, NULL, 10);
    if (row < 0 || (size_t)row >= get_record_count(tb)) {
        snprintf(out, outlen, "ERR row %ld out of range (rows=%zu)", row, get_record_count(tb));
        return -1;
    }

    size_t col = 0;
    if (resolve_col(tb, &t[3], &col) != 0) {
        snprintf(out, outlen, "ERR no such field: %s", t[3].text);
        return -1;
    }

    Obj v = parse_value(&t[4]);
    if (!v) { snprintf(out, outlen, "ERR out of memory"); return -1; }

    /* set_value 接管新值的所有权，并会释放该位置上原有的值 */
    if (set_value(tb, (size_t)row, col, v) != 0) {
        bignum_destroy(v);
        snprintf(out, outlen, "ERR tset failed");
        return -1;
    }

    char cname[64];
    cname[0] = '\0';
    if (tb->field[col].name) {
        size_t n = mstrlen(tb->field[col].name);
        if (n > sizeof(cname) - 1) n = sizeof(cname) - 1;
        memcpy(cname, mstr_cstr(tb->field[col].name), n);
        cname[n] = '\0';
    }
    snprintf(out, outlen, "OK tset %s row=%ld field=%s", t[1].text, row, cname);
    return 0;
}

static int cmd_tdel(token_t *t, int argc, char *out, size_t outlen)
{
    if (argc != 3) { snprintf(out, outlen, "ERR usage: tdel <name> <row>"); return -1; }
    BHS *obj = get_obj(t[1].text, BIGNUM_TYPE_TABLE, out, outlen);
    if (!obj) return -1;

    TABLE *tb = bignum_get_table(obj);
    long row = strtol(t[2].text, NULL, 10);
    if (row < 0 || (size_t)row >= get_record_count(tb)) {
        snprintf(out, outlen, "ERR row %ld out of range (rows=%zu)", row, get_record_count(tb));
        return -1;
    }

    if (rm_record(tb, (size_t)row) != 0) {
        snprintf(out, outlen, "ERR tdel failed");
        return -1;
    }
    obj->length = get_record_count(tb);
    snprintf(out, outlen, "OK tdel %s row=%ld, rows=%zu", t[1].text, row, get_record_count(tb));
    return 0;
}

static int cmd_trows(token_t *t, int argc, char *out, size_t outlen)
{
    if (argc != 2) { snprintf(out, outlen, "ERR usage: trows <name>"); return -1; }
    BHS *obj = get_obj(t[1].text, BIGNUM_TYPE_TABLE, out, outlen);
    if (!obj) return -1;
    TABLE *tb = bignum_get_table(obj);
    snprintf(out, outlen, "OK %zu", get_record_count(tb));
    return 0;
}

static int cmd_tfields(token_t *t, int argc, char *out, size_t outlen)
{
    if (argc != 2) { snprintf(out, outlen, "ERR usage: tfields <name>"); return -1; }
    BHS *obj = get_obj(t[1].text, BIGNUM_TYPE_TABLE, out, outlen);
    if (!obj) return -1;

    TABLE *tb = bignum_get_table(obj);
    out[0] = '\0';
    append(out, outlen, "OK %zu field(s):\n", get_field_count(tb));
    for (size_t i = 0; i < get_field_count(tb); i++) {
        const char *fname = "?";
        if (tb->field[i].name) fname = mstr_cstr(tb->field[i].name);
        append(out, outlen, "%-16s %s\n", fname, type_name(tb->field[i].type));
    }
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

    if (!strcmp(cmd, "kset"))    return cmd_kset   (toks, argc, out, outlen);
    if (!strcmp(cmd, "kget"))    return cmd_kget   (toks, argc, out, outlen);
    if (!strcmp(cmd, "kdel"))    return cmd_kdel   (toks, argc, out, outlen);
    if (!strcmp(cmd, "klen"))    return cmd_klen   (toks, argc, out, outlen);
    if (!strcmp(cmd, "kexists")) return cmd_kexists(toks, argc, out, outlen);

    if (!strcmp(cmd, "tadd"))     return cmd_tadd   (toks, argc, out, outlen);
    if (!strcmp(cmd, "tget"))     return cmd_tget   (toks, argc, out, outlen);
    if (!strcmp(cmd, "tset"))     return cmd_tset   (toks, argc, out, outlen);
    if (!strcmp(cmd, "tdel"))     return cmd_tdel   (toks, argc, out, outlen);
    if (!strcmp(cmd, "trows"))    return cmd_trows  (toks, argc, out, outlen);
    if (!strcmp(cmd, "tfields"))  return cmd_tfields(toks, argc, out, outlen);

    snprintf(out, outlen, "ERR unknown command: %s", cmd);
    return -1;
}
