#ifndef MERR_H
#define MERR_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include "log.h"

//Mhuixs return codes
enum mrc {
    success = 0,//成功
    
    merr = -1,// 通用错误码！
    merr_open_file,// 文件打开错误！

    register_failed,//hook注册失败！
    hook_already_registered,//hook已经注册,禁止重复注册！
    null_hook,//hook为空！

    permission_denied,//权限不足！
    

    
    //...
    //...
};


void report(mrc code) //报告错误
{
    char* message;
    int iflog = 0;//是否记录错误

    switch (code) 
    {
        case merr:
            message = "[Error]:Random error\n",iflog=1;break;
        case merr_open_file:
            message="[Error]:Failed to open file\n",iflog=1;break;
        case register_failed:
            message="[Error]:Failed to register hook\n",iflog=1;break;
        case hook_already_registered:
            message="[Hint]:Hook already registered\n";break;
        case null_hook: 
            message="[Error]:Hook is null\n",iflog=1;break;
        case permission_denied:
            message="[Error]:Permission denied\n";break;

        default:
            message="[Error]:Unknown error\n",iflog=1;
    }
    if(iflog) log(message);
    printf("%s",message);
}
void report(const char* err_message){
    log((char*)err_message);
    printf("%s",err_message);
}





#ifdef __cplusplus
}
#endif

#endif