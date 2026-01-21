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
 *   - 自动检测程序所在目录并加载 Mhuixs.config
 *   - 跨平台支持: Windows (GetModuleFileName) 和 Linux (/proc/self/exe)
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
    fprintf(stderr, "%s: %s\n", warnmsg, value);
    return NULL;
}

// 获取当前程序所在目录，拼接配置文件名（使用 mstring）
static mstring get_config_path() {
    char exe_path[ENV_PATH_MAX];
#ifdef _WIN32
    DWORD len = GetModuleFileNameA(NULL, exe_path, ENV_PATH_MAX);
    if (len == 0 || len >= ENV_PATH_MAX) return mstr((char*)"Mhuixs.config");
    char* last_slash = strrchr(exe_path, '\\');
    if (!last_slash) last_slash = strrchr(exe_path, '/');
    if (!last_slash) return mstr((char*)"Mhuixs.config");
    *last_slash = '\0';
    mstring dir = mstr(exe_path);
    mstring separator = mstr((char*)"\\");
#else
    ssize_t len = readlink("/proc/self/exe", exe_path, ENV_PATH_MAX - 1);
    if (len <= 0 || len >= ENV_PATH_MAX) return mstr((char*)"Mhuixs.config");
    exe_path[len] = '\0';
    char* last_slash = strrchr(exe_path, '/');
    if (!last_slash) return mstr((char*)"Mhuixs.config");
    *last_slash = '\0';
    mstring dir = mstr(exe_path);
    mstring separator = mstr((char*)"/");
#endif
    mstring config_name = mstr((char*)"Mhuixs.config");
    mstring dir_with_sep = mstr_concat(dir, separator);
    mstring result = mstr_concat(dir_with_sep, config_name);
    mstr_free(dir);
    mstr_free(separator);
    mstr_free(config_name);
    mstr_free(dir_with_sep);
    return result;
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
    mstring config_path = get_config_path();
    char* config_path_cstr = mstr_to_cstr(config_path);
    FILE* fp = fopen(config_path_cstr, "r");
    free(config_path_cstr);
    if (!fp) {
        const char* path = mstr_cstr(config_path);
        fprintf(stderr, "[env] 配置文件不存在: %.*s\n", (int)mstrlen(config_path), path);
        mstr_free(config_path);
        return 1;
    }
    mstring MhuixsHomePath = NULL;
    int threadslimit = 0, memmorylimit = 0, disablecompression = 0;
    size_t max_sessions = 1024;
    int port = 18185;
    char line[512];
    int found_datapath = 0, found_threadslimit = 0, found_memmorylimit = 0, found_max_sessions = 0, found_disablecompression = 0, found_port = 0;
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
            MhuixsHomePath = parse_path(value, "[env] 数据目录不可用");
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
        const char* path = MhuixsHomePath ? mstr_cstr(MhuixsHomePath) : "";
        fprintf(stderr, "[env] 数据目录不可用: %.*s\n", (int)(MhuixsHomePath ? mstrlen(MhuixsHomePath) : 0), path);
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
