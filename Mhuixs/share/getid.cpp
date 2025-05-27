#include "getid.hpp"
/*
#版权所有 (c) HuJi 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2024.11
Email:hj18914255909@outlook.com
*/

static mutex sid_mutex,uid_mutex,gid_mutex;//分别为sid/uid/gid分配器添加静态互斥锁

// 会话ID分配
SID IDalloc::get_sid() {
    lock_guard<mutex> lock(sid_mutex);// 锁定会话ID位图
    int64_t idx = sid_bitmap.find(0, 0, 65535);
    if(idx == merr) return merr;
    sid_bitmap.set((uint32_t)idx, 1);
    return (SID)idx;
}

// 释放会话ID
SID IDalloc::del_sid(SID sid) {
    lock_guard<mutex> lock(sid_mutex);// 锁定会话ID位图
    if(sid < 0 || sid > 65535) return merr;
    sid_bitmap.set((SID)sid, 0);
    return 0;
}

// 用户ID分配
UID IDalloc::get_uid(UID_t type) {
    uint32_t start = 0, end = 0;
    switch(type) {
        case ROOT_UID:    start = 0; end = 0; break;
        case SYSTEM_UID:  start = 1; end = 98; break;
        case COMMON_UID:  start = 99; end = 49999; break;
        case TEMP_UID:    start = 50000; end = 65535; break;
        default: return merr;
    }
    lock_guard<mutex> lock(uid_mutex);// 锁定用户ID位图
    int64_t idx = uid_bitmap.find(0, start, end);
    if(idx == merr) return merr;
    uid_bitmap.set((uint32_t)idx, 1);
    return (UID)idx;
}

// 释放用户ID
UID IDalloc::del_uid(UID_t type, UID uid) {
    uint32_t start = 0, end = 0;
    switch(type) {
        case ROOT_UID:    start = 0; end = 0; break;
        case SYSTEM_UID:  start = 1; end = 98; break;
        case COMMON_UID:  start = 99; end = 49999; break;
        case TEMP_UID:    start = 50000; end = 65535; break;
        default: return merr;
    }
    if(uid < (int)start || uid > (int)end) return merr;
    lock_guard<mutex> lock(uid_mutex);// 锁定用户ID位图
    uid_bitmap.set((uint32_t)uid, 0);
    return 0;
}

// 组ID分配
GID IDalloc::get_gid(GID_t type) {    
    uint32_t start = 0, end = 0;
    switch(type) {
        case ROOT_GID:    start = 0; end = 0; break;
        case ADMIN_GID:   start = 1; end = 1; break;
        case SYSTEM_GID:  start = 2; end = 999; break;
        case MY_GID:      start = 1000; end = 65535; break;
        default: return merr;
    }
    lock_guard<mutex> lock(gid_mutex);// 锁定组ID位图
    int64_t idx = gid_bitmap.find(0, start, end);
    if(idx == merr) return merr;
    gid_bitmap.set((GID)idx, 1);
    return (GID)idx;
}

// 释放组ID
GID IDalloc::del_gid(GID_t type, GID gid) {
    uint32_t start = 0, end = 0;
    switch(type) {
        case ROOT_GID:    start = 0; end = 0; break;
        case ADMIN_GID:   start = 1; end = 1; break;
        case SYSTEM_GID:  start = 2; end = 999; break;
        case MY_GID:      start = 1000; end = 65535; break;
        default: return merr;
    }
    if(gid < (int)start || gid > (int)end) return merr;

    lock_guard<mutex> lock(gid_mutex);// 锁定组ID位图
    gid_bitmap.set((GID)gid, 0);
    return 0;
}

