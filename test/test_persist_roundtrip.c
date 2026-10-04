/* 临时回归测试：验证 TABLE / KVALOT 挂到 HOOK 上之后的落盘-恢复往返。
 * 放在 /tmp 下，不进仓库。 */
#include <stdio.h>
#include <string.h>

#include "registry.h"
#include "hook.h"
#include "bignum.h"
#include "tblh.h"
#include "kvalh.h"

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("  FAIL: %s\n", msg); fails++; } } while (0)

int main(void)
{
    logger_init(".");
    if (reg_init() != 0) { printf("reg_init failed\n"); return 1; }

    const char *path = "rt_test.dat";

    /* ---- 建一个 kvalot ---- */
    HOOK *hk = NULL;
    CHECK(reg_register_hook(0, "cache", &hk) == 0, "register cache");
    {
        BHS *nb = bignum_from_raw_string("cache");
        KVALOT *kv = kvalot_create(nb);
        bignum_destroy(nb);
        CHECK(kv != NULL, "kvalot_create");

        BHS *k1 = bignum_from_raw_string("user:1");
        BHS *v1 = bignum_from_raw_string("alice");
        CHECK(kvalot_add(kv, k1, v1) == 0, "kvalot_add user:1");
        bignum_destroy(k1);

        BHS *k2 = bignum_from_raw_string("num");
        BHS *v2 = bignum_from_string("12345");
        CHECK(kvalot_add(kv, k2, v2) == 0, "kvalot_add num");
        bignum_destroy(k2);

        BHS *kbhs = bignum_from_kvalot(kv);
        kvalot_destroy(kv);
        CHECK(kbhs != NULL, "bignum_from_kvalot");
        CHECK(hook_set_bhs(hk, 0, kbhs) == 0, "hook_set_bhs kvalot");
        bignum_destroy(kbhs);
    }

    /* ---- 建一张表 ---- */
    HOOK *ht = NULL;
    CHECK(reg_register_hook(0, "users", &ht) == 0, "register users");
    {
        int types[3] = { BIGNUM_TYPE_NUMBER, BIGNUM_TYPE_STRING, BIGNUM_TYPE_NUMBER };
        mstring names[3];
        names[0] = mstr("id");
        names[1] = mstr("name");
        names[2] = mstr("age");
        TABLE *t = create_table(types, names, 3, mstr("users"));
        CHECK(t != NULL, "create_table");

        Obj row1[3];
        row1[0] = bignum_from_string("1");
        row1[1] = bignum_from_raw_string("alice");
        row1[2] = bignum_from_string("25");
        CHECK(add_record(t, row1, 3) == 0, "add_record row1");

        Obj row2[3];
        row2[0] = bignum_from_string("2");
        row2[1] = bignum_from_raw_string("bob");
        row2[2] = NULL;                       /* 空单元格 */
        CHECK(add_record(t, row2, 3) == 0, "add_record row2");

        BHS *tbhs = bignum_from_table(t);
        free_table(t);
        CHECK(tbhs != NULL, "bignum_from_table");
        CHECK(hook_set_bhs(ht, 0, tbhs) == 0, "hook_set_bhs table");
        bignum_destroy(tbhs);
    }

    CHECK(reg_get_hook_count() == 2, "hook count == 2");
    CHECK(reg_save_to_disk(path) == 0, "save_to_disk");
    printf("saved, hook count = %d\n", reg_get_hook_count());

    /* ---- 模拟重启：销毁后重新加载 ---- */
    reg_destroy();
    CHECK(reg_init() == 0, "reg_init #2");

    int n = reg_load_from_disk(path);
    printf("loaded %d hooks\n", n);
    CHECK(n == 2, "loaded 2 hooks");

    /* ---- 校验 kvalot ---- */
    HOOK *hc = reg_find_hook("cache");
    CHECK(hc != NULL, "find cache");
    CHECK(hc && hc->obj != NULL, "cache obj not null");
    CHECK(hc && hc->obj->type == BIGNUM_TYPE_KVALOT, "cache type == KVALOT");
    if (hc && hc->obj) {
        KVALOT *kv = bignum_get_kvalot(hc->obj);
        CHECK(kv != NULL, "bignum_get_kvalot after load");
        if (kv) {
            BHS *qk = bignum_from_raw_string("user:1");
            Obj got = kvalot_find(kv, qk);
            bignum_destroy(qk);
            CHECK(got != NULL, "key user:1 survived");
            if (got) {
                char buf[256]; buf[0] = '\0';
                bignum_to_string(got, buf, sizeof buf, 0);
                printf("  cache[user:1] = %s\n", buf);
                CHECK(strcmp(buf, "\"alice\"") == 0, "cache[user:1] == \"alice\"");
            }
            printf("  cache keys = %u\n", kvalot_size(kv));
            CHECK(kvalot_size(kv) == 2, "cache has 2 keys");
        }
    }

    /* ---- 校验 table ---- */
    HOOK *hu = reg_find_hook("users");
    CHECK(hu != NULL, "find users");
    CHECK(hu && hu->obj != NULL, "users obj not null");
    CHECK(hu && hu->obj->type == BIGNUM_TYPE_TABLE, "users type == TABLE");
    if (hu && hu->obj) {
        TABLE *t = bignum_get_table(hu->obj);
        CHECK(t != NULL, "bignum_get_table after load");
        if (t) {
            printf("  users rows=%zu fields=%zu\n", get_record_count(t), get_field_count(t));
            CHECK(get_record_count(t) == 2, "2 rows");
            CHECK(get_field_count(t) == 3, "3 fields");

            /* 按列名取字段下标（验证 get_field_index 的修复） */
            size_t ci = get_field_index(t, "name", 4);
            CHECK(ci != FIELD_NOT_FOUND, "get_field_index(name)");
            CHECK(ci == 1, "name is column 1");

            char b[256];
            Obj c00 = get_value(t, 0, 0);
            Obj c01 = get_value(t, 0, 1);
            Obj c12 = get_value(t, 1, 2);
            b[0] = '\0'; if (c00) bignum_to_string(c00, b, sizeof b, 0);
            printf("  t[0][id]   = %s\n", b);
            CHECK(c00 && strcmp(b, "1") == 0, "t[0][id] == 1");
            b[0] = '\0'; if (c01) bignum_to_string(c01, b, sizeof b, 0);
            printf("  t[0][name] = %s\n", b);
            CHECK(c01 && strcmp(b, "\"alice\"") == 0, "t[0][name] == \"alice\"");
            printf("  t[1][age]  = %s (空单元格应为 null)\n", c12 ? "non-null" : "null");
            CHECK(c12 == NULL, "t[1][age] stays empty");
        }
    }

    reg_destroy();
    remove(path);

    printf(fails == 0 ? "\nROUNDTRIP OK\n" : "\nROUNDTRIP FAILED (%d)\n", fails);
    return fails == 0 ? 0 : 1;
}
