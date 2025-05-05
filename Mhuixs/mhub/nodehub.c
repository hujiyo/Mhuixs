/*
#版权所有 (c) Mhuixs-team 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2024.11
Email:hj18914255909@outlook.com
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

//含所有数据结构的头文件
#include "datstrc.h"
/*
下面的queue是完全可以被list替换的，
在最新的速度测试中，基于strmap.h进行内存分配的list库
的执行速度要比基于calloc和realloc的queue快一点
*/


typedef struct COMMEND{
    uint32_t command; // 命令类型
    char*    object; // 对象名
    uint32_t param1;      // 参数1
    uint32_t param2;      // 参数2
    uint32_t param3;      // 参数3
    //提供两个指针参数，有些函数要用到的
    uint32_t* param4;      // 参数4
    uint32_t* param5;      // 参数5
    uint32_t* param6;      // 参数6
} COMMEND;

uint8_t* run_commend(uint8_t* data, uint32_t len, COMMEND* commend){

}