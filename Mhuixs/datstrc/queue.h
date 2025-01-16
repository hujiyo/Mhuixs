/*
#版权所有 (c) Mhuixs-team 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2024.11
Email:hj18914255909@outlook.com
*/
#ifndef QUEUE_H
#define QUEUE_H
#include <stdint.h>

/*
QUEUE 队列
队列中每个成员(字节流)都是一个节点NODE_Q
节点中的data存放的是len长度的数据

缺点:频繁的申请内存不仅会造成内存碎片，
    还给对象压缩带来挑战。
替代:LIST
*/

static struct NODE_Q{
    NODE_Q* next;
    /*
    队列是单方向性的， 由头方向节点指向尾方向节点
    next为NULL时，说明自身为尾节点
    */
    uint8_t* data;
    uint32_t len;
    /*
    节点data中存放的是len长度的数据
    */    
}NODE_Q;

typedef struct NODE_Q NODE_Q;

typedef struct QUEUE{
    NODE_Q* queue_head;//指向队列头
    NODE_Q* queue_tail;//指向队列尾
    uint32_t node_num;//节点数量
}QUEUE;

QUEUE* makeQUEUE();
/*
创建一个队列对象
QUEUE *object = makeQUEUE();
*/
void freeQUEUE(QUEUE* queue);
int putintoQUEUE(QUEUE* queue,uint8_t* bitestream,uint32_t len);
/*
将长度为len的数据bitestream加入队列
putintoQUEUE(object,bitestream,len);
*/
int getfromQUEUE(QUEUE* queue,uint8_t* bitestream,uint32_t len);
/*
将队列头节点的数据弹出到bitestream中
*/

#endif