#ifndef GETID_HPP
#define GETID_HPP
#include "Mhudef.hpp"
/*
#版权所有 (c) HuJi 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2024.11
Email:hj18914255909@outlook.com
*/

/*
ID分配器
会话ID：1-65535 其中：1-99为管理员ID 100-999为用户ID 1000-9999为AI ID 10000-65535为游客ID
组ID：0-65535
*/
int init_getid(void);//初始化ID分配器
int getid(char IDTYPE);//获得相应种类的ID
int delid(char IDTYPE,uint16_t id);//删除相应种类的ID

//ID类型 ：IDTYPE
#define ADMIN_ID 0
#define HUMAN_ID 1
#define AI_ID 2
#define GUEST_ID 3

#define USER_ID 6 //上面4个ID类型的并称为用户ID

#define GROUP_ID 4



#endif