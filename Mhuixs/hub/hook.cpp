#include "hook.hpp"

HOOK::HOOK(UID owner,string name)
:owner(owner),name(name),if_register(0){
    pm_s.owner_read=1;
    pm_s.owner_change=1;
    pm_s.owner_add=1;

    pm_s.group_read=1;
    pm_s.group_change=1;
    pm_s.group_add=1;

    pm_s.other_read=0;
    pm_s.other_change=0;
    pm_s.other_add=0;

    pm_s.ifisinit=1;//权限初始化完成
}

HOOK::~HOOK() {
    // 如果已注册，先注销
    if (if_register && Reg.is_registered(name)) {
        Reg.unregister_hook(name);
    }
    bhs.clear_self();
}
int HOOK::hook_new(UID caller,obj_type objtype,void *parameter1, void *parameter2, void *parameter3)
{
    // 用钩子建立一个新对象:先删除原有对象，再增加新对象。必须同时拥有add和change权限
    //先检查权限
    if(caller!=0){
        if( (!Ugmanager.is_entitled(*this,caller,HOOK_ADD)) || (!Ugmanager.is_entitled(*this,caller,HOOK_CHANGE))){
            return permission_denied;
        }
    }
    //删除原有对象
    bhs.clear_self();
    int res = bhs.make_self(objtype,parameter1, parameter2, parameter3);
    if(res==-1){
        report(error,"HOOK","HOOK::hook_new::bhs.make_self() failed.\n");
        return -1;
    }
} 
void HOOK::reset_pm(UID caller, string pm_str) {
    // 仅允许root或owner修改权限
    if (caller != 0 && caller != owner) return;
    // 只允许长度为3（八进制）或9（二进制）
    if (!(pm_str.length() == 3 || pm_str.length() == 9)) return;
    // 临时权限结构体
    permission_struct new_pm = pm_s;
    // 解析三位八进制 (类比Linux: r=4, a=2, c=1)
    if (pm_str.length() == 3) {
        for (int i = 0; i < 3; ++i) {
            if (pm_str[i] < '0' || pm_str[i] > '7') return;
            int val = pm_str[i] - '0';
            // owner/group/other
            switch (i) {
                case 0:
                    new_pm.owner_read = (val & 4) ? 1 : 0;
                    new_pm.owner_add  = (val & 2) ? 1 : 0;
                    new_pm.owner_change  = (val & 1) ? 1 : 0;
                    break;
                case 1:
                    new_pm.group_read = (val & 4) ? 1 : 0;
                    new_pm.group_add  = (val & 2) ? 1 : 0;
                    new_pm.group_change  = (val & 1) ? 1 : 0;
                    break;
                case 2:
                    new_pm.other_read = (val & 4) ? 1 : 0;
                    new_pm.other_add  = (val & 2) ? 1 : 0;
                    new_pm.other_change  = (val & 1) ? 1 : 0;
                    break;
            }
        }
    } else if (pm_str.length() == 9) {
        // 解析九位二进制 (rac-rac-rac格式)
        for (int i = 0; i < 9; ++i) {
            if (pm_str[i] != '0' && pm_str[i] != '1') return;
        }
        new_pm.owner_read = pm_str[0] - '0';
        new_pm.owner_add  = pm_str[1] - '0';
        new_pm.owner_change  = pm_str[2] - '0';
        new_pm.group_read = pm_str[3] - '0';
        new_pm.group_add  = pm_str[4] - '0';
        new_pm.group_change  = pm_str[5] - '0';
        new_pm.other_read = pm_str[6] - '0';
        new_pm.other_add  = pm_str[7] - '0';
        new_pm.other_change  = pm_str[8] - '0';
    } else {
        return;
    }
    new_pm.ifisinit = 1;
    pm_s = new_pm;
}
