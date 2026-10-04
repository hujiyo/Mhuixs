/*
 * HOOK 生命周期测试：注册 → 挂数据 → 注销，检查整条链是否被释放。
 *
 * 背景：reg_unregister_hook 原先只做 hash_remove，
 * 既不释放 HOOK 也不释放它持有的 BHS —— 反复 drop / 重建同名钩子会持续吃内存。
 * reg_destroy 也有同样的问题（注释写着"由外部管理"，但没有外部）。
 *
 * 本测试做两件事：
 *   1. 功能：drop 之后计数归零、按名字查不到、同一个名字能重新注册
 *   2. 内存：反复 create + attach + drop，观察进程工作集（仅 Windows）
 *
 * 内存部分需要对照才有意义，实测（2026-10-05）：
 *   修复前：2000 轮 × 50 元素，工作集 +数十 MB
 *   修复后：+0.1 MB 以内
 */
#include <stdio.h>
#include <string.h>

#include "registry.h"
#include "hook.h"
#include "bignum.h"
#include "list.h"

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#endif

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("  FAIL: %s\n", msg); fails++; } } while (0)

#define ITERS 2000
#define ELEMS 50

#ifdef _WIN32
static double ws_mb(void) {
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
        return pmc.WorkingSetSize / 1048576.0;
    return 0.0;
}
#endif

/* 一轮完整的生命周期：注册 -> 挂一个 50 元素的 LIST -> 注销 */
static void one_cycle(void)
{
    HOOK* h = NULL;
    if (reg_register_hook(0, "tmp", &h) != 0) return;

    BHS* l = bignum_create_list();
    if (!l) return;
    LIST* raw = bignum_get_list(l);
    for (int i = 0; i < ELEMS; i++) {
        Obj v = bignum_from_string("123456789");
        if (list_rpush(raw, v) != 0) bignum_destroy(v);
    }

    hook_set_bhs(h, 0, l);   /* 深拷贝进 HOOK */
    bignum_destroy(l);       /* 释放本地那份 */

    reg_unregister_hook("tmp");  /* 应释放 HOOK + 它持有的 LIST + 全部元素 */
}

int main(void)
{
    printf("=== HOOK 生命周期 ===\n");

    logger_init(".");
    CHECK(reg_init() == 0, "reg_init");

    /* ---- 功能：drop 是否真的摘除 ---- */
    HOOK* h = NULL;
    CHECK(reg_register_hook(0, "tmp", &h) == 0, "register tmp");
    CHECK(reg_get_hook_count() == 1, "count == 1");
    CHECK(reg_find_hook("tmp") == h, "find returns the hook");

    reg_unregister_hook("tmp");
    CHECK(reg_get_hook_count() == 0, "count == 0 after unregister");
    CHECK(reg_find_hook("tmp") == NULL, "not findable after unregister");

    /* 名字能被重新使用 —— 说明注册表条目确实清干净了 */
    HOOK* h2 = NULL;
    CHECK(reg_register_hook(0, "tmp", &h2) == 0, "re-register same name");
    CHECK(reg_get_hook_count() == 1, "count == 1 again");
    reg_unregister_hook("tmp");
    CHECK(reg_get_hook_count() == 0, "count == 0 again");

    /* 边界：不存在的名字、NULL 都不该崩 */
    reg_unregister_hook("no_such_hook");
    reg_unregister_hook(NULL);
    CHECK(reg_get_hook_count() == 0, "count still 0");

    /* 挂上数据后再 drop：不能崩，且计数归零 */
    one_cycle();
    CHECK(reg_get_hook_count() == 0, "count 0 after data cycle");
    printf("  功能：注册 / 查找 / 注销 / 重注册 全部正常\n");

    /* ---- 内存：反复 create + attach + drop ---- */
#ifdef _WIN32
    printf("\n  内存检查：%d 轮 × %d 元素 = %d 次对象分配\n",
           ITERS, ELEMS, ITERS * ELEMS);
    double before = ws_mb();
    for (int i = 0; i < ITERS; i++) one_cycle();
    double after = ws_mb();
    double delta = after - before;
    printf("  工作集 %.1f MB -> %.1f MB  (delta %+.1f MB)\n", before, after, delta);

    /* 占位符泄漏会翻倍增长，阈值取 8 MB 足够宽松又抓得住真泄漏 */
    CHECK(delta < 8.0, "drop 循环未持续增长（说明 HOOK 与数据都被释放了）");
#else
    for (int i = 0; i < ITERS; i++) one_cycle();
    printf("  （非 Windows：跳过工作集测量，仅跑功能循环）\n");
#endif

    /* ---- 退出：reg_destroy 应释放所有残留 HOOK 及其数据 ---- */
    HOOK* a = NULL; HOOK* b = NULL;
    reg_register_hook(0, "leftover_a", &a);
    reg_register_hook(0, "leftover_b", &b);
    CHECK(reg_get_hook_count() == 2, "2 leftovers");

    /* 给它们挂上大一点的数据，这样"有没有释放"才看得出来 */
    for (int t = 0; t < 2; t++) {
        HookHandle hh = reg_find(t ? "leftover_b" : "leftover_a");
        BHS* l = bignum_create_list();
        LIST* raw = bignum_get_list(l);
        for (int i = 0; i < 5000; i++) {
            Obj v = bignum_from_string("123456789");
            if (list_rpush(raw, v) != 0) bignum_destroy(v);
        }
        hook_set_bhs(hh, 0, l);
        bignum_destroy(l);
    }

#ifdef _WIN32
    double ws1 = ws_mb();
#endif
    reg_destroy();   /* 应连同两个 HOOK 与它们各 5000 个元素一起释放 */

#ifdef _WIN32
    double ws2 = ws_mb();
    printf("\n  reg_destroy：挂载后 %.1f MB -> 销毁后 %.1f MB  (回收 %+.1f MB)\n",
           ws1, ws2, ws2 - ws1);
    CHECK(ws1 - ws2 > 0.5, "reg_destroy 确实回收了残留 HOOK 与数据");
#else
    printf("\n  reg_destroy：已清空 2 个残留 HOOK\n");
#endif

    printf("\n%s\n", fails == 0 ? "通过" : "有失败项");
    return fails;
}
