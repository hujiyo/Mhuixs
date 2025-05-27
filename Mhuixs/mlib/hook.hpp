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
#include "getid.hpp"

/*
hook在Mhuixs中被用来：
1.链接所有"需要有权限功能"的独立"数据结构"
2.在一种数据结构中引用独立于自己的另一个数据结构
3.分为有权HOOK和无权HOOK
*/
class HOOK {
    basic_handle_struct bhs;//对象
    RANK rank;//保护等级
    cprs cprs_stage;//压缩级别
    userid_t owner;//对应所有者ID
    groupid_t group;//对应组ID
    hookid_t hook_id;
    string name;//狗子名
public:
    HOOK(string name);//创建一个空钩子，空钩子将会被注册。
    int rise_rank(uint8_t caller_rank,uint8_t target_rank);//请求赋予权限
    int hook_obj(obj_type objtype);//链接一个对象

}HOOK;


#endif