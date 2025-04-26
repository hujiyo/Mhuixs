#include "list.hpp"
#include <time.h>
#include <stdio.h>

int main() {
    str s1="hello12345678901234567890";
    str s2("world12345678901234567890");
    str s3("mhuixs12345678901234567890");
    str s4("list12345678901234567890");
    str s5("test12345678901234567890");


    clock_t start, end;
    start = clock();

    for(int i = 0; i < 10000; i++){
        LIST list;
        list.lpush(s1);
        list.lpush(s2);
        list.lpush(s3);
        list.lpush(s4);
        list.lpush(s5);
        list.rpush(s1);
        list.rpush(s2);
        list.rpush(s3);
        list.rpush(s4);
        list.rpush(s5);
        list.lpop();
        list.lpop();
        list.lpop();
        list.lpop();
        list.lpop();
        list.lpop();
        list.lpop();
        list.lpop();
        list.lpop();
        list.lpop();
    }
    end = clock();
    int tm = end - start;
    printf("time: %d\n", tm);//单位为毫秒
    system("pause");
}