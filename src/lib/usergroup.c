#include "usergroup.h"
/*
#版权所有 (c) HUJI 2025
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2025.5
Email:hj18914255909@outlook.com
*/

/* 全局用户组管理器实例 */
user_group_manager_t Ugmanager = {NULL, 0, 0, NULL, 0, 0, NULL, NULL, NULL, NULL};

/* 获取错误消息 */
const char* ug_strerror(int error_code) {
    switch (error_code) {
        case UG_SUCCESS: return "Success";
        case UG_ERR_USER_EXISTS: return "User already exists";
        case UG_ERR_USER_NOT_FOUND: return "User not found";
        case UG_ERR_GROUP_EXISTS: return "Group already exists";
        case UG_ERR_GROUP_NOT_FOUND: return "Group not found";
        case UG_ERR_INVALID_USERNAME: return "Invalid username";
        case UG_ERR_INVALID_PASSWORD: return "Invalid password";
        case UG_ERR_NO_MEMORY: return "Out of memory";
        case UG_ERR_IO_ERROR: return "I/O error";
        case UG_ERR_PERMISSION_DENIED: return "Permission denied";
        case UG_ERR_UID_EXHAUSTED: return "UID exhausted";
        case UG_ERR_GID_EXHAUSTED: return "GID exhausted";
        case UG_ERR_INVALID_INPUT: return "Invalid input";
        case UG_ERR_SAVE_FAILED: return "Failed to save data";
        default: return "Unknown error";
    }
}

/* 输入验证：用户名 */
static int validate_username(const char *username) {
    if (!username) return UG_ERR_INVALID_USERNAME;
    
    size_t len = strlen(username);
    if (len == 0 || len >= 64) return UG_ERR_INVALID_USERNAME;
    
    /* 只允许字母、数字、下划线、连字符 */
    for (size_t i = 0; i < len; i++) {
        char c = username[i];
        if (!((c >= 'a' && c <= 'z') || 
              (c >= 'A' && c <= 'Z') || 
              (c >= '0' && c <= '9') || 
              c == '_' || c == '-')) {
            return UG_ERR_INVALID_USERNAME;
        }
    }
    
    /* 不能以数字开头 */
    if (username[0] >= '0' && username[0] <= '9') {
        return UG_ERR_INVALID_USERNAME;
    }
    
    return UG_SUCCESS;
}

/* 输入验证：密码 */
static int validate_password(const char *passwd) {
    if (!passwd) return UG_ERR_INVALID_PASSWORD;
    
    size_t len = strlen(passwd);
    
    /* 最小长度 8 */
    if (len < 8) return UG_ERR_INVALID_PASSWORD;
    
    /* 最大长度 128 */
    if (len > 128) return UG_ERR_INVALID_PASSWORD;
    
    /* 检查密码强度：至少包含大写、小写、数字 */
    int has_upper = 0, has_lower = 0, has_digit = 0;
    for (size_t i = 0; i < len; i++) {
        if (passwd[i] >= 'A' && passwd[i] <= 'Z') has_upper = 1;
        if (passwd[i] >= 'a' && passwd[i] <= 'z') has_lower = 1;
        if (passwd[i] >= '0' && passwd[i] <= '9') has_digit = 1;
    }
    
    if (!has_upper || !has_lower || !has_digit) {
        return UG_ERR_INVALID_PASSWORD;
    }
    
    return UG_SUCCESS;
}

/* 输入验证：组名 */
static int validate_groupname(const char *groupname) {
    if (!groupname) return UG_ERR_INVALID_INPUT;
    
    size_t len = strlen(groupname);
    if (len == 0 || len >= 64) return UG_ERR_INVALID_INPUT;
    
    /* 只允许字母、数字、下划线、连字符 */
    for (size_t i = 0; i < len; i++) {
        char c = groupname[i];
        if (!((c >= 'a' && c <= 'z') || 
              (c >= 'A' && c <= 'Z') || 
              (c >= '0' && c <= '9') || 
              c == '_' || c == '-')) {
            return UG_ERR_INVALID_INPUT;
        }
    }
    
    /* 不能以数字开头 */
    if (groupname[0] >= '0' && groupname[0] <= '9') {
        return UG_ERR_INVALID_INPUT;
    }
    
    return UG_SUCCESS;
}

/* 辅助函数：查找用户索引 */
static int find_user_by_uid(UID uid) {
    if (!Ugmanager.uid_to_idx) {
        /* 哈希表未初始化，使用线性查找 */
        for (uint32_t i = 0; i < Ugmanager.num_users; i++) {
            if (Ugmanager.users[i].uid == uid) {
                return (int)i;
            }
        }
        return -1;
    }
    
    /* 使用哈希表 O(1) 查找 */
    char key[32];
    int_to_key(uid, key, sizeof(key));
    
    void *value = hash_get(Ugmanager.uid_to_idx, key);
    if (!value) return -1;
    
    return *(int*)value;
}

/* 辅助函数：查找用户索引（通过用户名） */
static int find_user_by_username(const char *username) {
    if (!username) return -1;
    
    if (!Ugmanager.username_to_idx) {
        /* 哈希表未初始化，使用线性查找 */
        for (uint32_t i = 0; i < Ugmanager.num_users; i++) {
            if (strcmp(Ugmanager.users[i].username, username) == 0) {
                return (int)i;
            }
        }
        return -1;
    }
    
    /* 使用哈希表 O(1) 查找 */
    void *value = hash_get(Ugmanager.username_to_idx, username);
    if (!value) return -1;
    
    return *(int*)value;
}

/* 辅助函数：查找组索引 */
static int find_group_by_gid(GID gid) {
    if (!Ugmanager.gid_to_idx) {
        /* 哈希表未初始化，使用线性查找 */
        for (uint32_t i = 0; i < Ugmanager.num_groups; i++) {
            if (Ugmanager.groups[i].gid == gid) {
                return (int)i;
            }
        }
        return -1;
    }
    
    /* 使用哈希表 O(1) 查找 */
    char key[32];
    int_to_key(gid, key, sizeof(key));
    
    void *value = hash_get(Ugmanager.gid_to_idx, key);
    if (!value) return -1;
    
    return *(int*)value;
}

/* 辅助函数：查找组索引（通过组名） */
static int find_group_by_groupname(const char *groupname) {
    if (!groupname) return -1;
    
    if (!Ugmanager.groupname_to_idx) {
        /* 哈希表未初始化，使用线性查找 */
        for (uint32_t i = 0; i < Ugmanager.num_groups; i++) {
            if (strcmp(Ugmanager.groups[i].groupname, groupname) == 0) {
                return (int)i;
            }
        }
        return -1;
    }
    
    /* 使用哈希表 O(1) 查找 */
    void *value = hash_get(Ugmanager.groupname_to_idx, groupname);
    if (!value) return -1;
    
    return *(int*)value;
}

/* 辅助函数：检查GID是否在用户的组列表中 */
static int user_has_group(user_info_t *user, GID gid) {
    for (uint32_t i = 0; i < user->num_groups; i++) {
        if (user->groups[i] == gid) {
            return 1;
        }
    }
    return 0;
}

/* 辅助函数：检查UID是否在组的成员列表中 */
static int group_has_member(group_info_t *group, UID uid) {
    for (uint32_t i = 0; i < group->num; i++) {
        if (group->members[i] == uid) {
            return 1;
        }
    }
    return 0;
}

/* 辅助函数：从用户的组列表中移除GID */
static void remove_gid_from_user(user_info_t *user, GID gid) {
    for (uint32_t i = 0; i < user->num_groups; i++) {
        if (user->groups[i] == gid) {
            /* 移动后续元素 */
            for (uint32_t j = i; j < user->num_groups - 1; j++) {
                user->groups[j] = user->groups[j + 1];
            }
            user->num_groups--;
            return;
        }
    }
}

/* 辅助函数：从组的成员列表中移除UID */
static void remove_uid_from_group(group_info_t *group, UID uid) {
    for (uint32_t i = 0; i < group->num; i++) {
        if (group->members[i] == uid) {
            /* 移动后续元素 */
            for (uint32_t j = i; j < group->num - 1; j++) {
                group->members[j] = group->members[j + 1];
            }
            group->num--;
            return;
        }
    }
}

/* 辅助函数：更新用户索引 */
static int update_user_index(uint32_t idx) {
    if (!Ugmanager.username_to_idx || !Ugmanager.uid_to_idx) {
        return UG_SUCCESS;  /* 索引未启用 */
    }
    
    user_info_t *u = &Ugmanager.users[idx];
    
    /* 更新 username -> idx */
    int *idx_ptr = malloc(sizeof(int));
    if (!idx_ptr) return UG_ERR_NO_MEMORY;
    *idx_ptr = idx;
    
    void *old_value = hash_remove(Ugmanager.username_to_idx, u->username);
    if (old_value) free(old_value);
    
    if (hash_put(Ugmanager.username_to_idx, u->username, idx_ptr) != 0) {
        free(idx_ptr);
        return UG_ERR_NO_MEMORY;
    }
    
    /* 更新 UID -> idx */
    char uid_key[32];
    int_to_key(u->uid, uid_key, sizeof(uid_key));
    
    idx_ptr = malloc(sizeof(int));
    if (!idx_ptr) return UG_ERR_NO_MEMORY;
    *idx_ptr = idx;
    
    old_value = hash_remove(Ugmanager.uid_to_idx, uid_key);
    if (old_value) free(old_value);
    
    if (hash_put(Ugmanager.uid_to_idx, uid_key, idx_ptr) != 0) {
        free(idx_ptr);
        return UG_ERR_NO_MEMORY;
    }
    
    return UG_SUCCESS;
}

/* 辅助函数：更新组索引 */
static int update_group_index(uint32_t idx) {
    if (!Ugmanager.groupname_to_idx || !Ugmanager.gid_to_idx) {
        return UG_SUCCESS;  /* 索引未启用 */
    }
    
    group_info_t *g = &Ugmanager.groups[idx];
    
    /* 更新 groupname -> idx */
    int *idx_ptr = malloc(sizeof(int));
    if (!idx_ptr) return UG_ERR_NO_MEMORY;
    *idx_ptr = idx;
    
    void *old_value = hash_remove(Ugmanager.groupname_to_idx, g->groupname);
    if (old_value) free(old_value);
    
    if (hash_put(Ugmanager.groupname_to_idx, g->groupname, idx_ptr) != 0) {
        free(idx_ptr);
        return UG_ERR_NO_MEMORY;
    }
    
    /* 更新 GID -> idx */
    char gid_key[32];
    int_to_key(g->gid, gid_key, sizeof(gid_key));
    
    idx_ptr = malloc(sizeof(int));
    if (!idx_ptr) return UG_ERR_NO_MEMORY;
    *idx_ptr = idx;
    
    old_value = hash_remove(Ugmanager.gid_to_idx, gid_key);
    if (old_value) free(old_value);
    
    if (hash_put(Ugmanager.gid_to_idx, gid_key, idx_ptr) != 0) {
        free(idx_ptr);
        return UG_ERR_NO_MEMORY;
    }
    
    return UG_SUCCESS;
}

/* 辅助函数：重建所有用户索引 */
static int rebuild_user_indexes(void) {
    if (!Ugmanager.username_to_idx || !Ugmanager.uid_to_idx) {
        return UG_SUCCESS;  /* 索引未启用 */
    }
    
    /* 清空旧索引 */
    hash_clear(Ugmanager.username_to_idx, free);
    hash_clear(Ugmanager.uid_to_idx, free);
    
    /* 重建索引 */
    for (uint32_t i = 0; i < Ugmanager.num_users; i++) {
        int ret = update_user_index(i);
        if (ret != UG_SUCCESS) return ret;
    }
    
    return UG_SUCCESS;
}

/* 辅助函数：重建所有组索引 */
static int rebuild_group_indexes(void) {
    if (!Ugmanager.groupname_to_idx || !Ugmanager.gid_to_idx) {
        return UG_SUCCESS;  /* 索引未启用 */
    }
    
    /* 清空旧索引 */
    hash_clear(Ugmanager.groupname_to_idx, free);
    hash_clear(Ugmanager.gid_to_idx, free);
    
    /* 重建索引 */
    for (uint32_t i = 0; i < Ugmanager.num_groups; i++) {
        int ret = update_group_index(i);
        if (ret != UG_SUCCESS) return ret;
    }
    
    return UG_SUCCESS;
}

/* 添加用户 */
int add_user(const char *username, const char *passwd) {
    int ret;
    
    /* 输入验证 */
    ret = validate_username(username);
    if (ret != UG_SUCCESS) return ret;
    
    ret = validate_password(passwd);
    if (ret != UG_SUCCESS) return ret;
    
    /* 检查用户名是否已存在 */
    if (find_user_by_username(username) >= 0) {
        return UG_ERR_USER_EXISTS;
    }
    
    /* 分配UID */
    UID uid = get_uid(COMMON_UID);
    if (uid == merr) return UG_ERR_UID_EXHAUSTED;
    
    /* 扩展用户数组 */
    if (Ugmanager.num_users >= Ugmanager.users_capacity) {
        uint32_t new_capacity = Ugmanager.users_capacity == 0 ? 8 : Ugmanager.users_capacity * 2;
        user_info_t *new_users = (user_info_t *)realloc(Ugmanager.users, 
                                                         new_capacity * sizeof(user_info_t));
        if (!new_users) {
            del_uid(COMMON_UID, uid);
            return UG_ERR_NO_MEMORY;
        }
        Ugmanager.users = new_users;
        Ugmanager.users_capacity = new_capacity;
    }
    
    /* 初始化新用户 */
    user_info_t *u = &Ugmanager.users[Ugmanager.num_users];
    u->uid = uid;
    strncpy(u->username, username, sizeof(u->username) - 1);
    u->username[sizeof(u->username) - 1] = '\0';
    
    /* 密码哈希 */
    char salt[BCRYPT_HASHSIZE];
    if (bcrypt_gensalt(12, salt) != 0) {
        del_uid(COMMON_UID, uid);
        return UG_ERR_IO_ERROR;
    }
    if (bcrypt_hashpw(passwd, salt, u->password) != 0) {
        del_uid(COMMON_UID, uid);
        return UG_ERR_IO_ERROR;
    }
    
    u->groups = NULL;
    u->num_groups = 0;
    u->groups_capacity = 0;
    u->main_hook[0] = '\0';
    u->description[0] = '\0';
    
    Ugmanager.num_users++;
    
    /* 更新索引 */
    ret = update_user_index(Ugmanager.num_users - 1);
    if (ret != UG_SUCCESS) {
        /* 回滚 */
        Ugmanager.num_users--;
        if (u->groups) free(u->groups);
        del_uid(COMMON_UID, uid);
        return ret;
    }
    
    /* 保存到文件 */
    ret = save_User_group_manager();
    if (ret != UG_SUCCESS) {
        /* 回滚操作 */
        Ugmanager.num_users--;
        if (u->groups) free(u->groups);
        del_uid(COMMON_UID, uid);
        rebuild_user_indexes();  /* 重建索引 */
        return ret;
    }
    
    return UG_SUCCESS;
}

/* 删除用户 */
int del_user(const char *username) {
    int ret;
    
    ret = validate_username(username);
    if (ret != UG_SUCCESS) return ret;
    
    int idx = find_user_by_username(username);
    if (idx < 0) return UG_ERR_USER_NOT_FOUND;
    
    user_info_t *user = &Ugmanager.users[idx];
    UID uid = user->uid;
    
    /* 从所有组移除该用户 */
    for (uint32_t i = 0; i < Ugmanager.num_groups; i++) {
        remove_uid_from_group(&Ugmanager.groups[i], uid);
    }
    
    /* 释放用户的组列表 */
    if (user->groups) {
        free(user->groups);
    }
    
    /* 释放UID */
    del_uid(COMMON_UID, uid);
    
    /* 移动后续用户 */
    for (uint32_t i = idx; i < Ugmanager.num_users - 1; i++) {
        Ugmanager.users[i] = Ugmanager.users[i + 1];
    }
    Ugmanager.num_users--;
    
    /* 重建索引（因为索引值变了） */
    ret = rebuild_user_indexes();
    if (ret != UG_SUCCESS) {
        return ret;
    }
    
    /* 保存到文件 */
    ret = save_User_group_manager();
    if (ret != UG_SUCCESS) {
        return ret;
    }
    
    return UG_SUCCESS;
}

/* 设置用户密码 */
int set_user_password(const char *username, const char *old_passwd, const char *new_passwd) {
    int ret;
    
    ret = validate_username(username);
    if (ret != UG_SUCCESS) return ret;
    
    ret = validate_password(new_passwd);
    if (ret != UG_SUCCESS) return ret;
    
    if (!old_passwd) return UG_ERR_INVALID_INPUT;
    
    int idx = find_user_by_username(username);
    if (idx < 0) return UG_ERR_USER_NOT_FOUND;
    
    user_info_t *user = &Ugmanager.users[idx];
    
    /* 验证旧密码 */
    if (bcrypt_checkpw(old_passwd, user->password) != 0) {
        return UG_ERR_PERMISSION_DENIED;
    }
    
    /* 生成新哈希 */
    char salt[BCRYPT_HASHSIZE];
    char hash[BCRYPT_HASHSIZE];
    if (bcrypt_gensalt(12, salt) != 0) return UG_ERR_IO_ERROR;
    if (bcrypt_hashpw(new_passwd, salt, hash) != 0) return UG_ERR_IO_ERROR;
    
    memcpy(user->password, hash, BCRYPT_HASHSIZE);
    
    /* 保存到文件 */
    ret = save_User_group_manager();
    if (ret != UG_SUCCESS) {
        return ret;
    }
    
    return UG_SUCCESS;
}

/* 添加组 */
int add_group(const char *groupname) {
    int ret;
    
    ret = validate_groupname(groupname);
    if (ret != UG_SUCCESS) return ret;
    
    /* 检查组名是否已存在 */
    if (find_group_by_groupname(groupname) >= 0) {
        return UG_ERR_GROUP_EXISTS;
    }
    
    /* 分配GID */
    GID gid = get_gid(COMMON_GID);
    if (gid == merr) return UG_ERR_GID_EXHAUSTED;
    
    /* 扩展组数组 */
    if (Ugmanager.num_groups >= Ugmanager.groups_capacity) {
        uint32_t new_capacity = Ugmanager.groups_capacity == 0 ? 8 : Ugmanager.groups_capacity * 2;
        group_info_t *new_groups = (group_info_t *)realloc(Ugmanager.groups, 
                                                            new_capacity * sizeof(group_info_t));
        if (!new_groups) {
            del_gid(COMMON_GID, gid);
            return UG_ERR_NO_MEMORY;
        }
        Ugmanager.groups = new_groups;
        Ugmanager.groups_capacity = new_capacity;
    }
    
    /* 初始化新组 */
    group_info_t *g = &Ugmanager.groups[Ugmanager.num_groups];
    g->gid = gid;
    strncpy(g->groupname, groupname, sizeof(g->groupname) - 1);
    g->groupname[sizeof(g->groupname) - 1] = '\0';
    g->members = NULL;
    g->num = 0;
    g->members_capacity = 0;
    
    Ugmanager.num_groups++;
    
    /* 更新索引 */
    ret = update_group_index(Ugmanager.num_groups - 1);
    if (ret != UG_SUCCESS) {
        /* 回滚 */
        Ugmanager.num_groups--;
        if (g->members) free(g->members);
        del_gid(COMMON_GID, gid);
        return ret;
    }
    
    /* 保存到文件 */
    ret = save_User_group_manager();
    if (ret != UG_SUCCESS) {
        /* 回滚操作 */
        Ugmanager.num_groups--;
        if (g->members) free(g->members);
        del_gid(COMMON_GID, gid);
        rebuild_group_indexes();  /* 重建索引 */
        return ret;
    }
    
    return UG_SUCCESS;
}

/* 删除组 */
int del_group(const char *groupname) {
    int ret;
    
    ret = validate_groupname(groupname);
    if (ret != UG_SUCCESS) return ret;
    
    int idx = find_group_by_groupname(groupname);
    if (idx < 0) return UG_ERR_GROUP_NOT_FOUND;
    
    group_info_t *group = &Ugmanager.groups[idx];
    GID gid = group->gid;
    
    /* 从所有用户移除该组 */
    for (uint32_t i = 0; i < Ugmanager.num_users; i++) {
        remove_gid_from_user(&Ugmanager.users[i], gid);
    }
    
    /* 释放组的成员列表 */
    if (group->members) {
        free(group->members);
    }
    
    /* 释放GID */
    del_gid(COMMON_GID, gid);
    
    /* 移动后续组 */
    for (uint32_t i = idx; i < Ugmanager.num_groups - 1; i++) {
        Ugmanager.groups[i] = Ugmanager.groups[i + 1];
    }
    Ugmanager.num_groups--;
    
    /* 重建索引（因为索引值变了） */
    ret = rebuild_group_indexes();
    if (ret != UG_SUCCESS) {
        return ret;
    }
    
    /* 保存到文件 */
    ret = save_User_group_manager();
    if (ret != UG_SUCCESS) {
        return ret;
    }
    
    return UG_SUCCESS;
}

/* 添加用户到组 */
int add_user_to_group(UID uid, GID gid) {
    int ret;
    
    int uidx = find_user_by_uid(uid);
    if (uidx < 0) return UG_ERR_USER_NOT_FOUND;
    
    int gidx = find_group_by_gid(gid);
    if (gidx < 0) return UG_ERR_GROUP_NOT_FOUND;
    
    user_info_t *user = &Ugmanager.users[uidx];
    group_info_t *group = &Ugmanager.groups[gidx];
    
    /* 检查用户是否已在组中 */
    if (!user_has_group(user, gid)) {
        /* 扩展用户的组列表 */
        if (user->num_groups >= user->groups_capacity) {
            uint32_t new_capacity = user->groups_capacity == 0 ? 4 : user->groups_capacity * 2;
            GID *new_groups = (GID *)realloc(user->groups, new_capacity * sizeof(GID));
            if (!new_groups) return UG_ERR_NO_MEMORY;
            user->groups = new_groups;
            user->groups_capacity = new_capacity;
        }
        
        /* 如果当前没有主组，插入首位；否则插入末尾 */
        if (user->num_groups == 0) {
            user->groups[0] = gid;
        } else {
            user->groups[user->num_groups] = gid;
        }
        user->num_groups++;
    }
    
    /* 添加用户到组的成员列表 */
    if (!group_has_member(group, uid)) {
        /* 扩展组的成员列表 */
        if (group->num >= group->members_capacity) {
            uint32_t new_capacity = group->members_capacity == 0 ? 4 : group->members_capacity * 2;
            UID *new_members = (UID *)realloc(group->members, new_capacity * sizeof(UID));
            if (!new_members) return UG_ERR_NO_MEMORY;
            group->members = new_members;
            group->members_capacity = new_capacity;
        }
        
        group->members[group->num] = uid;
        group->num++;
    }
    
    /* 保存到文件 */
    ret = save_User_group_manager();
    if (ret != UG_SUCCESS) {
        return ret;
    }
    
    return UG_SUCCESS;
}

/* 从组中删除用户 */
int del_user_from_group(UID uid, GID gid) {
    int ret;
    
    int uidx = find_user_by_uid(uid);
    if (uidx < 0) return UG_ERR_USER_NOT_FOUND;
    
    int gidx = find_group_by_gid(gid);
    if (gidx < 0) return UG_ERR_GROUP_NOT_FOUND;
    
    user_info_t *user = &Ugmanager.users[uidx];
    group_info_t *group = &Ugmanager.groups[gidx];
    
    /* 从用户的组列表移除 */
    remove_gid_from_user(user, gid);
    
    /* 从组的成员列表移除 */
    remove_uid_from_group(group, uid);
    
    /* 保存到文件 */
    ret = save_User_group_manager();
    if (ret != UG_SUCCESS) {
        return ret;
    }
    
    return UG_SUCCESS;
}

/* 通过用户名获取UID */
UID get_uid_by_username(const char *username) {
    if (!username) return UG_ERR_INVALID_USERNAME;
    
    int idx = find_user_by_username(username);
    if (idx < 0) return UG_ERR_USER_NOT_FOUND;
    
    return Ugmanager.users[idx].uid;
}

/* 通过组名获取GID */
GID get_gid_by_groupname(const char *groupname) {
    if (!groupname) return UG_ERR_INVALID_INPUT;
    
    int idx = find_group_by_groupname(groupname);
    if (idx < 0) return UG_ERR_GROUP_NOT_FOUND;
    
    return Ugmanager.groups[idx].gid;
}

/* 获取用户的主组GID */
GID get_primary_gid_by_uid(UID uid) {
    int idx = find_user_by_uid(uid);
    if (idx < 0) return UG_ERR_USER_NOT_FOUND;
    
    user_info_t *user = &Ugmanager.users[idx];
    if (user->num_groups == 0) return UG_ERR_GROUP_NOT_FOUND;
    
    return user->groups[0];
}

/* 判断UID是否合法 */
int is_valid_uid(UID uid) {
    /* 检查UID是否为负数（错误码） */
    if (uid < 0) {
        return 0;
    }
    
    /* 检查UID是否存在于系统中 */
    int idx = find_user_by_uid(uid);
    if (idx < 0) {
        return 0;
    }
    
    return 1;
}

/* 获取用户的所有组GID列表 */
int get_gids_by_uid(UID uid, GID **gids, uint32_t *num_gids) {
    if (!gids || !num_gids) {
        return UG_ERR_INVALID_INPUT;
    }
    
    int idx = find_user_by_uid(uid);
    if (idx < 0) {
        return UG_ERR_USER_NOT_FOUND;
    }
    
    user_info_t *user = &Ugmanager.users[idx];
    
    /* 如果用户没有组 */
    if (user->num_groups == 0) {
        *gids = NULL;
        *num_gids = 0;
        return UG_SUCCESS;
    }
    
    /* 分配内存并复制GID列表 */
    *gids = (GID *)malloc(user->num_groups * sizeof(GID));
    if (!*gids) {
        return UG_ERR_NO_MEMORY;
    }
    
    memcpy(*gids, user->groups, user->num_groups * sizeof(GID));
    *num_gids = user->num_groups;
    
    return UG_SUCCESS;
}

/* 认证 */
int certification(SID session_id, const char *username_to_be_verified,
                  const char *passwd_to_be_verified) {
    if (!username_to_be_verified || !passwd_to_be_verified) {
        return certificate_failed;
    }
    
    UID uid_to_be_verified = get_uid_by_username(username_to_be_verified);
    if (uid_to_be_verified < 0) {
        return certificate_failed;
    }
    
    int idx = find_user_by_uid(uid_to_be_verified);
    if (idx < 0) return certificate_failed;
    
    user_info_t *user = &Ugmanager.users[idx];
    
    /* 验证密码 */
    if (bcrypt_checkpw(passwd_to_be_verified, user->password) != 0) {
        return certificate_failed;
    }
    
    auth_session(session_id, uid_to_be_verified);
    return certificate_success;
}

/* 权限检查实现 */
int is_entitled(HOOK *hook, UID applicant_uid, Mode_type mode) {
    if (!hook) return merr;
    
    /* 判断权限是否已经初始化 */
    if (!hook->pm_s.ifisinit) return merr;
    
    /* 判断是否为所有者 */
    if (hook->owner == applicant_uid) {
        switch (mode) {
            case HOOK_READ: return hook->pm_s.owner_read ? 1 : 0;
            case HOOK_ADD:  return hook->pm_s.owner_add ? 1 : 0;
            case HOOK_CHANGE: return hook->pm_s.owner_change ? 1 : 0;
            default: return merr;
        }
    }
    
    /* 判断是否为组成员 */
    int idx = find_user_by_uid(applicant_uid);
    if (idx >= 0) {
        user_info_t *user = &Ugmanager.users[idx];
        if (user_has_group(user, hook->group)) {
            switch (mode) {
                case HOOK_READ: return hook->pm_s.group_read ? 1 : 0;
                case HOOK_ADD:  return hook->pm_s.group_add ? 1 : 0;
                case HOOK_CHANGE: return hook->pm_s.group_change ? 1 : 0;
                default: return merr;
            }
        }
    }
    
    /* 其他用户 */
    switch (mode) {
        case HOOK_READ: return hook->pm_s.other_read ? 1 : 0;
        case HOOK_ADD:  return hook->pm_s.other_add ? 1 : 0;
        case HOOK_CHANGE: return hook->pm_s.other_change ? 1 : 0;
        default: return merr;
    }
}

/* 保存用户组管理器到文件 */
int save_User_group_manager(void) {
    char user_path[512];
    char group_path[512];
    char temp_user_path[520];
    char temp_group_path[520];
    FILE *fp;
    
    /* 构建文件路径 */
    snprintf(user_path, sizeof(user_path), "%s/etc/user.config", Env.MhuixsHomePath.data);
    snprintf(group_path, sizeof(group_path), "%s/etc/group.config", Env.MhuixsHomePath.data);
    snprintf(temp_user_path, sizeof(temp_user_path), "%s.tmp", user_path);
    snprintf(temp_group_path, sizeof(temp_group_path), "%s.tmp", group_path);
    
    /* 1. 保存用户到临时文件 */
    fp = fopen(temp_user_path, "w");
    if (!fp) return UG_ERR_IO_ERROR;
    
    for (uint32_t i = 0; i < Ugmanager.num_users; i++) {
        user_info_t *u = &Ugmanager.users[i];
        
        /* 写入：username:UID:GID:description:main_hook:password */
        fprintf(fp, "%s:%d:", u->username, u->uid);
        
        /* 写入组列表（用逗号分隔） */
        for (uint32_t j = 0; j < u->num_groups; j++) {
            fprintf(fp, "%d", u->groups[j]);
            if (j < u->num_groups - 1) fprintf(fp, ",");
        }
        
        fprintf(fp, ":%s:%s:%s\n", u->description, u->main_hook, u->password);
    }
    
    if (fclose(fp) != 0) {
        remove(temp_user_path);
        return UG_ERR_IO_ERROR;
    }
    
    /* 2. 保存组到临时文件 */
    fp = fopen(temp_group_path, "w");
    if (!fp) {
        remove(temp_user_path);
        return UG_ERR_IO_ERROR;
    }
    
    for (uint32_t i = 0; i < Ugmanager.num_groups; i++) {
        group_info_t *g = &Ugmanager.groups[i];
        fprintf(fp, "%s:%d\n", g->groupname, g->gid);
    }
    
    if (fclose(fp) != 0) {
        remove(temp_user_path);
        remove(temp_group_path);
        return UG_ERR_IO_ERROR;
    }
    
    /* 3. 原子性替换（使用 rename） */
    if (rename(temp_user_path, user_path) != 0) {
        remove(temp_user_path);
        remove(temp_group_path);
        return UG_ERR_SAVE_FAILED;
    }
    
    if (rename(temp_group_path, group_path) != 0) {
        remove(temp_group_path);
        return UG_ERR_SAVE_FAILED;
    }
    
    return UG_SUCCESS;
}

/* 清理用户组管理器 */
void cleanup_User_group_manager(void) {
    /* 清理所有用户 */
    for (uint32_t i = 0; i < Ugmanager.num_users; i++) {
        if (Ugmanager.users[i].groups) {
            free(Ugmanager.users[i].groups);
        }
    }
    if (Ugmanager.users) {
        free(Ugmanager.users);
        Ugmanager.users = NULL;
    }
    Ugmanager.num_users = 0;
    Ugmanager.users_capacity = 0;
    
    /* 清理所有组 */
    for (uint32_t i = 0; i < Ugmanager.num_groups; i++) {
        if (Ugmanager.groups[i].members) {
            free(Ugmanager.groups[i].members);
        }
    }
    if (Ugmanager.groups) {
        free(Ugmanager.groups);
        Ugmanager.groups = NULL;
    }
    Ugmanager.num_groups = 0;
    Ugmanager.groups_capacity = 0;
    
    /* 清理哈希表索引 */
    if (Ugmanager.username_to_idx) {
        hash_destroy(Ugmanager.username_to_idx, free);
        Ugmanager.username_to_idx = NULL;
    }
    if (Ugmanager.uid_to_idx) {
        hash_destroy(Ugmanager.uid_to_idx, free);
        Ugmanager.uid_to_idx = NULL;
    }
    if (Ugmanager.groupname_to_idx) {
        hash_destroy(Ugmanager.groupname_to_idx, free);
        Ugmanager.groupname_to_idx = NULL;
    }
    if (Ugmanager.gid_to_idx) {
        hash_destroy(Ugmanager.gid_to_idx, free);
        Ugmanager.gid_to_idx = NULL;
    }
}

/* 辅助函数：解析一行配置 */
static char* trim_whitespace(char *str) {
    char *end;
    
    /* 去除前导空格 */
    while (*str == ' ' || *str == '\t' || *str == '\r' || *str == '\n') str++;
    
    if (*str == 0) return str;
    
    /* 去除尾部空格 */
    end = str + strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) end--;
    
    *(end + 1) = '\0';
    return str;
}

/* 辅助函数：分割字符串 */
static int split_string(char *str, char delim, char **tokens, int max_tokens) {
    int count = 0;
    char *start = str;
    
    while (*str && count < max_tokens) {
        if (*str == delim) {
            *str = '\0';
            tokens[count++] = start;
            start = str + 1;
        }
        str++;
    }
    
    if (count < max_tokens && *start) {
        tokens[count++] = start;
    }
    
    return count;
}

/* 初始化用户组管理器 */
int init_User_group_manager(void) {
    char user_path[512];
    char group_path[512];
    FILE *fp;
    char line[1024];
    
    /* 清理旧数据 */
    cleanup_User_group_manager();
    
    /* 构建配置文件路径 */
    snprintf(user_path, sizeof(user_path), "%s/etc/user.config", Env.MhuixsHomePath.data);
    snprintf(group_path, sizeof(group_path), "%s/etc/group.config", Env.MhuixsHomePath.data);
    
    /* 1. 读取 group.config */
    fp = fopen(group_path, "r");
    if (!fp) return merr;
    
    while (fgets(line, sizeof(line), fp)) {
        char *trimmed = trim_whitespace(line);
        if (strlen(trimmed) == 0) continue;
        
        char *tokens[2];
        int token_count = split_string(trimmed, ':', tokens, 2);
        
        if (token_count != 2) {
            fclose(fp);
            cleanup_User_group_manager();
            return UG_ERR_IO_ERROR;
        }
        
        char *groupname = trim_whitespace(tokens[0]);
        char *gidstr = trim_whitespace(tokens[1]);
        
        if (strlen(groupname) == 0 || strlen(gidstr) == 0) {
            fclose(fp);
            cleanup_User_group_manager();
            return UG_ERR_IO_ERROR;
        }
        
        GID gid = (GID)atoi(gidstr);
        
        /* 检查GID和组名是否重复 */
        for (uint32_t i = 0; i < Ugmanager.num_groups; i++) {
            if (Ugmanager.groups[i].gid == gid || 
                strcmp(Ugmanager.groups[i].groupname, groupname) == 0) {
                fclose(fp);
                cleanup_User_group_manager();
                return UG_ERR_IO_ERROR;
            }
        }
        
        /* 扩展组数组 */
        if (Ugmanager.num_groups >= Ugmanager.groups_capacity) {
            uint32_t new_capacity = Ugmanager.groups_capacity == 0 ? 8 : Ugmanager.groups_capacity * 2;
            group_info_t *new_groups = (group_info_t *)realloc(Ugmanager.groups, 
                                                                new_capacity * sizeof(group_info_t));
            if (!new_groups) {
                fclose(fp);
                cleanup_User_group_manager();
                return merr;
            }
            Ugmanager.groups = new_groups;
            Ugmanager.groups_capacity = new_capacity;
        }
        
        /* 添加组 */
        group_info_t *g = &Ugmanager.groups[Ugmanager.num_groups];
        g->gid = gid;
        strncpy(g->groupname, groupname, sizeof(g->groupname) - 1);
        g->groupname[sizeof(g->groupname) - 1] = '\0';
        g->members = NULL;
        g->num = 0;
        g->members_capacity = 0;
        
        Ugmanager.num_groups++;
    }
    
    fclose(fp);
    
    /* 2. 读取 user.config */
    fp = fopen(user_path, "r");
    if (!fp) {
        cleanup_User_group_manager();
        return merr;
    }
    
    while (fgets(line, sizeof(line), fp)) {
        char *trimmed = trim_whitespace(line);
        if (strlen(trimmed) == 0) continue;
        
        char *tokens[6];
        int token_count = split_string(trimmed, ':', tokens, 6);
        
        if (token_count != 6) {
            fclose(fp);
            cleanup_User_group_manager();
            return UG_ERR_IO_ERROR;
        }
        
        char *username = trim_whitespace(tokens[0]);
        char *uidstr = trim_whitespace(tokens[1]);
        char *gidstr = trim_whitespace(tokens[2]);
        char *desc = trim_whitespace(tokens[3]);
        char *main_hook = trim_whitespace(tokens[4]);
        char *passwd = trim_whitespace(tokens[5]);
        
        if (strlen(username) == 0 || strlen(uidstr) == 0 || strlen(gidstr) == 0) {
            fclose(fp);
            cleanup_User_group_manager();
            return UG_ERR_IO_ERROR;
        }
        
        UID uid = (UID)atoi(uidstr);
        
        /* 检查UID和用户名是否重复 */
        for (uint32_t i = 0; i < Ugmanager.num_users; i++) {
            if (Ugmanager.users[i].uid == uid || 
                strcmp(Ugmanager.users[i].username, username) == 0) {
                fclose(fp);
                cleanup_User_group_manager();
                return UG_ERR_IO_ERROR;
            }
        }
        
        /* 扩展用户数组 */
        if (Ugmanager.num_users >= Ugmanager.users_capacity) {
            uint32_t new_capacity = Ugmanager.users_capacity == 0 ? 8 : Ugmanager.users_capacity * 2;
            user_info_t *new_users = (user_info_t *)realloc(Ugmanager.users, 
                                                             new_capacity * sizeof(user_info_t));
            if (!new_users) {
                fclose(fp);
                cleanup_User_group_manager();
                return merr;
            }
            Ugmanager.users = new_users;
            Ugmanager.users_capacity = new_capacity;
        }
        
        /* 添加用户 */
        user_info_t *u = &Ugmanager.users[Ugmanager.num_users];
        u->uid = uid;
        strncpy(u->username, username, sizeof(u->username) - 1);
        u->username[sizeof(u->username) - 1] = '\0';
        strncpy(u->password, passwd, sizeof(u->password) - 1);
        u->password[sizeof(u->password) - 1] = '\0';
        strncpy(u->description, desc, sizeof(u->description) - 1);
        u->description[sizeof(u->description) - 1] = '\0';
        strncpy(u->main_hook, main_hook, sizeof(u->main_hook) - 1);
        u->main_hook[sizeof(u->main_hook) - 1] = '\0';
        
        /* 解析GID列表 */
        u->groups = NULL;
        u->num_groups = 0;
        u->groups_capacity = 0;
        
        char gidstr_copy[256];
        strncpy(gidstr_copy, gidstr, sizeof(gidstr_copy) - 1);
        gidstr_copy[sizeof(gidstr_copy) - 1] = '\0';
        
        char *gid_tokens[32];
        int gid_count = split_string(gidstr_copy, ',', gid_tokens, 32);
        
        for (int i = 0; i < gid_count; i++) {
            char *gid_item = trim_whitespace(gid_tokens[i]);
            if (strlen(gid_item) > 0) {
                GID gid = (GID)atoi(gid_item);
                
                /* 检查GID是否存在 */
                int found = 0;
                for (uint32_t j = 0; j < Ugmanager.num_groups; j++) {
                    if (Ugmanager.groups[j].gid == gid) {
                        found = 1;
                        break;
                    }
                }
                
                if (!found) {
                    fclose(fp);
                    cleanup_User_group_manager();
                    return UG_ERR_IO_ERROR;
                }
                
                /* 扩展用户的组列表 */
                if (u->num_groups >= u->groups_capacity) {
                    uint32_t new_capacity = u->groups_capacity == 0 ? 4 : u->groups_capacity * 2;
                    GID *new_groups = (GID *)realloc(u->groups, new_capacity * sizeof(GID));
                    if (!new_groups) {
                        fclose(fp);
                        cleanup_User_group_manager();
                        return merr;
                    }
                    u->groups = new_groups;
                    u->groups_capacity = new_capacity;
                }
                
                u->groups[u->num_groups] = gid;
                u->num_groups++;
            }
        }
        
        Ugmanager.num_users++;
    }
    
    fclose(fp);
    
    /* 3. 遍历所有组，收集成员 */
    for (uint32_t i = 0; i < Ugmanager.num_groups; i++) {
        group_info_t *group = &Ugmanager.groups[i];
        group->members = NULL;
        group->num = 0;
        group->members_capacity = 0;
        
        for (uint32_t j = 0; j < Ugmanager.num_users; j++) {
            user_info_t *user = &Ugmanager.users[j];
            if (user_has_group(user, group->gid)) {
                /* 扩展组的成员列表 */
                if (group->num >= group->members_capacity) {
                    uint32_t new_capacity = group->members_capacity == 0 ? 4 : group->members_capacity * 2;
                    UID *new_members = (UID *)realloc(group->members, new_capacity * sizeof(UID));
                    if (!new_members) {
                        cleanup_User_group_manager();
                        return merr;
                    }
                    group->members = new_members;
                    group->members_capacity = new_capacity;
                }
                
                group->members[group->num] = user->uid;
                group->num++;
            }
        }
    }
    
    /* 4. 初始化哈希表索引 */
    Ugmanager.username_to_idx = hash_create(Ugmanager.num_users * 2);
    Ugmanager.uid_to_idx = hash_create(Ugmanager.num_users * 2);
    Ugmanager.groupname_to_idx = hash_create(Ugmanager.num_groups * 2);
    Ugmanager.gid_to_idx = hash_create(Ugmanager.num_groups * 2);
    
    if (!Ugmanager.username_to_idx || !Ugmanager.uid_to_idx ||
        !Ugmanager.groupname_to_idx || !Ugmanager.gid_to_idx) {
        cleanup_User_group_manager();
        return UG_ERR_NO_MEMORY;
    }
    
    /* 5. 建立索引 */
    int ret = rebuild_user_indexes();
    if (ret != UG_SUCCESS) {
        cleanup_User_group_manager();
        return ret;
    }
    
    ret = rebuild_group_indexes();
    if (ret != UG_SUCCESS) {
        cleanup_User_group_manager();
        return ret;
    }
    
    return UG_SUCCESS;
}
