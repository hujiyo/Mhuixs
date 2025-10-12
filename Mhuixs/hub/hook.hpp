#ifndef HOOK_HPP
#define HOOK_HPP
/*
#版权所有 (c) Mhuixs-team 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2025.1
Email:hj18914255909@outlook.com
*/
#include <string>

#include "merr.h"
#include "getid.hpp"
#include "Mhudef.hpp"

// 前向声明，避免循环包含
class User_group_manager;
class Registry;
class FunCall;

enum Mode_type {
    HOOK_READ = 'r',
    HOOK_ADD = 'a',
    HOOK_CHANGE = 'c',
};//HOOK的操作类型

using namespace std;

/*
hook在Mhuixs中被用来：
1.链接所有"需要有权限功能"的独立"数据结构"
2.在一种数据结构中引用独立于自己的另一个数据结构
3.分为有权HOOK和无权HOOK
*/
/*
可读：包括hook的可见性，如果用户没有权限，则hook不可见
*/


struct permission_struct{
    char ifisinit;//是否初始化:一旦初始化权限就生效

    //权限:可读:r 可添:a 可改:c (类比Linux的rwx权限系统)
    //所有者权限
    char owner_read;//所有者可读
    char owner_add;//所有者可添内容
    char owner_change;//所有者可改内容    
    //组权限
    char group_read;//组可读
    char group_add;//组可添内容
    char group_change;//组可改内容    
    //其他权限
    char other_read;//其他可读
    char other_add;//其他可添内容
    char other_change;//其他可改内容
}; 

class HOOK {
private:
    basic_handle_struct bhs; // 操作对象
    UID owner; // 所有者ID
    GID group; // 组ID
public:
    string name; // 钩子名
private:
    permission_struct pm_s; // 权限结构体
    int if_register;//是否注册(由Registry管理)
    friend class User_group_manager;
    friend class Registry; // 允许注册表访问
    friend class FunCall;

    HOOK(UID owner,string name);//由注册表统一创建、统一管理
    ~HOOK();    
public:    
    int hook_new(UID caller,obj_type objtype,void *parameter1, void *parameter2, void *parameter3); // 用钩子建立一个新对象
    void reset_pm(UID caller,string pm_str);// 设置组和权限
};


#endif