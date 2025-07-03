#include "fcall.hpp"

#define FUNC_NUM 1024
/*
给每个函数分配一个编号,通过编号即可调用函数
*/
//void funtable[FUNC_NUM];

int fun_call_init(){
    
    return 0;
}
enum FunID{
    HOOK_CREATE,//创建HOOK
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
