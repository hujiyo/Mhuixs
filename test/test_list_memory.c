/*
 * LIST 内存回收检查（手动运行；仅 Windows —— 依赖 psapi 读取进程工作集）
 *
 * 用途：反复创建 / 填充 / 释放 LIST，观察进程工作集变化。
 * 若 list_clear 忘记销毁元素，这里会看到工作集持续增长。
 *
 * 编译运行：
 *   cd src
 *   gcc -O2 -std=c99 -I. -Ilib -o run_memcheck ../test/test_list_memory.c \
 *       lib/list.c lib/bignum.c lib/bitmap.c lib/bitcpy.c lib/tblh.c lib/kvalh.c \
 *       -lm -lpsapi
 *   ./run_memcheck
 *
 * 实测（2026-10-05，80 万次分配）：
 *   修复前：A) push+free_list +61.4 MB   B) +list_copy 深拷贝 +122.6 MB
 *   修复后：A) +0.1 MB                   B) +0.0 MB
 *
 * 不纳入 make test：它较慢，且 psapi 是 Windows 专有。
 */
#include <stdio.h>
#include <windows.h>
#include <psapi.h>

#include "list.h"
#include "bignum.h"

static SIZE_T ws(void) {
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
        return pmc.WorkingSetSize;
    return 0;
}

#define ITERS  4000
#define ELEMS  200

int main(void) {
    SIZE_T before, after;
    printf("iters=%d elems=%d  total allocs=%d\n", ITERS, ELEMS, ITERS * ELEMS);

    /* ---- 场景A：push 后直接 free_list ---- */
    before = ws();
    for (int i = 0; i < ITERS; i++) {
        LIST *l = list_create();
        for (int j = 0; j < ELEMS; j++) {
            Obj v = bignum_from_string("123456");
            if (list_rpush(l, v) != 0) bignum_destroy(v);
        }
        free_list(l);
    }
    after = ws();
    printf("A) push+free_list      工作集 %6.1f MB -> %6.1f MB  (delta %+6.1f MB)\n",
           before / 1048576.0, after / 1048576.0, (double)((long long)after - (long long)before) / 1048576.0);

    /* ---- 场景B：走 list_copy 深拷贝 ---- */
    before = ws();
    for (int i = 0; i < ITERS; i++) {
        LIST *l = list_create();
        for (int j = 0; j < ELEMS; j++) {
            Obj v = bignum_from_string("123456");
            if (list_rpush(l, v) != 0) bignum_destroy(v);
        }
        LIST *c = list_copy(l);       /* 深拷贝，元素加倍 */
        free_list(c);
        free_list(l);
    }
    after = ws();
    printf("B) +list_copy 深拷贝    工作集 %6.1f MB -> %6.1f MB  (delta %+6.1f MB)\n",
           before / 1048576.0, after / 1048576.0, (double)((long long)after - (long long)before) / 1048576.0);

    /* ---- 场景C：pop 拿回所有权并销毁 ---- */
    before = ws();
    for (int i = 0; i < ITERS; i++) {
        LIST *l = list_create();
        for (int j = 0; j < ELEMS; j++) {
            Obj v = bignum_from_string("123456");
            if (list_rpush(l, v) != 0) bignum_destroy(v);
        }
        while (list_size(l) > 0) {
            Obj v = list_rpop(l);
            if (v != (Obj)(intptr_t)-1) bignum_destroy(v);   /* 所有权拿回来，自己销毁 */
        }
        free_list(l);
    }
    after = ws();
    printf("C) pop+destroy         工作集 %6.1f MB -> %6.1f MB  (delta %+6.1f MB)\n",
           before / 1048576.0, after / 1048576.0, (double)((long long)after - (long long)before) / 1048576.0);

    printf("\n说明：若 delta 持续为大正数（数百 MB），说明元素未被回收。\n");
    return 0;
}
