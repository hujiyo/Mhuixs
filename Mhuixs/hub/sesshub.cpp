/*
#版权所有 (c) Mhuixs-team 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2024.11
Email:hj18914255909@outlook.com
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "mlib/session.hpp"

/*
会话管理模块(类比操作系统中的进程管理)
负责管理会话，包括会话的创建、销毁、状态管理等

会话管理模块的结构：
1.会话池管理
2.会话状态管理(维护多个不同状态的会话池)
3.会话优先级管理(根据会话任务队列负荷，负荷为元素数*元素处理难度，负荷越大，优先级越高)
(和权限无关，root用户除外，它分为本地登陆和session登陆,session登陆时与普通会话同等对待，本地登陆时不归会话管理模块管理)
*/



typedef struct SESSION_POOL{
    SESSION* session_pool;
    int session_pool_size;
} SESSION_POOL;


