#include "list.hpp"
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
这个库依托于memap.h库，提前分配内存池
所有的数据都存储在数组内存池中
默认初始列表容量为64*6400=409600字节=400kb
*/
#define BLOCK_SIZE 64 //数据块大小
#define BLOCK_NUM 6400//数据块数量

//memap分配内存时默认不字节对齐，所以这里需要硬性规范1字节对齐
#pragma pack(1)
typedef struct NODE_LST{
    OFFSET pre;//前一个节点
    OFFSET next;//后一个节点
    uint32_t length;//数据长度
}NODE_LST;
#pragma pack(4)

LIST::LIST():head(0),tail(0),num(0),strpool(BLOCK_SIZE, BLOCK_NUM),state(0){
    if(strpool.strpool==NULL) state++;
}

LIST::LIST(int block_size,int block_num):strpool(block_size, block_num),head(0),tail(0),num(0){
    if(strpool.strpool==NULL) state++;
}

LIST::~LIST(){
    strpool.~MEMAP();//释放内存池
}

int LIST::lpush(str &s){
    if(s.string == NULL || s.len == 0 )return merr;
    //先生成一个节点
    OFFSET node_o = strpool.smalloc(s.len + sizeof(NODE_LST));
    if(node_o == 0) return merr;
    memcpy(this->strpool.strpool + node_o + sizeof(NODE_LST),s.string,s.len);

    ((NODE_LST*)(this->strpool.strpool + node_o))->length = s.len;    
    ((NODE_LST*)(this->strpool.strpool + node_o))->pre = 0;//将节点的前一个节点设置为0 

    if(this->num == 0){
        this->tail = this->head = node_o;
        ((NODE_LST*)(this->strpool.strpool + node_o))->next = 0;
        this->num++;
        return 0;
    }
    ((NODE_LST*)(this->strpool.strpool + node_o))->next = this->head;
    ((NODE_LST*)(this->strpool.strpool + this->head))->pre = node_o;
    this->head = node_o;//将头节点设置为新节点
    this->num++;//最后将队列的长度加1

    //将元素偏移量索引添加到队列中
    this->index.push_front(node_o);
    return 0;
}
int LIST::rpush(str &s){
    if(s.string == NULL || s.len == 0)return merr;
    //先生成一个节点
    OFFSET node_o = strpool.smalloc(s.len + sizeof(NODE_LST));
    if(node_o == 0) return merr;
    memcpy(this->strpool.strpool + node_o + sizeof(NODE_LST),s.string,s.len);
    
    ((NODE_LST*)(this->strpool.strpool + node_o))->length = s.len;
    ((NODE_LST*)(this->strpool.strpool + node_o))->next = 0;//将节点的后一个节点设置为0

    if(this->num == 0){
        this->tail = this->head = node_o;
        ((NODE_LST*)(this->strpool.strpool + node_o))->pre = 0;
        this->num++;
        return 0;
    }
    ((NODE_LST*)(this->strpool.strpool + node_o))->pre = this->tail;   
    ((NODE_LST*)(this->strpool.strpool + this->tail))->next = node_o;
    this->tail = node_o;//将尾节点设置为新节点
    this->num++;//最后将队列的长度加1

    //将元素偏移量索引添加到队列中
    this->index.push_back(node_o);
    return 0;
}

LIST::str LIST::lpop()
{
    if(this->num == 0)return "";
    //将元素偏移量索引从队列中删除
    this->index.pop_front();
    //复制数据流
    uint32_t length = ((NODE_LST*)(this->strpool.strpool + this->head))->length;
    str s(this->strpool.strpool + this->head + sizeof(NODE_LST),length);

    //将头节点的后一个节点的前一个节点设置为0
    if(this->num > 1){    
        ((NODE_LST*)(this->strpool.strpool + ((NODE_LST*)(this->strpool.strpool + this->head))->next))->pre = 0;
        OFFSET old_node = this->head;
        this->head = ((NODE_LST*)(this->strpool.strpool + this->head))->next;
        this->strpool.sfree(old_node,length + sizeof(NODE_LST));
        this->num--;
        return s;
    }
    this->strpool.sfree(this->head,length + sizeof(NODE_LST));
    this->head = this->tail = this->num = 0;

    return s;
}
LIST::str LIST::rpop()
{
    if(this->num == 0) return "";
    //将元素偏移量索引从队列中删除
    this->index.pop_back();
    //复制数据流
    uint32_t length = ((NODE_LST*)(this->strpool.strpool + this->tail))->length;
    str s(this->strpool.strpool + this->tail + sizeof(NODE_LST),length);

    //将尾节点的前一个节点的后一个节点设置为0
    if(this->num > 1){
        ((NODE_LST*)(this->strpool.strpool + ((NODE_LST*)(this->strpool.strpool + this->tail))->pre))->next = 0;
        OFFSET old_node = this->tail;
        this->tail = ((NODE_LST*)(this->strpool.strpool + this->tail))->pre;
        this->strpool.sfree(old_node,length + sizeof(NODE_LST));
        this->num--;
        return s;
    }
    this->strpool.sfree(this->tail,length + sizeof(NODE_LST));
    this->head = this->tail = this->num = 0;
    return s;
}
int LIST::iserr(){
    return this->state;
}

/*
str 字节流类型
使用len记录字节流的长度，防止字符串泄露
同时暴露所有成员，方便底层操作
str的目的不是封装，而是利用C++的特性将成员函数和str本身进行绑定
*/

LIST::str::str(char* s):len(strlen(s)),state(0),string((uint8_t*)malloc(strlen(s))){
    if(string == NULL){
        #ifdef bitmap_debug
        printf("str init malloc error\n");
        #endif
        len = 0,state++;
        return;
    }
    memcpy(string, s, len);
}

LIST::str::str(uint8_t *s, uint32_t len):len(len),state(0),string((uint8_t*)malloc(len)){
    if(string == NULL){
        #ifdef bitmap_debug
        printf("str init malloc error\n");
        #endif
        len = 0,state++;
        return;
    }
    memcpy(string, s, len);
}

LIST::str::str(const str& s):len(s.len),state(s.state),string((uint8_t*)malloc(s.len)){
    if(string == NULL){
        #ifdef bitmap_debug
        printf("str init malloc error\n");
        #endif
        len = 0,state++;
        return;
    }
    memcpy(string, s.string, s.len);
}

LIST::str::~str(){
    free(string);
}


/************************************
 * 下面是std::list和std::deque的测试代码
 * 这里直接放总结：
 * LIST对比std::list的优势在于
 * 1.参数自由，可根据需求设置LIST以提高性能上限
 * 2.提前的内存分配，在对象操作过程中性能更好
 * 3.通过MEMAP管理的独立内存池，易于进行整块内存的对象压缩，内存占用更小
 ************************************/


/*
void print(LIST::str s){
    for(int i = 0;i < s.len;i++){
        printf("%c",s.string[i]);
    } 
}
//下面是LIST的测试代码

int main(){
    LIST list(64,1000);
    for(int i = 0;i < 1000000;i++){
        
        LIST::str s1("hello1########################");
        LIST::str s2("hello2########################");
        LIST::str s3("hello3########################");
        LIST::str s4("hello4########################");
        LIST::str s5("hello5########################");
        list.lpush(s1);
        list.lpush(s2);
        list.lpush(s3);
        list.lpush(s4);
        list.lpush(s5);
        for(int i = 0;i < 5;i++){
            list.lpop();
        }  
        list.rpush(s1);
        list.rpush(s2);
        list.rpush(s3);
        list.rpush(s4);
        list.rpush(s5);
        for(int i = 0;i < 5;i++){
        list.rpop();
        }
        list.rpush(s1);
        list.lpush(s2);
        list.rpush(s3);
        list.lpush(s4);
        list.rpush(s5);
        for(int i = 0;i < 5;i++){
            list.lpop();
        }
    }
    printf("end\n");
    return 0;
}
*/

//下面是std::list的测试代码
/*
#include <list>
#include <string>
int main(){
    for(int i = 0;i < 1000000;i++){
        std::list<std::string> list;
        std::string s1("hello1########################");
        std::string s2("hello2########################");
        std::string s3("hello3########################");
        std::string s4("hello4########################");
        std::string s5("hello5########################");
        list.push_front(s1);
        list.push_front(s2);
        list.push_front(s3);
        list.push_front(s4);
        list.push_front(s5);
        for(int i = 0;i < 5;i++){
            list.pop_front();
        }
        list.push_back(s1);
        list.push_back(s2);
        list.push_back(s3);
        list.push_back(s4);
        list.push_back(s5);
        for(int i = 0;i < 5;i++){
            list.pop_back();
        }
        list.push_back(s1);
        list.push_front(s2);
        list.push_back(s3);
        list.push_front(s4);
        list.push_back(s5);
        for(int i = 0;i < 5;i++){
            list.pop_front();
        }
    }
    printf("end\n");
    return 0;
}
*/
//下面是std::deque的测试代码
/*
#include <deque>
#include <string>
int main(){
    for(int i = 0;i < 1000000;i++){
        std::deque<std::string> list;
        std::string s1("hello1");
        std::string s2("hello2");
        std::string s3("hello3");
        std::string s4("hello4");
        std::string s5("hello5");
        list.push_front(s1);
        list.push_front(s2);
        list.push_front(s3);
        list.push_front(s4);
        list.push_front(s5);
        for(int i = 0;i < 5;i++){
            list.pop_front();
        }
        list.push_back(s1);
        list.push_back(s2);
        list.push_back(s3);
        list.push_back(s4);
        list.push_back(s5);
        for(int i = 0;i < 5;i++){
            list.pop_back();
        }
        list.push_back(s1);
        list.push_front(s2);
        list.push_back(s3);
        list.push_front(s4);
        list.push_back(s5);
        for(int i = 0;i < 5;i++){
            list.pop_front();
        } 
    } 
    printf("end\n");
    return 0;
}
*/