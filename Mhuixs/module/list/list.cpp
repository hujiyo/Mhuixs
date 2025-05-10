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

//默认数据块大小和数量
#define LIST_MEMAP_DEF_block_size 64 //数据块大小
#define LIST_MEMAP_DEF_block_num 3200//数据块数量

LIST::LIST():strpool(LIST_MEMAP_DEF_block_size, LIST_MEMAP_DEF_block_num),index(),state(0){
    if(strpool.strpool==NULL) state++;
}

LIST::LIST(int block_size,int block_num):strpool(block_size, block_num),index(),state(0){
    if(strpool.strpool==NULL) state++;
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
        reg = regex(pattern, regex::extended);//构造正则表达式对象
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


//#define TEST_LIST_FIND
#ifdef TEST_LIST_FIND
int main() {
    LIST list;
    LIST::str s1("hello world");
    LIST::str s2("foo bar");
    LIST::str s3("test123");
    LIST::str s4("abc123xyz");
    list.lpush(s1);
    list.lpush(s2);
    list.lpush(s3);
    list.lpush(s4);

    // 测试正则查找
    LIST::str pattern1("foo.*");
    LIST::str pattern2("123");
    LIST::str pattern3("^hello");
    LIST::str pattern4("notfound");

    printf("find 'foo.*' : %lld\n", list.find(pattern1));      // 期望输出1
    printf("find '123'  : %lld\n", list.find(pattern2));        // 期望输出2
    printf("find '^hello': %lld\n", list.find(pattern3));       // 期望输出3
    printf("find 'notfound': %lld\n", list.find(pattern4));     // 期望输出-1

    system("pause");

    return 0;
}
#endif

#define TEST_LIST_PERF
#ifdef TEST_LIST_PERF
#include <chrono>

int main() {
    using namespace std::chrono;
    LIST list(32,6400);
    const int N = 100000;
    // 批量插入
    auto t1 = high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        char buf[64];
        snprintf(buf, sizeof(buf), "item_%d_abc123", i);
        LIST::str s(buf);
        list.lpush(s);
    }
    auto t2 = high_resolution_clock::now();
    printf("插入%d个元素耗时: %.3f ms\n", N, duration<double, std::milli>(t2-t1).count());

    // 多次查找并打印部分命中信息
    int found = 0;
    t1 = high_resolution_clock::now();
    for (int i = 0; i < 1000; ++i) {
        char pat[64];
        snprintf(pat, sizeof(pat), "item_%d_abc123", N/2 + i);
        LIST::str pattern(pat);
        int64_t idx = list.find(pattern);
        if (idx != -1) {
            found++;
            if (found <= 10) {
                // 打印实际内容，辅助调试
                LIST::str real = list.get(idx);
                printf("pattern: %s, idx: %lld, value: %.*s\n", pat, idx, real.len, real.string);
            }
        }
    }
    t2 = high_resolution_clock::now();
    printf("查找1000次耗时: %.3f ms，命中次数: %d\n", duration<double, std::milli>(t2-t1).count(), found);

    // 调试：打印前10个元素内容，确认顺序
    printf("前10个元素内容：\n");
    for (int i = 0; i < 10; ++i) {
        LIST::str s = list.get(i);
        printf("idx %d: %.*s\n", i, s.len, s.string);
    }
    printf("最后10个元素内容：\n");
    for (int i = N-10; i < N; ++i) {
        LIST::str s = list.get(i);
        printf("idx %d: %.*s\n", i, s.len, s.string);
    }

    // 新增：打印实际元素数量，排查amount()和index.size()问题
    printf("list.amount() = %u\n", list.amount());

    // 新增：打印index.size()，排查UintDeque内部实现问题
    // printf("index.size() = %u\n", list.index.size()); // 若index为private可临时加public打印

    return 0;
}
#endif


