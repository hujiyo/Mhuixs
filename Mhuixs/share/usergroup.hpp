#ifndef USERGROUP_HPP
#define USERGROUP_HPP
/*
#版权所有 (c) HUJI 2025
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2025.5
Email:hj18914255909@outlook.com
*/
#include <string>
#include <vector>

#include <algorithm>

#include <string.h>
#include <fstream>
#include <sstream>
#include <set>

#include "getid.hpp"
#include "hook.hpp"
#include "env.hpp"
#include "ml ib/bcrypt.h"

using namespace std;

#define merr -1
/*
用户/组管理模块
封装为一个对象类
不考虑线程安全
*/
struct user_info_struct {
    UID uid;// 用户ID
    string username;// 用户名
    string password; // 密码(哈希后)
    vector<GID> groups; // 所属组列表（第一个为主组，后续为附加组，所有操作需保证主组始终在首位）
    uint32_t num_groups; // 所属组数量(刚创建时为0)
    string main_hook; // 主钩子名
    string description; // 用户描述
};

struct group_info_struct {
    GID gid;// 组ID
    string groupname; // 组名    
    vector<UID> members; // 成员列表
    uint32_t num; // 成员数量
};

class User_group_manager {
private:
    vector<user_info_struct> users; // 用户列表
    vector<group_info_struct> groups; // 组列表
public:
    User_group_manager() = default;
    ~User_group_manager() = default;

    // 密码参数均为明文，内部自动哈希
    // 注意：add_user_to_group/del_user_from_group等所有组操作，必须保证groups[0]始终为主组，后续为附加组
    int add_user(string username, string passwd); // 添加用户时必须设置明文密码
    int del_user(string username);//删除用户
    int set_user_password(string username, string old_passwd, string new_passwd); // old_passwd为明文，验证后设置新明文密码
    
    int add_group(string groupname);//添加组
    int del_group(string groupname);//删除组

    // add_user_to_group: 若添加的gid为主组，插入到groups[0]，否则插入末尾，且不重复
    int add_user_to_group(UID uid,GID gid);//添加用户到组中
    // del_user_from_group: 若删除groups[0]且有附加组，则自动提升下一个为主组
    int del_user_from_group(UID uid,GID gid);//删除用户从组中

    UID get_uid_by_username(string username);//通过用户名获取用户ID
    GID get_gid_by_groupname(string groupname);//通过组名获取组ID

    int is_entitled(HOOK &hook,UID applicant_uid,Mode_type mode);//检查用户是否有权限访问钩子 1有权限 0无权限 -1错误

    GID get_primary_gid_by_uid(UID uid);

    friend int init_User_group_manager();
};

extern User_group_manager Ugmanager;//用户 组管理器

int init_User_group_manager();

/*
用户、用户组及权限信息通过特定文件存储

[用户信息存储]
文件路径：%MhuixsHomePath%/etc/user.config
数据格式：每行一个用户，字段以冒号（:）分隔，共6个字段：
username:UID:GID:description:main_hook:password

username:用户名
UID：用户ID
GID：所属组ID(可以多个GID,用','隔开)
description：用户描述
main_hook：主钩子
password：用户密码（哈希值（数字明文保存））

例如:
root:0:0:root user:root hook:1949

[用户组信息存储]
文件路径：%MhuixsHomePath%/etc/group.config
数据格式：每行一个组，字段以冒号分隔，共2个字段：
groupname:GID

示例：
developers:1001

*/

#endif
