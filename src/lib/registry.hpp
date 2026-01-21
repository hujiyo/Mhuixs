#ifndef REGISTRY_HPP
#define REGISTRY_HPP

#include <string>
#include <unordered_map>
#include <mutex>
#include "merr.h"
#include "env.hpp"
#include "Mhudef.hpp"
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
    
    // 注册HOOK，0=成功，1=重名失败,hook_return是返回的HOOK
    int register_hook(UID owner,string name,HOOK* hook_return);// 注册HOOK
    
    void unregister_hook(const string& name);// 注销HOOK
    
    HOOK* find_hook(const string& name);// 查找HOOK
    
    bool is_registered(const string& name);// 判断HOOK是否已注册
};

extern Registry Reg;//全局注册表

int reg_init();//初始化注册表

/* ==================== C 接口层 ==================== */
/* 供 Logex（纯 C）调用的接口 */

#ifdef __cplusplus
extern "C" {
#endif

/* 不透明句柄类型 */
typedef void* HookHandle;

/* 注册表操作 */
int reg_c_register(uint64_t owner, const char *name, HookHandle *out_hook);
void reg_c_unregister(const char *name);
HookHandle reg_c_find(const char *name);
int reg_c_exists(const char *name);

/* HOOK 操作 */
int hook_c_new(HookHandle hook, uint64_t caller, int objtype, void *p1, void *p2, void *p3);
int hook_c_set_permission(HookHandle hook, uint64_t caller, const char *pm_str);

/* BHS 操作（用于 static let 持久化变量） */
int hook_c_set_bhs(HookHandle hook, uint64_t caller, void *bhs);
void* hook_c_get_bhs(HookHandle hook, uint64_t caller);

#ifdef __cplusplus
}
#endif

#endif
