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


