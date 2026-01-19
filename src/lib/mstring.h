#ifndef MSTRING_H
#define MSTRING_H

#include <string.h>
#include <stdlib.h>
#include <stdint.h>


typedef char* mstring;

static inline mstring mstr(char* str){
    mstring result = (mstring)malloc(strlen(str)+sizeof(size_t));
    if(result == NULL){
        return NULL;
    }
    *(size_t*)result = strlen(str);
    memcpy(result+sizeof(size_t), str, strlen(str));
    return result;
}

static inline size_t mstrlen(mstring str){
    if(str == NULL){
        return SIZE_MAX;
    }
    return *(size_t*)str;
}

#endif