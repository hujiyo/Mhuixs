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

#include "getid.hpp"
#include "hook.hpp"
#include "bcrypt.h" // 新增
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
    vector<GID> groups; // 所属组列表
    uint32_t num_groups; // 所属组数量(刚创建时为0)
    //其它信息会在后续版本中添加
};

struct group_info_struct {
    GID gid;// 组ID
    string groupname; // 组名    
    vector<UID> members; // 成员列表
    uint32_t num; // 成员数量
    //其它信息会在后续版本中添加
};

class User_group_manager {
private:
    vector<user_info_struct> users; // 用户列表
    vector<group_info_struct> groups; // 组列表
public:
    User_group_manager();
    ~User_group_manager();

    // 密码参数均为明文，内部自动哈希
    int add_user(string username, string passwd); // 添加用户时必须设置明文密码
    int set_user_password(string username, string old_passwd, string new_passwd); // old_passwd为明文，验证后设置新明文密码
    int del_user(string username);//删除用户

    int add_group(string groupname);//添加组
    int del_group(string groupname);//删除组

    int add_user_to_group(UID uid,GID gid);//添加用户到组中
    int del_user_from_group(UID uid,GID gid);//删除用户从组中

    UID get_uid_by_username(string username);//通过用户名获取用户ID
    GID get_gid_by_groupname(string groupname);//通过组名获取组ID

    int is_entitled(HOOK &hook,UID applicant_uid,Mode_type mode);//检查用户是否有权限访问钩子 1有权限 0无权限 -1错误

};

#endif
