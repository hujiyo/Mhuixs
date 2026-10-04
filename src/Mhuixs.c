/*
#版权所有 (c) Mhuixs-team 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2024.11
Email:hj18914255909@outlook.com
*/
/*
Mhuixs 内核入口

最初的设计（2024.11）：
    内存数据结构库 + HOOK 统一引用 + 权限等级。
    数据是一片蓝海，HOOK 是鱼钩，各种数据结构是不同类型的鱼；
    任何能被 HOOK 引用的东西，才叫数据结构；
    数据结构之间可以通过索引（也可以是 HOOK）互相引用。

本文件目前只做三件事：
    1. 按顺序初始化各内核模块
    2. 从磁盘恢复已注册的 HOOK
    3. 跑一遍自检，确认"数据结构 -> HOOK -> 注册表"主链路是通的

说明：Logex 语言层、网络层、用户组层已在本次减重中剥离。
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define _MHUIXS_ /* Mhuixs 服务端标志宏 */

#include "merr.h"      /* 错误码与日志 */
#include "env.h"       /* 环境变量模块 */
#include "getid.h"     /* ID 分配器模块 */
#include "hook.h"      /* HOOK 模块 */
#include "mstring.h"   /* 字符串模块 */
#include "registry.h"  /* 注册表模块 */

/*
存储在 Mhuixs 数据库的所有数据结构都需要使用钩子进行引用：
每定义一个新的数据结构，注册表中就会自动添加一个钩子。
作用：
1. 防止用户忘记钩子名称后，一块数据占着内存却无法访问；
2. 让权限控制统一收口在钩子上；
3. 数据压缩与落盘的基本单位都是 hook。
*/

#define SELFCHECK_HOOK_NAME "mhuixs_selfcheck"

/* ------------------------------------------------------------------ */
/* 自检：把"数据结构 -> HOOK -> 注册表 -> 查找"这条主链路跑一遍           */
/* ------------------------------------------------------------------ */
static int self_check(void)
{
    int failures = 0;

    /* 1. 任意精度数值（BHS） */
    BHS *a = bignum_from_string("100");
    BHS *b = bignum_from_string("200");
    BHS *c = (a && b) ? bignum_add(a, b) : NULL;

    if (c) {
        char buf[64];
        buf[0] = '\0';
        bignum_to_string(c, buf, sizeof(buf), 0);
        printf("  [1] 任意精度数值     100 + 200 = %s\n", buf);
        bignum_destroy(c);
    } else {
        printf("  [1] 任意精度数值     失败\n");
        failures++;
    }
    if (a) bignum_destroy(a);
    if (b) bignum_destroy(b);

    /* 2. 注册一个 HOOK（先清掉上次运行留下的同名项，保证自检可重复） */
    reg_unregister_hook(SELFCHECK_HOOK_NAME);

    HOOK *h = NULL;
    int r = reg_register_hook(0 /* root */, SELFCHECK_HOOK_NAME, &h);
    if (r == 0 && h) {
        printf("  [2] HOOK 注册        '%s' 注册成功（当前 HOOK 数=%d）\n",
               SELFCHECK_HOOK_NAME, reg_get_hook_count());
    } else {
        printf("  [2] HOOK 注册        失败 (ret=%d)\n", r);
        failures++;
    }

    /* 3. 按名字查找 HOOK */
    HOOK *found = reg_find_hook(SELFCHECK_HOOK_NAME);
    if (found) {
        printf("  [3] HOOK 查找        '%s' 找到，名字=%s\n",
               SELFCHECK_HOOK_NAME, mstr_cstr(found->name));
    } else {
        printf("  [3] HOOK 查找        失败\n");
        failures++;
    }

    /* 4. 重名保护 */
    HOOK *dup = NULL;
    r = reg_register_hook(0, SELFCHECK_HOOK_NAME, &dup);
    if (r == 1) {
        printf("  [4] 重名保护         重复注册被拒绝 (ret=1)\n");
    } else {
        printf("  [4] 重名保护         异常 (ret=%d，期望 1)\n", r);
        failures++;
    }

    /* 5. 权限检查：root 恒真；其他用户取决于权限位是否初始化 */
    if (found) {
        printf("  [5] 权限检查         root 可读=%d，其他用户可读=%d\n",
               is_entitled(found, 0, HOOK_READ),
               is_entitled(found, 12345, HOOK_READ));
    }

    return failures;
}

/* 拼出 <MhuixsHomePath>/registry.dat 的完整路径，调用方负责 mstr_free */
static char *make_registry_path(void)
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

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    printf("==== Mhuixs 内核启动 ====\n");

    /* 环境变量模块 */
    if (env_init() != 0) {
        printf("ENV 模块初始化失败\n");
        return 1;
    }
    printf("  ENV 模块就绪\n");

    /* 日志模块 */
    {
        char *log_path = mstr_to_cstr(Env.MhuixsHomePath);
        if (logger_init(log_path) != 0) {
            printf("  Logger 模块未能初始化（日志功能禁用）\n");
        } else {
            printf("  Logger 模块就绪\n");
        }
        free(log_path);
    }

    /* ID 分配器模块 */
    if (idalloc_init() != success) {
        printf("ID 分配器初始化失败\n");
        return 1;
    }
    printf("  ID 分配器就绪\n");

    /* 注册表模块（统一管理 HOOK 与权限） */
    if (reg_init() != 0) {
        printf("注册表初始化失败\n");
        return 1;
    }
    printf("  注册表就绪\n");

    /* 从磁盘恢复 HOOK */
    {
        char *reg_path = make_registry_path();
        int loaded = reg_load_from_disk(reg_path);
        if (loaded > 0) {
            printf("  从磁盘恢复 %d 个 HOOK\n", loaded);
        } else if (loaded == 0) {
            printf("  无已持久化的 HOOK（首次运行或数据为空）\n");
        } else {
            printf("  从磁盘恢复 HOOK 失败\n");
        }
        free(reg_path);
    }

    /* 主链路自检 */
    printf("\n---- 主链路自检 ----\n");
    int failures = self_check();

    /* 保存 HOOK 到磁盘 */
    {
        char *reg_path = make_registry_path();
        if (reg_save_to_disk(reg_path) == 0) {
            printf("\n  已保存 %d 个 HOOK 到磁盘\n", reg_get_hook_count());
        } else {
            printf("\n  保存 HOOK 到磁盘失败\n");
        }
        free(reg_path);
    }

    printf("\n==== 自检结果：%s ====\n",
           failures == 0 ? "全部通过" : "有失败项");

    reg_destroy();
    return failures == 0 ? 0 : 1;
}
