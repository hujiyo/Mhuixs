#include "registry.hpp"

Registry Reg;
int Registry::register_hook(UID owner,string name,HOOK* hook_return) 
{
    void* mem = malloc(sizeof(HOOK));
    if(!hook_return) return -2;//空HOOK，注册失败
    hook_return = new (mem) HOOK(owner,name);
    HOOK *hook = hook_return;
    //注册HOOK
    lock_guard<mutex> lock(mtx);
    if (hook && !hook->name.empty()) {
        if (hook_map.find(hook->name) != hook_map.end()) {            
            return 1;// 已有同名HOOK，注册失败
        }
        hook_map[hook->name] = hook;
        hook->if_register = 1;
        return 0;
    }
    return -1;
}

void Registry::unregister_hook(const string& name) {
    lock_guard<mutex> lock(mtx);
    auto it = hook_map.find(name);
    if (it != hook_map.end()) {
        it->second->if_register = 0;
        hook_map.erase(it);
    }
}

HOOK* Registry::find_hook(const string& name) {
    lock_guard<mutex> lock(mtx);
    auto it = hook_map.find(name);
    if (it != hook_map.end()) {
        return it->second;
    }
    return nullptr;
}

bool Registry::is_registered(const string& name) {
    lock_guard<mutex> lock(mtx);
    return hook_map.find(name) != hook_map.end();
}

int reg_init(){
    // 可根据需要初始化全局注册表
    return 0;
}

/* ==================== C 接口层实现 ==================== */

extern "C" {

int reg_c_register(uint64_t owner, const char *name, HookHandle *out_hook) {
    if (!name || !out_hook) return -2;
    HOOK *hook = nullptr;
    int ret = Reg.register_hook(owner, string(name), hook);
    *out_hook = (HookHandle)hook;
    return ret;
}

void reg_c_unregister(const char *name) {
    if (!name) return;
    Reg.unregister_hook(string(name));
}

HookHandle reg_c_find(const char *name) {
    if (!name) return nullptr;
    return (HookHandle)Reg.find_hook(string(name));
}

int reg_c_exists(const char *name) {
    if (!name) return 0;
    return Reg.is_registered(string(name)) ? 1 : 0;
}

int hook_c_new(HookHandle hook, uint64_t caller, int objtype, void *p1, void *p2, void *p3) {
    if (!hook) return -1;
    HOOK *h = (HOOK*)hook;
    return h->hook_new(caller, (obj_type)objtype, p1, p2, p3);
}

int hook_c_set_permission(HookHandle hook, uint64_t caller, const char *pm_str) {
    if (!hook || !pm_str) return -1;
    HOOK *h = (HOOK*)hook;
    h->reset_pm(caller, string(pm_str));
    return 0;
}

int hook_c_set_bhs(HookHandle hook, uint64_t caller, void *bhs) {
    if (!hook || !bhs) return -1;
    HOOK *h = (HOOK*)hook;
    /* 直接设置 HOOK 内部的 BHS */
    /* 注意：需要权限检查，这里简化处理 */
    basic_handle_struct *src = (basic_handle_struct*)bhs;
    /* TODO: 实现 BHS 复制到 HOOK 内部 */
    return 0;
}

void* hook_c_get_bhs(HookHandle hook, uint64_t caller) {
    if (!hook) return nullptr;
    HOOK *h = (HOOK*)hook;
    /* 返回 HOOK 内部 BHS 的指针 */
    /* 注意：需要权限检查，这里简化处理 */
    /* TODO: 返回 HOOK 内部的 BHS 指针 */
    return nullptr;
}

} /* extern "C" */
