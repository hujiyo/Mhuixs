#ifndef REGISTRY_H
#define REGISTRY_H

#include <stdio.h>
#include "merr.h"
#include "env.h"
#include "mstring.h"
#include "hash.h"
#include "hook.h"

#ifndef _WIN32
#include <pthread.h>
#endif

/*
全局注册表模块
集中化管理HOOK的申请、分配、销毁
集中化管理用户权限
*/

/* 注册表结构体 */
typedef struct Registry {
    hash_table_t* hook_map;  /* 存储HOOK名字和索引（使用优化后的 hash.h 实现） */
#ifdef _WIN32
    void* lock;              /* Windows 临界区 (CRITICAL_SECTION*) */
#else
    pthread_mutex_t lock;    /* POSIX 互斥锁 */
#endif
} Registry;

/* 全局注册表实例 */
extern Registry Reg;

/* 初始化注册表 */
int reg_init(void);

/* 销毁注册表 */
void reg_destroy(void);

/* 注册HOOK，0=成功，1=重名失败，负数=其他错误，hook_return是返回的HOOK */
int reg_register_hook(M_UID owner, const char* name, HOOK** hook_return);

/* 注销HOOK：从注册表摘除，并释放 HOOK 及其持有的对象 */
void reg_unregister_hook(const char* name);

/*
 * 按【指针身份】把指定 HOOK 从注册表摘除（只摘除，不释放）。
 * 用于 HOOK_logout：按名字摘除会误删同名的另一条。
 * 本函数自行获取注册表锁。
 */
void reg_detach_hook(HOOK* hook);

/* 查找HOOK */
HOOK* reg_find_hook(const char* name);

/* 判断HOOK是否已注册 */
int reg_is_registered(const char* name);

/* ====== C 接口层 ====== */
/* 供 Logex（纯 C）调用的接口 */

/* 不透明句柄类型 */
typedef void* HookHandle;

/* 注册表操作 */
int reg_register(uint64_t owner, const char *name, HookHandle *out_hook);
void reg_unregister(const char *name);
HookHandle reg_find(const char *name);
int hook_reg_exists(const char *name);

/* HOOK 操作 */
int hook_new(HookHandle hook, uint64_t caller, int objtype, void *p1, void *p2, void *p3);
int hook_set_permission(HookHandle hook, uint64_t caller, const char *pm_str);

/* BHS 操作（用于 static let 持久化变量） */
int hook_set_bhs(HookHandle hook, uint64_t caller, void *bhs);
void* hook_get_bhs(HookHandle hook, uint64_t caller);

/* 持久化功能 */
int reg_save_to_disk(const char *path);
int reg_load_from_disk(const char *path);
int reg_get_hook_count(void);

/* BHS 序列化/反序列化（供内部和外部使用） */
int bhs_serialize(const BHS *bhs, FILE *fp);
BHS* bhs_deserialize(FILE *fp);

/* BHS 深拷贝（堆分配，返回新 BHS 指针） */
BHS* bhs_deep_copy(const BHS *src);

#endif
