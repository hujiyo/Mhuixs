#include "list-ud.hpp"
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
#define BLOCK_NUM 3200//数据块数量

LIST::LIST():strpool(BLOCK_SIZE, BLOCK_NUM),state(0),index(){
    if(strpool.strpool==NULL) state++;
}

LIST::LIST(int block_size,int block_num):strpool(block_size, block_num),state(0),index(){
    if(strpool.strpool==NULL) state++;
}

LIST::~LIST(){
    strpool.~MEMAP();//释放内存池
}

int LIST::lpush(str &s){
    if(s.len == 0 || s.string == NULL) {
        #ifdef bitmap_debug
        printf("LIST::lpush: empty string, skip\n");
        #endif
        return merr;
    }
    //先生成一个节点
    OFFSET node_o = strpool.smalloc(s.len + sizeof(uint32_t));
    if(node_o == NULL_OFFSET) {
        state++;
        #ifdef bitmap_debug
        printf("LIST::lpush:smalloc error\n");
        #endif
        return merr;
    }
    memcpy(strpool.addr(node_o) + sizeof(uint32_t),s.string,s.len);
    *(uint32_t*)strpool.addr(node_o) = s.len;

    //将元素偏移量索引添加到队列中
    this->index.lpush(node_o);
    return 0;
}
int LIST::rpush(str &s){
    if(s.len == 0 || s.string == NULL) {
        #ifdef bitmap_debug
        printf("LIST::rpush: empty string, skip\n");
        #endif
        return merr;
    }
    //先生成一个节点
    OFFSET node_o = strpool.smalloc(s.len + sizeof(uint32_t));
    if(node_o == NULL_OFFSET){
        state++;
        #ifdef bitmap_debug
        printf("LIST::rpush:smalloc error\n");
        #endif
        return merr;
    }
    memcpy(strpool.addr(node_o) + sizeof(uint32_t),s.string,s.len);
    *(uint32_t*)strpool.addr(node_o) = s.len;

    //将元素偏移量索引添加到队列中
    this->index.rpush(node_o);
    return 0;
}

LIST::str LIST::lpop()
{
    if(!this->index.size()) return "";
    //将元素偏移量索引从队列中删除
    int64_t offset= index.lpop();//获取队列头元素偏移量
    if(offset == merr) {
        state++;
        #ifdef bitmap_debug
        printf("LIST::lpop:uintdeque:lpop error\n");
        #endif
        return "";//如果队列为空，返回空字符串
    }
    OFFSET head_offset = offset;//获取队列头元素偏移量
    //复制数据流
    uint32_t length;
    memcpy(&length,strpool.addr(head_offset),sizeof(uint32_t));
    str s(strpool.addr(head_offset) + sizeof(uint32_t),length);
    this->strpool.sfree(head_offset,length + sizeof(uint32_t));//释放内存    
    return s;
}
LIST::str LIST::rpop()
{
    if(!this->index.size()) return "";
    //将元素偏移量索引从队列中删除
    int64_t offset= index.rpop();//获取队列头元素偏移量
    if(offset == merr) {
        state++;
        #ifdef bitmap_debug
        printf("LIST::rpop:uintdeque:rpop error\n");
        #endif
        return "";//如果队列为空，返回空字符串 
    }
    OFFSET tail_offset = offset;//获取队列尾元素偏移量
    //复制数据流
    uint32_t length;
    memcpy(&length,strpool.addr(tail_offset),sizeof(uint32_t));
    str s(strpool.addr(tail_offset) + sizeof(uint32_t),length);
    this->strpool.sfree(tail_offset,length + sizeof(uint32_t));//释放内存
    return s;
}
int LIST::iserr(){
    if(this->strpool.iserr()){
        this->state++;
        #ifdef bitmap_debug
        printf("LIST::iserr:strpool iserr\n");
        #endif
        return merr;
    }
    return this->state;
}

int LIST::insert(str &s, int64_t idx) 
{
    //0,1,2....表示从左往右数,-1,-2,-3...表示从右往左数,0表示第一个元素，-1表示最后一个元素
    // 处理负数索引，-1表示最后一个元素，-2倒数第二个...
    int64_t adjusted_idx = (idx < 0) ? (this->index.size() + idx) : idx;
    // 参数范围检查
    if (adjusted_idx < 0 || adjusted_idx >= this->index.size()) return merr;

    OFFSET node_o = strpool.smalloc(s.len + sizeof(uint32_t));
    if (node_o == NULL_OFFSET) {
        state++;
        #ifdef bitmap_debug
        printf("LIST::insert:smalloc error\n");
        #endif
        return merr;
    }
    //向index中插入元素
    this->index.insert(adjusted_idx, node_o);
    // 写入数据到节点
    memcpy(strpool.addr(node_o) + sizeof(uint32_t), s.string, s.len);
    *(uint32_t*)strpool.addr(node_o) = s.len;
    return 0;
}

int LIST::update(str &s, int64_t idx)
{
    //先检查索引是否有效
    int64_t adjusted_idx = (idx < 0) ? (this->index.size() + idx) : idx;
    // 参数范围检查
    if (adjusted_idx < 0 || adjusted_idx >= this->index.size()) return merr;
    //获取节点偏移量
    OFFSET node_o = this->index.get_index(adjusted_idx);
    
    //释放旧数据
    this->strpool.sfree(node_o, *(uint32_t*)strpool.addr(node_o) + sizeof(uint32_t));
    //分配新数据
    node_o = strpool.smalloc(s.len + sizeof(uint32_t));
    if (node_o == NULL_OFFSET) {
        state++;
        #ifdef bitmap_debug
        printf("LIST::update:smalloc error\n");
        #endif
        return merr; 
    }
    //写入新数据
    memcpy(strpool.addr(node_o) + sizeof(uint32_t), s.string, s.len);
    *(uint32_t*)strpool.addr(node_o) = s.len;
    //更新索引
    this->index.set_index(adjusted_idx, node_o);
    return 0;
}

int LIST::del(int64_t index){
    //先检查索引是否有效
    int64_t adjusted_idx = (index < 0)? (this->index.size() + index) : index;
    // 参数范围检查
    if (adjusted_idx < 0 || adjusted_idx >= this->index.size()) return merr;
    //获取节点偏移量
    OFFSET node_o = this->index.get_index(adjusted_idx);
    //释放内存
    this->strpool.sfree(node_o, *(uint32_t*)strpool.addr(node_o) + sizeof(uint32_t));
    //删除索引
    this->index.rm_index(adjusted_idx);
    return 0;
}

LIST::str LIST::get(int64_t index){
    //先检查索引是否有效
    int64_t adjusted_idx = (index < 0)? (this->index.size() + index) : index;
    // 参数范围检查
    if (adjusted_idx < 0 || adjusted_idx >= this->index.size()) return "INDEX OUT OF RANGE";
    //获取节点偏移量
    OFFSET node_o = this->index.get_index(adjusted_idx);
    //读取数据
    uint32_t length = *(uint32_t*)strpool.addr(node_o);
    str s(strpool.addr(node_o) + sizeof(uint32_t),length);
    return s;
}

uint32_t LIST::num(){
    return this->index.size();
}

int LIST::clear(){
    //释放内存
    this->strpool.~MEMAP();
    //清空索引
    this->index.clear();
    //重新初始化
    this->strpool = MEMAP(BLOCK_SIZE, BLOCK_NUM);
    if(this->strpool.strpool==NULL) {
        this->state++;
        #ifdef bitmap_debug
        printf("LIST::clear:MEMAP calloc error\n");
        #endif
        return merr;
    }
    this->state = 0;
    return 0;
}

int LIST::find(str &s){
    
}

/*
str 字节流类型
使用len记录字节流的长度，防止字符串泄露
同时暴露所有成员，方便底层操作
str的目的不是封装，而是利用C++的特性将成员函数和str本身进行绑定
*/

LIST::str::str(const char* s) {
    len = strlen(s);
    state = 0;
    if(len == 0) {
        string = NULL;
        // 不报错，允许空字符串
        return;
    }
    string = (uint8_t*)malloc(len);
    if(string == NULL){
        #ifdef bitmap_debug
        printf("str init malloc error\n");
        #endif
        len = 0; state++;
        return;
    }
    memcpy(string, s, len);
}

LIST::str::str(uint8_t *s, uint32_t len):string((uint8_t*)malloc(len)),len(len),state(0){
    if(string == NULL){
        #ifdef bitmap_debug
        printf("str init malloc error\n");
        #endif
        len = 0,state++;
        return;
    }
    memcpy(string, s, len);
}

LIST::str::str(const str& s):string((uint8_t*)malloc(s.len)),len(s.len),state(s.state){
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



void print(LIST::str s){
    for(uint32_t i = 0;i < s.len;i++){
        printf("%c",s.string[i]);
    }
}
//下面是LIST的测试代码
#include <time.h>
int main(){
    LIST list(32,32);
    

    //测试代码
    time_t start, end;
    start = clock();

    for(int i = 0;i < 10000;i++){
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
        list.lpush(s1);
        list.lpush(s2);
        list.lpush(s3);
        list.lpush(s4);
        list.lpush(s5);
        for(int i = 0;i < 10;i++){
            list.insert(s1,i);
        }
        for(int i = 0;i > -10;i--){
            list.insert(s2,i);
        }
    }
    end = clock();
    #define CLOCKS_PER_SEC 1000
    printf("LIST耗时: %.3f秒\n", (end - start) / (double)CLOCKS_PER_SEC);
    printf("end\n");
    return 0;
}


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