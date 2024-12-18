#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifndef STACK_H
#define STACK_H
/*
#版权所有 (c) Mhuixs-team 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2024.11
Email:hj18914255909@outlook.com
*/

typedef struct STACK{
    uint8_t* data;
    uint32_t data_size;
    uint32_t* elem_offset;    
    uint32_t elem_num;
}STACK;

int initSTACK(STACK* stack,uint32_t max_size){
    if(stack == NULL)return 1;
    stack->data = NULL;
    stack->elem_offset = NULL;
    stack->data_size = 0;
    stack->elem_num = 0;
    return 0;
}
void deleteSTACK(STACK* stack){
    if(stack == NULL)return;
    free(stack->data);
    free(stack->elem_offset);
    stack->data_size = 0;
    stack->elem_num = 0;
}
int pushintoSTACK(STACK* stack,uint8_t* stream,uint32_t len)
{
    if(stack == NULL)return 1;
    uint8_t* newdata = (uint8_t*)realloc(stack->data,stack->data_size+len);
    if(newdata == NULL)return 1;
    uint32_t* p_offset = (uint32_t*)realloc(stack->elem_offset,(stack->elem_num + 1) * sizeof(uint32_t));
    if(p_offset == NULL)return 1;//不用还原data
    stack->elem_offset = p_offset;    
    stack->elem_offset[stack->elem_num] = stack->data_size;
    stack->data_size += len;
    stack->data = newdata;
    memcpy(stack->data + stack->elem_offset[stack->elem_num], stream, len); // 从输入流复制数据到栈的数据区  
    return stack->elem_num++;
}
int popfromSTACK(STACK* stack,uint8_t* stream,uint32_t len)
{
    //你必须保证stream的大小足够容纳栈顶元素
    if(stack == NULL||stack->elem_num == 0)return 1;
    stack->elem_num--;
    if(len > stack->data_size - stack->elem_offset[stack->elem_num])return 1;
    memcpy(stream,stack->data + stack->elem_offset[stack->elem_num],len);
    stack->data_size -= len;
    return 0;
}


#endif