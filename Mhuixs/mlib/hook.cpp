#include "hook.hpp"
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

HOOK::HOOK(uint8_t caller_rank,uint8_t target_rank,string name){
    /*
    分配一个HOOK对象
    注意：target_rank 必须小于等于caller_rank
    */
    HOOK* hook=(HOOK*)calloc(1,sizeof(HOOK));
    hook->handle=NULL;
    hook->type=M_NULL;
    hook->rank=target_rank;
    hook->cprs_stage=lv0;
    hook->owner=VOID;//默认是VOID
    hook->group=VOID;//默认是VOID
    hook->hook_id=getid(HOOK_ID);
}
int hooksth(HOOK* hook,uint8_t caller_rank,HOOKTYPE hooktype){
    /*
    使用hook创建一个数据结构对象
    */
    if(caller_rank<hook->rank){
    }
}