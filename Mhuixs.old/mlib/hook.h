/*
#版权所有 (c) Mhuixs-team 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2025.1
Email:hj18914255909@outlook.com
*/
#ifndef HOOK_H
#define HOOK_H
#include "Mhudef.h"

typedef struct HOOK{
    void* handle;//指向任意数据结构描述符
    HOOKTYPE type;//描述符类型
    RANK rank;//保护等级
    cprs cprs_stage;//压缩级别
    userid_t owner;//对应所有者ID
    groupid_t group;//对应组ID
    hookid_t hook_id;
    char* name;//狗子名
}HOOK;

HOOK *makeHOOK(uint8_t caller_rank,uint8_t target_rank,str* name);

#endif