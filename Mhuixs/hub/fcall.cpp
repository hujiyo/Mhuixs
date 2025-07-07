#include "fcall.hpp"

#define FUNC_NUM 1024

//void funtable[FUNC_NUM];

/*
给每个函数分配一个名字（编号）,通过编号即可调用函数
*/
enum FunID{
    HOOK_CREATE,//创建HOOK
    //...    
};

mrc Mhuixscall(UID caller,basic_handle_struct bhs,FunID funseq,int argc,void* argv[])
{
    switch(funseq){
        case HOOK_CREATE:
            // [hook type,new hook name]
            void* mem = malloc(sizeof(HOOK));
            HOOK *hook = new (mem) HOOK(caller,*(string*)argv[1]);

            
            break;
        case :
            // Call function B
            break;
    }
}

typedef struct COMMEND{
    UID caller;//调用者
    basic_handle_struct bhs;//句柄
    FunID funseq;//函数编号
    int argc;//参数数目
    void* argv[];//参数列表
} COMMEND;