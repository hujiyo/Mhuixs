#include "list.hpp"
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
*/


LIST::LIST():index(),state(0){}

LIST::LIST(const LIST& other):index(other.index), state(other.state){
    // 成员的拷贝构造已实现深拷贝，无需额外处理
}

LIST& LIST::operator=(const LIST& other){
    if (this == &other) return *this;
    index = other.index;
    state = other.state;
    return *this;
}

LIST::~LIST() = default;

int LIST::lpush(str &s){
    if(s.len() == 0 || s.stream == NULL) {
        #ifdef bitmap_debug
        printf("LIST::lpush: empty string, skip\n");
        #endif
        return merr;
    }
    //先生成一个节点
    mstring node_o = (mstring)malloc(s.len() + sizeof(uint32_t));
    if(node_o == NULL) {
        report(merr, "list module", "malloc error");
        return merr;
    }
    memcpy(node_o + sizeof(uint32_t),s.stream,s.len());
    *(uint32_t*)node_o = s.len();

    //将元素偏移量索引添加到队列中
    this->index.lpush((uint64_t)node_o);
    return 0;
}
int LIST::rpush(str &s){
    if(s.len() == 0 || s.string() == NULL) {
        report(merr, "list module", "empty string");
        return merr;
    }
    //先生成一个节点
    mstring node_o = (mstring)malloc(s.len() + sizeof(uint32_t));
    if(node_o == NULL){
        report(merr, "list module", "malloc error");
        return merr;
    }
    memcpy(node_o + sizeof(uint32_t),s.string(),s.len());
    *(uint32_t*)node_o = s.len();

    //将元素偏移量索引添加到队列中
    this->index.rpush((uint64_t)node_o);
    return 0;
}

str LIST::lpop()
{
    if(!this->index.size()) return "";
    //将元素偏移量索引从队列中删除
    mstring mstr = (mstring)index.lpop();//获取队列头元素偏移量
    if(mstr == NULL) {
        report(merr, "list module", "lpop error");
        return "";//如果队列为空，返回空字符串
    }
    //复制数据流
    uint32_t length;
    memcpy(&length,mstr,sizeof(uint32_t));
    str s(mstr + sizeof(uint32_t),length);
    free(mstr);
    return s;
}
str LIST::rpop()
{
    if(!this->index.size()) return "";
    //将元素偏移量索引从队列中删除
    mstring mstr = (mstring)index.rpop();//获取队列头元素偏移量
    if(mstr == NULL) {
        report(merr, "list module", "rpop error");
        return "";//如果队列为空，返回空字符串 
    }
    //复制数据流
    uint32_t length;
    memcpy(&length,mstr,sizeof(uint32_t));
    str s(mstr + sizeof(uint32_t),length);
    free(mstr);
    return s;
}
int LIST::iserr(){
    if(this->state != 0){
        report(merr, "list module", "index iserr");
        return merr;
    }
    return 0;
}

int LIST::insert(str &s, int64_t idx) 
{
    //0,1,2....表示从左往右数,-1,-2,-3...表示从右往左数,0表示第一个元素，-1表示最后一个元素
    // 处理负数索引，-1表示最后一个元素，-2倒数第二个...
    int64_t adjusted_idx = (idx < 0) ? (this->index.size() + idx) : idx;
    // 参数范围检查
    if (adjusted_idx < 0 || adjusted_idx >= this->index.size()) return merr;

    mstring node_o = (mstring)malloc(s.len() + sizeof(uint32_t));
    if (node_o == NULL) {
        report(merr, "list module", "malloc error");
        return merr;
    }
    //向index中插入元素
    this->index.insert(adjusted_idx, (uint64_t)node_o);
    // 写入数据到节点
    memcpy(node_o + sizeof(uint32_t), s.string(), s.len());
    *(uint32_t*)node_o = s.len();
    return 0;
}

int LIST::update(str &s, int64_t idx)
{
    //先检查索引是否有效
    int64_t adjusted_idx = (idx < 0) ? (this->index.size() + idx) : idx;
    // 参数范围检查
    if (adjusted_idx < 0 || adjusted_idx >= this->index.size()) return merr;
    //获取节点偏移量
    mstring node_o = (mstring)this->index.get_index(adjusted_idx);
    
    //释放旧数据
    free(node_o);
    //分配新数据
    node_o = (mstring)malloc(s.len() + sizeof(uint32_t));
    if (node_o == NULL) {
        report(merr, "list module", "malloc error");
        return merr; 
    }
    //写入新数据
    memcpy(node_o + sizeof(uint32_t), s.string(), s.len());
    *(uint32_t*)node_o = s.len();
    //更新索引
    this->index.set_index(adjusted_idx, (uint64_t)node_o);
    return 0;
}

int LIST::del(int64_t index){
    //先检查索引是否有效
    int64_t adjusted_idx = (index < 0)? (this->index.size() + index) : index;
    // 参数范围检查
    if (adjusted_idx < 0 || adjusted_idx >= this->index.size()) return merr;
    //获取节点偏移量
    mstring node_o = (mstring)this->index.get_index(adjusted_idx);
    //释放内存
    free(node_o);
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
    mstring node_o = (mstring)this->index.get_index(adjusted_idx);
    //读取数据
    uint32_t length = *(uint32_t*)node_o;
    str s(node_o + sizeof(uint32_t),length);
    return s;
}

uint32_t LIST::amount(){
    return this->index.size();
}

int LIST::clear(){
    //释放内存
    for(uint32_t i = 0; i < this->index.size(); i++){
        mstring node_o = (mstring)this->index.get_index(i);
        free(node_o);
    }
    //清空索引
    this->index.clear();
    this->state = 0;
    return 0;
}

int64_t LIST::find(str &s)
{
    if (s.len() == 0 || s.string() == NULL) return merr;
    string pattern((const char*)s.string(), s.len());
    regex reg;
    try {
        reg = regex(pattern, regex::ECMAScript);//构造正则表达式对象，支持?和*等常用语法
    } catch (const regex_error& e) {
        report(merr, "list module", "regex compile error");
        return merr;
    }
    uint32_t n = this->index.size();
    for (uint32_t i = 0; i < n; ++i) {
        str elem = this->get(i);
        if (elem.len() == 0 || elem.string() == NULL) {
            continue;
        }
        string elem_str((const char*)elem.string(), elem.len());
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
    this->index.swap(i1, i2);
    return 0;
}

