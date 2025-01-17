#ifndef GETID_H
#define GETID_H
#include "Mhudef.h"
/*
#版权所有 (c) Mhuixs-team 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2024.11
Email:hj18914255909@outlook.com
*/

int init_getid(void);//初始化ID分配器
int getid(char IDTYPE);//获得相应种类的ID
int delid(char IDTYPE,uint16_t id);//删除相应种类的ID

#define ADMIN_ID 0
#define HUMAN_ID 1
#define AI_ID 2
#define GUEST_ID 3
#define GROUP_ID 4
#define HOOK_ID 5

/*
extern userid_t _ADMIN_ID_ ;//全局变量:ADMIN ID分配器
extern userid_t _AI_ID_ ;//全局变量:AI ID分配器
extern userid_t _HUMAIN_ID_ ;//全局变量:用户ID分配器
extern userid_t _GUEST_ID_ ;//全局变量:GUEST ID分配器
extern groupid_t _GROUP_ID_ ;//全局变量:当前组ID分配器
extern hookid_t _HOOK_ID_ ;//全局变量:当前钩子ID分配器
*/

#endif