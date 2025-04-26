#include "list.h"
#include "stdstr.h"
#include <time.h>
#include <stdio.h>

int main() {
    
    str *s1=stostr("hello12345678901234567890",25);
    str *s2=stostr("world12345678901234567890",25);
    str *s3=stostr("mhuixs12345678901234567890",26);
    str *s4=stostr("list12345678901234567890",24);
    str *s5=stostr("test12345678901234567890",24);
    clock_t start, ed;char* buffer = (char*)malloc(50);LIST *list = makeLIST_AND_SET_BLOCK_SIZE_NUM(512, 51200);
    start = clock();
    

    for(int i = 0; i < 10000; i++){
        
        add_head(list, s1->string, s1->len);
        add_head(list, s2->string, s2->len);
        add_head(list, s3->string, s3->len);
        add_head(list, s4->string, s4->len);
        add_head(list, s5->string, s5->len);
        add_tail(list, s1->string, s1->len);
        add_tail(list, s2->string, s2->len);
        add_tail(list, s3->string, s3->len);
        add_tail(list, s4->string, s4->len);
        add_tail(list, s5->string, s5->len);
        pop_head(list, buffer, 50);
        pop_head(list,buffer, 50);
        pop_head(list,buffer, 50);       
        pop_head(list,buffer, 50);
        pop_head(list,buffer, 50);
        pop_head(list,buffer, 50);
        pop_head(list,buffer, 50);
        pop_head(list,buffer, 50);
        pop_head(list,buffer, 50);
        pop_head(list,buffer, 50);               
    }
    freeLIST(list); 
    ed = clock();
    int tm = ed - start;
    printf("time: %d\n", tm);//单位为毫秒
    system("pause");
}
