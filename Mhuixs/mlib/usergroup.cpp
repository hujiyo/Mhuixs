 #include "usergroup.hpp"
using namespace std;

const UserInfo* UserGroupManager::get_user(UID uid) const {
    for (const auto& kv : users) {
        if (kv.second.uid == uid) return &kv.second;
    }
    return nullptr;
}

const GroupInfo* UserGroupManager::get_group(GID gid) const {
    for (const auto& kv : groups) {
        if (kv.second.gid == gid) return &kv.second;
    }
    return nullptr;
}

bool UserGroupManager::user_in_group(UID uid, GID gid) const {
    const GroupInfo* group = get_group(gid);
    if (!group) return false;
    for (UID member : group->members) {
        if (member == uid) return true;
    }
    return false;
}

const UserInfo* UserGroupManager::get_user_by_name(const string& username) const {
    auto it = users.find(username);
    if (it != users.end()) return &it->second;
    return nullptr;
}

const GroupInfo* UserGroupManager::get_group_by_name(const string& groupname) const {
    auto it = groups.find(groupname);
    if (it != groups.end()) return &it->second;
    return nullptr;
}

bool UserGroupManager::add_user(const UserInfo& user) {
    if (users.count(user.username)) return false;
    users[user.username] = user;
    return true;
}

bool UserGroupManager::add_group(const GroupInfo& group) {
    if (groups.count(group.groupname)) return false;
    groups[group.groupname] = group;
    return true;
}
