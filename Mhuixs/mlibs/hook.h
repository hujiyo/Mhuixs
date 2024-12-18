/*
#版权所有 (c) Mhuixs-team 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2024.11
Email:hj18914255909@outlook.com
*/
#include <stdint.h>
#include "Mhudef.h"

/*
hook在Mhuixs中被用来链接所有"数据结构"
此外hook也可以被hook、hook-tree链接
*/
#define hooktype_TABLE

typedef struct HOOK{
    void* handle;//指向任意数据结构描述符
    char type;//数据类型
    char* rank;//保护等级
    OWNER_ID owner;
    GROUP_ID group;
    HOOK_ID hook_id;
}HOOK;
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/*
tree在Mhuixs中不是一个数据结构
而是专门用来存储hook的
*/
typedef struct TREE_NODE{
    TREE_NODE *next_node;
    uint32_t next_node_num;
    TREE_NODE *pre_node;//如果是根节点，那么pre_node为NULL

    void* hook;
    char* hook_group;
    uint32_t hook_num;
}TREE_NODE;