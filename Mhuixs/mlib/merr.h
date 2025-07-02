#ifndef MERR_H
#define MERR_H

#include <stdio.h>

//Mhuixs return codes
enum mrc {
    success = 0,//成功
    
    merr = -1,// 通用错误码
    merr_open_file,// 文件打开错误

    
    //...
    //...
};


void report(mrc code) //报告错误
{
    switch (code) {
        case merr:printf("[Error]:Random error\n");break;
        case merr_open_file:printf("[Error]:Failed to open file\n");break;
        default:printf("[Error]:Unknown error\n");
    }
}








#endif