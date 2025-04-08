#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "memap.hpp"
#include "stdstr.hpp"
#include "list.hpp"
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

LIST::LIST():head(0),tail(0),num(0),strpool(BLOCK_SIZE, BLOCK_NUM)
{
    //先为队列分配内存池
    if(strpool.iserr()){
        #ifdef bitmap_debug
        printf("MEMAP error\n");
        #endif
        state++;
    }
    return;
}
LIST::LIST(int block_size,int block_num):strpool(block_size, block_num),head(0),tail(0),num(0)
{
    //先为队列分配内存池
    if(strpool.iserr()){
        #ifdef bitmap_debug
        printf("build_strpool error\n");
        #endif
        state++;
    }
    return;
}
LIST::~LIST(){    
    //释放内存池
    strpool.~MEMAP();
}

int LIST::lpush(str &s)
{
    if(this == NULL ||s.string == NULL || s.len == 0 ){
        return merr;
    }
    //先生成一个节点
    OFFSET node_o = strpool.smalloc(s.len + sizeof(NODE_LST));
    if(node_o == 0){
        return merr;
    }
    memcpy(this->strpool.strpool + node_o + sizeof(NODE_LST),s.string,s.len);

    ((NODE_LST*)(this->strpool.strpool + node_o))->length = s.len;    
    ((NODE_LST*)(this->strpool.strpool + node_o))->pre = 0;//将节点的前一个节点设置为0 

    if(this->num == 0){
        this->tail = node_o;
        this->head = node_o;
        ((NODE_LST*)(this->strpool.strpool + node_o))->next = 0;
        this->num++;
        return 0;
    }
    ((NODE_LST*)(this->strpool.strpool + node_o))->next = this->head;
    ((NODE_LST*)(this->strpool.strpool + this->head))->pre = node_o;

    this->head = node_o;//将头节点设置为新节点
    this->num++;//最后将队列的长度加1
    return 0;
}
int LIST::rpush(str &s)
{
    if(this == NULL || s.string == NULL || s.len == 0){
        return merr;
    }
    //先生成一个节点
    OFFSET node_o = strpool.smalloc(s.len + sizeof(NODE_LST));
    if(node_o == 0){
        return merr;
    }
    memcpy(this->strpool.strpool + node_o + sizeof(NODE_LST),s.string,s.len);
    ((NODE_LST*)(this->strpool.strpool + node_o))->length = s.len;
    ((NODE_LST*)(this->strpool.strpool + node_o))->next = 0;//将节点的后一个节点设置为0

    if(this->num == 0){
        this->tail = node_o;
        this->head = node_o;
        ((NODE_LST*)(this->strpool.strpool + node_o))->pre = 0;
        this->num++;
        return 0;
    }
    ((NODE_LST*)(this->strpool.strpool + node_o))->pre = this->tail;   
    ((NODE_LST*)(this->strpool.strpool + this->tail))->next = node_o;

    this->tail = node_o;//将尾节点设置为新节点
    this->num++;//最后将队列的长度加1
    return 0;
}
int LIST::lpop()
{
    if(this == NULL){
        return merr;
    }
    if(this->num == 0){
        return merr;
    }
    //将头节点的数据复制到stream中
    uint32_t length = ((NODE_LST*)(this->strpool.strpool + this->head))->length;
    memcpy(this->strpool.strpool + this->head + sizeof(NODE_LST),this->strpool.strpool + this->head + sizeof(NODE_LST),length);

    //将头节点的后一个节点的前一个节点设置为0
    if(this->num > 1){    
        ((NODE_LST*)(this->strpool.strpool + ((NODE_LST*)(this->strpool.strpool + this->head))->next))->pre = 0;
        OFFSET old_node = this->head;
        this->head = ((NODE_LST*)(this->strpool.strpool + this->head))->next;
        this->strpool.sfree(old_node,length + sizeof(NODE_LST));
        this->num--;
        return 0;
    }
    this->strpool.sfree(this->head,length + sizeof(NODE_LST));
    this->head = this->tail = this->num = 0;
    return 0;
}
int LIST::rpop()
{
    if(this == NULL){
        return merr;
    }
    if(this->num == 0){
        return merr;
    }
    //将尾节点的数据复制到stream中
    uint32_t length = ((NODE_LST*)(this->strpool.strpool + this->tail))->length;
    memcpy(this->strpool.strpool + this->tail + sizeof(NODE_LST),this->strpool.strpool + this->tail + sizeof(NODE_LST),length);

    //将尾节点的前一个节点的后一个节点设置为0
    if(this->num > 1){
        ((NODE_LST*)(this->strpool.strpool + ((NODE_LST*)(this->strpool.strpool + this->tail))->pre))->next = 0;
        OFFSET old_node = this->tail;
        this->tail = ((NODE_LST*)(this->strpool.strpool + this->tail))->pre;
        this->strpool.sfree(old_node,length + sizeof(NODE_LST));
        this->num--;
        return 0;
    }
    this->strpool.sfree(this->tail,length + sizeof(NODE_LST));
    this->head = this->tail = this->num = 0;

    return 0;
}
int LIST::iserr(){
    if(this == NULL ||this->state != 0){
        return merr; 
    }
    return 0;
}