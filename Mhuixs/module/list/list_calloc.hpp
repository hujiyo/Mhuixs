#ifndef LIST_CALLOC_H
#define LIST_CALLOC_H
/*
#版权所有 (c) Mhuixs-team 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2024.11
Email:hj18914255909@outlook.com
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <regex>
#include <vector>

using namespace std;

#define merr -1
#define bitmap_debug

class LIST_CALLOC {
private:
    struct Node {
        uint8_t* data;
        uint32_t len;
    };
    std::vector<Node*> index; // 存储节点指针
    int state;
public:
    struct str {
        uint8_t *string;
        uint32_t len;
        int state;
        str(const char* s);
        str(uint8_t* s, uint32_t len);
        str(const str& s);
        ~str();
    };
    LIST_CALLOC();
    ~LIST_CALLOC();

    int lpush(str &s);
    int rpush(str &s);
    str lpop();
    str rpop();
    int insert(str &s, int64_t index);
    int update(str &s, int64_t index);
    int del(int64_t index);
    str get(int64_t index);
    uint32_t amount();
    int clear();
    int64_t find(str &s);
    int iserr();
};

#endif
