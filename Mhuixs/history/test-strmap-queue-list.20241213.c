#include "list.h"
#include <string.h>
#include "mlibs/strmap.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "queue.h"

//本test主要测试大量“小内存”和“中等大小内存”分配和释放的性能
//小内存：2-1008字节
//普通申请：1009-10000字节
//对于大内存申请，使用malloc和calloc即可，不在测试范围内

//######### strmap.h测试 2024.12.13
/*
int main()
{
    uint8_t* offset1=NULL;
    uint8_t* offset2=NULL;
    //随机数分配
    srand(time(NULL));
    //测试性能,100万次用时
    //2-1008字节:95-110ms
    //1009-10000:290ms-450ms内缓慢递增
    clock_t start,end;
    start = clock();

    for(int i=0;i<1000000;i++){
        offset1 = calloc(1000,1);
        if(offset1==NULL)printf("malloc fail\n");
        offset2 = calloc(1000,1);        
        if(offset2==NULL)printf("malloc fail\n");
        free(offset1);
        free(offset2);
    }

    end = clock();
    printf("time:%d ms \n",end-start);
}
*/
/*
int main()
{
    STRPOOL strpool=build_strpool(1024,200);
    if(strpool==NULL)printf("build strpool fail\n");

    OFFSET offset1,offset2;
    //测试性能,100万次用时    块大小：4
    //2：28ms           2:小于块的大小
    //6：45ms           6:2个块的大小
    //9：61ms           9:3个块的大小
    //11：54ms           11:3个块的大小
    //50:270-280ms       50：13个块的大小
    //综上：当块的大小为4时，每个块的掠过时间为20-30ms

    //测试性能,100万次用时    块大小：1024
    //2：34ms  
    //9: 30ms
    //900: 31ms       
    //1000:26ms
    //1024：33ms  
    //1025：42ms 
    //2000: 42ms       
    //3000：62ms           :3个块的大小
    //100000: 1953ms           :98个块的大小
    //综上：当块的大小为1024时，每个块的掠过时间为20-25ms

    clock_t start,end;
    start = clock();
    uint32_t len=100000;
    for(int i=0;i<1000000;i++){
        offset1 = STRmalloc(strpool,len);
        if(!offset1)printf("malloc fail\n");
        offset2 = STRmalloc(strpool,len);
        if(!offset2)printf("malloc fail\n");
        STRfree(strpool,offset1,len);
        STRfree(strpool,offset2,len);
    }

    end = clock();
    printf("time:%d ms \n",end-start);

}
*/

//######### list.h VS queue.h 测试 2024.12.13
//介绍:list.h基于strmap.h实现    queue.h基于malloc/calloc实现
/*
int main()
{
    QUEUE queue;
    initQUEUE(&queue);

    char ch[50]="#qwdijbwijdbqwijdbwqjd#";
    //测试性能,增加减少100万次用时
    //结果:1040ms
    clock_t start,end;
    start = clock();

    char output[50]={0};
    for(int i=0;i<1000000;i++){
        putintoQUEUE(&queue,(uint8_t*)ch,strlen(ch));//向队列末尾增加3个字符串元素
        putintoQUEUE(&queue,(uint8_t*)"123456789",9);
        putintoQUEUE(&queue,(uint8_t*)"swdwdwdw",8);
        getfromQUEUE(&queue,output,50);//从队列头弹出3个字符串元素
        getfromQUEUE(&queue,output,50);
        getfromQUEUE(&queue,output,50);
        putintoQUEUE(&queue,(uint8_t*)ch,strlen(ch));//向队列末尾增加3个字符串元素
        putintoQUEUE(&queue,(uint8_t*)"123456789",9);
        putintoQUEUE(&queue,(uint8_t*)"swdwdwdw",8);
        getfromQUEUE(&queue,output,50);//从队列头弹出3个字符串元素
        getfromQUEUE(&queue,output,50);
        getfromQUEUE(&queue,output,50);
    }
    end = clock();
    printf("time:%d ms \n",end-start);
}
*/
/*
int main()
{
    LIST list;//为了实现更好的性能，你可以在list.h中修改BLOCK_SIZE的大小
    //注意:大的block_size可以提高性能，但是会产生更多的内存碎片，请根据实际情况进行调整
    initLIST(&list);

    char ch[50]="#qwdijbwijdbqwijdbwqjd#";
    //测试性能,增加减少100万次用时
    clock_t start,end;
    start = clock();
    //块大小：4  结果:9ms(10000次)
    //块大小：1024  结果:0ms(10000次)      结果：210-250ms(100万次)
    //块大小：64    结果:220-280ms(100万次)

    char output[50]={0};
    for(int i=0;i<1000000;i++){
        add_head(&list,(uint8_t*)ch,strlen(ch));//向列表头增加3个字符串元素
        add_head(&list,(uint8_t*)"123456789",9);
        add_head(&list,(uint8_t*)"swdwdwdw",8);
        pop_tail(&list,output,50);//从列表尾弹出3个字符串元素
        pop_tail(&list,output,50);
        pop_tail(&list,output,50);
        add_tail(&list,(uint8_t*)ch,strlen(ch));//向列表尾增加3个字符串元素
        add_tail(&list,(uint8_t*)"123456789",9);
        add_tail(&list,(uint8_t*)"swdwdwdw",8);
        pop_head(&list,output,50);//从列表头弹出3个字符串元素 
        pop_head(&list,output,50); 
        pop_head(&list,output,50);
    }    
    end = clock();
    printf("time:%d ms \n",end-start);
    
}
*/