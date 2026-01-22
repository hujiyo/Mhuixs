#include "getid.h"
#include <pthread.h>

/*
#版权所有 (c) HuJi 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2024.11
Email:hj18914255909@outlook.com
*/

#define merr -1

/* 全局位图 */
static BITMAP sid_bitmap;
static BITMAP uid_bitmap;
static BITMAP gid_bitmap;

/* 互斥锁 */
static pthread_mutex_t sid_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t uid_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t gid_mutex = PTHREAD_MUTEX_INITIALIZER;

static int if_init = 0;

/* ==================== 初始化和清理 ==================== */

int idalloc_init(void) {
    if (if_init) {
        return 0; /* 已经初始化 */
    }
    
    sid_bitmap = *bitmap_create_with_size(65536);
    uid_bitmap = *bitmap_create_with_size(65536);
    gid_bitmap = *bitmap_create_with_size(65536);
    
    if (bitmap_iserr(&sid_bitmap) || bitmap_iserr(&uid_bitmap) || bitmap_iserr(&gid_bitmap)) {
        free_bitmap(&sid_bitmap);
        free_bitmap(&uid_bitmap);
        free_bitmap(&gid_bitmap);
        return init_failed;
    }
    
    if_init = 1;
    return 0;
}

int idalloc_close(void) {
    if (!if_init) {
        return 0;
    }
    
    if_init = 0;
    free_bitmap(&sid_bitmap);
    free_bitmap(&uid_bitmap);
    free_bitmap(&gid_bitmap);
    return 0;
}

/* ==================== 会话ID分配 ==================== */

SID get_sid(void) {
    pthread_mutex_lock(&sid_mutex);
    
    int64_t idx = bitmap_find(&sid_bitmap, 0, 0, 65535);
    if (idx == merr) {
        pthread_mutex_unlock(&sid_mutex);
        return merr;
    }
    
    bitmap_set(&sid_bitmap, (uint32_t)idx, 1);
    pthread_mutex_unlock(&sid_mutex);
    return (SID)idx;
}

SID del_sid(SID sid) {
    if (sid < 0 || sid > 65535) {
        return merr;
    }
    
    pthread_mutex_lock(&sid_mutex);
    bitmap_set(&sid_bitmap, (uint32_t)sid, 0);
    pthread_mutex_unlock(&sid_mutex);
    return 0;
}

/* ==================== 用户ID分配 ==================== */

UID get_uid(UID_t type) {
    uint32_t start = 0, end = 0;
    
    switch (type) {
        case ROOT_UID:    start = 0; end = 0; break;
        case SYSTEM_UID:  start = 1; end = 99; break;
        case COMMON_UID:  start = 100; end = 65535; break;
        default: return merr;
    }
    
    pthread_mutex_lock(&uid_mutex);
    
    int64_t idx = bitmap_find(&uid_bitmap, 0, start, end);
    if (idx == merr) {
        pthread_mutex_unlock(&uid_mutex);
        return merr;
    }
    
    bitmap_set(&uid_bitmap, (uint32_t)idx, 1);
    pthread_mutex_unlock(&uid_mutex);
    return (UID)idx;
}

UID del_uid(UID_t type, UID uid) {
    uint32_t start = 0, end = 0;
    
    switch (type) {
        case ROOT_UID:    start = 0; end = 0; break;
        case SYSTEM_UID:  start = 1; end = 99; break;
        case COMMON_UID:  start = 100; end = 65535; break;
        default: return merr;
    }
    
    if (uid < (int)start || uid > (int)end) {
        return merr;
    }
    
    pthread_mutex_lock(&uid_mutex);
    bitmap_set(&uid_bitmap, (uint32_t)uid, 0);
    pthread_mutex_unlock(&uid_mutex);
    return 0;
}

/* ==================== 组ID分配 ==================== */

GID get_gid(GID_t type) {
    int start = 0, end = 0;
    
    switch (type) {
        case SYSTEM_GID:  start = 0; end = 0; break;
        case COMMON_GID:  start = 1; end = 65535; break;
        default: return merr;
    }
    
    pthread_mutex_lock(&gid_mutex);
    
    int64_t idx = bitmap_find(&gid_bitmap, 0, start, end);
    if (idx == merr) {
        pthread_mutex_unlock(&gid_mutex);
        return merr;
    }
    
    bitmap_set(&gid_bitmap, (uint32_t)idx, 1);
    pthread_mutex_unlock(&gid_mutex);
    return (GID)idx;
}

GID del_gid(GID_t type, GID gid) {
    int start = 0, end = 0;
    
    switch (type) {
        case SYSTEM_GID:  start = 0; end = 0; break;
        case COMMON_GID:  start = 1; end = 65535; break;
        default: return merr;
    }
    
    if (gid < start || gid > end) {
        return merr;
    }
    
    pthread_mutex_lock(&gid_mutex);
    bitmap_set(&gid_bitmap, (uint32_t)gid, 0);
    pthread_mutex_unlock(&gid_mutex);
    return 0;
}
