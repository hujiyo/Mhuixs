#include "list.hpp"
#include "nlohmann/json.hpp"
/*
#版权所有 (c) HUJI 2025
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2025.4
Email:hj18914255909@outlook.com
*/

#define bitmap_debug

/*
使用双向链表实现队列
这个库依托于memap.h库，提前分配内存池
所有的数据都存储在数组内存池中
默认初始列表容量为64*3200=204800字节=200kb
*/

//默认数据块大小和数量
#define LIST_MEMAP_DEF_block_size 64 //数据块大小
#define LIST_MEMAP_DEF_block_num 3200//数据块数量

LIST::LIST():strpool(LIST_MEMAP_DEF_block_size, LIST_MEMAP_DEF_block_num),index(),state(0){
    if(strpool.strpool ==NULL) state++;
}

LIST::LIST(int block_size,int block_num):strpool(block_size, block_num),index(),state(0){
    if(strpool.strpool==NULL) state++;
}

LIST::LIST(const LIST& other)
    : strpool(other.strpool), index(other.index), state(other.state)
{
    // 成员的拷贝构造已实现深拷贝，无需额外处理
}

LIST& LIST::operator=(const LIST& other)
{
    if (this == &other) return *this;
    strpool = other.strpool;
    index = other.index;
    state = other.state;
    return *this;
}

LIST::~LIST() = default;

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

str LIST::lpop()
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
str LIST::rpop()
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

nlohmann::json LIST::get_all_info() const {
    nlohmann::json info;
    info["element_count"] = this->index.size();
    nlohmann::json elements = nlohmann::json::array();
    for (uint32_t i = 0; i < this->index.size(); i++) {
        OFFSET node_o = this->index.get_index(i);
        uint32_t len = *(uint32_t*)strpool.addr(node_o);
        std::string val((char*)strpool.addr(node_o) + sizeof(uint32_t), len);
        elements.push_back(val);
    }
    info["elements"] = elements;
    return info;
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

str LIST::get(int64_t index){
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

uint32_t LIST::amount(){
    return this->index.size();
}

int LIST::clear(){
    //释放内存
    this->strpool.~MEMAP();
    //清空索引
    this->index.clear();
    //重新初始化
    this->strpool = MEMAP(LIST_MEMAP_DEF_block_size, LIST_MEMAP_DEF_block_num);
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

int64_t LIST::find(str &s)
{
    if (s.len == 0 || s.string == NULL) return merr;
    string pattern((const char*)s.string, s.len);
    regex reg;
    try {
        reg = regex(pattern, regex::ECMAScript);//构造正则表达式对象，支持?和*等常用语法
    } catch (const regex_error& e) {
        #ifdef bitmap_debug
        printf("LIST::find: regex compile error: %s\n", e.what());
        #endif
        return merr;
    }
    uint32_t n = this->index.size();
    for (uint32_t i = 0; i < n; ++i) {
        str elem = this->get(i);
        if (elem.len == 0 || elem.string == NULL) {
            continue;
        }
        string elem_str((const char*)elem.string, elem.len);
        bool matched = regex_search(elem_str, reg);
        if (matched) {
            return i;
        }
    }
    return -1;
}

int LIST::swap(int64_t idx1, int64_t idx2) {
    int64_t n = this->index.size();// 获取当前列表的大小
    int64_t i1 = (idx1 < 0) ? (n + idx1) : idx1;// 处理负数索引
    int64_t i2 = (idx2 < 0) ? (n + idx2) : idx2;// 处理负数索引
    if (i1 < 0 || i1 >= n || i2 < 0 || i2 >= n) return merr;// 参数范围检查
    // 交换索引
    if (i1 == i2) return 0;
    OFFSET o1 = this->index.get_index(i1);// 获取索引1的偏移量
    OFFSET o2 = this->index.get_index(i2);// 获取索引2的偏移量
    this->index.set_index(i1, o2);// 设置索引1为索引2的偏移量
    this->index.set_index(i2, o1);// 设置索引2为索引1的偏移量
    return 0;
}

//#define MAIN
#ifdef MAIN
#include <time.h>
int main(){
    LIST list(64,64000);
    time_t start, end;
    start = clock();
    for(int i = 0;i < 1000000;i++){
        LIST::str s("hello world");
        list.rpush(s); 
    }
    end = clock();
    #define CLOCKS_PER_SEC 1000
    printf("LIST推入耗时: %.3f秒\n", (end - start) / (double)CLOCKS_PER_SEC);

    // 插入测试：插入10000条到中间，每次都重新计算mid
    int insert_count = 10000;
    LIST::str insert_str("inserted");
    start = clock();
    for(int i = 0; i < insert_count; ++i){
        int mid = list.amount() / 2;
        list.insert(insert_str, mid);
    }
    end = clock();
    printf("LIST插入%d条耗时: %.3f秒\n", insert_count, (end - start) / (double)CLOCKS_PER_SEC);
    printf("插入后中间位置内容: ");
    int mid = list.amount() / 2;
    LIST::str got = list.get(mid);
    print(got);
    printf("\n");
    printf("end\n");
    return 0;
}
#endif

//#define main2
#ifdef main2
#include <time.h>
#include <string>
#include <deque>
#include <iostream>

int main(){
    std::deque<std::string> deque;
    time_t start, end;
    start = clock();
    for(int i = 0;i < 1000000;i++){
        deque.push_back("hello world");
    }
    end = clock();
    #define CLOCKS_PER_SEC 1000
    printf("deque推入耗时: %.3f秒\n", (end - start) / (double)CLOCKS_PER_SEC);

    // 插入测试：插入10000条到中间，每次都重新计算mid
    int insert_count = 10000;
    start = clock();
    for(int i = 0; i < insert_count; ++i){
        int mid = deque.size() / 2;
        deque.insert(deque.begin() + mid, "inserted");
    }
    end = clock();
    printf("deque插入%d条耗时: %.3f秒\n", insert_count, (end - start) / (double)CLOCKS_PER_SEC);
    printf("插入后中间位置内容: ");
    int mid = deque.size() / 2;
    std::cout << deque[mid] << std::endl;
    printf("end\n");
    return 0;
}
#endif



//#define main_test_all
#ifdef main_test_all
#include <iostream>

int main(){    
    LIST *list = new LIST;
    
    str s1 = "hello arduino";
    str s2 = "hello world";
    str s3 = "hello world,my friend";
    str s4 = "hello world,my friend,how are you";


    list->lpush(s1);
    list->lpush(s2);
    list->lpush(s3);
    list->lpush(s4);
    printf("list size: %d\n", list->amount());
    list->rpush(s1);
    list->rpush(s2);
    list->rpush(s3);
    list->rpush(s4);
    printf("list size: %d\n", list->amount());


    str ss = "this is the middle";
    list->insert(ss, 4);
    printf("list size: %d\n", list->amount());
    str temp = list->get(4);
    printf("list[4]: ");
    for(uint32_t i = 0;i < temp.len;i++){
        printf("%c",temp.string[i]);
    }
    printf("\n");


    str ss2 = "this is the middle, updated";
    list->update(ss2, 4);
    printf("list size: %d\n", list->amount());
    printf("list[4](updated): ");
    str temp1 = list->get(4);
    for(uint32_t i = 0;i < temp1.len;i++){
        printf("%c",temp1.string[i]);
    }
    printf("\n");


    list->del(4);
    printf("list size: %d\n", list->amount());
    printf("list[4](deleted): ");
    str temp2 = list->get(4);
    for(uint32_t i = 0;i < temp2.len;i++){
        printf("%c",temp2.string[i]);
    }
    printf("\n");


    // 查找包含"are"的元素
    str regex_str = "ard?i*";
    int64_t idx = list->find(regex_str);
    printf("find: %d\n", idx);
    temp = list->get(idx);
    for(uint32_t i = 0;i < temp.len;i++){
        printf("%c",temp.string[i]);
    }
    printf("\n");


    list->lpop();
    printf("list size after lpop: %d\n", list->amount());
    list->rpop();
    printf("list size after rpop: %d\n", list->amount());


    // 测试清空列表
    list->clear();
    printf("list size after clear: %d\n", list->amount());
    

    // 测试错误处理
    int err = list->iserr();
    printf("list error state: %d\n", err);
    delete list;
}


#endif