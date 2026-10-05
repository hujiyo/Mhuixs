/* SPDX-License-Identifier: Apache-2.0 */
/*
 * env.c - Mhuixs 环境变量管理模块实现
 *
 * 本文件实现了环境配置的加载、解析和校验功能。支持从配置文件
 * 读取运行时参数,并进行合法性检查和默认值处理。
 *
 * Copyright (C) 2024-2025 Mhuixs Project
 * Author: hujiyo <hj18914255909@outlook.com>
 *
 * 实现细节:
 *   - 配置文件格式: 键值对,空格分隔,支持注释行(#开头)
 *   - 配置文件查找顺序:
 *       1. <可执行文件目录>/Mhuixs.config   部署时可在此覆盖
 *       2. ~/.mhuixs/Mhuixs.config          用户配置；不存在则自动生成
 *     配置文件是【机器相关】的（数据目录、端口、内存上限），不随仓库分发。
 *   - 数据目录不存在时自动递归创建（数据库首次运行不该失败）
 *   - 跨平台支持: Windows (GetModuleFileName / USERPROFILE)
 *                 和 Linux (/proc/self/exe / $HOME)
 *   - 内存限制自动适配系统物理内存 (默认75%,最高90%)
 *   - 所有参数均有合理的默认值和范围检查
 *
 * 配置项说明:
 *   MhuixsHomePath    - 数据存储目录路径 (必需)
 *   threadslimit      - 线程池大小 (2-1024,默认2)
 *   memmorylimit      - 内存限制/MB (64-系统90%,默认系统75%)
 *   max_sessions      - 最大并发会话数 (默认1024)
 *   disablecompression - 是否禁用压缩 (0/1,默认0)
 *   islittleendian    - 是否是小端 (0/1,默认1)
 *   port              - 端口号 (1-65535,默认18185)
 */

#include "env.h"
#include <ctype.h>
#include <errno.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <direct.h>      /* _mkdir */
#undef WIN32_LEAN_AND_MEAN
#else
#include <pwd.h>         /* getpwuid：$HOME 取不到时的兜底 */
#endif

struct ENV Env = {NULL, 0, 0, 1024, 0, 1, 18185};

// 解析整数值，带范围检查
static int parse_int(const char* value, int min_val, int max_val, int default_val, const char* error_msg) {
    int result = atoi(value);
    if (result < min_val || result > max_val) {
        fprintf(stderr, "%s\n", error_msg);
        return default_val;
    }
    return result;
}

// 获取系统物理内存（MB）
static int get_system_memory_mb() {
#ifdef _WIN32
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        return (int)(memInfo.ullTotalPhys / (1024 * 1024));
    }
#else
    struct sysinfo info;
    if (sysinfo(&info) == 0) {
        return (int)(info.totalram * info.mem_unit / (1024 * 1024));
    }
#endif
    fprintf(stderr, "[env] 获取系统内存信息失败，使用默认值4096MB。\n");
    return 4096;
}

static int check_dir_available(const char* path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

// 校验目录存在性
static mstring parse_path(const char* value, const char* warnmsg) {
    if (check_dir_available(value)) return mstr((char*)value);
    if (warnmsg) fprintf(stderr, "%s: %s\n", warnmsg, value);
    return NULL;
}

/* 判断普通文件是否存在 */
static int file_exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

/* 递归创建目录，等价于 mkdir -p。已存在视为成功。 */
static int make_dirs(const char* path) {
    char tmp[ENV_PATH_MAX];
    size_t len;

    if (!path || !path[0]) return -1;
    len = strlen(path);
    if (len >= sizeof(tmp)) return -1;
    memcpy(tmp, path, len + 1);

    for (char* p = tmp + 1; *p; p++) {
        if (*p != '/' && *p != '\\') continue;
#ifdef _WIN32
        /* 跳过盘符，如 "C:" —— 否则会把 "C:" 当成一级目录去创建 */
        if (p == tmp + 2 && tmp[1] == ':') continue;
#endif
        char sep = *p;
        *p = '\0';
        if (!check_dir_available(tmp)) {
#ifdef _WIN32
            if (_mkdir(tmp) != 0 && errno != EEXIST) return -1;
#else
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) return -1;
#endif
        }
        *p = sep;
    }

    if (!check_dir_available(tmp)) {
#ifdef _WIN32
        if (_mkdir(tmp) != 0 && errno != EEXIST) return -1;
#else
        if (mkdir(tmp, 0755) != 0 && errno != EEXIST) return -1;
#endif
    }
    return check_dir_available(tmp) ? 0 : -1;
}

/* 取用户家目录（不带结尾分隔符）。Windows 和 Linux 都有家目录，只是取法不同。 */
static int get_home_dir(char* buf, size_t buflen) {
#ifdef _WIN32
    const char* up = getenv("USERPROFILE");
    if (up && up[0]) { snprintf(buf, buflen, "%s", up); return 0; }
    const char* hd = getenv("HOMEDRIVE");
    const char* hp = getenv("HOMEPATH");
    if (hd && hp && hd[0] && hp[0]) { snprintf(buf, buflen, "%s%s", hd, hp); return 0; }
#else
    const char* hm = getenv("HOME");
    if (hm && hm[0]) { snprintf(buf, buflen, "%s", hm); return 0; }
    {
        struct passwd* pw = getpwuid(getuid());
        if (pw && pw->pw_dir && pw->pw_dir[0]) {
            snprintf(buf, buflen, "%s", pw->pw_dir);
            return 0;
        }
    }
#endif
    return -1;
}

/* 生成一份默认配置。内容与仓库里的 Mhuixs.config.example 对应。 */
static int write_default_config(const char* path, const char* data_dir) {
    FILE* fp = fopen(path, "w");
    if (!fp) return -1;

    fprintf(fp,
        "# Mhuixs 配置（首次运行时自动生成）\n"
        "# 本文件放在用户目录下，是【机器相关】的，不要提交进仓库。\n"
        "# 可以自行修改；删掉它，下次启动会重新生成一份默认的。\n"
        "\n"
        "# 数据存储目录（不存在会自动创建）\n"
        "MhuixsHomePath %s\n"
        "\n"
        "# 线程数限制 (2-1024)\n"
        "threadslimit 4\n"
        "\n"
        "# 内存限制 (MB)\n"
        "memmorylimit 1024\n"
        "\n"
        "# 最大会话数\n"
        "max_sessions 1024\n"
        "\n"
        "# 禁用压缩 (0=启用, 1=禁用)\n"
        "disablecompression 0\n"
        "\n"
        "# 服务端口\n"
        "port 18185\n",
        data_dir);

    fclose(fp);
    return 0;
}

/* 取可执行文件所在目录 */
static int get_exe_dir(char* buf, size_t buflen) {
#ifdef _WIN32
    DWORD len = GetModuleFileNameA(NULL, buf, (DWORD)buflen);
    if (len == 0 || len >= buflen) return -1;
    char* slash = strrchr(buf, '\\');
    if (!slash) slash = strrchr(buf, '/');
#else
    ssize_t len = readlink("/proc/self/exe", buf, buflen - 1);
    if (len <= 0 || (size_t)len >= buflen) return -1;
    buf[len] = '\0';
    char* slash = strrchr(buf, '/');
#endif
    if (!slash) return -1;
    *slash = '\0';
    return 0;
}

/*
 * 决定这次运行使用哪个配置文件，必要时生成一份默认的。
 *
 * 查找顺序：
 *   1. <可执行文件目录>/Mhuixs.config   部署时可在此放一份覆盖
 *   2. ~/.mhuixs/Mhuixs.config          用户配置；不存在则生成
 *
 * 为什么生成到 ~ 下：配置文件是机器相关的（数据目录、端口、内存上限），
 * 不该提交进仓库。以前仓库里带了一份写死 Windows 路径的配置，
 * 导致 Linux 上开箱不可用 —— 这是这个改动要解决的问题。
 */
static mstring resolve_config_path(void) {
    char path[ENV_PATH_MAX];
    char home[ENV_PATH_MAX];

    /* 1. 可执行文件同目录（部署覆盖用） */
    if (get_exe_dir(path, ENV_PATH_MAX) == 0)
    {
        mstring sep = mstr(
#ifdef _WIN32
            (char*)"\\"
#else
            (char*)"/"
#endif
        );
        mstring dir = mstr(path);
        mstring name = mstr((char*)"Mhuixs.config");
        mstring with_sep = mstr_concat(dir, sep);
        mstring candidate = mstr_concat(with_sep, name);
        char* c = mstr_to_cstr(candidate);
        int found = (c && file_exists(c));
        free(c);
        mstr_free(sep);
        mstr_free(dir);
        mstr_free(name);
        mstr_free(with_sep);
        if (found) return candidate;
        mstr_free(candidate);
    }

    /* 2. 用户目录，不存在则生成 */
    if (get_home_dir(home, sizeof(home)) != 0) {
        /* 家目录都取不到：退回当前目录，至少还能跑起来 */
        fprintf(stderr, "[env] 无法确定用户家目录，配置将放在当前目录\n");
        return mstr((char*)"Mhuixs.config");
    }

    {
        mstring h = mstr(home);
        mstring sep = mstr(
#ifdef _WIN32
            (char*)"\\"
#else
            (char*)"/"
#endif
        );
        mstring dotdir_name = mstr((char*)".mhuixs");
        mstring cfg_name = mstr((char*)"Mhuixs.config");
        mstring data_name = mstr((char*)"data");

        mstring hs = mstr_concat(h, sep);                /* <home>/ */
        mstring dotdir = mstr_concat(hs, dotdir_name);   /* <home>/.mhuixs */
        mstring ds = mstr_concat(dotdir, sep);           /* <home>/.mhuixs/ */
        mstring cfg = mstr_concat(ds, cfg_name);         /* .../Mhuixs.config */
        mstring data = mstr_concat(ds, data_name);       /* .../data */

        char* cfg_c = mstr_to_cstr(cfg);
        int exists = (cfg_c && file_exists(cfg_c));

        if (!exists) {
            char* dotdir_c = mstr_to_cstr(dotdir);
            char* data_c = mstr_to_cstr(data);
            int ok = (dotdir_c && make_dirs(dotdir_c) == 0);
            if (ok && cfg_c) {
                ok = (write_default_config(cfg_c, data_c ? data_c : ".mhuixs/data") == 0);
            }
            if (ok) {
                fprintf(stderr, "[env] 已生成默认配置: %s\n", cfg_c ? cfg_c : "(?)");
            } else {
                fprintf(stderr, "[env] 生成默认配置失败: %s\n", cfg_c ? cfg_c : "(?)");
            }
            free(dotdir_c);
            free(data_c);
        }
        free(cfg_c);

        mstr_free(h);
        mstr_free(sep);
        mstr_free(dotdir_name);
        mstr_free(cfg_name);
        mstr_free(data_name);
        mstr_free(hs);
        mstr_free(dotdir);
        mstr_free(ds);
        mstr_free(data);
        return cfg;
    }
}

uint8_t islittlendian(){
    union {
        uint16_t i;
        uint8_t c;
    } un;
    un.i = 1;
    return un.c; // c 和 i 共用内存，读取 c 即可
}    

// 辅助函数：去除首尾空白
static void trim(char* s) {
    char* start = s;
    char* end = s + strlen(s) - 1;
    while (*start && isspace((unsigned char)*start)) start++;
    while (end > start && isspace((unsigned char)*end)) end--;
    *(end + 1) = '\0';
    if (start != s) {
        memmove(s, start, end - start + 2);
    }
}

// 解析空格分隔的配置文件
int env_init() {
    mstring config_path = resolve_config_path();
    char* config_path_cstr = mstr_to_cstr(config_path);
    FILE* fp = fopen(config_path_cstr, "r");
    if (!fp) {
        fprintf(stderr, "[env] 配置文件不存在: %s\n", config_path_cstr ? config_path_cstr : "(?)");
        free(config_path_cstr);
        mstr_free(config_path);
        return 1;
    }
    free(config_path_cstr);
    mstring MhuixsHomePath = NULL;
    int threadslimit = 0, memmorylimit = 0, disablecompression = 0;
    size_t max_sessions = 1024;
    int port = 18185;
    char line[512];
    int found_datapath = 0, found_threadslimit = 0, found_memmorylimit = 0, found_max_sessions = 0, found_disablecompression = 0, found_port = 0;
    int datapath_key_seen = 0;   /* 配置里是否出现过 MhuixsHomePath 这个键 */
    int sys_mem = get_system_memory_mb();
    int mem_max = sys_mem * 90 / 100;
    int mem_default = sys_mem * 75 / 100;
    while (fgets(line, sizeof(line), fp)) {
        trim(line);
        if (line[0] == '\0' || line[0] == '#') continue;
        char key[128], value[256];
        if (sscanf(line, "%127s %255s", key, value) != 2) continue;
        trim(key);
        trim(value);
        if (strcmp(key, "MhuixsHomePath") == 0) {
            datapath_key_seen = 1;
            /* 先静默检查；目录不存在就自动创建再检查一次。
             * 数据库首次运行时目录本来就不存在，不该因此拒绝启动。 */
            MhuixsHomePath = parse_path(value, NULL);
            if (!MhuixsHomePath) {
                if (make_dirs(value) == 0) {
                    MhuixsHomePath = parse_path(value, "[env] 数据目录不可用");
                } else {
                    fprintf(stderr, "[env] 数据目录不可用且无法创建: %s\n", value);
                }
            }
            found_datapath = (MhuixsHomePath != NULL);
        } else if (strcmp(key, "threadslimit") == 0) {
            threadslimit = parse_int(value, 2, 1024, 2, "[env] 线程数配置非法(<2)");
            found_threadslimit = 1;
        } else if (strcmp(key, "memmorylimit") == 0) {
            memmorylimit = parse_int(value, 64, mem_max, mem_default, "[env] 内存限制配置非法(<64MB)");
            found_memmorylimit = 1;
        } else if (strcmp(key, "max_sessions") == 0) {
            unsigned long sessions = strtoul(value, NULL, 10);
            if (sessions > 0) {
                max_sessions = (size_t)sessions;
                found_max_sessions = 1;
            } else {
                fprintf(stderr, "[env] 最大会话数配置非法(<=0)，将使用默认值1024。\n");
            }
        } else if (strcmp(key, "disablecompression") == 0) {
            int compression_flag = parse_int(value, 0, 1, 0, "[env] 禁用压缩标志配置非法(只能为0或1)");
            disablecompression = compression_flag;
            found_disablecompression = 1;
        } else if (strcmp(key, "port") == 0) {
            port = parse_int(value, 1, 65535, 18185, "[env] 端口号配置非法(1-65535)");
            found_port = 1;
        }
        // 以后可在此添加更多变量解析
    }
    fclose(fp);
    if (!found_datapath) {
        /* 只在"配置里压根没有这个键"时补一条提示。
         * 键存在但目录不可用时，parse_path 已经报过具体路径了；
         * 这里原来又报一次，而此时 MhuixsHomePath 已是 NULL，
         * 于是打印出一行空路径 —— 看起来像内存坏了，实际是重复报错。 */
        if (!datapath_key_seen) {
            fprintf(stderr, "[env] 配置中缺少 MhuixsHomePath\n");
        }
        mstr_free(config_path);
        return 1;
    }
    if (!found_memmorylimit || memmorylimit < 64 || memmorylimit > mem_max) {
        fprintf(stderr, "[env] 内存限制配置不合法或超出系统上限，将采用系统内存的75%%: %dMB\n", mem_default);
        memmorylimit = mem_default;
    }
    if (!found_threadslimit || threadslimit < 2) {
        fprintf(stderr, "[env] 线程数配置不合法(<2)，将采用默认值2。\n");
        threadslimit = 2;
    }
    if (!found_max_sessions) {
        fprintf(stderr, "[env] 最大会话数配置未找到，将使用默认值1024。\n");
    }
    if (!found_disablecompression) {
        fprintf(stderr, "[env] 禁用压缩标志配置未找到，将使用默认值0(启用压缩)。\n");
    }
    if (!found_port) {
        fprintf(stderr, "[env] 端口号配置未找到，将使用默认值18185。\n");
    }
    if (Env.MhuixsHomePath) {
        mstr_free(Env.MhuixsHomePath);
    }
    Env.MhuixsHomePath = MhuixsHomePath;
    Env.threadslimit = threadslimit;
    Env.memmorylimit = memmorylimit;
    Env.max_sessions = max_sessions;
    Env.disablecompression = disablecompression;
    Env.islittleendian = islittlendian();
    Env.port = port;

    mstr_free(config_path);
    return 0;
}
