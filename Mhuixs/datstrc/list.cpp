#include "strmap.h"
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
#define bitmap_debug

/*
使用双向链表实现队列
这个库依托于strmap.h库
所有的数据都存储在数组内存池中
*/
#define BLOCK_SIZE 64 //数据块大小
#define BLOCK_NUM 6400//数据块数量
#define add_BLOCK_NUM 6400//每次增加的块数量
/*
LIST采用提前分配内存池的方式实现
默认最小列表容量为64*6400=409600字节=400kb
*/

#pragma pack(1)
/*
避免strmap分配内存时出现的非对齐问题
*/
typedef struct NODE_LST{
    OFFSET pre;//前一个节点
    OFFSET next;//后一个节点
    uint32_t length;//数据长度
}NODE_LST;
#pragma pack(4)

class LIST{
private:
    STRPOOL strpool;//先声明一个数组内存池
    OFFSET head;//头节点偏移量
    OFFSET tail;//尾节点偏移量
    uint32_t num;//节点数量
    int state = 0;//对象状态,成员函数通过改变对象状态来表示对象的异常状态。
public:
    LIST();
    LIST(int block_size,int block_num);
    ~LIST();

    int lpush(uint8_t* stream,uint32_t len);
    int rpush(uint8_t* stream,uint32_t len);
    int lpop();
    int rpop();
    int iserr();

};

LIST::LIST():head(0),tail(0),num(0),strpool(build_strpool(BLOCK_SIZE, BLOCK_NUM))
{
    //先为队列分配内存池
    if(this->strpool == NULL){
        #ifdef bitmap_debug
        printf("build_strpool error\n");
        #endif
        state++;
    }
    return;
}
LIST::LIST(int block_size,int block_num):strpool(build_strpool(block_size, block_num)),head(0),tail(0),num(0)
{
    //先为队列分配内存池
    if(this->strpool == NULL){
        #ifdef bitmap_debug
        printf("build_strpool error\n");
        #endif
        state++;
    }
    return;
}
LIST::~LIST(){    
    //释放内存池
    free(this->strpool);
}

int LIST::lpush(uint8_t* stream,uint32_t len)
{
    if(stream == NULL || len == 0 ){
        return err;
    }
    //先生成一个节点
    OFFSET node_o = STRmalloc(this->strpool,len + sizeof(NODE_LST));
    if(node_o == 0){
        return err;
    }
    memcpy(this->strpool + node_o + sizeof(NODE_LST),stream,len);

    ((NODE_LST*)(list->strpool + node_o))->length = len;    
    ((NODE_LST*)(list->strpool + node_o))->pre = 0;//将节点的前一个节点设置为0 

    if(list->num == 0){
        list->tail = node_o;
        list->head = node_o;
        ((NODE_LST*)(list->strpool + node_o))->next = 0;
        list->num++;
        return 0;
    }
    ((NODE_LST*)(list->strpool + node_o))->next = list->head;
    ((NODE_LST*)(list->strpool + list->head))->pre = node_o;

    list->head = node_o;//将头节点设置为新节点
    list->num++;//最后将队列的长度加1
    return 0;
}
int add_tail(LIST* list, uint8_t* stream, uint32_t len)
{
    if(list == NULL || stream == NULL || len == 0){
        return err;
    }
    //先生成一个节点
    OFFSET node_o = STRmalloc(list->strpool,len + sizeof(NODE_LST));
    if(node_o == 0){
        return err;
    }
    memcpy(list->strpool + node_o + sizeof(NODE_LST),stream,len);
    ((NODE_LST*)(list->strpool + node_o))->length = len;
    ((NODE_LST*)(list->strpool + node_o))->next = 0;//将节点的后一个节点设置为0

    if(list->num == 0){
        list->tail = node_o;
        list->head = node_o;
        ((NODE_LST*)(list->strpool + node_o))->pre = 0;
        list->num++;
        return 0;
    }
    ((NODE_LST*)(list->strpool + node_o))->pre = list->tail;   
    ((NODE_LST*)(list->strpool + list->tail))->next = node_o;

    list->tail = node_o;//将尾节点设置为新节点
    list->num++;//最后将队列的长度加1
    return 0;
}
int pop_head(LIST* list, uint8_t* stream, uint32_t len)
{
    if(list == NULL || stream == NULL || len == 0 ){
        return err;
    }
    if(list->num == 0){
        return err;
    }
    //将头节点的数据复制到stream中
    uint32_t length = ((NODE_LST*)(list->strpool + list->head))->length;
    memcpy(stream,list->strpool + list->head + sizeof(NODE_LST),(length>len)?len:length);

    //将头节点的后一个节点的前一个节点设置为0
    if(list->num > 1){    
        ((NODE_LST*)(list->strpool + ((NODE_LST*)(list->strpool + list->head))->next))->pre = 0;
        OFFSET old_node = list->head;
        list->head = ((NODE_LST*)(list->strpool + list->head))->next;
        STRfree(list->strpool,old_node,length + sizeof(NODE_LST));
        list->num--;
        return 0;
    }
    STRfree(list->strpool,list->head,length + sizeof(NODE_LST));
    list->head = list->tail = list->num = 0;
    return 0;
}
int pop_tail(LIST* list, uint8_t* stream, uint32_t len)
{
    if(list == NULL || stream == NULL || len == 0){
        return err;
    }
    if(list->tail == 0){
        return err;
    }
    //将尾节点的数据复制到stream中
    uint32_t length = ((NODE_LST*)(list->strpool + list->tail))->length;
    memcpy(stream,list->strpool + list->tail + sizeof(NODE_LST),(length>len)?len:length);

    //将尾节点的前一个节点的后一个节点设置为0
    if(list->num > 1){
        ((NODE_LST*)(list->strpool + ((NODE_LST*)(list->strpool + list->tail))->pre))->next = 0;
        OFFSET old_node = list->tail;
        list->tail = ((NODE_LST*)(list->strpool + list->tail))->pre;
        STRfree(list->strpool,old_node,length + sizeof(NODE_LST));
        list->num--;
        return 0;
    }
    STRfree(list->strpool,list->tail,length + sizeof(NODE_LST));
    list->head = list->tail = list->num = 0;

    return 0;
}
uint32_t retLISTnum(LIST* list){
    return list->num;
}