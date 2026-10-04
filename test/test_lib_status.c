/*
 * Mhuixs 基础库测试：LIST / BITMAP / TABLE
 *
 * 重点验证 LIST 的所有权规则与深拷贝：
 *   1. 存入交出所有权、弹出拿回所有权、lget 只借用
 *   2. list_copy 必须深拷贝（改副本不能影响原表，且不能 double free）
 *   3. 嵌套 LIST 的深拷贝递归正确
 *
 * 注意：元素一律是真实的 BHS 对象。不能用 (Obj)(intptr_t)100 这种
 * 把整数当指针的写法 —— LIST 会销毁它拥有的元素，那会直接崩。
 */
#include <stdio.h>
#include <string.h>

#include "list.h"
#include "bitmap.h"
#include "tblh.h"
#include "bignum.h"

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("  FAIL: %s\n", msg); fails++; } } while (0)

/* LIST 的取值类函数在出错时返回 (Obj)(intptr_t)-1 这个哨兵值 */
#define IS_OBJ_VALID(o) ((o) != NULL && (o) != (Obj)(intptr_t)-1)

/* 造一个数字型 BHS */
static Obj num(const char *s) { return bignum_from_string(s); }

/* 造一个字符串型 BHS */
static Obj str(const char *s) { return bignum_from_raw_string(s); }

/* 把 BHS 转成可比较的字符串；字符串类型会带引号 */
static const char *txt(Obj v, char *buf, size_t n) {
    buf[0] = '\0';
    if (v) bignum_to_string(v, buf, n, 0);
    return buf;
}

static void test_list(void)
{
    printf("=== LIST ===\n");
    char b[256];

    LIST *lst = list_create();
    CHECK(lst != NULL, "list_create");
    CHECK(list_size(lst) == 0, "empty size == 0");

    /* 存入即交出所有权 */
    CHECK(list_rpush(lst, num("100")) == 0, "rpush 100");
    CHECK(list_rpush(lst, str("apple")) == 0, "rpush apple");
    CHECK(list_lpush(lst, num("50")) == 0, "lpush 50");
    CHECK(list_size(lst) == 3, "size == 3");

    CHECK(strcmp(txt(list_get_index(lst, 0), b, sizeof b), "50") == 0, "[0] == 50");
    CHECK(strcmp(txt(list_get_index(lst, 1), b, sizeof b), "100") == 0, "[1] == 100");
    CHECK(strcmp(txt(list_get_index(lst, 2), b, sizeof b), "\"apple\"") == 0, "[2] == apple");

    /* 拒绝 NULL 元素 */
    CHECK(list_rpush(lst, NULL) != 0, "rpush(NULL) rejected");
    CHECK(list_size(lst) == 3, "size still 3 after NULL reject");

    /* lset 覆盖已有值 */
    CHECK(list_set_index(lst, 1, num("999")) == 0, "lset [1] = 999");
    CHECK(strcmp(txt(list_get_index(lst, 1), b, sizeof b), "999") == 0, "[1] == 999");

    /* 弹出即拿回所有权 —— 由本节负责销毁 */
    Obj popped = list_rpop(lst);
    CHECK(strcmp(txt(popped, b, sizeof b), "\"apple\"") == 0, "rpop == apple");
    bignum_destroy(popped);
    CHECK(list_size(lst) == 2, "size == 2 after rpop");

    /* lrem 也把元素交还出来 */
    Obj removed = list_rm_index(lst, 0);
    CHECK(IS_OBJ_VALID(removed), "rm_index(0) returns value");
    CHECK(strcmp(txt(removed, b, sizeof b), "50") == 0, "removed == 50");
    bignum_destroy(removed);
    CHECK(list_size(lst) == 1, "size == 1 after rm");
    CHECK(strcmp(txt(list_get_index(lst, 0), b, sizeof b), "999") == 0, "[0] == 999");

    /* 越界应报错而不是崩 */
    CHECK(list_get_index(lst, 99) == (Obj)(intptr_t)-1, "lget out of range sentinel");
    CHECK(list_rm_index(lst, 99) == (Obj)(intptr_t)-1, "lrem out of range sentinel");

    /* ---- 深拷贝：副本必须完全独立 ---- */
    LIST *cp = list_copy(lst);
    CHECK(cp != NULL, "list_copy");
    CHECK(list_size(cp) == 1, "copy size == 1");

    Obj a = list_get_index(lst, 0);
    Obj c = list_get_index(cp, 0);
    CHECK(a != NULL && c != NULL, "both indices valid");
    CHECK(a != c, "深拷贝：两份是不同的对象（指针不同）");

    /* 改副本，原表不受影响 */
    CHECK(list_set_index(cp, 0, num("7")) == 0, "lset on copy");
    CHECK(strcmp(txt(list_get_index(lst, 0), b, sizeof b), "999") == 0,
          "改副本后原表仍是 999");
    CHECK(strcmp(txt(list_get_index(cp, 0), b, sizeof b), "7") == 0, "副本是 7");

    /* 原表照常可写（若之前是浅拷贝，此处可能已 double free） */
    CHECK(list_rpush(lst, str("tail")) == 0, "rpush after copy");
    CHECK(list_size(lst) == 2, "original size == 2");
    CHECK(list_size(cp) == 1, "copy size still 1");

    /* 两个都释放，不应崩 */
    free_list(cp);
    free_list(lst);
    printf("  LIST 所有权与深拷贝：通过\n\n");
}

/* 嵌套 LIST：外层元素的深拷贝必须递归下去 */
static void test_nested_list(void)
{
    printf("=== 嵌套 LIST ===\n");
    char b[256];

    Obj outer = bignum_create_list();
    CHECK(outer != NULL, "create outer list");
    LIST *ol = bignum_get_list(outer);

    Obj inner = bignum_create_list();
    CHECK(inner != NULL, "create inner list");
    CHECK(list_rpush(bignum_get_list(inner), num("1")) == 0, "inner push 1");
    CHECK(list_rpush(bignum_get_list(inner), num("2")) == 0, "inner push 2");

    /* 内层交给外层，所有权转移 */
    CHECK(list_rpush(ol, inner) == 0, "outer push inner");

    /* 深拷贝整棵结构 */
    BHS *dup = bignum_create();
    CHECK(dup != NULL, "bignum_create for dup");
    CHECK(bignum_copy(outer, dup) == 0, "bignum_copy(outer)");

    LIST *dl = bignum_get_list(dup);
    CHECK(dl != NULL, "dup is a LIST");
    CHECK(list_size(dl) == 1, "dup size == 1");

    Obj d0 = list_get_index(dl, 0);
    CHECK(d0 != NULL && d0->type == BIGNUM_TYPE_LIST, "dup[0] is LIST");
    Obj o0 = list_get_index(ol, 0);
    CHECK(d0 != o0, "深拷贝：内层也是新对象");

    LIST *dil = bignum_get_list(d0);
    CHECK(list_size(dil) == 2, "dup inner size == 2");
    CHECK(strcmp(txt(list_get_index(dil, 0), b, sizeof b), "1") == 0, "dup inner[0] == 1");

    /* 改副本内层，原结构不受影响 */
    CHECK(list_set_index(dil, 0, num("777")) == 0, "set dup inner[0]");
    LIST *oil = bignum_get_list(o0);
    CHECK(strcmp(txt(list_get_index(oil, 0), b, sizeof b), "1") == 0,
          "改副本内层后原内层仍是 1");

    bignum_destroy(dup);
    bignum_destroy(outer);
    printf("  嵌套 LIST 递归深拷贝：通过\n\n");
}

static void test_bitmap(void)
{
    printf("=== BITMAP ===\n");
    BHS *bm = bitmap_create_with_size(100);
    CHECK(bm != NULL, "bitmap_create_with_size");
    CHECK(check_if_bitmap(bm) == 1, "check_if_bitmap");
    CHECK(bitmap_set(bm, 10, 1) == 0, "bitmap_set(10,1)");
    CHECK(bitmap_get(bm, 10) == 1, "bitmap_get(10) == 1");
    CHECK(bitmap_get(bm, 11) == 0, "bitmap_get(11) == 0");
    CHECK(bitmap_count(bm, 0, 99) == 1, "bitmap_count == 1");
    free_bitmap(bm);
    printf("  BITMAP 基础功能：通过\n\n");
}

static void test_table(void)
{
    printf("=== TABLE ===\n");
    char b[256];

    int types[2] = { BIGNUM_TYPE_NUMBER, BIGNUM_TYPE_STRING };
    mstring names[2];
    names[0] = mstr("id");
    names[1] = mstr("name");
    TABLE *t = create_table(types, names, 2, mstr("users"));
    CHECK(t != NULL, "create_table");

    Obj row[2];
    row[0] = num("1");
    row[1] = str("alice");
    CHECK(add_record(t, row, 2) == 0, "add_record");
    CHECK(get_record_count(t) == 1, "1 row");
    CHECK(get_field_count(t) == 2, "2 fields");

    /* 按列名取字段下标（此前该函数因误读 mstring 头而永远失败） */
    size_t ci = get_field_index(t, "name", 4);
    CHECK(ci == 1, "get_field_index(name) == 1");

    CHECK(strcmp(txt(get_value(t, 0, 0), b, sizeof b), "1") == 0, "t[0][id] == 1");
    CHECK(strcmp(txt(get_value(t, 0, 1), b, sizeof b), "\"alice\"") == 0, "t[0][name] == alice");

    /* 未赋值区域必须是空单元格，不能是野指针 */
    Obj extra = num("2");
    CHECK(add_record(t, &extra, 1) == 0, "add_record partial");
    CHECK(get_value(t, 1, 1) == NULL, "t[1][name] 空单元格为 NULL");

    /* 深拷贝整张表 */
    TABLE *cp = table_copy(t);
    CHECK(cp != NULL, "table_copy");
    CHECK(get_record_count(cp) == 2, "copy has 2 rows");
    Obj ov = get_value(t, 0, 1);
    Obj cv = get_value(cp, 0, 1);
    CHECK(ov != cv, "深拷贝：单元格是不同对象");

    /* 删中间行（走行交换路径），然后释放 */
    CHECK(rm_record(t, 0) == 0, "rm_record(0)");
    CHECK(get_record_count(t) == 1, "1 row after rm");

    free_table(cp);
    free_table(t);
    printf("  TABLE 基础功能与深拷贝：通过\n\n");
}

int main(void)
{
    printf("========================================\n");
    printf("Mhuixs lib/ 数据结构测试\n");
    printf("========================================\n\n");

    test_list();
    test_nested_list();
    test_bitmap();
    test_table();

    printf("========================================\n");
    if (fails == 0) printf("全部通过\n");
    else            printf("%d 项失败\n", fails);
    printf("========================================\n");

    return fails;
}
