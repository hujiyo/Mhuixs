#include "registry.hpp"

Registry Reg;

int Registry::register_hook(HOOK* hook) {
    lock_guard<mutex> lock(mtx);
    if (hook && !hook->name.empty()) {
        if (hook_map.find(hook->name) != hook_map.end()) {
            // 已有同名HOOK，注册失败
            return -1;
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

mrc reg_init(){
    // 可根据需要初始化全局注册表
    return;
}


