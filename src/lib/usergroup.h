#ifndef USERGROUP_H
#define USERGROUP_H
/*
#版权所有 (c) HUJI 2025
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2025.5
Email:hj18914255909@outlook.com
*/

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "getid.h"
#include "env.h"
#include "bcrypt.h"
#include "merr.h"
#include "hash.h"

/* 前向声明，避免循环包含 */
typedef struct HOOK HOOK;
typedef enum Mode_type Mode_type;

/* 声明 auth_session 函数 */
int auth_session(SID session_id, UID uid);

#ifdef __cplusplus
extern "C" {
#endif

/* 错误码定义 */
typedef enum {
    UG_SUCCESS = 0,
    UG_ERR_USER_EXISTS = -100,
    UG_ERR_USER_NOT_FOUND = -101,
    UG_ERR_GROUP_EXISTS = -102,
    UG_ERR_GROUP_NOT_FOUND = -103,
    UG_ERR_INVALID_USERNAME = -104,
    UG_ERR_INVALID_PASSWORD = -105,
    UG_ERR_NO_MEMORY = -106,
    UG_ERR_IO_ERROR = -107,
    UG_ERR_PERMISSION_DENIED = -108,
    UG_ERR_UID_EXHAUSTED = -109,
    UG_ERR_GID_EXHAUSTED = -110,
    UG_ERR_INVALID_INPUT = -111,
    UG_ERR_SAVE_FAILED = -112,
} ug_error_t;

/* 兼容旧定义 */
#define no_such_username UG_ERR_USER_NOT_FOUND
#define certificate_failed -2
#define certificate_success 0

/* 用户信息结构体 */
typedef struct {
    UID uid;                    /* 用户ID */
    char username[64];          /* 用户名 */
    char password[BCRYPT_HASHSIZE]; /* 密码(哈希后) */
    GID *groups;                /* 所属组列表（第一个为主组，后续为附加组） */
    uint32_t num_groups;        /* 所属组数量 */
    uint32_t groups_capacity;   /* groups数组容量 */
    char main_hook[64];         /* 主钩子名 */
    char description[256];      /* 用户描述 */
} user_info_t;

/* 组信息结构体 */
typedef struct {
    GID gid;                    /* 组ID */
    char groupname[64];         /* 组名 */
    UID *members;               /* 成员列表 */
    uint32_t num;               /* 成员数量 */
    uint32_t members_capacity;  /* members数组容量 */
} group_info_t;

/* 用户组管理器结构体 */
typedef struct {
    user_info_t *users;         /* 用户列表 */
    uint32_t num_users;         /* 用户数量 */
    uint32_t users_capacity;    /* users数组容量 */
    
    group_info_t *groups;       /* 组列表 */
    uint32_t num_groups;        /* 组数量 */
    uint32_t groups_capacity;   /* groups数组容量 */
    
    /* 哈希表索引 - O(1) 查找 */
    hash_table_t *username_to_idx;  /* username -> user index */
    hash_table_t *uid_to_idx;       /* UID -> user index */
    hash_table_t *groupname_to_idx; /* groupname -> group index */
    hash_table_t *gid_to_idx;       /* GID -> group index */
} user_group_manager_t;

/* 全局用户组管理器 */
extern user_group_manager_t Ugmanager;

/* 初始化用户组管理器 */
int init_User_group_manager(void);

/* 清理用户组管理器 */
void cleanup_User_group_manager(void);

/* 保存用户组管理器到文件 */
int save_User_group_manager(void);

/* 获取错误消息 */
const char* ug_strerror(int error_code);

/* 用户管理 */
int add_user(const char *username, const char *passwd);
int del_user(const char *username);
int set_user_password(const char *username, const char *old_passwd, const char *new_passwd);

/* 组管理 */
int add_group(const char *groupname);
int del_group(const char *groupname);

/* 用户组关系管理 */
int add_user_to_group(UID uid, GID gid);
int del_user_from_group(UID uid, GID gid);

/* 查询函数 */
UID get_uid_by_username(const char *username);
GID get_gid_by_groupname(const char *groupname);
GID get_primary_gid_by_uid(UID uid);
int get_gids_by_uid(UID uid, GID **gids, uint32_t *num_gids);

/* UID合法性检查 */
int is_valid_uid(UID uid);

/* 权限检查 */
int is_entitled(HOOK *hook, UID applicant_uid, Mode_type mode);

/* 认证 */
int certification(SID session_id, const char *username_to_be_verified, 
                  const char *passwd_to_be_verified);

/*
用户、用户组及权限信息通过特定文件存储

[用户信息存储]
文件路径：%MhuixsHomePath%/etc/user.config
数据格式：每行一个用户，字段以冒号（:）分隔，共6个字段：
username:UID:GID:description:main_hook:password

username:用户名
UID：用户ID
GID：所属组ID(可以多个GID,用','隔开)
description：用户描述
main_hook：主钩子
password：用户密码（哈希值（数字明文保存））

例如:
root:0:0:root user:root hook:1949

[用户组信息存储]
文件路径：%MhuixsHomePath%/etc/group.config
数据格式：每行一个组，字段以冒号分隔，共2个字段：
groupname:GID

示例：
developers:1001
*/

#ifdef __cplusplus
}
#endif

#endif /* USERGROUP_H */
