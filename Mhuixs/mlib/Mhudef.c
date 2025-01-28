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