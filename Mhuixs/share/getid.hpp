#ifndef GETID_HPP
#define GETID_HPP

#include "bitmap.hpp"

#include <mutex>
using namespace std;
/*
#版权所有 (c) HuJi 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2024.11
Email:hj18914255909@outlook.com
*/
#define merr -1
/*
===================================
ID分配器模块 线程安全：所有公有方法均加锁
===================================

会话SID:0-65535,类似linux的PID

用户UID:0-65535,类似linux
    0为root用户ID            ROOT_UID
    1-99为系统用户ID         SYSTEM_UID
    100-49999为普通用户ID     COMMON_UID
    50000-65536为临时用户ID  TEMP_UID

组GID：0-65535,类似linux
    0为系统组ID             SYSTEM_GID
    1-65535为普通组ID    COMMON_GID
    65536为临时用户组ID     TEMP_GID
*/
typedef int SID,UID,GID;

enum UID_t{
    ROOT_UID,     // root用户ID
    SYSTEM_UID,   // 系统用户ID
    COMMON_UID,   // 普通用户ID
    TEMP_UID      // 临时用户ID
};//用户ID类型

enum GID_t{
    SYSTEM_GID,     // 系统组ID  
    COMMON_GID,    // 普通组ID
    TEMP_GID,   // 临时用户组ID 
};//组ID类型

class Id_alloctor{
private:
    BITMAP sid_bitmap;  // 会话ID位图
    BITMAP uid_bitmap;  // 用户ID位图    
    BITMAP gid_bitmap;  // 组ID位图
public:
    Id_alloctor():sid_bitmap(65536),uid_bitmap(65536),gid_bitmap(65536){}
    ~Id_alloctor()=default;

    SID get_sid();//获得会话ID
    SID del_sid(SID sid);//释放会话ID

    UID get_uid(UID_t type);//获得用户ID
    UID del_uid(UID_t type,UID uid);//释放用户ID

    GID get_gid(GID_t type);//获得组ID    
    GID del_gid(GID_t type,GID gid);//释放组ID
};

extern Id_alloctor Idalloc;//全局唯一ID分配器

int id_alloc_init();//启动ID分配器模块

#endif