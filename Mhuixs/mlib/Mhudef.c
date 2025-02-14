#include "Mhudef.h"


int _IS_LITTLE_ENDIAN_=1;//全局变量:是否是小端 0-否 1-是

void* retkeyobject(void* bhs)
{
    /*
    返回引用指向的数据结构对象的结构体指针
    KEY和HOOK都是数据结构对象的引用

    bhs:指向KEY或HOOK的指针
    */
    if(bhs==NULL){
        return NULL;
    }
    return ((BHS*)bhs)->handle;
}

OBJECTYPE retkeytype(void* bhs)
{
    /*
    返回引用指向的数据结构对象的类型
    KEY和HOOK都是数据结构对象的引用

    bhs:指向KEY或HOOK的指针
    */
    if(bhs==NULL){
        return M_NULL;
    }
    return ((BHS*)bhs)->type;
}



/*
//########################################################
    //对象识别模块：识别每个stmt的操作对象类型，需要识别出隐性操作对象
    //########################################################

    /*
    下一句语言如果没有明确指定操作对象，
    那么默认操作对象为当前stmt的上一句的操作对象
    */
    /*
    标准语句下,可以实现操作对象切换的语句有这些：
    标准 HOOK_CHECKOUT 语句：[HOOK objname;]，该语句用于手动切换到一个已经存在的操作对象。
    标准 HOOK_MAKE 语句：[HOOK objtype objname1 objname2 ...;]，使用钩子创建一个操作对象，在创建后操作对象也会切换到新创建的对象。
    标准 HOOK 语句：[HOOK;]，回归 HOOK 根，此时无数据操作对象，相当于将操作对象切换为无实际数据操作对象的 HOOK 根状态。
    标准 HOOK_DEL_OGJ 语句：[HOOK DEL objname1 objname2 ...;]，删除指定的 HOOK 操作对象，然后回归 HOOK 根，此时无数据操作对象，也实现了操作对象的切换。
    标准 KEY_CHECKOUT 语句：[KEY key;]，进入键对象操作，将操作对象切换为指定的键对象。
    标准 BACK 语句：[BACK;]
    */
    
    for(int i = 0; i < stmts.num; i++){
        //获取当前语句
        stmt* curstmt = &stmts.stmt[i];

        //开始寻找当前语句的操作对象
        //switch()

        
    }
*/