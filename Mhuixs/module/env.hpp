#ifndef ENV_HPP
#define ENV_HPP

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/sysinfo.h>

#include <string>
#include <cctype>
#include <sstream>
#include <iostream>
#include <stdexcept>

using namespace std;

/*
====================
Mhuixs 环境变量模块
====================
本模块用于全局唯一地管理程序运行时的环境参数，自动从程序目录下的 Mhuixs.config 文件读取配置。

主要功能：
- 读取数据库数据目录、线程数、内存限制等参数
- 提供全局唯一的 env 变量，任何文件只需 include 本头文件即可访问
- 只需调用一次 env_init()，即可自动完成配置加载和校验
*/

#define ENV_PATH_MAX 256

//字段：
struct ENV {
    string MhuixsHomePath;// Mhuixs内部储存位置
    int threadslimit;// 线程数量限制
    int memmorylimit;// 内存限制
    size_t max_sessions;// 最大会话数限制
    int disablecompression;// 禁用压缩标志
};


extern ENV Env;// 全局唯一环境配置结构体

int env_init();//返回0成功，非0失败


#endif