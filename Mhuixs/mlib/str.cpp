#include "str.hpp"

str::str(const char* s) {
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

str::str(uint8_t *s, uint32_t len):string((uint8_t*)malloc(len)),len(len),state(0){
    if(string == NULL){
        #ifdef bitmap_debug
        printf("str init malloc error\n");
        #endif
        len = 0,state++;
        return;
    }
    memcpy(string, s, len);
}

str::str(const str& s):string((uint8_t*)malloc(s.len)),len(s.len),state(s.state){
    if(string == NULL){
        #ifdef bitmap_debug
        printf("str init malloc error\n");
        #endif
        len = 0,state++;
        return;
    }
    memcpy(string, s.string, s.len);
}

str::~str(){
    free(string);
}

str& str::operator=(const str& s) {
    if (this == &s) return *this; // 自赋值保护
    free(string);
    len = s.len;
    state = s.state;
    if (len == 0) {
        string = NULL;
        return *this;
    }
    string = (uint8_t*)malloc(len);
    if (string == NULL) {
        #ifdef bitmap_debug
        printf("str assign malloc error\n");
        #endif
        len = 0; state++;
        return *this;
    }
    memcpy(string, s.string, len);
    return *this;
}

str& str::operator=(const char* s) {
    free(string);
    len = strlen(s);
    state = 0;
    if (len == 0) {
        string = NULL;
        return *this;
    }
    string = (uint8_t*)malloc(len);
    if (string == NULL) {
        #ifdef bitmap_debug
        printf("str assign malloc error\n");
        #endif
        len = 0; state++;
        return *this;
    }
    memcpy(string, s, len);
    return *this;
}

void str::clear() {
    free(string);
    string = NULL;
    len = 0;
    state = 0;
}

char* str::c_str() const {
    char* buf = (char*)malloc(len + 1);
    if (!buf) return NULL;
    if (len > 0 && string) {
        memcpy(buf, string, len);
    }
    buf[len] = '\0';
    return buf;
}

bool str::operator==(const str& other) const {
    if (len != other.len) return false;
    if (len == 0) return true;
    if (string == NULL || other.string == NULL) return false;
    return memcmp(string, other.string, len) == 0;//0表示相等 1表示不相等
}

void print(str s){
    for(uint32_t i = 0;i < s.len;i++){
        printf("%c",s.string[i]);
    }
}