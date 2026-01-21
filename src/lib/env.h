/* SPDX-License-Identifier: Apache-2.0 */
/*
 * env.h - Mhuixs 环境变量管理模块头文件
 *
 * 本模块提供全局环境配置管理功能,负责从配置文件读取和维护
 * 运行时环境参数,包括数据目录、线程限制、内存限制、是否是小端等。
 *
 * Copyright (C) 2024-2025 Mhuixs Project
 * Author: hujiyo <hj18914255909@outlook.com>
 *
 * Repository: https://github.com/hujiyo/Mhuixs
 *
 * 主要功能:
 *   - 自动从 Mhuixs.config 配置文件加载环境参数
 *   - 提供全局唯一的 Env 结构体供整个程序访问
 *   - 支持跨平台的配置文件路径解析 (Windows/Linux)
 *   - 运行时参数校验和默认值处理
 *
 * 使用方法:
 *   1. 在程序启动时调用 env_init() 初始化环境
 *   2. 通过全局变量 Env 访问配置参数
 */

#ifndef ENV_H
#define ENV_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <stdbool.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/sysinfo.h>
#endif

#include "lib/mstring.h"

#define ENV_PATH_MAX 256

struct ENV {
    mstring MhuixsHomePath;// Mhuixs内部储存位置
    int threadslimit;// 线程数量限制
    int memmorylimit;// 内存限制
    size_t max_sessions;// 最大会话数限制
    int disablecompression;// 禁用压缩标志
    bool islittleendian;//是否是小端 0-否 1-是
    int port;// 端口号
};

// 配置项说明:
//   MhuixsHomePath    - 数据存储目录路径 (必需)
//   threadslimit      - 线程池大小 (2-1024,默认2)
//   memmorylimit      - 内存限制/MB (64-系统90%,默认系统75%)
//   max_sessions      - 最大并发会话数 (默认1024)
//   disablecompression - 是否禁用压缩 (0/1,默认0)
//   islittleendian    - 是否是小端 (0/1,默认0)
//   port              - 端口号 (1-65535,默认18185)

extern struct ENV Env;// 全局唯一环境配置结构体

int env_init();//返回0成功，非0失败

#endif 