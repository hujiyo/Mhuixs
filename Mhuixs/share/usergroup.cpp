#include "usergroup.hpp"
/*
#版权所有 (c) HUJI 2025
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2025.5
Email:hj18914255909@outlook.com
*/

int User_group_manager::add_user(string username, string passwd) {
    // 检查用户名是否已存在
    for (const auto& user : users) {
        if (user.username == username) return merr;
    }
    // 分配UID
    Id_alloctor idalloc;
    UID uid = idalloc.get_uid(COMMON_UID);
    if (uid == merr) return merr;
    user_info_struct u;
    u.uid = uid;
    u.username = username;
    // 密码哈希
    char salt[BCRYPT_HASHSIZE];
    char hash[BCRYPT_HASHSIZE];
    if (bcrypt_gensalt(12, salt) != 0) return merr;
    if (bcrypt_hashpw(passwd.c_str(), salt, hash) != 0) return merr;
    u.password = string(hash);
    u.groups.clear();
    u.num_groups = 0;
    users.push_back(u);
    return 0;
}

int User_group_manager::set_user_password(string username, string old_passwd, string new_passwd) {
    for (auto& user : users) {
        if (user.username == username) {
            // 验证旧密码
            if (bcrypt_checkpw(old_passwd.c_str(), user.password.c_str()) != 0) return merr;
            // 生成新哈希
            char salt[BCRYPT_HASHSIZE];
            char hash[BCRYPT_HASHSIZE];
            if (bcrypt_gensalt(12, salt) != 0) return merr;
            if (bcrypt_hashpw(new_passwd.c_str(), salt, hash) != 0) return merr;
            user.password = string(hash);
            return 0;
        }
    }
    return merr;
}

int User_group_manager::del_user(string username) {
    auto it = std::find_if(users.begin(), users.end(), [&](const user_info_struct& u) {
        return u.username == username;
    });
    if (it == users.end()) return merr;
    UID uid = it->uid;
    // 从所有组移除该用户
    for (auto& group : groups) {
        auto mit = std::find(group.members.begin(), group.members.end(), uid);
        if (mit != group.members.end()) {
            group.members.erase(mit);
            group.num = group.members.size();
        }
    }
    // 释放UID
    Id_alloctor idalloc;
    idalloc.del_uid(COMMON_UID, uid);
    users.erase(it);
    return 0;
}

int User_group_manager::add_group(string groupname) {
    // 检查组名是否已存在
    for (const auto& group : groups) {
        if (group.groupname == groupname) return merr;
    }
    // 分配GID
    Id_alloctor idalloc;
    GID gid = idalloc.get_gid(MY_GID);
    if (gid == merr) return merr;
    group_info_struct g;
    g.gid = gid;
    g.groupname = groupname;
    g.members.clear();
    g.num = 0;
    groups.push_back(g);
    return 0;
}

int User_group_manager::del_group(string groupname) {
    auto it = std::find_if(groups.begin(), groups.end(), [&](const group_info_struct& g) {
        return g.groupname == groupname;
    });
    if (it == groups.end()) return merr;
    GID gid = it->gid;
    // 从所有用户移除该组
    for (auto& user : users) {
        auto git = std::find(user.groups.begin(), user.groups.end(), gid);
        if (git != user.groups.end()) {
            user.groups.erase(git);
            user.num_groups = user.groups.size();
        }
    }
    // 释放GID
    Id_alloctor idalloc;
    idalloc.del_gid(MY_GID, gid);
    groups.erase(it);
    return 0;
}

int User_group_manager::add_user_to_group(UID uid, GID gid) {
    // 查找用户
    auto uit = std::find_if(users.begin(), users.end(), [&](const user_info_struct& u) {
        return u.uid == uid;
    });
    if (uit == users.end()) return merr;
    // 查找组
    auto git = std::find_if(groups.begin(), groups.end(), [&](const group_info_struct& g) {
        return g.gid == gid;
    });
    if (git == groups.end()) return merr;
    // 检查是否已在组中
    if (std::find(uit->groups.begin(), uit->groups.end(), gid) == uit->groups.end()) {
        uit->groups.push_back(gid);
        uit->num_groups = uit->groups.size();
    }
    if (std::find(git->members.begin(), git->members.end(), uid) == git->members.end()) {
        git->members.push_back(uid);
        git->num = git->members.size();
    }
    return 0;
}

int User_group_manager::del_user_from_group(UID uid, GID gid) {
    // 查找用户
    auto uit = std::find_if(users.begin(), users.end(), [&](const user_info_struct& u) {
        return u.uid == uid;
    });
    if (uit == users.end()) return merr;
    // 查找组
    auto git = std::find_if(groups.begin(), groups.end(), [&](const group_info_struct& g) {
        return g.gid == gid;
    });
    if (git == groups.end()) return merr;
    // 从用户组列表移除
    auto ugit = std::find(uit->groups.begin(), uit->groups.end(), gid);
    if (ugit != uit->groups.end()) {
        uit->groups.erase(ugit);
        uit->num_groups = uit->groups.size();
    }
    // 从组成员列表移除
    auto gmit = std::find(git->members.begin(), git->members.end(), uid);
    if (gmit != git->members.end()) {
        git->members.erase(gmit);
        git->num = git->members.size();
    }
    return 0;
}

UID User_group_manager::get_uid_by_username(string username) {
    for (const auto& user : users) {
        if (user.username == username) return user.uid;
    }
    return merr;
}

GID User_group_manager::get_gid_by_groupname(string groupname) {
    for (const auto& group : groups) {
        if (group.groupname == groupname) return group.gid;
    }
    return merr;
}

int User_group_manager::is_entitled(HOOK &hook, UID applicant_uid, Mode_type mode) {
    // 权限检查
    // 1. 判断是否为所有者
    if (hook.owner == applicant_uid) {
        switch (mode) {
            case HOOK_READ: return hook.pm_s.owner_read ? 1 : 0;
            case HOOK_ADD:  return hook.pm_s.owner_add ? 1 : 0;
            case HOOK_DEL:  return hook.pm_s.owner_del ? 1 : 0;
            default: return merr;
        }
    }
    // 2. 判断是否为组成员
    GID group = hook.group;
    auto uit = std::find_if(users.begin(), users.end(), [&](const user_info_struct& u) {
        return u.uid == applicant_uid;
    });
    if (uit != users.end()) {
        if (std::find(uit->groups.begin(), uit->groups.end(), group) != uit->groups.end()) {
            switch (mode) {
                case HOOK_READ: return hook.pm_s.group_read ? 1 : 0;
                case HOOK_ADD:  return hook.pm_s.group_add ? 1 : 0;
                case HOOK_DEL:  return hook.pm_s.group_del ? 1 : 0;
                default: return merr;
            }
        }
    }
    // 3. 其他用户
    switch (mode) {
        case HOOK_READ: return hook.pm_s.other_read ? 1 : 0;
        case HOOK_ADD:  return hook.pm_s.other_add ? 1 : 0;
        case HOOK_DEL:  return hook.pm_s.other_del ? 1 : 0;
        default: return merr;
    }
}

int init_User_group_manager() {    
    string user_path = Env.MhuixsHomePath + "/etc/user.config";
    string group_path = Env.MhuixsHomePath + "/etc/group.config";

    Ugmanager = User_group_manager(); // 清空原有数据

    // 1. 读取 group.config，先加载所有组
    ifstream gfin(group_path);
    if (!gfin.is_open()) return merr;
    string gline;
    set<GID> gid_set;
    set<string> groupname_set;
    while (getline(gfin, gline)) {
        if (gline.empty()) continue;
        stringstream ss(gline);
        string groupname, gidstr;
        if (!getline(ss, groupname, ':')) return 1;
        if (!getline(ss, gidstr, ':')) return 1;
        if (groupname.empty() || gidstr.empty()) return 1;
        GID gid = static_cast<GID>(stoul(gidstr));
        if (gid_set.count(gid) > 0) return 1; // GID重复
        if (groupname_set.count(groupname) > 0) return 1; // 组名重复
        gid_set.insert(gid);
        groupname_set.insert(groupname);
        group_info_struct g;
        g.groupname = groupname;
        g.gid = gid;
        g.members.clear();
        g.num = 0;
        Ugmanager.groups.push_back(g);
    }
    gfin.close();

    // 2. 读取 user.config，加载所有用户
    ifstream ufin(user_path);
    if (!ufin.is_open()) return merr;
    string uline;
    set<UID> uid_set;
    set<string> username_set;
    while (getline(ufin, uline)) {
        if (uline.empty()) continue;
        stringstream ss(uline);
        string username, uidstr, gidstr, desc, main_hook, passwd;
        if (!getline(ss, username, ':')) return 1;
        if (!getline(ss, uidstr, ':')) return 1;
        if (!getline(ss, gidstr, ':')) return 1;
        if (!getline(ss, desc, ':')) return 1;
        if (!getline(ss, main_hook, ':')) return 1;
        if (!getline(ss, passwd, ':')) return 1;
        if (username.empty() || uidstr.empty() || gidstr.empty()) return 1;
        UID uid = static_cast<UID>(stoul(uidstr));
        if (uid_set.count(uid) > 0) return 1; // UID重复
        if (username_set.count(username) > 0) return 1; // 用户名重复
        uid_set.insert(uid);
        username_set.insert(username);
        user_info_struct u;
        u.username = username;
        u.uid = uid;
        u.password = passwd;
        u.description = desc;
        u.main_hook = main_hook;
        u.groups.clear();
        // 解析 GID 列表
        stringstream gidss(gidstr);
        string giditem;
        while (getline(gidss, giditem, ',')) {
            if (!giditem.empty()) {
                GID gid = static_cast<GID>(stoul(giditem));
                if (gid_set.count(gid) == 0) return 1; // 用户引用了不存在的组
                u.groups.push_back(gid);
            }
        }
        u.num_groups = u.groups.size();
        Ugmanager.users.push_back(u);
    }
    ufin.close();

    // 3. 遍历所有组，收集成员
    for (auto& group : Ugmanager.groups) {
        group.members.clear();
        for (const auto& user : Ugmanager.users) {
            if (find(user.groups.begin(), user.groups.end(), group.gid) != user.groups.end()) {
                group.members.push_back(user.uid);
            }
        }
        group.num = group.members.size();
    }
    return 0;
}