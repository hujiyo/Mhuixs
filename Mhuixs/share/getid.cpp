#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "bitmap.hpp"
#include "getid.hpp"

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

BITMAP* _USERS_ID_BITMAP_=NULL; //全局变量:用户ID位图
BITMAP* _GROUP_ID_BITMAP_=NULL; //全局变量:组ID位图


int init_getid()
{
    /*
    初始ID分配器

    返回值：0：成功  -1：失败
    */
    //下面4个全局变量共用一个位图
    _ADMIN_ID_ = 1;//1-99
    _HUMAIN_ID_ = 100;//100-999
    _AI_ID_ = 1000;//1000-9999
    _GUEST_ID_ = 10000;//10000-65535
    _USERS_ID_BITMAP_ = makeBITMAP(65536);//0-65535
    if(_USERS_ID_BITMAP_ == NULL){
        return merr;
    }
    setBIT(_USERS_ID_BITMAP_,0,1);//隐藏root位

    _GROUP_ID_ = 0;//0-65535
    _GROUP_ID_BITMAP_ = makeBITMAP(65536);
    if(_GROUP_ID_BITMAP_ == NULL){
        return merr;
    }
}
int getid(char IDTYPE)
{
    /*
    获得相应种类的ID

    返回值：ID  -1：失败
    */
    switch(IDTYPE){
        case ADMIN_ID:{
            int64_t offset = retuoffset(_USERS_ID_BITMAP_,1,99);
            if(offset == merr){
                return merr;
            }
            setBIT(_USERS_ID_BITMAP_,offset,1);
            return offset;
        }
        case HUMAN_ID:{
            int64_t offset = retuoffset(_USERS_ID_BITMAP_,100,999);
            if(offset == merr){
                return merr;
            }
            setBIT(_USERS_ID_BITMAP_,offset,1);
            return offset;
        }
        case AI_ID:{
            int64_t offset = retuoffset(_USERS_ID_BITMAP_,1000,9999);
            if(offset == merr){
                return merr;
            }
            setBIT(_USERS_ID_BITMAP_,offset,1);
            return offset;
        }
        case GUEST_ID:{
            int64_t offset = retuoffset(_USERS_ID_BITMAP_,10000,65535);
            if(offset == merr){
                return merr;
            }
            setBIT(_USERS_ID_BITMAP_,offset,1);
            return offset;
        }
        case GROUP_ID:{
            int64_t offset = retuoffset(_GROUP_ID_BITMAP_,0,65535);
            if(offset == merr){
                return merr;
            }
            setBIT(_GROUP_ID_BITMAP_,offset,1);
            return offset;
        }
        default:return merr;
    }
}
int delid(char IDTYPE,uint16_t id)
{
    /*
    删除相应种类的ID
    返回值：0：成功  -1：失败
    */
    switch(IDTYPE){
        case ADMIN_ID:{
            if(id < 1 || id > 99){
                return merr;
            }
            setBIT(_USERS_ID_BITMAP_,id,0);
            return 0;
        }
        case HUMAN_ID:{
            if(id < 100 || id > 999){
                return merr;
            }
            setBIT(_USERS_ID_BITMAP_,id,0);
            return 0;
        }
        case AI_ID:{
            if(id < 1000 || id > 9999){
                return merr;
            }
            setBIT(_USERS_ID_BITMAP_,id,0);
            return 0;
        }
        case GUEST_ID:{            
            if(id < 10000 || id > 65535){
                return merr;
            }
            setBIT(_USERS_ID_BITMAP_,id,0);
            return 0;
        }
        case GROUP_ID:{
            setBIT(_GROUP_ID_BITMAP_,id,0);
            return 0;
        }
        case USER_ID:{
            if(id == 0){
                return merr;
            }
            setBIT(_USERS_ID_BITMAP_,id,0);
            return 0;
        }
        default:return merr;
    }
}