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
#include <unordered_map>
#include "getid.hpp"
using namespace std;
/*
用户/组管理模块
封装为一个对象类
之后考虑加入线程安全
*/
struct UserInfo {
    UID uid;// 用户ID
    string username;// 用户名
    string password; // 密码(哈希后)
    //其它信息会在后续版本中添加
};

struct GroupInfo {
    GID gid;// 组ID
    string groupname; // 组名    
    vector<UID> members; // 成员列表
    uint32_t num; // 成员数量
    //其它信息会在后续版本中添加
};

class UserGroupManager {
public:
    unordered_map<string, UserInfo> users; // 用户名->用户信息
    unordered_map<string, GroupInfo> groups; // 组名->组信息

    // 通过uid查找用户
    const UserInfo* get_user(UID uid) const;
    // 通过gid查找组
    const GroupInfo* get_group(GID gid) const;
    // 判断用户是否属于某组
    bool user_in_group(UID uid, GID gid) const;
    // 通过用户名查找用户
    const UserInfo* get_user_by_name(const string& username) const;
    // 通过组名查找组
    const GroupInfo* get_group_by_name(const string& groupname) const;
    // 添加用户
    bool add_user(const UserInfo& user);
    // 添加组
    bool add_group(const GroupInfo& group);
};

#endif
