#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "Mhudef.h"//包含bitmap.h
#include "getid.h"
/*
#版权所有 (c) Mhuixs-team 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2025.1
Email:hj18914255909@outlook.com
*/

userid_t _ADMIN_ID_=1; //全局变量:ADMIN ID分配器
userid_t _HUMAIN_ID_=100; //全局变量:用户ID分配器
userid_t _AI_ID_=1000; //全局变量:AI ID分配器
userid_t _GUEST_ID_=10000; //全局变量:GUEST ID分配器
groupid_t _GROUP_ID_=0; //全局变量:当前组ID分配器
hookid_t _HOOK_ID_=0; //全局变量:当前钩子ID分配器

BITMAP* _USERS_ID_BITMAP_=NULL; //全局变量:用户ID位图
BITMAP* _GROUP_ID_BITMAP_=NULL; //全局变量:组ID位图
BITMAP* _HOOK_ID_BITMAP_=NULL; //全局变量:钩子ID位图


int init_getid()
{
    //下面4个全局变量共用一个位图
    _ADMIN_ID_ = 1;//1-99
    _HUMAIN_ID_ = 100;//100-999
    _AI_ID_ = 1000;//1000-9999
    _GUEST_ID_ = 10000;//10000-65535
    _USERS_ID_BITMAP_ = initBITMAP(65535);
    if(_USERS_ID_BITMAP_ == NULL){
        return err;
    }
    setBIT(_USERS_ID_BITMAP_,0,1);//隐藏root位

    _GROUP_ID_ = 0;//0-65535
    _GROUP_ID_BITMAP_ = initBITMAP(65535);
    if(_GROUP_ID_BITMAP_ == NULL){
        return err;
    }

    _HOOK_ID_ = 0;//0-65535
    _HOOK_ID_BITMAP_ = initBITMAP(65535);
    if(_HOOK_ID_BITMAP_ == NULL){
        return err;
    }
}
int getid(char IDTYPE)
{
    switch(IDTYPE){
        case ADMIN_ID:{
            
        }
        case HUMAN_ID:
        case AI_ID:
        case GUEST_ID:
        case GROUP_ID:
        case HOOK_ID:
        default:return err;

    }
}

