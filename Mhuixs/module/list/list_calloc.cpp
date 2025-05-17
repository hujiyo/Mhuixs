#include "list_calloc.hpp"

/*
#版权所有 (c) Mhuixs-team 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2024.11
Email:hj18914255909@outlook.com
*/

#define bitmap_debug

LIST_CALLOC::LIST_CALLOC() : index(), state(0) {}

LIST_CALLOC::~LIST_CALLOC() {
    clear();
}

int LIST_CALLOC::lpush(str &s) {
    if (s.len == 0 || s.string == NULL) {
        #ifdef bitmap_debug
        printf("LIST_CALLOC::lpush: empty string, skip\n");
        #endif
        return merr;
    }
    Node* node = (Node*)calloc(1, sizeof(Node));
    if (!node) {
        state++;
        return merr;
    }
    node->data = (uint8_t*)calloc(1, s.len);
    if (!node->data) {
        free(node);
        state++;
        return merr;
    }
    memcpy(node->data, s.string, s.len);
    node->len = s.len;
    index.insert(index.begin(), node);
    return 0;
}

int LIST_CALLOC::rpush(str &s) {
    if (s.len == 0 || s.string == NULL) {
        #ifdef bitmap_debug
        printf("LIST_CALLOC::rpush: empty string, skip\n");
        #endif
        return merr;
    }
    Node* node = (Node*)calloc(1, sizeof(Node));
    if (!node) {
        state++;
        return merr;
    }
    node->data = (uint8_t*)calloc(1, s.len);
    if (!node->data) {
        free(node);
        state++;
        return merr;
    }
    memcpy(node->data, s.string, s.len);
    node->len = s.len;
    index.push_back(node);
    return 0;
}

LIST_CALLOC::str LIST_CALLOC::lpop() {
    if (index.empty()) return "";
    Node* node = index.front();
    index.erase(index.begin());
    str s(node->data, node->len);
    free(node->data);
    free(node);
    return s;
}

LIST_CALLOC::str LIST_CALLOC::rpop() {
    if (index.empty()) return "";
    Node* node = index.back();
    index.pop_back();
    str s(node->data, node->len);
    free(node->data);
    free(node);
    return s;
}

int LIST_CALLOC::insert(str &s, int64_t idx) {
    int64_t adjusted_idx = (idx < 0) ? (index.size() + idx) : idx;
    if (adjusted_idx < 0 || adjusted_idx > (int64_t)index.size()) return merr;
    Node* node = (Node*)calloc(1, sizeof(Node));
    if (!node) {
        state++;
        return merr;
    }
    node->data = (uint8_t*)calloc(1, s.len);
    if (!node->data) {
        free(node);
        state++;
        return merr;
    }
    memcpy(node->data, s.string, s.len);
    node->len = s.len;
    index.insert(index.begin() + adjusted_idx, node);
    return 0;
}

int LIST_CALLOC::update(str &s, int64_t idx) {
    int64_t adjusted_idx = (idx < 0) ? (index.size() + idx) : idx;
    if (adjusted_idx < 0 || adjusted_idx >= (int64_t)index.size()) return merr;
    Node* node = index[adjusted_idx];
    free(node->data);
    node->data = (uint8_t*)calloc(1, s.len);
    if (!node->data) {
        state++;
        return merr;
    }
    memcpy(node->data, s.string, s.len);
    node->len = s.len;
    return 0;
}

int LIST_CALLOC::del(int64_t idx) {
    int64_t adjusted_idx = (idx < 0) ? (index.size() + idx) : idx;
    if (adjusted_idx < 0 || adjusted_idx >= (int64_t)index.size()) return merr;
    Node* node = index[adjusted_idx];
    free(node->data);
    free(node);
    index.erase(index.begin() + adjusted_idx);
    return 0;
}

LIST_CALLOC::str LIST_CALLOC::get(int64_t idx) {
    int64_t adjusted_idx = (idx < 0) ? (index.size() + idx) : idx;
    if (adjusted_idx < 0 || adjusted_idx >= (int64_t)index.size()) return "INDEX OUT OF RANGE";
    Node* node = index[adjusted_idx];
    return str(node->data, node->len);
}

uint32_t LIST_CALLOC::amount() {
    return (uint32_t)index.size();
}

int LIST_CALLOC::clear() {
    for (auto node : index) {
        free(node->data);
        free(node);
    }
    index.clear();
    state = 0;
    return 0;
}

int64_t LIST_CALLOC::find(str &s) {
    if (s.len == 0 || s.string == NULL) return merr;
    string pattern((const char*)s.string, s.len);
    regex reg;
    try {
        reg = regex(pattern, regex::extended);
    } catch (const regex_error& e) {
        #ifdef bitmap_debug
        printf("LIST_CALLOC::find: regex compile error: %s\n", e.what());
        #endif
        return merr;
    }
    uint32_t n = (uint32_t)index.size();
    for (uint32_t i = 0; i < n; ++i) {
        Node* node = index[i];
        if (!node || node->len == 0 || node->data == NULL) continue;
        string elem_str((const char*)node->data, node->len);
        if (regex_search(elem_str, reg)) {
            return i;
        }
    }
    return -1;
}

int LIST_CALLOC::iserr() {
    return state;
}

// str实现
LIST_CALLOC::str::str(const char* s) {
    len = strlen(s);
    state = 0;
    if (len == 0) {
        string = NULL;
        return;
    }
    string = (uint8_t*)malloc(len);
    if (string == NULL) {
        #ifdef bitmap_debug
        printf("str init malloc error\n");
        #endif
        len = 0; state++;
        return;
    }
    memcpy(string, s, len);
}

LIST_CALLOC::str::str(uint8_t* s, uint32_t len) : string((uint8_t*)malloc(len)), len(len), state(0) {
    if (string == NULL) {
        #ifdef bitmap_debug
        printf("str init malloc error\n");
        #endif
        len = 0, state++;
        return;
    }
    memcpy(string, s, len);
}

LIST_CALLOC::str::str(const str& s) : string((uint8_t*)malloc(s.len)), len(s.len), state(s.state) {
    if (string == NULL) {
        #ifdef bitmap_debug
        printf("str init malloc error\n");
        #endif
        len = 0, state++;
        return;
    }
    memcpy(string, s.string, s.len);
}

LIST_CALLOC::str::~str() {
    free(string);
}
#define MAIN
#ifdef MAIN
#include <time.h>
int main(){
    LIST_CALLOC list;
    time_t start, end;
    start = clock();
    for(int i = 0;i < 1000000;i++){
        LIST_CALLOC::str s("hello world");
        list.rpush(s); 
    }
    end = clock();
    #define CLOCKS_PER_SEC 1000
    printf("LIST_CALLOC耗时: %.3f秒\n", (end - start) / (double)CLOCKS_PER_SEC);
    printf("end\n");
    return 0;
}
#endif