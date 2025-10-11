#include <string.h>
#include <stdlib.h>

typedef char* mstring;

mstring mstr(char* str){
    mstring result = (mstring)malloc(strlen(str)+sizeof(size_t));
    if(result == NULL){
        return NULL;
    }
    *(size_t*)result = strlen(str);
    memcpy(result+sizeof(size_t), str, strlen(str));
    return result;
}
