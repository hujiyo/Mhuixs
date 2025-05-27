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
using namespace std;

#include "Mhudef.hpp"

/*
hook在Mhuixs中被用来：
1.链接所有"需要有权限功能"的独立"数据结构"
2.在一种数据结构中引用独立于自己的另一个数据结构
3.分为有权HOOK和无权HOOK
*/
class HOOK {
    basic_handle_struct bhs;//对象    
    Cprs cprs;//压缩级别
    UID owner;//对应所有者ID
    GID group;//对应组ID
    string name;//狗子名

    uint16_t ifisinit:1;//是否初始化:一旦初始化权限就生效

    //权限:可读:r 可添:a 可删:d 
    //所有者权限
    uint16_t owner_read:1;//所有者可读
    uint16_t owner_add:1;//所有者可添内容
    uint16_t owner_del:1;//所有者可删内容    
    //组权限
    uint16_t group_read:1;//组可读
    uint16_t group_add:1;//组可添内容
    uint16_t group_del:1;//组可删内容    
    //其他权限
    uint16_t other_read:1;//其他可读
    uint16_t other_add:1;//其他可添内容
    uint16_t other_del:1;//其他可删内容    

public:
    HOOK(UID owner,string name);//创建一个空钩子，空钩子将会被注册。
    set(GID group,
    int hook_obj(obj_type objtype);//链接一个对象

};


#endif