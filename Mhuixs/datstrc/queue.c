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
static struct NODE_Q{
    NODE_Q* next;
    uint8_t* data;
    uint32_t len;
}NODE_Q;
typedef struct NODE_Q NODE_Q;

typedef struct QUEUE{
    NODE_Q* queue_head;//指向队列头
    NODE_Q* queue_tail;//指向队列尾
    uint32_t node_num;//节点数量
}QUEUE;

QUEUE* makeQUEUE(){
    return (QUEUE*)calloc(1,sizeof(QUEUE));
}
void freeQUEUE(QUEUE* queue){
    if(queue == NULL){
        return;
    }
    NODE_Q* p = queue->queue_head;
    NODE_Q* tmp= NULL;
    while(p!= NULL){
        tmp = p;
        p = p->next;
        free(tmp->data);
        free(tmp);
    }
    free(queue);
}
int putintoQUEUE(QUEUE* queue,uint8_t* stream,uint32_t len){
    /*
    putintoQUEUE的缺点和STACK中的pushintoSTACK一样，都是频繁的申请内存。
    */
    if(queue == NULL || len == 0 || stream == NULL){
        return err;
    }

    //申请新节点
    NODE_Q* new_node = (NODE_Q*)calloc(1,sizeof(NODE_Q));
    if(new_node == NULL){
        return err;
    }
    new_node->data = (uint8_t*)calloc(len,1);
    if(new_node->data == NULL){
        free(new_node);
        return err;
    }
    new_node->len = len;
    new_node->next = NULL;
    memcpy(new_node->data,stream,len);
    //至此，新节点已经初始化完毕
    //将节点插入队列末尾
    if(queue->node_num==0){//队列为空
        queue->queue_head = queue->queue_tail = new_node;
    }
    else{
        queue->queue_tail->next = new_node;//旧的队尾节点指向新节点
        queue->queue_tail = new_node;//新节点成为队尾节点
    }
    return ++queue->node_num;//返回队列元素数量
}
int getfromQUEUE(QUEUE* queue,uint8_t* stream,uint32_t len){//调用这个函数必须保证stream有足够的空间
    if(queue == NULL||queue->node_num == 0){
        return err;
    }

    NODE_Q* tmp = queue->queue_head;//指向队列头
    queue->queue_head = queue->queue_head->next;//队列头指针后移
    memcpy(stream,tmp->data,(len < tmp->len)?len:tmp->len);//缓存区长度不够，函数自动丢弃多余数据

    free(tmp->data);
    free(tmp);
    return --queue->node_num;//返回剩余队列元素数量
}
