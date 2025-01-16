#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#define err -1
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

STACK* makeSTACK(){
    return (STACK*)calloc(1,sizeof(STACK));
}
void freeSTACK(STACK* stack){
    if(stack == NULL){
        return;
    }
    free(stack->data);
    free(stack->elem_offset);
    free(stack);
}
int pushintoSTACK(STACK* stack,uint8_t* stream,uint32_t len){
    if(stack == NULL){
        return err;
    }
    /*
    pushintoSTACK的缺点就是，每次push都要重新扩展分配内存，这有可能会导致
    频繁的入栈时的性能下降。不过Mhuixs中栈的目标是节约内存。
    在对性能有要求的场景下，可以使用LIST来代替STACK。
    */
    uint8_t* newdata = (uint8_t*)realloc(stack->data,stack->data_size+len);
    if(newdata == NULL){
        return err;
    }
    stack->data = newdata;
    uint32_t* p_offset = (uint32_t*)realloc(stack->elem_offset,(stack->elem_num + 1) * sizeof(uint32_t));
    if(p_offset == NULL){
        return err;//不用还原data
    }
    stack->elem_offset = p_offset;
    stack->elem_offset[stack->elem_num] = stack->data_size;
    stack->data_size += len;
    
    memcpy(stack->data + stack->elem_offset[stack->elem_num], stream, len); // 从输入流复制数据到栈的数据区  
    return stack->elem_num++;
}
int popfromSTACK(STACK* stack,uint8_t* stream,uint32_t len){//你必须保证stream的大小足够容纳栈顶元素
    if(stack == NULL||stack->elem_num == 0){
        return err;
    }
    /*
    出栈必须保证安全，因为出栈意味着数据永久丢失。
    */
    stack->elem_num--;
    if(len > stack->data_size - stack->elem_offset[stack->elem_num]){//如果len大于栈顶元素的大小，返回错误
        stack->elem_num++;
        return err;
    }
    memcpy(stream,stack->data + stack->elem_offset[stack->elem_num],len);
    stack->data_size -= len;
    return 0;
}