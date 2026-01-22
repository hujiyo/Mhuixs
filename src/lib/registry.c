#include "registry.h"
#include <stdlib.h>
#include <string.h>

/* 全局注册表实例 */
Registry Reg;

/* 初始化注册表 */
int reg_init(void) {
    Reg.hook_map = hash_create(1024); /* 初始容量 1024，适合大规模数据 */
    if (!Reg.hook_map) {
        return -1;
    }
    
#ifdef _WIN32
    InitializeCriticalSection(&Reg.lock);
#else
    if (pthread_mutex_init(&Reg.lock, NULL) != 0) {
        hash_destroy(Reg.hook_map, NULL);
        return -1;
    }
#endif
    
    return 0;
}

/* 销毁注册表 */
void reg_destroy(void) {
    if (Reg.hook_map) {
        hash_destroy(Reg.hook_map, NULL); /* 不释放 HOOK*，由外部管理 */
        Reg.hook_map = NULL;
    }
    
#ifdef _WIN32
    DeleteCriticalSection(&Reg.lock);
#else
    pthread_mutex_destroy(&Reg.lock);
#endif
}

/* 加锁 */
static inline void reg_lock(void) {
#ifdef _WIN32
    EnterCriticalSection(&Reg.lock);
#else
    pthread_mutex_lock(&Reg.lock);
#endif
}

/* 解锁 */
static inline void reg_unlock(void) {
#ifdef _WIN32
    LeaveCriticalSection(&Reg.lock);
#else
    pthread_mutex_unlock(&Reg.lock);
#endif
}

/* 注册HOOK */
int reg_register_hook(UID owner, const char* name, HOOK** hook_return) {
    if (!hook_return) return -2; /* 空HOOK指针，注册失败 */
    if (!name || strlen(name) == 0) return -1; /* 空名字，注册失败 */
    
    /* 创建 mstring */
    mstring mname = mstr_from_cstr(name);
    if (!mname) return -1;
    
    /* 创建 HOOK */
    HOOK* hook = HOOK_login(owner, mname, NULL);
    if (!hook) {
        mstr_free(mname);
        return -1;
    }
    
    /* 注册HOOK */
    reg_lock();
    
    /* 检查是否已存在同名HOOK */
    if (hash_contains(Reg.hook_map, name)) {
        reg_unlock();
        HOOK_logout(hook);
        free(hook);
        return 1; /* 已有同名HOOK，注册失败 */
    }
    
    /* 添加到哈希表 */
    if (hash_put(Reg.hook_map, name, hook) != 0) {
        reg_unlock();
        HOOK_logout(hook);
        free(hook);
        return -1;
    }
    
    reg_unlock();
    
    *hook_return = hook;
    return 0;
}

/* 注销HOOK */
void reg_unregister_hook(const char* name) {
    if (!name) return;
    
    reg_lock();
    
    HOOK* hook = (HOOK*)hash_get(Reg.hook_map, name);
    if (hook) {
        hash_remove(Reg.hook_map, name);
    }
    
    reg_unlock();
}

/* 查找HOOK */
HOOK* reg_find_hook(const char* name) {
    if (!name) return NULL;
    
    reg_lock();
    HOOK* hook = (HOOK*)hash_get(Reg.hook_map, name);
    reg_unlock();
    
    return hook;
}

/* 判断HOOK是否已注册 */
int reg_is_registered(const char* name) {
    if (!name) return 0;
    
    reg_lock();
    int exists = hash_contains(Reg.hook_map, name);
    reg_unlock();
    
    return exists;
}

/* ==================== C 接口层实现 ==================== */

int reg_register(uint64_t owner, const char *name, HookHandle *out_hook) {
    if (!name || !out_hook) return -2;
    
    HOOK *hook = NULL;
    int ret = reg_register_hook(owner, name, &hook);
    *out_hook = (HookHandle)hook;
    return ret;
}

void reg_unregister(const char *name) {
    if (!name) return;
    reg_unregister_hook(name);
}

HookHandle reg_find(const char *name) {
    if (!name) return NULL;
    return (HookHandle)reg_find_hook(name);
}

int hook_reg_exists(const char *name) {
    if (!name) return 0;
    return reg_is_registered(name);
}

int hook_new(HookHandle hook, uint64_t caller, int objtype, void *p1, void *p2, void *p3) {
    if (!hook) return -1;
    HOOK *h = (HOOK*)hook;
    return hook_new_obj(h, caller, (obj_type)objtype, p1, p2, p3);
}

int hook_set_permission(HookHandle hook, uint64_t caller, const char *pm_str) {
    if (!hook || !pm_str) return -1;
    HOOK *h = (HOOK*)hook;
    hook_reset_pm(h, caller, pm_str);
    return 0;
}

int hook_set_bhs(HookHandle hook, uint64_t caller, void *bhs) {
    if (!hook || !bhs) return -1;
    HOOK *h = (HOOK*)hook;
    /* 直接设置 HOOK 内部的 BHS */
    /* 注意：需要权限检查，这里简化处理 */
    /* TODO: 实现 BHS 复制到 HOOK 内部 */
    (void)caller; /* 避免未使用警告 */
    return 0;
}

void* hook_get_bhs(HookHandle hook, uint64_t caller) {
    if (!hook) return NULL;
    HOOK *h = (HOOK*)hook;
    /* 返回 HOOK 内部 BHS 的指针 */
    /* 注意：需要权限检查，这里简化处理 */
    /* TODO: 返回 HOOK 内部的 BHS 指针 */
    (void)caller; /* 避免未使用警告 */
    return NULL;
}
