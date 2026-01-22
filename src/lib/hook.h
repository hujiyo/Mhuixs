#ifndef HOOK_H
#define HOOK_H
/*
#版权所有 (c) Mhuixs-team 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2025.1
Email:hj18914255909@outlook.com
*/
#include <string.h>

#include "merr.h"
#include "getid.h"
#include "mstring.h"
#include "usergroup.h"
#include "bignum.h"

/* 前向声明，避免循环包含 */

/* HOOK的操作类型 */
typedef enum {
    HOOK_READ = 'r',
    HOOK_ADD = 'a',
    HOOK_CHANGE = 'c',
} Mode_type;

/*
hook在Mhuixs中被用来：
1.链接所有"需要有权限功能"的独立"数据结构"
2.在一种数据结构中引用独立于自己的另一个数据结构
3.分为有权HOOK和无权HOOK
*/
/*
可读：包括hook的可见性，如果用户没有权限，则hook不可见
*/

/* 权限结构体 */
typedef struct permission_struct {
    char ifisinit;        /* 是否初始化:一旦初始化权限就生效 */

    /* 权限:可读:r 可添:a 可改:c (类比Linux的rwx权限系统) */
    /* 所有者权限 */
    char owner_read;      /* 所有者可读 */
    char owner_add;       /* 所有者可添内容 */
    char owner_change;    /* 所有者可改内容 */
    
    /* 组权限 */
    char group_read;      /* 组可读 */
    char group_add;       /* 组可添内容 */
    char group_change;    /* 组可改内容 */
    
    /* 其他权限 */
    char other_read;      /* 其他可读 */
    char other_add;       /* 其他可添内容 */
    char other_change;    /* 其他可改内容 */
} permission_struct;

/* HOOK 结构体 */
typedef struct HOOK {
    Obj obj;              /* 操作对象 */
    UID owner;            /* 所有者ID */
    GID group;            /* 组ID */
    mstring name;         /* 钩子名 */
    permission_struct pm_s; /* 权限结构体 */
} HOOK;

/* obj_type 枚举（如果未在其他地方定义） */
typedef enum {
    OBJ_TYPE_NULL = BIGNUM_TYPE_NULL,
    OBJ_TYPE_NUMBER = BIGNUM_TYPE_NUMBER,
    OBJ_TYPE_STRING = BIGNUM_TYPE_STRING,
    OBJ_TYPE_BITMAP = BIGNUM_TYPE_BITMAP,
    OBJ_TYPE_LIST = BIGNUM_TYPE_LIST,
    OBJ_TYPE_TABLE = BIGNUM_TYPE_TABLE,
    OBJ_TYPE_KVALOT = BIGNUM_TYPE_KVALOT,
    OBJ_TYPE_HOOK = BIGNUM_TYPE_HOOK,
    OBJ_TYPE_KEY = BIGNUM_TYPE_KEY,
} obj_type;

/* HOOK 函数声明 */
HOOK* HOOK_login(UID owner, mstring name, Obj obj); /* bhs必须是堆分配的！或者为NULL */
int HOOK_logout(HOOK* hook);
int hook_new_obj(HOOK* hook, UID caller, obj_type objtype, void *parameter1, void *parameter2, void *parameter3); /* 用钩子建立一个新对象 */
void hook_reset_pm(HOOK* hook, UID caller, const char* pm_str); /* 设置组和权限 */

#endif
