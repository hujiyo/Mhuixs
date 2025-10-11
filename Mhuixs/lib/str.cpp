#include "str.hpp"

str::str(const char* s) {
    size_t len = strlen(s);
    stream = (mstring)malloc(len+sizeof(size_t));
    if(stream == NULL){
        #ifdef bitmap_debug
        printf("str init malloc error\n");
        #endif
        return;
    }
    *(size_t*)stream = len;
    memcpy(stream+sizeof(size_t), s, len);
}

str::str(uint8_t *s, uint32_t len):stream((mstring)malloc(len+sizeof(size_t))){
    if(stream == NULL){
        #ifdef bitmap_debug
        printf("str init malloc error\n");
        #endif
        return;
    }
    *(size_t*)stream = len; // 设置长度字段
    memcpy(stream+sizeof(size_t), s, len);
}

str::str(const str& s):stream((mstring)malloc(s.len()+sizeof(size_t))){
    if(stream == NULL){
        #ifdef bitmap_debug
        printf("str init malloc error\n");
        #endif
        return;
    }
    *(size_t*)stream = s.len(); // 设置长度字段
    memcpy(stream+sizeof(size_t), s.stream+sizeof(size_t), s.len());
}

// 从 mstring 构造
str::str(mstring ms, bool take_ownership) {
    if (take_ownership) {
        // 直接接管所有权，零拷贝
        stream = ms;
    } else {
        // 拷贝数据
        if (ms == NULL) {
            stream = NULL;
            return;
        }
        size_t len = *(size_t*)ms;
        stream = (mstring)malloc(len + sizeof(size_t));
        if (stream == NULL) {
            #ifdef bitmap_debug
            printf("str init from mstring malloc error\n");
            #endif
            return;
        }
        *(size_t*)stream = len;
        memcpy(stream + sizeof(size_t), ms + sizeof(size_t), len);
    }
}

// 默认构造函数
str::str() : stream(NULL) {
}

str::~str(){
    free(stream);
}

size_t str::len() const{
    if (stream == NULL) return 0;
    return *(size_t*)stream;
}

str& str::operator=(const str& s) {
    if (this == &s) return *this; // 自赋值保护
    free(stream);
    size_t new_len = s.len(); // 先保存长度，避免free后访问
    if (new_len == 0) {
        stream = NULL;
        return *this;
    }
    stream = (mstring)malloc(new_len+sizeof(size_t));
    if (stream == NULL) {
        #ifdef bitmap_debug
        printf("str assign malloc error\n");
        #endif
        return *this;
    }
    *(size_t*)stream = new_len; // 设置长度字段
    memcpy(stream+sizeof(size_t), s.stream+sizeof(size_t), new_len);
    return *this;
}

str& str::operator=(const char* s) {
    free(stream);
    size_t new_len = strlen(s); // 使用strlen计算新字符串的长度
    if (new_len == 0) {
        stream = NULL;
        return *this;
    }
    stream = (mstring)malloc(new_len+sizeof(size_t));
    if (stream == NULL) {
        #ifdef bitmap_debug
        printf("str assign malloc error\n");
        #endif
        return *this;
    }
    *(size_t*)stream = new_len; // 设置长度字段
    memcpy(stream+sizeof(size_t), s, new_len);
    return *this;
}

void str::clear() {
    if (stream != NULL) {
        free(stream);
        stream = NULL;
    }
}

char* str::string() const {
    if (len() > 0 && stream) {
        return (char*)stream+sizeof(size_t);
    }
    return NULL;
}

bool str::operator==(const str& other) const {
    if (len() != other.len()) return false;
    if (len() == 0) return true;
    if (stream == NULL || other.stream == NULL) return false;
    return memcmp(stream+sizeof(size_t), other.stream+sizeof(size_t), len()) == 0;//0表示相等 1表示不相等
}

// 释放所有权，返回 mstring
mstring str::release() {
    mstring tmp = stream;
    stream = NULL;  // 放弃所有权，避免析构时 free
    return tmp;
}

void print(str s){
    for(uint32_t i = 0;i < s.len();i++){
        printf("%c",s.stream[i]);
    }
}