#include <stdint.h>
/*
#版权所有 (c) Mhuixs-team 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2024.11
Email:hj18914255909@outlook.com
*/
#ifndef STACK_H
#define STACK_H

/*
STACK 栈
栈中每个成员在内存中是连续紧挨着的。
因此，栈的核心优势在于非常节约内存。
同时，内存是整块的，对象压缩较容易。
*/

typedef struct STACK{
    uint8_t* data;
    uint32_t data_size;
    /*
    data指向整个栈的数据区
    size是栈的占用大小
    */
    uint32_t* elem_offset;
    uint32_t elem_num;
    /*
    element元素
    elem_offset[elem_num] => 栈顶元素的起始位置偏移量
    */
}STACK;

STACK* makeSTACK();
/*
创建一个栈对象
STACK *object = makeSTACK();
*/
void freeSTACK(STACK* stack);
/*
释放栈对象
freeSTACK(object);
*/
int pushintoSTACK(STACK* stack,uint8_t* bitestream,uint32_t len);
/*
将长度为len的数据压入栈
pushintoSTACK(object,bitestream,len);
*/
int popfromSTACK(STACK* stack,uint8_t* bitestream,uint32_t len);
/*
将栈顶元素弹出
popfromSTACK(object,bitestream,len);
*/
#endif