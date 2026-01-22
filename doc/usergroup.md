# usergroup 模块技术文档

## 概述

纯 C 实现的用户组管理模块，支持用户/组的增删改查、权限验证、密码管理。

**特点：**
- O(1) 查找性能（哈希表索引）
- 数据持久化（自动保存）
- 详细错误处理
- 输入验证

**性能：**
- 支持 10000+ 用户
- 查找速度：40-50 万次/秒

---

## API 参考

### 初始化与清理

```c
int init_User_group_manager(void);      // 初始化，从配置文件加载
void cleanup_User_group_manager(void);  // 清理，释放内存
int save_User_group_manager(void);      // 手动保存（修改操作会自动保存）
```

### 用户管理

```c
int add_user(const char *username, const char *passwd);
int del_user(const char *username);
int set_user_password(const char *username, const char *old_passwd, const char *new_passwd);
UID get_uid_by_username(const char *username);
```

### 组管理

```c
int add_group(const char *groupname);
int del_group(const char *groupname);
GID get_gid_by_groupname(const char *groupname);
```

### 用户组关系

```c
int add_user_to_group(UID uid, GID gid);
int del_user_from_group(UID uid, GID gid);
GID get_primary_gid_by_uid(UID uid);
```

### 认证与权限

```c
int certification(SID session_id, const char *username, const char *passwd);
int is_entitled(HOOK *hook, UID applicant_uid, Mode_type mode);
```

### 错误处理

```c
const char* ug_strerror(int error_code);  // 获取错误消息
```

**错误码：**
```c
UG_SUCCESS              // 0: 成功
UG_ERR_USER_EXISTS      // -100: 用户已存在
UG_ERR_USER_NOT_FOUND   // -101: 用户不存在
UG_ERR_GROUP_EXISTS     // -102: 组已存在
UG_ERR_GROUP_NOT_FOUND  // -103: 组不存在
UG_ERR_INVALID_USERNAME // -104: 无效用户名
UG_ERR_INVALID_PASSWORD // -105: 无效密码
UG_ERR_NO_MEMORY        // -106: 内存不足
UG_ERR_IO_ERROR         // -107: I/O 错误
// ... 更多错误码见 usergroup.h
```

---

## 使用示例

```c
#include "lib/usergroup.h"

// 初始化
if (init_User_group_manager() != UG_SUCCESS) {
    fprintf(stderr, "初始化失败\n");
    return -1;
}

// 添加用户
int ret = add_user("alice", "Password123");
if (ret != UG_SUCCESS) {
    fprintf(stderr, "添加用户失败: %s\n", ug_strerror(ret));
}

// 查找用户
UID uid = get_uid_by_username("alice");
if (uid < 0) {
    fprintf(stderr, "用户不存在: %s\n", ug_strerror(uid));
}

// 添加组
add_group("developers");
GID gid = get_gid_by_groupname("developers");

// 添加用户到组
add_user_to_group(uid, gid);

// 认证
SID sid = 12345;
if (certification(sid, "alice", "Password123") == certificate_success) {
    printf("认证成功\n");
}

// 清理
cleanup_User_group_manager();
```

---

## 配置文件

### 用户配置：`%MhuixsHomePath%/etc/user.config`

格式：`username:UID:GID:description:main_hook:password`

示例：
```
root:0:0:root user:root hook:$2b$12$...
alice:1001:1001,1002:Alice User:alice hook:$2b$12$...
```

### 组配置：`%MhuixsHomePath%/etc/group.config`

格式：`groupname:GID`

示例：
```
root:0
developers:1001
admins:1002
```

---

## 输入验证规则

**用户名：**
- 长度：1-63 字符
- 字符：字母、数字、下划线、连字符
- 不能以数字开头

**密码：**
- 长度：8-128 字符
- 必须包含：大写字母、小写字母、数字

**组名：**
- 规则同用户名

---

## 性能特性

**哈希表索引：**
- 4 个索引：username→idx, uid→idx, groupname→idx, gid→idx
- 算法：MurmurHash3
- 查找复杂度：O(1)

**性能数据（1000 用户）：**
- 查找 10000 次：~20 毫秒
- 吞吐量：500000 查找/秒
- 提升：比线性查找快 1250 倍

---

## 编译

```bash
# 编译哈希表
gcc -c src/lib/usergroup_hash.c -o usergroup_hash.o -I src -std=c99

# 编译 usergroup
gcc -c src/lib/usergroup.c -o usergroup.o -I src -std=c99

# 链接时包含：usergroup.o usergroup_hash.o
```

---

## 注意事项

1. **非线程安全**：设计为单线程调用
2. **自动保存**：所有修改操作会自动保存到文件
3. **密码哈希**：使用 bcrypt（cost=12）
4. **原子性保存**：使用临时文件 + rename 保证数据安全
