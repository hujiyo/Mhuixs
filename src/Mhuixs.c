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
#include "execute.h"   /* 命令执行层 */

/*
存储在 Mhuixs 数据库的所有数据结构都需要使用钩子进行引用：
每定义一个新的数据结构，注册表中就会自动添加一个钩子。
作用：
1. 防止用户忘记钩子名称后，一块数据占着内存却无法访问；
2. 让权限控制统一收口在钩子上；
3. 数据压缩与落盘的基本单位都是 hook。
*/

#define SELFCHECK_HOOK_NAME "mhuixs_selfcheck"

#define MHUIXS_VERSION "0.1.3"

static char *make_registry_path(void);   /* 定义在后面 */

static void print_usage(void)
{
    printf("Mhuixs %s —— 内存数据结构库\n\n", MHUIXS_VERSION);
    printf("用法:\n");
    printf("  mhuixs              进入交互模式（REPL）\n");
    printf("  mhuixs demo         运行主链路自检与命令层演示后退出\n");
    printf("  mhuixs -h|--help    显示本说明\n");
    printf("\n交互模式下输入 :help 查看命令列表，:quit 退出。\n");
}

/* 保存注册表；成功返回保存的 HOOK 数，失败返回 -1 */
static int save_registry(void)
{
    char *path = make_registry_path();
    int rc = reg_save_to_disk(path);
    free(path);
    if (rc != 0) return -1;
    return reg_get_hook_count();
}

/* ------------------------------------------------------------------ */
/* 命令层演示：一行命令，一个动作，不需要语言                            */
/* ------------------------------------------------------------------ */
static void run_command(const char *line)
{
    char out[4096];
    int rc = mhx_execute(line, out, sizeof(out));

    printf("  mhuixs> %s\n", line);

    if (out[0] == '\0') return;

    /* 多行输出逐行缩进，失败行加 !! 标记 */
    const char *prefix = (rc == 0) ? "         " : "      !! ";
    char *p = out;
    while (*p) {
        char *nl = strchr(p, '\n');
        if (nl) *nl = '\0';
        printf("%s%s\n", prefix, p);
        if (!nl) break;
        p = nl + 1;
    }
}

static void demo_commands(void)
{
    /* 保证演示可重复：先摘除上次运行留下的同名钩子 */
    run_command("drop fruits");
    run_command("drop flags");
    run_command("drop cache");
    run_command("drop users");

    /* LIST：一个普通列表 */
    run_command("create list fruits");
    run_command("rpush fruits apple");
    run_command("rpush fruits 100");
    run_command("lpush fruits \"a quoted string\"");
    run_command("llen fruits");
    run_command("lget fruits 0");
    run_command("lget fruits 1");
    run_command("lget fruits 2");
    run_command("lget fruits 9");      /* 越界，应报错 */
    run_command("lset fruits 1 777");  /* 覆盖已有值 */
    run_command("lget fruits 1");
    run_command("lrem fruits 0");      /* 移除并返回该值 */
    run_command("llen fruits");
    run_command("rpop fruits");
    run_command("llen fruits");
    run_command("rpush fruits kiwi");  /* 补回一个，使后面的计数保持为 2 */
    run_command("llen fruits");

    /* BITMAP：一个位图 */
    run_command("create bitmap flags");
    run_command("bset flags 3 1");
    run_command("bset flags 10 1");
    run_command("bset flags 63 1");
    run_command("bcount flags 0 63");
    run_command("bcount flags 0 64");   /* 越界，应给出明确报错而不是一个巨大数字 */
    run_command("bget flags 3");
    run_command("bget flags 4");
    run_command("bsize flags");

    /* 类型检查与重名保护 */
    run_command("type fruits");
    run_command("create list fruits");     /* 重名，应被拒绝 */
    run_command("llen fruits");            /* 且原数据必须还在 */

    /* KVALOT：键值对。注意键一律按字符串处理 */
    run_command("create kvalot cache");
    run_command("kset cache user:1 alice");
    run_command("kset cache user:2 200");
    run_command("kset cache user:1 bob");      /* 同键覆盖 */
    run_command("klen cache");
    run_command("kget cache user:1");
    run_command("kget cache user:2");
    run_command("kexists cache user:3");
    run_command("kdel cache user:2");
    run_command("klen cache");

    /* TABLE：关系表，字段用 <名字>:<类型> 声明 */
    run_command("create table users id:int name:str age:int");
    run_command("tfields users");
    run_command("tadd users 1 alice 25");
    run_command("tadd users 2 bob 30");
    run_command("tadd users 3 carol");         /* 少给一列，age 为空 */
    run_command("trows users");
    run_command("tget users 0 id");
    run_command("tget users 1 name");
    run_command("tget users 2 age");           /* 空单元格 */
    run_command("tset users 0 age 26");        /* 覆盖已有值 */
    run_command("tget users 0 age");
    run_command("tdel users 1");               /* 删中间行 */
    run_command("trows users");
    run_command("tget users 1 name");          /* 删行后第 1 行变成 carol */

    /* 复合对象同样能被 hook 引用 —— 这就是最初那句承诺 */
    run_command("type cache");
    run_command("type users");

    /* 查看内容：hooks 给摘要，info 展开细节 */
    run_command("hooks");
    run_command("info users");
}

/* ------------------------------------------------------------------ */
/* 自检：把"数据结构 -> HOOK -> 注册表 -> 查找"这条主链路跑一遍           */
/* ------------------------------------------------------------------ */
static int self_check(int verbose)
{
    int failures = 0;
#define SC_LOG(...) do { if (verbose) printf(__VA_ARGS__); } while (0)

    /* 1. 任意精度数值（BHS） */
    BHS *a = bignum_from_string("100");
    BHS *b = bignum_from_string("200");
    BHS *c = (a && b) ? bignum_add(a, b) : NULL;

    if (c) {
        char buf[64];
        buf[0] = '\0';
        bignum_to_string(c, buf, sizeof(buf), -1);
        SC_LOG("  [1] 任意精度数值     100 + 200 = %s\n", buf);
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
        SC_LOG("  [2] HOOK 注册        '%s' 注册成功（当前 HOOK 数=%d）\n",
               SELFCHECK_HOOK_NAME, reg_get_hook_count());
    } else {
        printf("  [2] HOOK 注册        失败 (ret=%d)\n", r);
        failures++;
    }

    /* 3. 按名字查找 HOOK */
    HOOK *found = reg_find_hook(SELFCHECK_HOOK_NAME);
    if (found) {
        /* mstr_cstr 不带 \0，配 %s 会读到相邻未初始化内存，故用 %.*s */
        SC_LOG("  [3] HOOK 查找        '%s' 找到，名字=%.*s\n",
               SELFCHECK_HOOK_NAME,
               (int)mstrlen(found->name), mstr_cstr(found->name));
    } else {
        printf("  [3] HOOK 查找        失败\n");
        failures++;
    }

    /* 4. 重名保护 */
    HOOK *dup = NULL;
    r = reg_register_hook(0, SELFCHECK_HOOK_NAME, &dup);
    if (r == 1) {
        SC_LOG("  [4] 重名保护         重复注册被拒绝 (ret=1)\n");
    } else {
        printf("  [4] 重名保护         异常 (ret=%d，期望 1)\n", r);
        failures++;
    }

    /* 5. 权限检查：root 恒真；其他用户取决于权限位是否初始化 */
    if (found) {
        SC_LOG("  [5] 权限检查         root 可读=%d，其他用户可读=%d\n",
               is_entitled(found, 0, HOOK_READ),
               is_entitled(found, 12345, HOOK_READ));
    }

    /* 自检产物不留在数据目录里 */
    reg_unregister_hook(SELFCHECK_HOOK_NAME);

#undef SC_LOG
    return failures;
}

/* ------------------------------------------------------------------ */
/* 交互模式（REPL）                                                     */
/* ------------------------------------------------------------------ */

/* 输出格式。text 是人类可读（历史行为），json 给程序/AI 消费。
 * 用 :format 切换，默认 text。 */
static int g_json_mode = 0;

static void repl(void)
{
    char line[4096];
    /* JSON 是文本的超集（还含 text 字段），转义后可能膨胀，
     * 按 execute.h 的建议给到 128KB（文本模式当年是 16KB）。 */
    char out[131072];

    printf("\n输入命令，:help 查看用法，:quit 退出\n\n");

    for (;;) {
        printf("mhuixs> ");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) {   /* Ctrl-D / 输入结束 */
            printf("\n");
            break;
        }

        /* 去掉行尾换行 */
        size_t n = strlen(line);
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = '\0';

        /* 空白行直接跳过，不回声 */
        int blank = 1;
        for (const char *p = line; *p; p++) {
            if (*p != ' ' && *p != '\t') { blank = 0; break; }
        }
        if (blank) continue;

        /* 元命令（以 : 开头） */
        if (line[0] == ':') {
            if (!strcmp(line, ":quit") || !strcmp(line, ":q") || !strcmp(line, ":exit")) {
                printf("\n");
                break;
            }
            if (!strcmp(line, ":help") || !strcmp(line, ":?")) {
                fputs(mhx_help_text(), stdout);
                continue;
            }
            if (!strcmp(line, ":save")) {
                int k = save_registry();
                if (k < 0) printf("保存失败\n");
                else       printf("已保存 %d 个 HOOK\n", k);
                continue;
            }
            if (!strncmp(line, ":format", 7)) {
                const char *a = line + 7;
                while (*a == ' ' || *a == '\t') a++;
                if      (!strcmp(a, "json")) { g_json_mode = 1; printf("输出格式：json\n"); }
                else if (!strcmp(a, "text")) { g_json_mode = 0; printf("输出格式：text\n"); }
                else printf("用法：:format text|json（当前：%s）\n", g_json_mode ? "json" : "text");
                continue;
            }
            printf("未知的元命令：%s（可用 :help :save :format :quit）\n", line);
            continue;
        }

        if (g_json_mode) {
            /* 成败看返回码 / JSON 的 ok 字段，这里不再靠文本前缀判断 */
            mhx_execute_json(line, out, sizeof(out));
            if (out[0] != '\0') fputs(out, stdout);
            continue;
        }

        mhx_execute(line, out, sizeof(out));
        if (out[0] != '\0') {
            fputs(out, stdout);
            if (out[strlen(out) - 1] != '\n') putchar('\n');
        }
    }
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
    /* 入口分发：无参数 = 交互模式；demo = 自检+演示；-h = 帮助 */
    int mode_demo = 0;
    if (argc > 1) {
        if (!strcmp(argv[1], "demo")) {
            mode_demo = 1;
        } else if (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) {
            print_usage();
            return 0;
        } else {
            fprintf(stderr, "未知参数: %s\n\n", argv[1]);
            print_usage();
            return 2;
        }
    }

    printf("==== Mhuixs %s ====\n", MHUIXS_VERSION);

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
            /* reg_load_from_disk 已把坏文件改名留档，并打印了具体路径。
             * 这里以空注册表继续运行是安全的：退出时保存的是新文件，
             * 不会覆盖那份留档。 */
            printf("  ⚠ 从磁盘恢复 HOOK 失败，已按空注册表启动\n");
            printf("    （原文件已留档为 registry.dat.corrupt，数据未丢失，可手工抢救）\n");
        }
        free(reg_path);
    }

    /* 主链路自检：演示模式打印明细，交互模式静默（只在失败时出声） */
    int failures;
    if (mode_demo) {
        printf("\n---- 主链路自检 ----\n");
        failures = self_check(1);

        printf("\n---- 命令层演示 ----\n");
        demo_commands();

        /*
         * demo 模式【不写盘】。
         *
         * 演示会在内存里创建一批同名钩子（fruits/flags/cache/users 等），
         * 如果保存回去，就会把用户自己同名的数据覆盖掉 ——
         * 跑一次 demo 毁一次数据，而用户只是"想看一眼示例"。
         * 改动只留在内存里，进程退出即丢弃，磁盘上的 registry.dat 保持原样。
         */
        printf("\n  （demo 模式不写入磁盘，你的数据未受影响）\n");

        printf("\n==== 自检结果：%s ====\n",
               failures == 0 ? "全部通过" : "有失败项");

        reg_destroy();
        return failures == 0 ? 0 : 1;
    }

    /* 交互模式 */
    failures = self_check(0);
    if (failures) {
        printf("\n⚠ 主链路自检有 %d 项失败（用 demo 参数可看明细），仍进入交互模式\n",
               failures);
    } else {
        printf("  主链路自检通过\n");
    }

    repl();

    /* 退出时保存 */
    int saved = save_registry();
    if (saved < 0) printf("保存 HOOK 到磁盘失败\n");
    else           printf("已保存 %d 个 HOOK 到磁盘\n", saved);

    reg_destroy();
    return 0;
}
