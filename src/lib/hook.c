#include "hook.h"
#include <stdlib.h>
#include <string.h>

/* 创建并初始化 HOOK */
HOOK* HOOK_login(UID owner, mstring name, Obj obj) {
    GID main_gid = get_primary_gid_by_uid(owner);
    if (main_gid < 0) {
        return NULL;
    }
    
    HOOK* hook = (HOOK*)calloc(1, sizeof(HOOK));
    if (!hook) {
        return NULL;
    }
    
    memset(hook, 0, sizeof(HOOK));
    hook->obj = obj;
    hook->name = name;
    hook->owner = owner;
    hook->group = main_gid;

    /* 设置默认权限 */
    hook->pm_s.owner_read = 1;
    hook->pm_s.owner_change = 1;
    hook->pm_s.owner_add = 1;

    hook->pm_s.group_read = 1;
    hook->pm_s.group_change = 1;
    hook->pm_s.group_add = 1;

    hook->pm_s.other_read = 0;
    hook->pm_s.other_change = 0;
    hook->pm_s.other_add = 0;

    hook->pm_s.ifisinit = 1; /* 权限初始化完成 */
    
    return hook;
}

/* 注销 HOOK */
int HOOK_logout(HOOK* hook) {
    if (!hook) return -1;
    
    /* 如果已注册，先注销 */
    if (hook->name) {
        const char* name_cstr = mstr_to_cstr(hook->name);
        if (name_cstr && hook_reg_exists(name_cstr)) {
            reg_unregister(name_cstr);
        }
        free((void*)name_cstr);
    }
    
    /* 清理 obj */
    if (hook->obj) {
        /* TODO: 调用 BHS 的清理函数 */
        /* bignum_clear(hook->obj); */
    }
    
    return 0;
}

/* 用钩子建立一个新对象 */
int hook_new_obj(HOOK* hook, UID caller, obj_type objtype, void *parameter1, void *parameter2, void *parameter3) {
    if (!hook) return -1;
    
    /* 用钩子建立一个新对象:先删除原有对象，再增加新对象。必须同时拥有add和change权限 */
    /* 先检查权限 */
    if (caller != 0) {
        if (!is_entitled(hook, caller, HOOK_ADD) || 
            !is_entitled(hook, caller, HOOK_CHANGE)) {
            return permission_denied;
        }
    }
    
    /* 删除原有对象 */
    if (hook->obj) {
        /* TODO: 调用 BHS 的清理函数 */
        /* bignum_clear(hook->obj); */
    }
    
    /* 创建新对象 */
    /* TODO: 实现 BHS 的 make_self 功能 */
    /* int res = bhs_make_self(&hook->obj, objtype, parameter1, parameter2, parameter3); */
    /* if (res == -1) { */
    /*     report(error, "HOOK", "hook_new_obj::bhs_make_self() failed.\n"); */
    /*     return -1; */
    /* } */
    
    (void)objtype;
    (void)parameter1;
    (void)parameter2;
    (void)parameter3;
    
    return 0;
}

/* 重置权限 */
void hook_reset_pm(HOOK* hook, UID caller, const char* pm_str) {
    if (!hook) return;
    
    /* 仅允许root或owner修改权限 */
    if (caller != 0 && caller != hook->owner) return;
    if (!pm_str) return;
    
    size_t len = strlen(pm_str);
    /* 只允许长度为3（八进制）或9（二进制） */
    if (!(len == 3 || len == 9)) return;
    
    /* 临时权限结构体 */
    permission_struct new_pm = hook->pm_s;
    
    /* 解析三位八进制 (类比Linux: r=4, a=2, c=1) */
    if (len == 3) {
        for (int i = 0; i < 3; ++i) {
            if (pm_str[i] < '0' || pm_str[i] > '7') return;
            int val = pm_str[i] - '0';
            /* owner/group/other */
            switch (i) {
                case 0:
                    new_pm.owner_read = (val & 4) ? 1 : 0;
                    new_pm.owner_add  = (val & 2) ? 1 : 0;
                    new_pm.owner_change  = (val & 1) ? 1 : 0;
                    break;
                case 1:
                    new_pm.group_read = (val & 4) ? 1 : 0;
                    new_pm.group_add  = (val & 2) ? 1 : 0;
                    new_pm.group_change  = (val & 1) ? 1 : 0;
                    break;
                case 2:
                    new_pm.other_read = (val & 4) ? 1 : 0;
                    new_pm.other_add  = (val & 2) ? 1 : 0;
                    new_pm.other_change  = (val & 1) ? 0;
                    break;
            }
        }
    } else if (len == 9) {
        /* 解析九位二进制 (rac-rac-rac格式) */
        for (int i = 0; i < 9; ++i) {
            if (pm_str[i] != '0' && pm_str[i] != '1') return;
        }
        new_pm.owner_read = pm_str[0] - '0';
        new_pm.owner_add  = pm_str[1] - '0';
        new_pm.owner_change  = pm_str[2] - '0';
        new_pm.group_read = pm_str[3] - '0';
        new_pm.group_add  = pm_str[4] - '0';
        new_pm.group_change  = pm_str[5] - '0';
        new_pm.other_read = pm_str[6] - '0';
        new_pm.other_add  = pm_str[7] - '0';
        new_pm.other_change  = pm_str[8] - '0';
    } else {
        return;
    }
    
    new_pm.ifisinit = 1;
    hook->pm_s = new_pm;
}
