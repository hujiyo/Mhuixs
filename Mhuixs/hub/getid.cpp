#include "getid.hpp"
/*
#版权所有 (c) HuJi 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2024.11
Email:hj18914255909@outlook.com
*/
#define merr -1
static mutex sid_mutex,uid_mutex,gid_mutex;//分别为sid/uid/gid分配器添加静态互斥锁

// 会话ID分配
SID Id_alloctor::get_sid() {
    lock_guard<mutex> lock(sid_mutex);// 锁定会话ID位图
    int64_t idx = bitmap_find(&sid_bitmap, 0, 0, 65535);
    if(idx == merr) return merr;
    bitmap_set(&sid_bitmap, (uint32_t)idx, 1);
    return (SID)idx;
}

// 释放会话ID
SID Id_alloctor::del_sid(SID sid) {
    lock_guard<mutex> lock(sid_mutex);// 锁定会话ID位图
    if(sid < 0 || sid > 65535) return merr;
    bitmap_set(&sid_bitmap, (SID)sid, 0);
    return 0;
}

// 用户ID分配
UID Id_alloctor::get_uid(UID_t type) {
    uint32_t start = 0, end = 0;
    switch(type) {
        case ROOT_UID:    start = 0; end = 0; break;
        case SYSTEM_UID:  start = 1; end = 99; break;
        case COMMON_UID:  start = 100; end = 65535; break;
        default: return merr;
    }
    lock_guard<mutex> lock(uid_mutex);// 锁定用户ID位图
    int64_t idx = bitmap_find(&uid_bitmap, 0, start, end);
    if(idx == merr) return merr;
    bitmap_set(&uid_bitmap, (uint32_t)idx, 1);
    return (UID)idx;
}

// 释放用户ID
UID Id_alloctor::del_uid(UID_t type, UID uid) {
    uint32_t start = 0, end = 0;
    switch(type) {
        case ROOT_UID:    start = 0; end = 0; break;
        case SYSTEM_UID:  start = 1; end = 99; break;
        case COMMON_UID:  start = 100; end = 65535; break;
        default: return merr;
    }
    if(uid < (int)start || uid > (int)end) return merr;
    lock_guard<mutex> lock(uid_mutex);// 锁定用户ID位图
    bitmap_set(&uid_bitmap, (uint32_t)uid, 0);
    return 0;
}

// 组ID分配
GID Id_alloctor::get_gid(GID_t type) {    
    int start = 0, end = 0;
    switch(type) {
        case SYSTEM_GID:    start = 0; end = 0; break;
        case COMMON_GID:   start = 1; end = 65535; break;
        default: return merr;
    }
    lock_guard<mutex> lock(gid_mutex);// 锁定组ID位图
    int64_t idx = bitmap_find(&gid_bitmap, 0, start, end);
    if(idx == merr) return merr;
    bitmap_set(&gid_bitmap, idx, 1);
    return idx;
}

// 释放组ID
GID Id_alloctor::del_gid(GID_t type, GID gid) {
    int start = 0, end = 0;
    switch(type) {
        case SYSTEM_GID:    start = 0; end = 0; break;
        case COMMON_GID:   start = 1; end = 65535; break;
        default: return merr;
    }
    if(gid < start || gid > end) return merr;

    lock_guard<mutex> lock(gid_mutex);// 锁定组ID位图
    bitmap_set(&gid_bitmap, gid, 0);
    return 0;
}

static int if_init = 0;

//id分配器模块初始化
int Id_alloctor::close() {
    if_init=0;
    free_bitmap(&sid_bitmap);
    free_bitmap(&uid_bitmap);
    free_bitmap(&gid_bitmap);
    return 0;
}
int Id_alloctor::init()    {
    sid_bitmap = *bitmap_create_with_size(65536);
    uid_bitmap = *bitmap_create_with_size(65536);
    gid_bitmap = *bitmap_create_with_size(65536);
    if (bitmap_iserr(&sid_bitmap)||bitmap_iserr(&uid_bitmap)||bitmap_iserr(&gid_bitmap)) {
        close();
        return init_failed;
    }
    if_init=1;
    return 0;
}
