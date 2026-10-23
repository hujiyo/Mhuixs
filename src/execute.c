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

命令与值的完整清单见文件末的 mhx_help_text()（那是唯一出处，
交互层的 :help 直接打印它）。

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
#include "mstring.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>

/* 分词缓冲的规格。数值的唯一出处是 execute.h 里的 MHX_MAX_*，
 * 免得命令层放宽了限制、协议层还按老数字设界。 */
#define MAX_TOKENS    MHX_MAX_ARGS
#define MAX_TOKEN_LEN (MHX_MAX_ARG_BYTES + 1)

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

/* ====== 结构化结果 ======
 *
 * 为什么需要这一层：命令原来的返回值只能是给人看的一串字（out），
 * 调用方（AI / MCP 层 / 别的语言）想拿值就必须解析那串字，也分不清
 * "钩子不存在"和"类型不对"。
 *
 * 分工：
 *   text  仍然是原来那串人类可读文本（与历史输出逐字节相同）
 *   code  稳定 token，程序据此分支，不解析文案
 *   val   类型化的返回值，只有查询类命令会填
 *
 * JSON 是 text 的**超集**：任何命令都至少能拿到 ok/code/text，
 * 所以还没迁移的命令也不会丢信息。
 */

typedef enum {
    MHX_OK = 0,
    MHX_ERR_BAD_USAGE,     /* bad_usage        参数个数/格式不对 */
    MHX_ERR_NO_HOOK,       /* no_hook          钩子不存在 */
    MHX_ERR_NO_OBJECT,     /* no_object        钩子没有挂对象 */
    MHX_ERR_TYPE,          /* type_mismatch    钩子类型不符 */
    MHX_ERR_RANGE,         /* out_of_range     下标/偏移/区间越界 */
    MHX_ERR_NO_KEY,        /* no_key           键不存在 */
    MHX_ERR_NO_FIELD,      /* no_field         列不存在 */
    MHX_ERR_EMPTY,         /* empty            容器为空 */
    MHX_ERR_NAME_TAKEN,    /* name_taken       钩子名已被占用 */
    MHX_ERR_UNKNOWN_CMD,   /* unknown_command  命令不认识 */
    MHX_ERR_FAILED,        /* failed           其他失败 */
    MHX_ERR_INTERNAL       /* internal         调用方参数不合法 */
} mhx_code;

static const char *code_name(mhx_code c)
{
    switch (c) {
        case MHX_OK:              return "ok";
        case MHX_ERR_BAD_USAGE:   return "bad_usage";
        case MHX_ERR_NO_HOOK:     return "no_hook";
        case MHX_ERR_NO_OBJECT:   return "no_object";
        case MHX_ERR_TYPE:        return "type_mismatch";
        case MHX_ERR_RANGE:       return "out_of_range";
        case MHX_ERR_NO_KEY:      return "no_key";
        case MHX_ERR_NO_FIELD:    return "no_field";
        case MHX_ERR_EMPTY:       return "empty";
        case MHX_ERR_NAME_TAKEN:  return "name_taken";
        case MHX_ERR_UNKNOWN_CMD: return "unknown_command";
        case MHX_ERR_FAILED:      return "failed";
        default:                  return "internal";
    }
}

typedef enum {
    MHX_V_NONE = 0,   /* 无返回值（写命令） */
    MHX_V_NULL,       /* 明确的空值（如 TABLE 的空单元格） */
    MHX_V_INT,        /* 计数 / 下标 */
    MHX_V_NUM,        /* 任意精度数值，按十进制文本保存 */
    MHX_V_STR,        /* 字符串 */
    MHX_V_BOOL,       /* 布尔（某一位、键是否存在） */
    MHX_V_LIST        /* 字符串数组（字段列表） */
} mhx_vkind;

#define MHX_TEXT_MAX  16384
#define MHX_VSTR_MAX  512
#define MHX_VLIST_MAX 32
#define MHX_VITEM_MAX 64

typedef struct {
    mhx_vkind kind;
    long long i;
    int       b;
    char      s[MHX_VSTR_MAX];
    char      items[MHX_VLIST_MAX][MHX_VITEM_MAX];
    int       n;
    int       truncated;   /* LIST 超过 MHX_VLIST_MAX，只留了前若干项 */
} mhx_value;

typedef struct {
    int       ok;          /* 与 mhx_execute* 的返回码一致 */
    mhx_code  code;
    mhx_value val;
    char      text[MHX_TEXT_MAX];
} mhx_result;

static void res_init(mhx_result *r)
{
    r->ok   = 1;
    r->code = MHX_OK;
    r->val.kind = MHX_V_NONE;
    r->val.i = 0;
    r->val.b = 0;
    r->val.s[0] = '\0';
    r->val.n = 0;
    r->val.truncated = 0;
    r->text[0] = '\0';
}

/* 记一次失败。
 * fmt 里**不要**带 "ERR " 前缀 —— 这一层统一补上，
 * 这样 res->text 与历史文本输出保持逐字节一致。 */
static void res_err(mhx_result *r, mhx_code c, const char *fmt, ...)
{
    r->ok   = 0;
    r->code = c;
    r->val.kind = MHX_V_NONE;

    size_t used = 0;
    if (sizeof(r->text) > 5) {
        memcpy(r->text, "ERR ", 4);
        used = 4;
    }

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(r->text + used, sizeof(r->text) - used, fmt, ap);
    va_end(ap);
}

static void res_value_int(mhx_result *r, long long v)
{
    r->val.kind = MHX_V_INT;
    r->val.i = v;
}

static void res_value_bool(mhx_result *r, int v)
{
    r->val.kind = MHX_V_BOOL;
    r->val.b = v ? 1 : 0;
}

static void res_value_null(mhx_result *r)
{
    r->val.kind = MHX_V_NULL;
}

static void res_value_num(mhx_result *r, const char *text)
{
    r->val.kind = MHX_V_NUM;
    snprintf(r->val.s, sizeof(r->val.s), "%s", text ? text : "0");
}

/* 按长度取字符串。BIGNUM_DIGITS 出来的缓冲区**不带 \0**，
 * 所以这里必须显式给长度，不能靠 strlen。 */
static void res_value_str_len(mhx_result *r, const char *p, size_t n)
{
    r->val.kind = MHX_V_STR;
    if (!p) { r->val.s[0] = '\0'; return; }

    if (n >= sizeof(r->val.s)) n = sizeof(r->val.s) - 1;
    memcpy(r->val.s, p, n);
    r->val.s[n] = '\0';
}

static void res_value_str(mhx_result *r, const char *p)
{
    res_value_str_len(r, p, p ? strlen(p) : 0);
}

static void res_value_list_add(mhx_result *r, const char *item)
{
    r->val.kind = MHX_V_LIST;
    if (r->val.n >= MHX_VLIST_MAX) { r->val.truncated = 1; return; }
    snprintf(r->val.items[r->val.n], MHX_VITEM_MAX, "%s", item ? item : "");
    r->val.n++;
}

/* BHS -> 类型化返回值。
 * 数字走 NUM（JSON 里按数字字面量输出，保留任意精度）；
 * 字符串取**原始字符** —— 不能用 bignum_to_string，它会给字符串加引号。 */
static void res_value_from_bhs(mhx_result *r, const BHS *b)
{
    char buf[MHX_VSTR_MAX];

    if (!b) { res_value_null(r); return; }

    if (bignum_is_number(b)) {
        buf[0] = '\0';
        bignum_to_string(b, buf, sizeof(buf), -1);
        res_value_num(r, buf);
        return;
    }

    if (bignum_is_string(b)) {
        res_value_str_len(r, BIGNUM_DIGITS(b), (size_t)b->length);
        return;
    }

    /* 其余类型给字符串形式（与文本输出一致） */
    buf[0] = '\0';
    bignum_to_string(b, buf, sizeof(buf), -1);
    res_value_str(r, buf);
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
static BHS *get_obj(const char *name, int expect_type, mhx_result *res)
{
    HOOK *h = reg_find_hook(name);
    if (!h) {
        res_err(res, MHX_ERR_NO_HOOK, "no such hook: %s", name);
        return NULL;
    }
    if (!h->obj) {
        res_err(res, MHX_ERR_NO_OBJECT, "hook '%s' holds no object", name);
        return NULL;
    }
    if (expect_type >= 0 && h->obj->type != expect_type) {
        res_err(res, MHX_ERR_TYPE, "hook '%s' is %s, not %s",
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

static int cmd_drop(token_t *t, int argc, mhx_result *res)
{
    char *out = res->text;
    size_t outlen = sizeof(res->text);
    if (argc != 2) { res_err(res, MHX_ERR_BAD_USAGE, "usage: drop <name>"); return -1; }
    if (!reg_find_hook(t[1].text)) {
        /* 幂等：删除不存在的钩子视为成功，方便脚本与演示重复执行 */
        snprintf(out, outlen, "OK '%s' not found (nothing to drop)", t[1].text);
        return 0;
    }
    reg_unregister_hook(t[1].text);
    snprintf(out, outlen, "OK dropped '%s'", t[1].text);
    return 0;
}

static int cmd_type(token_t *t, int argc, mhx_result *res)
{
    char *out = res->text;
    size_t outlen = sizeof(res->text);
    if (argc != 2) { res_err(res, MHX_ERR_BAD_USAGE, "usage: type <name>"); return -1; }
    BHS *obj = get_obj(t[1].text, -1, res);
    if (!obj) return -1;
    snprintf(out, outlen, "OK %s -> %s", t[1].text, type_name(obj->type));
    res_value_str(res, type_name(obj->type));
    return 0;
}

/* 一行内容摘要，用于 hooks 列表 */
static void summarize(BHS *obj, char *buf, size_t n)
{
    if (!obj) { snprintf(buf, n, "(empty)"); return; }

    switch (obj->type) {
        case BIGNUM_TYPE_LIST: {
            LIST *l = bignum_get_list(obj);
            snprintf(buf, n, "len=%zu", l ? list_size(l) : 0);
            break;
        }
        case BIGNUM_TYPE_BITMAP: {
            uint64_t sz = bitmap_size(obj);
            uint64_t ones = (sz > 0) ? bitmap_count(obj, 0, sz - 1) : 0;
            snprintf(buf, n, "bits=%llu ones=%llu",
                     (unsigned long long)sz, (unsigned long long)ones);
            break;
        }
        case BIGNUM_TYPE_KVALOT: {
            KVALOT *kv = bignum_get_kvalot(obj);
            snprintf(buf, n, "keys=%u", kv ? kvalot_size(kv) : 0);
            break;
        }
        case BIGNUM_TYPE_TABLE: {
            TABLE *t = bignum_get_table(obj);
            snprintf(buf, n, "rows=%zu fields=%zu",
                     t ? get_record_count(t) : 0,
                     t ? get_field_count(t) : 0);
            break;
        }
        default: {
            char v[128];
            v[0] = '\0';
            bignum_to_string(obj, v, sizeof(v), -1);
            snprintf(buf, n, "%s", v);
            break;
        }
    }
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

    char summary[96];
    summarize(obj, summary, sizeof(summary));

    append(acc->out, acc->outlen, "%-16s %-8s %s\n",
           key, obj ? type_name(obj->type) : "?", summary);
    acc->count++;
    return 0;
}

static int cmd_hooks(token_t *t, int argc, mhx_result *res)
{
    char *out = res->text;
    size_t outlen = sizeof(res->text);
    (void)t;
    if (argc != 1) { res_err(res, MHX_ERR_BAD_USAGE, "usage: hooks"); return -1; }

    out[0] = '\0';
    append(out, outlen, "OK %d hook(s):\n", reg_get_hook_count());
    res_value_int(res, reg_get_hook_count());

    struct hook_list_acc acc;
    acc.out = out; acc.outlen = outlen; acc.count = 0;
    hash_foreach(Reg.hook_map, collect_hooks, &acc);
    return 0;
}

/* 内容最多展示多少条，避免一个巨大的表把输出刷爆 */
#define INSPECT_MAX 20

/* 追加一行内容，超过 INSPECT_MAX 就提示还有多少没显示 */
static void dump_line(char *out, size_t outlen, int *shown, int total,
                      const char *fmt, ...)
{
    if (*shown >= INSPECT_MAX) { (*shown)++; return; }

    size_t used = strlen(out);
    if (used >= outlen - 1) { (*shown)++; return; }

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(out + used, outlen - used, fmt, ap);
    va_end(ap);
    (*shown)++;

    if (*shown == INSPECT_MAX && total > INSPECT_MAX) {
        append(out, outlen, "  ... 还有 %d 项未显示\n", total - INSPECT_MAX);
    }
}

static int cmd_info(token_t *t, int argc, mhx_result *res)
{
    char *out = res->text;
    size_t outlen = sizeof(res->text);
    if (argc != 2) { res_err(res, MHX_ERR_BAD_USAGE, "usage: info <name>"); return -1; }

    BHS *obj = get_obj(t[1].text, -1, res);
    if (!obj) return -1;

    out[0] = '\0';
    append(out, outlen, "%s : %s\n", t[1].text, type_name(obj->type));

    switch (obj->type) {
        case BIGNUM_TYPE_LIST: {
            LIST *l = bignum_get_list(obj);
            size_t n = l ? list_size(l) : 0;
            append(out, outlen, "  元素 %zu 个\n", n);
            int shown = 0;
            for (size_t i = 0; i < n; i++) {
                Obj v = list_get_index(l, i);
                char s[256];
                s[0] = '\0';
                if (IS_MERR_OBJ(v)) continue;
                bignum_to_string(v, s, sizeof(s), -1);
                dump_line(out, outlen, &shown, (int)n, "  [%zu] %s\n", i, s);
            }
            break;
        }

        case BIGNUM_TYPE_BITMAP: {
            uint64_t sz = bitmap_size(obj);
            uint64_t ones = (sz > 0) ? bitmap_count(obj, 0, sz - 1) : 0;
            append(out, outlen, "  共 %llu 位，其中 1 有 %llu 个\n",
                   (unsigned long long)sz, (unsigned long long)ones);
            /* 前 64 位用 0/1 串展示，看得出分布 */
            uint64_t head = sz < 64 ? sz : 64;
            if (head > 0) {
                append(out, outlen, "  前 %llu 位: ", (unsigned long long)head);
                for (uint64_t i = 0; i < head; i++) {
                    append(out, outlen, "%d", bitmap_get(obj, i) ? 1 : 0);
                }
                append(out, outlen, "%s\n", sz > head ? " ..." : "");
            }
            break;
        }

        case BIGNUM_TYPE_KVALOT: {
            KVALOT *kv = bignum_get_kvalot(obj);
            uint32_t n = kv ? kvalot_size(kv) : 0;
            append(out, outlen, "  键 %u 个\n", n);
            int shown = 0;
            for (uint32_t i = 0; i < n && kv; i++) {
                char k[128];
                char v[256];
                v[0] = '\0';
                mstr_to_buf(kv->keypool[i].key, k, sizeof(k));
                if (kv->keypool[i].value) {
                    bignum_to_string(kv->keypool[i].value, v, sizeof(v), -1);
                }
                dump_line(out, outlen, &shown, (int)n, "  %s = %s\n", k, v);
            }
            break;
        }

        case BIGNUM_TYPE_TABLE: {
            TABLE *tb = bignum_get_table(obj);
            size_t rows = tb ? get_record_count(tb) : 0;
            size_t nf   = tb ? get_field_count(tb) : 0;

            append(out, outlen, "  字段 %zu 个:", nf);
            for (size_t c = 0; c < nf; c++) {
                char fname[64];
                append(out, outlen, " %s(%s)",
                       mstr_to_buf(tb->field[c].name, fname, sizeof(fname)),
                       type_name(tb->field[c].type));
            }
            append(out, outlen, "\n  记录 %zu 行\n", rows);

            int shown = 0;
            for (size_t r = 0; r < rows; r++) {
                if (shown >= INSPECT_MAX) { shown++; continue; }
                append(out, outlen, "  [%zu]", r);
                for (size_t c = 0; c < nf; c++) {
                    Obj v = get_value(tb, r, c);
                    char s[128];
                    s[0] = '\0';
                    if (v) bignum_to_string(v, s, sizeof(s), -1);
                    else   snprintf(s, sizeof(s), "(empty)");
                    append(out, outlen, " %s", s);
                }
                append(out, outlen, "\n");
                shown++;
            }
            if (rows > INSPECT_MAX) {
                append(out, outlen, "  ... 还有 %zu 行未显示\n", rows - INSPECT_MAX);
            }
            break;
        }

        default: {
            char v[256];
            v[0] = '\0';
            bignum_to_string(obj, v, sizeof(v), -1);
            append(out, outlen, "  值: %s\n", v);
            break;
        }
    }
    return 0;
}

/* ---------------- biggest：按估算内存列出前 n 个 hook ---------------- */

/* 抽样规模。这些数字只影响估算的精度和耗时，不影响命令语义。
 * 选 32 是为了让大容器也只走几十次元素访问；
 * 嵌套结构另用更小的采样数和总预算兜底，避免病态嵌套把成本放大。 */
#define BIGGEST_DEFAULT_N      10
#define BIGGEST_MAX_N          100
#define BIGGEST_SAMPLES        32    /* 顶层容器最多抽样元素数 */
#define BIGGEST_NESTED_SAMPLES 8     /* 嵌套容器每层最多抽样数 */
#define BIGGEST_TOTAL_SAMPLES  128   /* 每个 hook 的递归抽样总预算 */
#define BIGGEST_MAX_DEPTH      3     /* 根对象 depth=0，再深就只做浅估 */
#define BIGGEST_LINE_BUF       512

typedef struct {
    const char *name;   /* 指向注册表里的键，命令执行期间不变 */
    const BHS  *obj;
    uint64_t    bytes;
} biggest_ent;

typedef struct {
    biggest_ent item[BIGGEST_MAX_N];
    int count;          /* 已收集的条数 */
    int total;          /* 遍历到的 hook 总数 */
    int cap;            /* = n */
} biggest_acc;

/* 估算值会沿对象图累加，用饱和运算避免溢出回绕成一个小数字。 */
static uint64_t sat_add(uint64_t a, uint64_t b)
{
    return (a > UINT64_MAX - b) ? UINT64_MAX : a + b;
}

static uint64_t sat_mul(uint64_t a, uint64_t b)
{
    if (a != 0 && b > UINT64_MAX / a) return UINT64_MAX;
    return a * b;
}

/* 用样本平均值外推 total 个元素，四舍五入。 */
static uint64_t scale_count(uint64_t count, double avg)
{
    double v = (double)count * avg;
    if (v >= (double)UINT64_MAX) return UINT64_MAX;
    if (v <= 0.0) return 0;
    return (uint64_t)(v + 0.5);
}

/* 把第 t 个（共 k 个）样本均匀映射到 [0, n) 上。
 * 全部用整数运算：不用随机数，输出才可复现、测试才能断言顺序。 */
static size_t sample_index(size_t n, size_t t, size_t k)
{
    if (n == 0 || k == 0) return 0;
    size_t v = (n / k) * t + ((n % k) * t) / k;
    return v < n ? v : n - 1;
}

static uint64_t est_shallow(const BHS *b);
static uint64_t est_obj(const BHS *b, int depth, int *budget);

/* 只算容器自身的结构开销，不碰元素内容。深度/预算耗尽时走这里。 */
static uint64_t est_shallow(const BHS *b)
{
    if (!b) return 0;

    uint64_t n = sizeof(BHS);

    switch (b->type) {
        case BIGNUM_TYPE_LIST: {
            const LIST *l = b->data.list;
            if (!l) break;
            n = sat_add(n, sizeof(LIST));
            n = sat_add(n, sat_mul((uint64_t)l->num, sizeof(BHS)));
            break;
        }
        case BIGNUM_TYPE_TABLE: {
            const TABLE *t = b->data.table;
            if (!t) break;
            n = sat_add(n, sizeof(TABLE));
            n = sat_add(n, sat_mul(t->field_num, sizeof(FIELD)));
            n = sat_add(n, sat_mul(sat_mul(t->capacity, t->field_num), sizeof(Obj)));
            n = sat_add(n, sat_mul(t->capacity, 2 * sizeof(size_t)));
            n = sat_add(n, sat_mul(sat_mul(t->line_num, t->field_num), sizeof(BHS)));
            if (t->name) n = sat_add(n, MSTR_CAP(t->name));
            for (size_t c = 0; c < t->field_num; c++)
                if (t->field[c].name) n = sat_add(n, MSTR_CAP(t->field[c].name));
            break;
        }
        case BIGNUM_TYPE_KVALOT: {
            const KVALOT *kv = b->data.kvalot;
            if (!kv) break;
            n = sat_add(n, sizeof(KVALOT));
            n = sat_add(n, sat_mul(kv->num_buckets, sizeof(HASH_BUCKET)));
            n = sat_add(n, sat_mul(kv->keypool_capacity, sizeof(KVPAIR)));
            n = sat_add(n, sat_mul(kv->num_keys, sizeof(BHS)));
            if (kv->name) n = sat_add(n, est_shallow(kv->name));
            break;
        }
        default:
            /* 数字 / 字符串 / 位图：数据要么内联在 BHS 里，要么单独分配。 */
            if (b->is_large && b->data.large_data)
                n = sat_add(n, (uint64_t)b->capacity);
            break;
    }
    return n;
}

/* LIST：块结构是精确的；元素大小按"每块取一个中间元素"抽样外推。
 * 不能对每个下标调 list_get_index —— 它每次都从 head 串块，整体会退化成 O(n²)。 */
static uint64_t est_list(const LIST *l, int depth, int *budget)
{
    uint64_t base = sat_add(sizeof(BHS), sizeof(LIST));
    if (!l) return base;

    size_t blocks = 0;
    size_t seen = 0;
    for (const Block *blk = l->head_block; blk; blk = blk->next) {
        blocks++;
        seen += blk->size;
    }
    base = sat_add(base, sat_mul(blocks, sizeof(Block)));

    if (seen == 0) return base;

    size_t limit = (depth == 0) ? BIGGEST_SAMPLES : BIGGEST_NESTED_SAMPLES;
    size_t want = seen < limit ? seen : limit;
    if (budget) {
        if (*budget <= 0) want = 0;
        else if ((size_t)*budget < want) want = (size_t)*budget;
    }
    if (want == 0) return sat_add(base, sat_mul(seen, sizeof(BHS)));
    if (budget) *budget -= (int)want;

    size_t stride = (blocks + want - 1) / want;
    if (stride == 0) stride = 1;

    uint64_t sum = 0;
    size_t sampled = 0;
    size_t bi = 0;
    for (const Block *blk = l->head_block; blk && sampled < want; blk = blk->next, bi++) {
        if (bi % stride != 0 || blk->size == 0) continue;
        const Obj e = blk->data[blk->start + blk->size / 2];
        if (!e) continue;
        sum = sat_add(sum, est_obj(e, depth + 1, budget));
        sampled++;
    }

    if (sampled == 0) return sat_add(base, sat_mul(seen, sizeof(BHS)));
    return sat_add(base, scale_count(seen, (double)sum / (double)sampled));
}

/* TABLE：结构按 capacity 精确计；单元格按行/列两个方向均匀抽样。
 * 采样必须经过 logic_index：删除行后逻辑行号与物理行号不是一回事。 */
static uint64_t est_table(const TABLE *t, int depth, int *budget)
{
    uint64_t base = sat_add(sizeof(BHS), sizeof(TABLE));
    if (!t) return base;

    base = sat_add(base, sat_mul(t->field_num, sizeof(FIELD)));
    base = sat_add(base, sat_mul(sat_mul(t->capacity, t->field_num), sizeof(Obj)));
    base = sat_add(base, sat_mul(t->capacity, 2 * sizeof(size_t)));
    if (t->name) base = sat_add(base, MSTR_CAP(t->name));
    for (size_t c = 0; c < t->field_num; c++)
        if (t->field[c].name) base = sat_add(base, MSTR_CAP(t->field[c].name));

    uint64_t cells = sat_mul(t->line_num, t->field_num);
    if (cells == 0) return base;

    size_t limit = (depth == 0) ? BIGGEST_SAMPLES : BIGGEST_NESTED_SAMPLES;
    size_t want = cells < limit ? (size_t)cells : limit;
    if (budget) {
        if (*budget <= 0) want = 0;
        else if ((size_t)*budget < want) want = (size_t)*budget;
    }
    if (want == 0) return sat_add(base, sat_mul(cells, sizeof(BHS)));
    if (budget) *budget -= (int)want;

    uint64_t sum = 0;
    size_t sampled = 0;
    for (size_t k = 0; k < want; k++) {
        size_t row = sample_index(t->line_num, k, want);
        size_t col = (t->field_num <= want)
                   ? k % t->field_num
                   : sample_index(t->field_num, k, want);
        size_t phys = t->logic_index ? t->logic_index[row] : row;
        if (phys >= t->capacity) continue;
        if (!t->field[col].data) continue;
        const Obj cell = t->field[col].data[phys];
        if (!cell) continue;
        sum = sat_add(sum, est_obj(cell, depth + 1, budget));
        sampled++;
    }

    if (sampled == 0) return sat_add(base, sat_mul(cells, sizeof(BHS)));
    return sat_add(base, scale_count(cells, (double)sum / (double)sampled));
}

/* KVALOT：keypool 连续且无空洞，可 O(1) 随机取。
 * 桶内 key_indices 的容量不逐个桶扫（那会是 O(桶数)）；
 * 按"抽中的 key 所在桶的容量 / 该桶 key 数"取平均，期望值就是总容量。 */
static uint64_t est_kvalot(const KVALOT *kv, int depth, int *budget)
{
    uint64_t base = sat_add(sizeof(BHS), sizeof(KVALOT));
    if (!kv) return base;

    base = sat_add(base, sat_mul(kv->num_buckets, sizeof(HASH_BUCKET)));
    base = sat_add(base, sat_mul(kv->keypool_capacity, sizeof(KVPAIR)));
    if (kv->name) base = sat_add(base, est_obj(kv->name, depth + 1, budget));

    if (kv->num_keys == 0) return base;

    size_t limit = (depth == 0) ? BIGGEST_SAMPLES : BIGGEST_NESTED_SAMPLES;
    size_t want = kv->num_keys < limit ? kv->num_keys : limit;
    if (budget) {
        if (*budget <= 0) want = 0;
        else if ((size_t)*budget < want) want = (size_t)*budget;
    }
    if (want == 0)
        return sat_add(base, sat_mul(kv->num_keys, sat_add(sizeof(BHS), sizeof(uint32_t))));
    if (budget) *budget -= (int)want;

    uint64_t per_key = 0;
    double ratio_sum = 0.0;
    size_t sampled = 0;
    for (size_t k = 0; k < want; k++) {
        size_t idx = sample_index(kv->num_keys, k, want);
        const KVPAIR *pair = &kv->keypool[idx];
        uint64_t one = pair->key ? MSTR_CAP(pair->key) : 0;
        if (pair->value) one = sat_add(one, est_obj(pair->value, depth + 1, budget));
        per_key = sat_add(per_key, one);

        if (kv->hash_table && pair->hash_index < kv->num_buckets) {
            const HASH_BUCKET *bk = &kv->hash_table[pair->hash_index];
            if (bk->num_keys > 0)
                ratio_sum += (double)bk->capacity / (double)bk->num_keys;
        }
        sampled++;
    }

    if (sampled == 0) return base;
    double avg = (double)per_key / (double)sampled;
    double avg_ratio = ratio_sum / (double)sampled;
    uint64_t out = sat_add(base, scale_count(kv->num_keys, avg));
    out = sat_add(out, scale_count(kv->num_keys, avg_ratio * (double)sizeof(uint32_t)));
    return out;
}

static uint64_t est_obj(const BHS *b, int depth, int *budget)
{
    if (!b) return 0;
    if (depth >= BIGGEST_MAX_DEPTH) return est_shallow(b);

    switch (b->type) {
        case BIGNUM_TYPE_LIST:   return est_list(b->data.list, depth, budget);
        case BIGNUM_TYPE_TABLE:  return est_table(b->data.table, depth, budget);
        case BIGNUM_TYPE_KVALOT: return est_kvalot(b->data.kvalot, depth, budget);
        default:                 return est_shallow(b);
    }
}

/* 估算口径：HOOK 结构 + 名字 + 对象图。
 * 不含 malloc 分配器头部、对齐填充、堆碎片和注册表自身的哈希条目 ——
 * 所以输出用 "~" 标记，只适合比较相对大小，不适合当容量账本。 */
static uint64_t est_hook(const HOOK *h)
{
    if (!h) return 0;

    uint64_t n = sizeof(HOOK);
    if (h->name) n = sat_add(n, MSTR_CAP(h->name));
    if (h->obj) {
        int budget = BIGGEST_TOTAL_SAMPLES;
        n = sat_add(n, est_obj(h->obj, 0, &budget));
    }
    return n;
}

/* biggest 专用的摘要。
 * 不复用 summarize()：它的位图分支会调 bitmap_count()，那是逐位循环，
 * 大位图上等于全量遍历；而 hooks 的输出不能动，只能在旁边另写一份。 */
static void summarize_mem(const BHS *obj, char *buf, size_t n)
{
    if (!obj) { snprintf(buf, n, "(empty)"); return; }

    switch (obj->type) {
        case BIGNUM_TYPE_LIST: {
            const LIST *l = obj->data.list;
            snprintf(buf, n, "len=%zu", l ? l->num : 0);
            break;
        }
        case BIGNUM_TYPE_BITMAP:
            /* 只报位数，不数 1 —— 数 1 是 O(bits)。 */
            snprintf(buf, n, "bits=%llu", (unsigned long long)obj->length);
            break;
        case BIGNUM_TYPE_KVALOT: {
            const KVALOT *kv = obj->data.kvalot;
            snprintf(buf, n, "keys=%u", kv ? kv->num_keys : 0);
            break;
        }
        case BIGNUM_TYPE_TABLE: {
            const TABLE *t = obj->data.table;
            snprintf(buf, n, "rows=%zu fields=%zu",
                     t ? t->line_num : 0, t ? t->field_num : 0);
            break;
        }
        case BIGNUM_TYPE_STRING: {
            size_t len = (size_t)obj->length;
            size_t show = len < 24 ? len : 24;
            char preview[25];
            const char *p = BIGNUM_DIGITS(obj);
            for (size_t i = 0; i < show; i++) {
                unsigned char c = (unsigned char)p[i];
                preview[i] = (c >= 0x20 && c < 0x7f) ? (char)c : '?';
            }
            preview[show] = '\0';
            snprintf(buf, n, "len=%zu \"%s%s\"", len, preview, len > show ? "..." : "");
            break;
        }
        default: {
            char v[64];
            v[0] = '\0';
            bignum_to_string(obj, v, sizeof(v), -1);
            snprintf(buf, n, "%s", v);
            break;
        }
    }
}

/* 排序键：字节数降序；同大小按名字升序，保证输出可复现。 */
static int biggest_before(const biggest_ent *a, const biggest_ent *b)
{
    if (a->bytes != b->bytes) return a->bytes > b->bytes;
    return strcmp(a->name, b->name) < 0;
}

static void biggest_offer(biggest_acc *acc, const char *name,
                          const BHS *obj, uint64_t bytes)
{
    biggest_ent e;
    e.name = name;
    e.obj = obj;
    e.bytes = bytes;

    /* 榜已满且没超过末位时直接退出：绝大多数 hook 走这一条，
     * 不必每次都做 O(n) 的插入扫描。 */
    if (acc->count >= acc->cap && !biggest_before(&e, &acc->item[acc->cap - 1]))
        return;

    int pos = acc->count;
    while (pos > 0 && biggest_before(&e, &acc->item[pos - 1])) pos--;
    if (pos >= acc->cap) return;   /* 没挤进前 n */

    int last = (acc->count < acc->cap) ? acc->count : acc->cap - 1;
    for (int i = last; i > pos; i--) acc->item[i] = acc->item[i - 1];
    acc->item[pos] = e;
    if (acc->count < acc->cap) acc->count++;
}

static int collect_biggest(const char *key, void *value, void *user_data)
{
    biggest_acc *acc = (biggest_acc *)user_data;
    const HOOK *h = (const HOOK *)value;

    acc->total++;
    biggest_offer(acc, key, h ? h->obj : NULL, est_hook(h));
    return 0;
}

/* 严格解析 n：只接受十进制数字。超过上限时立刻返回，避免长数字溢出。 */
static int parse_biggest_n(const char *s, int *out, mhx_result *res)
{
    if (!s || !*s) {
        res_err(res, MHX_ERR_BAD_USAGE, "usage: biggest [n]");
        return -1;
    }

    unsigned long v = 0;
    for (const char *p = s; *p; p++) {
        if (*p < '0' || *p > '9') {
            res_err(res, MHX_ERR_BAD_USAGE, "usage: biggest [n]");
            return -1;
        }
        v = v * 10 + (unsigned long)(*p - '0');
        if (v > BIGGEST_MAX_N) {
            res_err(res, MHX_ERR_RANGE, "n must be between 1 and %d", BIGGEST_MAX_N);
            return -1;
        }
    }
    if (v == 0) {
        res_err(res, MHX_ERR_RANGE, "n must be between 1 and %d", BIGGEST_MAX_N);
        return -1;
    }

    *out = (int)v;
    return 0;
}

static int cmd_biggest(token_t *t, int argc, mhx_result *res)
{
    char *out = res->text;
    size_t outlen = sizeof(res->text);

    if (argc > 2) {
        res_err(res, MHX_ERR_BAD_USAGE, "usage: biggest [n]");
        return -1;
    }

    int n = BIGGEST_DEFAULT_N;
    if (argc == 2 && parse_biggest_n(t[1].text, &n, res) != 0) return -1;

    biggest_acc acc;
    memset(&acc, 0, sizeof(acc));
    acc.cap = n;
    hash_foreach(Reg.hook_map, collect_biggest, &acc);

    out[0] = '\0';
    append(out, outlen, "OK %d hook(s), top %d by estimated memory:\n",
           acc.total, acc.count);

    for (int i = 0; i < acc.count; i++) {
        char summary[96];
        summarize_mem(acc.item[i].obj, summary, sizeof(summary));

        const char *tn = acc.item[i].obj ? type_name(acc.item[i].obj->type) : "?";
        char row[BIGGEST_LINE_BUF];
        int rn = snprintf(row, sizeof(row), "%-16s %-8s ~%llu B  %s\n",
                          acc.item[i].name, tn,
                          (unsigned long long)acc.item[i].bytes, summary);
        if (rn < 0) continue;

        size_t need = (size_t)rn;
        if (need >= sizeof(row)) need = sizeof(row) - 1;
        if (strlen(out) + need + 1 >= outlen) {
            append(out, outlen, "... %d more hook(s) not shown (output buffer full)\n",
                   acc.count - i);
            break;
        }
        append(out, outlen, "%s", row);
    }

    /* JSON 契约：列表，每项 "名字 ~字节数"。
     * 列表上限由 mhx_value 决定（32 项）；文本模式才是完整输出。 */
    res->val.kind = MHX_V_LIST;
    for (int i = 0; i < acc.count; i++) {
        char item[MHX_VITEM_MAX];
        snprintf(item, sizeof(item), "%s ~%llu",
                 acc.item[i].name, (unsigned long long)acc.item[i].bytes);
        res_value_list_add(res, item);
    }
    return 0;
}

/* ---------- LIST ---------- */

static int cmd_push(token_t *t, int argc, mhx_result *res, int left)
{
    char *out = res->text;
    size_t outlen = sizeof(res->text);
    if (argc < 3) {
        res_err(res, MHX_ERR_BAD_USAGE, "usage: %s <name> <value> [<value>...]",
                 left ? "lpush" : "rpush");
        return -1;
    }
    BHS *obj = get_obj(t[1].text, BIGNUM_TYPE_LIST, res);
    if (!obj) return -1;

    LIST *lst = bignum_get_list(obj);
    if (!lst) { res_err(res, MHX_ERR_FAILED, "not a list"); return -1; }

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
             res_value_int(res, (long long)list_size(lst));
    return ok > 0 ? 0 : -1;
}

static int cmd_pop(token_t *t, int argc, mhx_result *res, int left)
{
    char *out = res->text;
    size_t outlen = sizeof(res->text);
    if (argc != 2) {
        res_err(res, MHX_ERR_BAD_USAGE, "usage: %s <name>", left ? "lpop" : "rpop");
        return -1;
    }
    BHS *obj = get_obj(t[1].text, BIGNUM_TYPE_LIST, res);
    if (!obj) return -1;

    LIST *lst = bignum_get_list(obj);
    if (!lst) { res_err(res, MHX_ERR_FAILED, "not a list"); return -1; }

    Obj v = left ? list_lpop(lst) : list_rpop(lst);
    if (IS_MERR_OBJ(v)) {
        res_err(res, MHX_ERR_EMPTY, "list '%s' is empty", t[1].text);
        return -1;
    }

    char buf[512];
    buf[0] = '\0';
    bignum_to_string(v, buf, sizeof(buf), -1);
    res_value_from_bhs(res, v);
    bignum_destroy(v);

    obj->length = list_size(lst);
    snprintf(out, outlen, "OK %s", buf);
    return 0;
}

static int cmd_llen(token_t *t, int argc, mhx_result *res)
{
    char *out = res->text;
    size_t outlen = sizeof(res->text);
    if (argc != 2) { res_err(res, MHX_ERR_BAD_USAGE, "usage: llen <name>"); return -1; }
    BHS *obj = get_obj(t[1].text, BIGNUM_TYPE_LIST, res);
    if (!obj) return -1;
    snprintf(out, outlen, "OK %zu", list_size(bignum_get_list(obj)));
    res_value_int(res, (long long)list_size(bignum_get_list(obj)));
    return 0;
}

static int cmd_lget(token_t *t, int argc, mhx_result *res)
{
    char *out = res->text;
    size_t outlen = sizeof(res->text);
    if (argc != 3) { res_err(res, MHX_ERR_BAD_USAGE, "usage: lget <name> <i>"); return -1; }
    BHS *obj = get_obj(t[1].text, BIGNUM_TYPE_LIST, res);
    if (!obj) return -1;

    long i = strtol(t[2].text, NULL, 10);
    Obj v = list_get_index(bignum_get_list(obj), (size_t)i);
    if (IS_MERR_OBJ(v)) {
        res_err(res, MHX_ERR_RANGE, "index %ld out of range", i);
        return -1;
    }
    char buf[512];
    buf[0] = '\0';
    bignum_to_string(v, buf, sizeof(buf), -1);
    snprintf(out, outlen, "OK %s", buf);
    res_value_from_bhs(res, v);
    return 0;
}

static int cmd_lset(token_t *t, int argc, mhx_result *res)
{
    char *out = res->text;
    size_t outlen = sizeof(res->text);
    if (argc != 4) { res_err(res, MHX_ERR_BAD_USAGE, "usage: lset <name> <i> <v>"); return -1; }
    BHS *obj = get_obj(t[1].text, BIGNUM_TYPE_LIST, res);
    if (!obj) return -1;

    long i = strtol(t[2].text, NULL, 10);
    BHS *v = parse_value(&t[3]);
    if (!v) { res_err(res, MHX_ERR_FAILED, "bad value"); return -1; }

    int r = list_set_index(bignum_get_list(obj), (size_t)i, v);
    if (r != 0) {
        bignum_destroy(v);
        res_err(res, MHX_ERR_RANGE, "index %ld out of range", i);
        return -1;
    }
    snprintf(out, outlen, "OK lset %s[%ld]", t[1].text, i);
    return 0;
}

static int cmd_lrem(token_t *t, int argc, mhx_result *res)
{
    char *out = res->text;
    size_t outlen = sizeof(res->text);
    if (argc != 3) { res_err(res, MHX_ERR_BAD_USAGE, "usage: lrem <name> <i>"); return -1; }
    BHS *obj = get_obj(t[1].text, BIGNUM_TYPE_LIST, res);
    if (!obj) return -1;

    LIST *lst = bignum_get_list(obj);
    if (!lst) { res_err(res, MHX_ERR_FAILED, "not a list"); return -1; }

    long i = strtol(t[2].text, NULL, 10);
    if (i < 0 || (size_t)i >= list_size(lst)) {
        res_err(res, MHX_ERR_RANGE, "index %ld out of range (len=%zu)", i, list_size(lst));
        return -1;
    }

    /* list_rm_index 把被移除元素的所有权交回调用方 */
    Obj v = list_rm_index(lst, (size_t)i);
    if (IS_MERR_OBJ(v)) { res_err(res, MHX_ERR_FAILED, "remove failed"); return -1; }

    char buf[512];
    buf[0] = '\0';
    bignum_to_string(v, buf, sizeof(buf), -1);
    res_value_from_bhs(res, v);
    bignum_destroy(v);

    obj->length = list_size(lst);
    snprintf(out, outlen, "OK removed %s, len=%zu", buf, list_size(lst));
    return 0;
}

/* ---------- BITMAP ---------- */

static int cmd_bset(token_t *t, int argc, mhx_result *res)
{
    char *out = res->text;
    size_t outlen = sizeof(res->text);
    if (argc != 4) { res_err(res, MHX_ERR_BAD_USAGE, "usage: bset <name> <off> <0|1>"); return -1; }
    BHS *obj = get_obj(t[1].text, BIGNUM_TYPE_BITMAP, res);
    if (!obj) return -1;

    unsigned long off = strtoul(t[2].text, NULL, 10);
    int val = (int)strtol(t[3].text, NULL, 10);
    if (bitmap_set(obj, off, (uint8_t)(val ? 1 : 0)) != 0) {
        res_err(res, MHX_ERR_FAILED, "bset failed");
        return -1;
    }
    snprintf(out, outlen, "OK set bit %lu = %d", off, val ? 1 : 0);
    res_value_bool(res, val ? 1 : 0);
    return 0;
}

static int cmd_bget(token_t *t, int argc, mhx_result *res)
{
    char *out = res->text;
    size_t outlen = sizeof(res->text);
    if (argc != 3) { res_err(res, MHX_ERR_BAD_USAGE, "usage: bget <name> <off>"); return -1; }
    BHS *obj = get_obj(t[1].text, BIGNUM_TYPE_BITMAP, res);
    if (!obj) return -1;
    unsigned long off = strtoul(t[2].text, NULL, 10);

    uint64_t len = bitmap_size(obj);
    if (off >= len) {
        res_err(res, MHX_ERR_RANGE,
                 "offset %lu out of bounds (bitmap has %llu bits)",
                 off, (unsigned long long)len);
        return -1;
    }
    snprintf(out, outlen, "OK %d", bitmap_get(obj, off));
    res_value_bool(res, bitmap_get(obj, off) ? 1 : 0);
    return 0;
}

static int cmd_bcount(token_t *t, int argc, mhx_result *res)
{
    char *out = res->text;
    size_t outlen = sizeof(res->text);
    if (argc != 4) { res_err(res, MHX_ERR_BAD_USAGE, "usage: bcount <name> <st> <ed>"); return -1; }
    BHS *obj = get_obj(t[1].text, BIGNUM_TYPE_BITMAP, res);
    if (!obj) return -1;
    unsigned long st = strtoul(t[2].text, NULL, 10);
    unsigned long ed = strtoul(t[3].text, NULL, 10);

    /* 区间语义是闭区间 [st, ed]，合法范围 ed <= length-1。
     * 注意 bitmap_count 在越界时返回 (uint64_t)-1，
     * 那会显示成一个巨大的合法数字，所以这里必须自己先挡住。 */
    uint64_t len = bitmap_size(obj);
    if (st > ed || ed >= len) {
        res_err(res, MHX_ERR_RANGE,
                 "range [%lu,%lu] out of bounds (bitmap has %llu bits, max index %llu)",
                 st, ed, (unsigned long long)len,
                 len ? (unsigned long long)(len - 1) : 0ULL);
        return -1;
    }
    snprintf(out, outlen, "OK %llu", (unsigned long long)bitmap_count(obj, st, ed));
    res_value_int(res, (long long)bitmap_count(obj, st, ed));
    return 0;
}

static int cmd_bsize(token_t *t, int argc, mhx_result *res)
{
    char *out = res->text;
    size_t outlen = sizeof(res->text);
    if (argc != 2) { res_err(res, MHX_ERR_BAD_USAGE, "usage: bsize <name>"); return -1; }
    BHS *obj = get_obj(t[1].text, BIGNUM_TYPE_BITMAP, res);
    if (!obj) return -1;
    snprintf(out, outlen, "OK %llu", (unsigned long long)bitmap_size(obj));
    res_value_int(res, (long long)bitmap_size(obj));
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

static int create_simple(const char *type, const char *name, mhx_result *res)
{
    char *out = res->text;
    size_t outlen = sizeof(res->text);
    BHS *obj = NULL;

    if (!strcmp(type, "list")) {
        obj = bignum_create_list();
    } else if (!strcmp(type, "bitmap")) {
        obj = bitmap_create();
    } else if (!strcmp(type, "kvalot")) {
        BHS *nb = bignum_from_raw_string(name);
        if (!nb) { res_err(res, MHX_ERR_FAILED, "out of memory"); return -1; }
        KVALOT *kv = kvalot_create(nb);
        bignum_destroy(nb);            /* kvalot_create 内部已复制名字 */
        if (!kv) { res_err(res, MHX_ERR_FAILED, "kvalot create failed"); return -1; }
        obj = bignum_from_kvalot(kv);
        kvalot_destroy(kv);            /* 已复制进 BHS */
    } else {
        res_err(res, MHX_ERR_FAILED, "unknown type: %s", type);
        return -1;
    }

    if (!obj) { res_err(res, MHX_ERR_FAILED, "out of memory"); return -1; }

    HOOK *hook = NULL;
    int r = reg_register_hook(0, name, &hook);
    if (r != 0) {
        bignum_destroy(obj);
        res_err(res, (r == 1) ? MHX_ERR_NAME_TAKEN : MHX_ERR_FAILED,
                "register hook failed (ret=%d, 1 = name taken)", r);
        return -1;
    }

    int sr = hook_set_bhs(hook, 0, obj);
    bignum_destroy(obj);
    if (sr != 0) {
        reg_unregister_hook(name);
        res_err(res, MHX_ERR_FAILED, "attach object failed (ret=%d)", sr);
        return -1;
    }

    snprintf(out, outlen, "OK created %s '%s'", type, name);
    return 0;
}

static int create_table_cmd(token_t *t, int argc, mhx_result *res)
{
    char *out = res->text;
    size_t outlen = sizeof(res->text);
    const char *name = t[2].text;
    int nf = argc - 3;   /* 字段个数 */

    /* 第一遍：只校验格式，不分配，避免失败时出现半成品需要回收 */
    for (int i = 0; i < nf; i++) {
        const char *spec = t[3 + i].text;
        const char *colon = strchr(spec, ':');
        if (!colon || colon == spec || *(colon + 1) == '\0') {
            res_err(res, MHX_ERR_BAD_USAGE,
                     "bad field spec '%s' (expected <name>:<type>, e.g. id:int)", spec);
            return -1;
        }
    }

    int     *types = (int*)calloc((size_t)nf, sizeof(int));
    mstring *names = (mstring*)calloc((size_t)nf, sizeof(mstring));
    if (!types || !names) {
        free(types); free(names);
        res_err(res, MHX_ERR_FAILED, "out of memory");
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
            res_err(res, MHX_ERR_FAILED, "out of memory");
            return -1;
        }
    }

    mstring tname = mstr((char*)name);
    if (!tname) {
        for (int i = 0; i < nf; i++) mstr_free(names[i]);
        free(types); free(names);
        res_err(res, MHX_ERR_FAILED, "out of memory");
        return -1;
    }

    /* create_table 接管 names[i] 与 tname 的所有权。
     * 若这里失败（只可能是 OOM），不再手动释放上述 mstring ——
     * create_table 的内部失败分支已经释放了一部分，手动再放会 double free。
     * 宁可在这种极端情况下泄漏，也不能崩溃。 */
    TABLE *tb = create_table(types, names, (size_t)nf, tname);
    free(types);
    free(names);            /* 数组本身归调用方，元素已交给表 */

    if (!tb) { res_err(res, MHX_ERR_FAILED, "table create failed"); return -1; }

    BHS *obj = bignum_from_table(tb);
    free_table(tb);         /* 已复制进 BHS */

    if (!obj) { res_err(res, MHX_ERR_FAILED, "out of memory"); return -1; }

    HOOK *hook = NULL;
    int r = reg_register_hook(0, name, &hook);
    if (r != 0) {
        bignum_destroy(obj);
        res_err(res, (r == 1) ? MHX_ERR_NAME_TAKEN : MHX_ERR_FAILED,
                "register hook failed (ret=%d, 1 = name taken)", r);
        return -1;
    }

    int sr = hook_set_bhs(hook, 0, obj);
    bignum_destroy(obj);
    if (sr != 0) {
        reg_unregister_hook(name);
        res_err(res, MHX_ERR_FAILED, "attach object failed (ret=%d)", sr);
        return -1;
    }

    snprintf(out, outlen, "OK created table '%s' with %d field(s)", name, nf);
    return 0;
}

static int cmd_create(token_t *t, int argc, mhx_result *res)
{
    if (argc < 3) {
        res_err(res, MHX_ERR_BAD_USAGE,
                 "usage: create <list|bitmap|kvalot> <name>\n"
                 "           create table <name> <field:type> [<field:type>...]");
        return -1;
    }
    if (!strcmp(t[1].text, "table")) {
        if (argc < 4) {
            res_err(res, MHX_ERR_BAD_USAGE,
                     "usage: create table <name> <field:type> [<field:type>...]");
            return -1;
        }
        return create_table_cmd(t, argc, res);
    }
    if (argc != 3) {
        res_err(res, MHX_ERR_BAD_USAGE, "usage: create <list|bitmap|kvalot> <name>");
        return -1;
    }
    return create_simple(t[1].text, t[2].text, res);
}

/* ---------- KVALOT ---------- */

static int cmd_kset(token_t *t, int argc, mhx_result *res)
{
    char *out = res->text;
    size_t outlen = sizeof(res->text);
    if (argc != 4) { res_err(res, MHX_ERR_BAD_USAGE, "usage: kset <name> <key> <value>"); return -1; }
    BHS *obj = get_obj(t[1].text, BIGNUM_TYPE_KVALOT, res);
    if (!obj) return -1;

    KVALOT *kv = bignum_get_kvalot(obj);
    if (!kv) { res_err(res, MHX_ERR_FAILED, "not a kvalot"); return -1; }

    BHS *key = parse_key(&t[2]);
    if (!key) { res_err(res, MHX_ERR_FAILED, "out of memory"); return -1; }

    /* SET 语义：键已存在则先删再加（KVALOT 本身没有更新接口） */
    if (kvalot_exists(kv, key)) {
        kvalot_remove(kv, key);
    }

    BHS *val = parse_value(&t[3]);
    if (!val) { bignum_destroy(key); res_err(res, MHX_ERR_FAILED, "out of memory"); return -1; }

    int r = kvalot_add(kv, key, val);
    bignum_destroy(key);                 /* 键不被接管，只取字符串副本 */
    if (r != 0) {
        bignum_destroy(val);             /* 失败时值也不被接管 */
        res_err(res, MHX_ERR_FAILED, "kset failed");
        return -1;
    }

    obj->length = kvalot_size(kv);
    snprintf(out, outlen, "OK kset %s, keys=%u", t[1].text, kvalot_size(kv));
    res_value_int(res, (long long)kvalot_size(kv));
    return 0;
}

static int cmd_kget(token_t *t, int argc, mhx_result *res)
{
    char *out = res->text;
    size_t outlen = sizeof(res->text);
    if (argc != 3) { res_err(res, MHX_ERR_BAD_USAGE, "usage: kget <name> <key>"); return -1; }
    BHS *obj = get_obj(t[1].text, BIGNUM_TYPE_KVALOT, res);
    if (!obj) return -1;

    BHS *key = parse_key(&t[2]);
    if (!key) { res_err(res, MHX_ERR_FAILED, "out of memory"); return -1; }

    Obj found = kvalot_find(bignum_get_kvalot(obj), key);
    bignum_destroy(key);

    if (!found) { res_err(res, MHX_ERR_NO_KEY, "key '%s' not found", t[2].text); return -1; }

    /* kvalot_find 返回借用指针，不要释放 */
    char buf[512];
    buf[0] = '\0';
    bignum_to_string(found, buf, sizeof(buf), -1);
    snprintf(out, outlen, "OK %s", buf);
    res_value_from_bhs(res, found);
    return 0;
}

static int cmd_kdel(token_t *t, int argc, mhx_result *res)
{
    char *out = res->text;
    size_t outlen = sizeof(res->text);
    if (argc != 3) { res_err(res, MHX_ERR_BAD_USAGE, "usage: kdel <name> <key>"); return -1; }
    BHS *obj = get_obj(t[1].text, BIGNUM_TYPE_KVALOT, res);
    if (!obj) return -1;

    BHS *key = parse_key(&t[2]);
    if (!key) { res_err(res, MHX_ERR_FAILED, "out of memory"); return -1; }

    int r = kvalot_remove(bignum_get_kvalot(obj), key);
    bignum_destroy(key);

    if (r != 0) { res_err(res, MHX_ERR_NO_KEY, "key '%s' not found", t[2].text); return -1; }

    obj->length = kvalot_size(obj->data.kvalot);
    snprintf(out, outlen, "OK kdel %s, keys=%u", t[1].text, kvalot_size(obj->data.kvalot));
    res_value_int(res, (long long)kvalot_size(obj->data.kvalot));
    return 0;
}

static int cmd_klen(token_t *t, int argc, mhx_result *res)
{
    char *out = res->text;
    size_t outlen = sizeof(res->text);
    if (argc != 2) { res_err(res, MHX_ERR_BAD_USAGE, "usage: klen <name>"); return -1; }
    BHS *obj = get_obj(t[1].text, BIGNUM_TYPE_KVALOT, res);
    if (!obj) return -1;
    snprintf(out, outlen, "OK %u", kvalot_size(bignum_get_kvalot(obj)));
    res_value_int(res, (long long)kvalot_size(bignum_get_kvalot(obj)));
    return 0;
}

static int cmd_kexists(token_t *t, int argc, mhx_result *res)
{
    char *out = res->text;
    size_t outlen = sizeof(res->text);
    if (argc != 3) { res_err(res, MHX_ERR_BAD_USAGE, "usage: kexists <name> <key>"); return -1; }
    BHS *obj = get_obj(t[1].text, BIGNUM_TYPE_KVALOT, res);
    if (!obj) return -1;

    BHS *key = parse_key(&t[2]);
    if (!key) { res_err(res, MHX_ERR_FAILED, "out of memory"); return -1; }

    int e = kvalot_exists(bignum_get_kvalot(obj), key);
    bignum_destroy(key);
    snprintf(out, outlen, "OK %d", e);
    res_value_bool(res, e);
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

static int cmd_tadd(token_t *t, int argc, mhx_result *res)
{
    char *out = res->text;
    size_t outlen = sizeof(res->text);
    if (argc < 2) { res_err(res, MHX_ERR_BAD_USAGE, "usage: tadd <name> [<value>...]"); return -1; }
    BHS *obj = get_obj(t[1].text, BIGNUM_TYPE_TABLE, res);
    if (!obj) return -1;

    TABLE *tb = bignum_get_table(obj);
    int n = argc - 2;
    if ((size_t)n > get_field_count(tb)) {
        res_err(res, MHX_ERR_BAD_USAGE,
                 "too many values: got %d, table has %zu field(s)",
                 n, get_field_count(tb));
        return -1;
    }

    Obj *values = NULL;
    if (n > 0) {
        values = (Obj*)calloc((size_t)n, sizeof(Obj));
        if (!values) { res_err(res, MHX_ERR_FAILED, "out of memory"); return -1; }
        for (int i = 0; i < n; i++) {
            values[i] = parse_value(&t[2 + i]);
            if (!values[i]) {
                for (int j = 0; j < i; j++) bignum_destroy(values[j]);
                free(values);
                res_err(res, MHX_ERR_FAILED, "out of memory");
                return -1;
            }
        }
    }

    /* add_record 接管每个元素的所有权；values 数组本身归调用方 */
    int r = add_record(tb, values, (size_t)n);
    if (r != 0) {
        for (int i = 0; i < n; i++) bignum_destroy(values[i]);
        free(values);
        res_err(res, MHX_ERR_FAILED, "add_record failed");
        return -1;
    }
    free(values);

    obj->length = get_record_count(tb);
    snprintf(out, outlen, "OK tadd %s, rows=%zu", t[1].text, get_record_count(tb));
    res_value_int(res, (long long)get_record_count(tb));
    return 0;
}

static int cmd_tget(token_t *t, int argc, mhx_result *res)
{
    char *out = res->text;
    size_t outlen = sizeof(res->text);
    if (argc != 4) { res_err(res, MHX_ERR_BAD_USAGE, "usage: tget <name> <row> <col>"); return -1; }
    BHS *obj = get_obj(t[1].text, BIGNUM_TYPE_TABLE, res);
    if (!obj) return -1;

    TABLE *tb = bignum_get_table(obj);

    long row = strtol(t[2].text, NULL, 10);
    if (row < 0 || (size_t)row >= get_record_count(tb)) {
        res_err(res, MHX_ERR_RANGE, "row %ld out of range (rows=%zu)", row, get_record_count(tb));
        return -1;
    }

    size_t col = 0;
    if (resolve_col(tb, &t[3], &col) != 0) {
        res_err(res, MHX_ERR_NO_FIELD, "no such field: %s", t[3].text);
        return -1;
    }

    /* get_value 对"越界"和"空单元格"都返回 NULL，所以先自己判边界 */
    Obj v = get_value(tb, (size_t)row, col);
    if (!v) { snprintf(out, outlen, "OK (empty)"); res_value_null(res); return 0; }

    char buf[512];
    buf[0] = '\0';
    bignum_to_string(v, buf, sizeof(buf), -1);
    snprintf(out, outlen, "OK %s", buf);
    res_value_from_bhs(res, v);
    return 0;
}

static int cmd_tset(token_t *t, int argc, mhx_result *res)
{
    char *out = res->text;
    size_t outlen = sizeof(res->text);
    if (argc != 5) { res_err(res, MHX_ERR_BAD_USAGE, "usage: tset <name> <row> <col> <value>"); return -1; }
    BHS *obj = get_obj(t[1].text, BIGNUM_TYPE_TABLE, res);
    if (!obj) return -1;

    TABLE *tb = bignum_get_table(obj);

    long row = strtol(t[2].text, NULL, 10);
    if (row < 0 || (size_t)row >= get_record_count(tb)) {
        res_err(res, MHX_ERR_RANGE, "row %ld out of range (rows=%zu)", row, get_record_count(tb));
        return -1;
    }

    size_t col = 0;
    if (resolve_col(tb, &t[3], &col) != 0) {
        res_err(res, MHX_ERR_NO_FIELD, "no such field: %s", t[3].text);
        return -1;
    }

    Obj v = parse_value(&t[4]);
    if (!v) { res_err(res, MHX_ERR_FAILED, "out of memory"); return -1; }

    /* set_value 接管新值的所有权，并会释放该位置上原有的值 */
    if (set_value(tb, (size_t)row, col, v) != 0) {
        bignum_destroy(v);
        res_err(res, MHX_ERR_FAILED, "tset failed");
        return -1;
    }

    char cname[64];
    snprintf(out, outlen, "OK tset %s row=%ld field=%s", t[1].text, row,
             mstr_to_buf(tb->field[col].name, cname, sizeof(cname)));
    return 0;
}

static int cmd_tdel(token_t *t, int argc, mhx_result *res)
{
    char *out = res->text;
    size_t outlen = sizeof(res->text);
    if (argc != 3) { res_err(res, MHX_ERR_BAD_USAGE, "usage: tdel <name> <row>"); return -1; }
    BHS *obj = get_obj(t[1].text, BIGNUM_TYPE_TABLE, res);
    if (!obj) return -1;

    TABLE *tb = bignum_get_table(obj);
    long row = strtol(t[2].text, NULL, 10);
    if (row < 0 || (size_t)row >= get_record_count(tb)) {
        res_err(res, MHX_ERR_RANGE, "row %ld out of range (rows=%zu)", row, get_record_count(tb));
        return -1;
    }

    if (rm_record(tb, (size_t)row) != 0) {
        res_err(res, MHX_ERR_FAILED, "tdel failed");
        return -1;
    }
    obj->length = get_record_count(tb);
    snprintf(out, outlen, "OK tdel %s row=%ld, rows=%zu", t[1].text, row, get_record_count(tb));
    res_value_int(res, (long long)get_record_count(tb));
    return 0;
}

static int cmd_trows(token_t *t, int argc, mhx_result *res)
{
    char *out = res->text;
    size_t outlen = sizeof(res->text);
    if (argc != 2) { res_err(res, MHX_ERR_BAD_USAGE, "usage: trows <name>"); return -1; }
    BHS *obj = get_obj(t[1].text, BIGNUM_TYPE_TABLE, res);
    if (!obj) return -1;
    TABLE *tb = bignum_get_table(obj);
    snprintf(out, outlen, "OK %zu", get_record_count(tb));
    res_value_int(res, (long long)get_record_count(tb));
    return 0;
}

static int cmd_tfields(token_t *t, int argc, mhx_result *res)
{
    char *out = res->text;
    size_t outlen = sizeof(res->text);
    if (argc != 2) { res_err(res, MHX_ERR_BAD_USAGE, "usage: tfields <name>"); return -1; }
    BHS *obj = get_obj(t[1].text, BIGNUM_TYPE_TABLE, res);
    if (!obj) return -1;

    TABLE *tb = bignum_get_table(obj);
    out[0] = '\0';
    append(out, outlen, "OK %zu field(s):\n", get_field_count(tb));
    for (size_t i = 0; i < get_field_count(tb); i++) {
        /* 注意：mstr_cstr 返回的指针**不带 \0**（见 mstring.h），
         * 不能直接配 %s 用，否则会读到相邻的未初始化内存。
         * 这里按长度拷进本地缓冲再补 \0。 */
        char fname[64];
        append(out, outlen, "%-16s %s\n",
               mstr_to_buf(tb->field[i].name, fname, sizeof(fname)),
               type_name(tb->field[i].type));
               res_value_list_add(res, fname);
    }
    return 0;
}

/* ---------------- 帮助文本 ----------------
 * 命令清单的【唯一出处】。交互层（Mhuixs.c 的 :help）直接打印这个，
 * 不要在别处再抄一份 —— 抄了就会两边不一致。
 */
const char *mhx_help_text(void)
{
    return
"数据结构\n"
"  create <list|bitmap|kvalot> <name>       创建并挂钩\n"
"  create table <name> <field:type> [...]   创建表，如 id:int name:str\n"
"  drop <name>                              摘除钩子（释放其数据）\n"
"  hooks                                    列出所有钩子（含内容摘要）\n"
"  biggest [n]                              按估算内存列出前 n 个钩子（默认 10）\n"
"  info <name>                              查看某个钩子的内容\n"
"  type <name>                              只看钩子指向的类型\n"
"\n"
"LIST\n"
"  rpush <name> <v> [<v>...]                右侧插入（可多个）\n"
"  lpush <name> <v> [<v>...]                左侧插入\n"
"  lpop  <name> / rpop <name>               左/右侧弹出并返回\n"
"  llen  <name>                             元素个数\n"
"  lget  <name> <i>                         读取下标 i\n"
"  lset  <name> <i> <v>                     改写下标 i\n"
"  lrem  <name> <i>                         移除下标 i，返回被移除的值\n"
"\n"
"BITMAP\n"
"  bset   <name> <off> <0|1>                设置某位\n"
"  bget   <name> <off>                      读取某位\n"
"  bcount <name> <st> <ed>                  统计闭区间 [st,ed] 内 1 的个数\n"
"  bsize  <name>                            位数\n"
"\n"
"KVALOT（键一律按字符串处理）\n"
"  kset <name> <key> <value>                写入（键已存在则覆盖）\n"
"  kget <name> <key>                        读取\n"
"  kdel <name> <key>                        删除\n"
"  klen <name>                              键数量\n"
"  kexists <name> <key>                     键是否存在\n"
"\n"
"TABLE（col 可用列名或下标）\n"
"  tadd  <name> [<v>...]                    追加一行\n"
"  tget  <name> <row> <col>                 读取单元格\n"
"  tset  <name> <row> <col> <v>             改写单元格\n"
"  tdel  <name> <row>                       删除一行\n"
"  trows <name>                             行数\n"
"  tfields <name>                           字段列表\n"
"\n"
"值语法\n"
"  123       -> 数字\n"
"  \"hello\"   -> 字符串（引号强制）\n"
"  hello     -> 字符串（非数字时自动回退）\n"
"\n"
"交互元命令\n"
"  :help / :?      显示本说明\n"
"  :save           立即保存到磁盘\n"
"  :format text|json  切换输出格式（json 供程序/AI 消费）\n"
"  :quit / :q      退出（退出时自动保存）\n";
}

/* ---------------- JSON 渲染 ---------------- */

/* num 字面量合法性：只接受 -?digits(.digits)?
 * 不合法就退回成字符串，宁可降级也不吐畸形 JSON。 */
static int json_num_literal_ok(const char *s)
{
    if (!s || !*s) return 0;
    if (*s == '-') s++;
    if (!*s) return 0;

    int digits = 0;
    while (*s >= '0' && *s <= '9') { s++; digits = 1; }
    if (*s == '.') {
        s++;
        while (*s >= '0' && *s <= '9') { s++; digits = 1; }
    }
    return digits && *s == '\0';
}

/* 把 src 的前 n 个字节按 JSON 字符串规则转义后追加到 dst。
 * 只转义 JSON 必须转义的字符，UTF-8 原样透传。 */
static void json_escape_append(char *dst, size_t dstlen, const char *src, size_t n)
{
    size_t used = strlen(dst);

    for (size_t i = 0; i < n; i++) {
        if (used + 8 >= dstlen) break;   /* 留够最坏情况（\uXXXX）的余量 */
        unsigned char c = (unsigned char)src[i];
        switch (c) {
            case '"':  dst[used++] = '\\'; dst[used++] = '"';  break;
            case '\\': dst[used++] = '\\'; dst[used++] = '\\'; break;
            case '\n': dst[used++] = '\\'; dst[used++] = 'n';  break;
            case '\r': dst[used++] = '\\'; dst[used++] = 'r';  break;
            case '\t': dst[used++] = '\\'; dst[used++] = 't';  break;
            case '\b': dst[used++] = '\\'; dst[used++] = 'b';  break;
            case '\f': dst[used++] = '\\'; dst[used++] = 'f';  break;
            default:
                if (c < 0x20) used += (size_t)snprintf(dst + used, dstlen - used, "\\u%04x", c);
                else          dst[used++] = (char)c;
                break;
        }
    }
    dst[used] = '\0';
}

static void render_json(const mhx_result *r, char *out, size_t outlen)
{
    out[0] = '\0';
    append(out, outlen, "{\"ok\":%s,\"code\":\"%s\"",
           r->ok ? "true" : "false", code_name(r->code));

    switch (r->val.kind) {
        case MHX_V_NULL:
            append(out, outlen, ",\"kind\":\"null\"");
            break;
        case MHX_V_INT:
            append(out, outlen, ",\"kind\":\"int\",\"value\":%lld", r->val.i);
            break;
        case MHX_V_BOOL:
            append(out, outlen, ",\"kind\":\"bool\",\"value\":%s",
                   r->val.b ? "true" : "false");
            break;
        case MHX_V_NUM:
            append(out, outlen, ",\"kind\":\"num\",\"value\":");
            if (json_num_literal_ok(r->val.s)) {
                append(out, outlen, "%s", r->val.s);
            } else {
                append(out, outlen, "\"");
                json_escape_append(out, outlen, r->val.s, strlen(r->val.s));
                append(out, outlen, "\"");
            }
            break;
        case MHX_V_STR:
            append(out, outlen, ",\"kind\":\"str\",\"value\":\"");
            json_escape_append(out, outlen, r->val.s, strlen(r->val.s));
            append(out, outlen, "\"");
            break;
        case MHX_V_LIST:
            append(out, outlen, ",\"kind\":\"list\",\"value\":[");
            for (int i = 0; i < r->val.n; i++) {
                if (i) append(out, outlen, ",");
                append(out, outlen, "\"");
                json_escape_append(out, outlen, r->val.items[i], strlen(r->val.items[i]));
                append(out, outlen, "\"");
            }
            append(out, outlen, "]");
            if (r->val.truncated) append(out, outlen, ",\"truncated\":true");
            break;
        case MHX_V_NONE:
        default:
            break;
    }

    /* text：与文本模式同一串，只去掉末尾那一个换行 */
    size_t tn = strlen(r->text);
    if (tn > 0 && r->text[tn - 1] == '\n') tn--;

    append(out, outlen, ",\"text\":\"");
    json_escape_append(out, outlen, r->text, tn);
    append(out, outlen, "\"}\n");
}

/* ---------------- 分发 ---------------- */

/* 分发：参数已经切好了。返回 0 成功 / 非 0 失败。
 *
 * 命令实现只认 token_t，所以文本入口和 argv 入口最终都汇到这里 ——
 * 分发表只有一份，加命令时不必考虑调用方从哪来。 */
static int mhx_run_tokens(token_t *toks, int argc, mhx_result *res)
{
    res_init(res);

    if (argc <= 0) return 0;   /* 空行 / 纯注释 */

    const char *cmd = toks[0].text;
    int rc;

    if      (!strcmp(cmd, "create"))  rc = cmd_create  (toks, argc, res);
    else if (!strcmp(cmd, "drop"))    rc = cmd_drop    (toks, argc, res);
    else if (!strcmp(cmd, "hooks"))   rc = cmd_hooks   (toks, argc, res);
    else if (!strcmp(cmd, "biggest")) rc = cmd_biggest (toks, argc, res);
    else if (!strcmp(cmd, "type"))    rc = cmd_type    (toks, argc, res);
    else if (!strcmp(cmd, "info"))    rc = cmd_info    (toks, argc, res);

    else if (!strcmp(cmd, "rpush"))   rc = cmd_push    (toks, argc, res, 0);
    else if (!strcmp(cmd, "lpush"))   rc = cmd_push    (toks, argc, res, 1);
    else if (!strcmp(cmd, "lpop"))    rc = cmd_pop     (toks, argc, res, 1);
    else if (!strcmp(cmd, "rpop"))    rc = cmd_pop     (toks, argc, res, 0);
    else if (!strcmp(cmd, "llen"))    rc = cmd_llen    (toks, argc, res);
    else if (!strcmp(cmd, "lget"))    rc = cmd_lget    (toks, argc, res);
    else if (!strcmp(cmd, "lset"))    rc = cmd_lset    (toks, argc, res);
    else if (!strcmp(cmd, "lrem"))    rc = cmd_lrem    (toks, argc, res);

    else if (!strcmp(cmd, "bset"))    rc = cmd_bset    (toks, argc, res);
    else if (!strcmp(cmd, "bget"))    rc = cmd_bget    (toks, argc, res);
    else if (!strcmp(cmd, "bcount"))  rc = cmd_bcount  (toks, argc, res);
    else if (!strcmp(cmd, "bsize"))   rc = cmd_bsize   (toks, argc, res);

    else if (!strcmp(cmd, "kset"))    rc = cmd_kset    (toks, argc, res);
    else if (!strcmp(cmd, "kget"))    rc = cmd_kget    (toks, argc, res);
    else if (!strcmp(cmd, "kdel"))    rc = cmd_kdel    (toks, argc, res);
    else if (!strcmp(cmd, "klen"))    rc = cmd_klen    (toks, argc, res);
    else if (!strcmp(cmd, "kexists")) rc = cmd_kexists (toks, argc, res);

    else if (!strcmp(cmd, "tadd"))    rc = cmd_tadd    (toks, argc, res);
    else if (!strcmp(cmd, "tget"))    rc = cmd_tget    (toks, argc, res);
    else if (!strcmp(cmd, "tset"))    rc = cmd_tset    (toks, argc, res);
    else if (!strcmp(cmd, "tdel"))    rc = cmd_tdel    (toks, argc, res);
    else if (!strcmp(cmd, "trows"))   rc = cmd_trows   (toks, argc, res);
    else if (!strcmp(cmd, "tfields")) rc = cmd_tfields (toks, argc, res);

    else {
        res_err(res, MHX_ERR_UNKNOWN_CMD, "unknown command: %s", cmd);
        return -1;
    }

    /* 处理器只 return -1、自己没记码时兜一个 failed。
     * ok 一律以返回码为准 —— text 是给人看的，返回码才是契约。 */
    if (rc != 0) {
        res->ok = 0;
        if (res->code == MHX_OK) res->code = MHX_ERR_FAILED;
    }
    return rc;
}

/* 文本入口：分词后走同一条分发路径。分词规则一行没动，
 * 所以 mhx_execute / mhx_execute_json 的输出与历史逐字节相同。 */
static int mhx_run(const char *line, mhx_result *res)
{
    token_t toks[MAX_TOKENS];
    int argc = tokenize(line, toks, MAX_TOKENS);
    return mhx_run_tokens(toks, argc, res);
}

/* 把结果按请求的版式写进 out。两个入口共用，避免版式分叉。 */
static void render_result(const mhx_result *res, char *out, size_t outlen, int json)
{
    if (json) render_json(res, out, outlen);
    else      snprintf(out, outlen, "%s", res->text);
}

int mhx_execute(const char *line, char *out, size_t outlen)
{
    if (!line || !out || outlen < 2) return -1;
    out[0] = '\0';

    mhx_result res;
    int rc = mhx_run(line, &res);

    render_result(&res, out, outlen, 0);
    return rc;
}

int mhx_execute_json(const char *line, char *out, size_t outlen)
{
    if (!line || !out || outlen < 2) return -1;
    out[0] = '\0';

    mhx_result res;
    int rc = mhx_run(line, &res);

    render_result(&res, out, outlen, 1);
    return rc;
}

int mhx_execute_argv(int argc, const struct mhx_arg *args,
                     char *out, size_t outlen, int json)
{
    if (argc < 0 || (argc > 0 && !args) || !out || outlen < 2) return -1;
    out[0] = '\0';

    mhx_result res;

    /* 参数不合法一律"报错并返回"，绝不静默截断 ——
     * 悄悄存下一个不一样的值，比直接失败难查得多。 */
    if (argc > MAX_TOKENS) {
        res_init(&res);
        res_err(&res, MHX_ERR_INTERNAL, "too many arguments (max %d)", MAX_TOKENS);
        render_result(&res, out, outlen, json);
        return -1;
    }

    token_t toks[MAX_TOKENS];
    for (int i = 0; i < argc; i++) {
        const struct mhx_arg *a = &args[i];

        if (a->len > MAX_TOKEN_LEN - 1) {
            res_init(&res);
            res_err(&res, MHX_ERR_INTERNAL,
                    "argument %d too long (%zu bytes, max %d)",
                    i, a->len, MAX_TOKEN_LEN - 1);
            render_result(&res, out, outlen, json);
            return -1;
        }
        if (a->len > 0 && !a->p) {
            res_init(&res);
            res_err(&res, MHX_ERR_INTERNAL, "argument %d is null", i);
            render_result(&res, out, outlen, json);
            return -1;
        }
        /* 内嵌 NUL：命令层把参数当 C 字符串，存进去只会被截断，
         * 客户端却以为存的是原值。这里直接拒绝。 */
        if (a->len > 0 && memchr(a->p, '\0', a->len)) {
            res_init(&res);
            res_err(&res, MHX_ERR_INTERNAL, "argument %d contains NUL", i);
            render_result(&res, out, outlen, json);
            return -1;
        }

        memcpy(toks[i].text, a->p, a->len);
        toks[i].text[a->len] = '\0';
        toks[i].quoted = a->quoted;
    }

    int rc = mhx_run_tokens(toks, argc, &res);
    render_result(&res, out, outlen, json);
    return rc;
}

int mhx_split_line(const char *line, char *storage, size_t storage_len,
                   struct mhx_arg *args, int max_args)
{
    if (!line || !storage || !args || max_args <= 0) return 0;

    token_t toks[MAX_TOKENS];
    int n = tokenize(line, toks, MAX_TOKENS);
    if (n > max_args) n = max_args;

    size_t used = 0;
    for (int i = 0; i < n; i++) {
        size_t len = strlen(toks[i].text);
        if (used + len + 1 > storage_len) { n = i; break; }   /* 装不下就少给几个 */

        memcpy(storage + used, toks[i].text, len + 1);
        args[i].p      = storage + used;
        args[i].len    = len;
        args[i].quoted = toks[i].quoted;
        used += len + 1;
    }
    return n;
}

void mhx_json_text(char *dst, size_t dstlen, int ok, const char *code, const char *text)
{
    if (!dst || dstlen == 0) return;
    dst[0] = '\0';

    append(dst, dstlen, "{\"ok\":%s,\"code\":\"%s\",\"text\":\"",
           ok ? "true" : "false",
           code ? code : (ok ? "ok" : "failed"));

    if (text) json_escape_append(dst, dstlen, text, strlen(text));

    append(dst, dstlen, "\"}\n");
}
