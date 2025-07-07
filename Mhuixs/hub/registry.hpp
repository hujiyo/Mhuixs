#ifndef REGISTRY_HPP
#define REGISTRY_HPP

#include <string>
#include <unordered_map>
#include <mutex>
#include "merr.h"
#include "env.hpp"
#include "hook.hpp"
using namespace std;
/*
全局注册表模块
集中化管理HOOK的申请、分配、销毁
集中化管理用户权限
*/

// 全局HOOK注册表
class Registry {
private:
    unordered_map<string, HOOK*> hook_map;//存储HOOK名字和索引
    mutex mtx;//全局锁
public:
    Registry() = default;
    ~Registry() = default;
    
    // 注册HOOK，0=成功，-1=重名失败,hook_return是返回的HOOK
    mrc register_hook(UID owner,string name,HOOK* hook_return);// 注册HOOK
    
    void unregister_hook(const string& name);// 注销HOOK
    
    HOOK* find_hook(const string& name);// 查找HOOK
};

extern Registry Reg;//全局注册表

mrc reg_init();//初始化注册表

#endif
