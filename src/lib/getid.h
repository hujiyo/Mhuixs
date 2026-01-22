#ifndef GETID_H
#define GETID_H

#include "bitmap.h"
#include "merr.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
#版权所有 (c) HuJi 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2024.11
Email:hj18914255909@outlook.com
*/

/*
===================================
ID分配器模块 线程安全：所有公有方法均加锁
===================================

会话SID:0-65535,类似linux的PID

用户UID:0-65535,类似linux
    0为root用户ID            ROOT_UID
    1-99为系统用户ID         SYSTEM_UID
    100-65535为普通用户ID     COMMON_UID
    （65536为临时用户ID的特殊标识符,不纳入id管理器管）

组GID：0-65535,类似linux
    0为系统组ID             SYSTEM_GID
    1-65535为普通组ID    COMMON_GID
    （65536为临时用户组ID的特殊标识符,不纳入id管理器管）
*/

typedef int SID, UID, GID;

typedef enum {
    ROOT_UID,     /* root用户ID */
    SYSTEM_UID,   /* 系统用户ID */
    COMMON_UID,   /* 普通用户ID */
} UID_t;

typedef enum {
    SYSTEM_GID,   /* 系统组ID */
    COMMON_GID,   /* 普通组ID */
} GID_t;

/* 初始化和清理 */
int idalloc_init(void);
int idalloc_close(void);

/* 会话ID管理 */
SID get_sid(void);
SID del_sid(SID sid);

/* 用户ID管理 */
UID get_uid(UID_t type);
UID del_uid(UID_t type, UID uid);

/* 组ID管理 */
GID get_gid(GID_t type);
GID del_gid(GID_t type, GID gid);

#ifdef __cplusplus
}
#endif

#endif /* GETID_H */
