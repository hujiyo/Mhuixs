#include "hook.hpp"
#include "registry.hpp"
/*
#版权所有 (c) Mhuixs-team 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2025.1
Email:hj18914255909@outlook.com
*/
/*
HOOK权限管理
待完成
*/
HOOK::HOOK(UID owner, string name)
    : owner(owner), name(name)
{
    
}
HOOK::~HOOK() {
    
}



void HOOK::set(GID group, permission_struct pm) {
   
}


int HOOK::hook_new(obj_type objtype) {
    
}

int HOOK::hook_obj(HOOK *hook) {
    
  
}
