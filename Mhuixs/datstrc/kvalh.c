/*
#版权所有 (c) Mhuixs-team 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2024.11
Email:hj18914255909@outlook.com
*/
/*
kvalh与redis相比，keval库的键值对数据结构具有可构建关系的特性
*/
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Mhudef.h"
#include "datstrc.h"
#define err -1

static uint32_t murmurhash(str* stream, uint32_t result_bits) 
{
    //备份数据
    int len = stream->len;
    uint8_t *data = (uint8_t*)malloc(len);
    memcpy(data,stream->string,len);//这样做是为了防止stream被修改
    /*
    这个哈希算法叫murmurhash,具体算法我是找的网上。
    由Austin Appleby在2008年发明的。
    */
    uint32_t seed=0;
    const int nblocks = len / 4;
    for (int i = 0; i < nblocks; i++) {
        uint32_t k1 = ((uint32_t)data[i*4]     ) |
                     ((uint32_t)data[i*4 + 1] <<  8) |
                     ((uint32_t)data[i*4 + 2] << 16) |
                     ((uint32_t)data[i*4 + 3] << 24); 
        k1 *= 0xcc9e2d51;k1 = (k1 << 15) | (k1 >> 17);k1 *= 0x1b873593; 
        seed ^= k1;
        seed = (seed << 13) | (seed >> 19);
        seed = seed * 5 + 0xe6546b64;
    } 
    const uint8_t *tail = (const uint8_t*)(data + nblocks*4); 
    uint32_t k1 = 0;
    switch(len & 3) {
        case 3: k1 ^= tail[2] << 16;
        case 2: k1 ^= tail[1] <<  8;
        case 1: k1 ^= tail[0];
                k1 *= 0xcc9e2d51;k1 = (k1 << 15) | (k1 >> 17);k1 *= 0x1b873593;
                seed ^= k1;
    } 
    seed ^= len;    
    seed ^= seed >> 16;
    seed *= 0x85ebca6b;
    seed ^= seed >> 13;
    seed *= 0xc2b2ae35;
    seed ^= seed >> 16; 

    free(data);
    return seed%result_bits;
}

#define hash_tong_1024ge        1024            //2^10
#define hash_tong_16384ge       16384           //2^14
#define hash_tong_65536ge       65536           //2^16
#define hash_tong_1048576ge     1048576         //2^20
#define hash_tong_16777216ge    16777216        //2^24
#define hash_k 0.75  //哈希表装载因子

//#include <math.h>
//#define bits(X) (int)(log(X)/log(2))
uint32_t bits(uint32_t X){
    /*
    功能：
    1.把哈希桶的数量转化为二进制位数
    2.根据大小等级返回对应的二进制位数

    无效输入返回0
    */
    switch (X)    {
    //哈希桶数量映射到二进制位数
    case hash_tong_1024ge:return 10;
    case hash_tong_16384ge:return 14;
    case hash_tong_65536ge:return 16;
    case hash_tong_1048576ge:return 20;
    case hash_tong_16777216ge:return 24;

    #define size1 0
    #define size2 1
    #define size3 2
    #define size4 3
    #define size5 4
    //根据大小等级返回对应的二进制位数
    case size1:return 10;
    case size2:return 14;
    case size3:return 16;
    case size4:return 20;
    case size5:return 24;

    default:return 0;
    }
}
/*
哈希桶的数量决定了哈希表的大小，哈希桶的数量越大，在面对更多数据时，哈希表的性能就越好。
但是占用的内存空间也会越大。根据键的数量来决定哈希桶的数量。
一般来说，当 键值对数量 / 哈希桶数量 <= 0.75 时，哈希表的性能最好。
*/

#define init_ROM 300
#define add_ROM 200

//支持自辐射神经网状式连接结构的“键值对数据结构”--->单向连接+连接系数 特点：支持复杂的网络结构和关系查询，如社交网络分析、路径查找等。
typedef struct KEY{
    void* handle;//指向任意数据结构描述符
    char type;//描述符类型
    str* name;//指向一个key名称存放的地址 
    uint32_t* linkey_offset;//数组,存放与当前key连接的key的偏移量
    uint32_t* linkey_coef;//数组,存放与当前key连接的key的连接系数
    uint32_t linkey_num;//与当前key连接的key数量
    uint32_t hash_index;//当前key在哈希表中的索引
}KEY;
typedef struct HASH_TONG{
    uint32_t numof_key;//桶内的key数量
    uint32_t* offsetof_key;//key偏移量数组
    /*
    这里的偏移量是在keypool中的偏移量
    */
}HASH_TONG;

typedef struct KVALOT{
    HASH_TONG* hash_table;//哈希表--->索引
    uint32_t numof_tong;//哈希桶数量

    KEY* keypool;//键池
    uint32_t keynum;//key数量
    uint32_t keypoolROM;//池总容量

    str* kvalot_name;//键值对池名称
}KVALOT;
void* retkeyobject(KEY* key){
    //返回key指向的对象
    if(key==NULL){
        return NULL;
    }
    return key->handle;
}

OBJECTYPE retkeytype(KEY* key){
    //返回key指向的对象的类型
    if(key==NULL){
        return M_NULL;
    }
    return key->type;
}
KVALOT* makeKVALOT(char* kvalot_name,uint8_t hash_tong_num)//只能是规定的数量
{
    /*
    创建一个键值对池的时候可以直接自定义哈希桶数量
    之后哈希桶的数量不会低于这个数量
    */
    //判断hash_tong_num是否是规定的数量
    if(!bits(hash_tong_num)){
        return NULL;
    }
    KVALOT* kvalot=(KVALOT*)calloc(1,sizeof(KVALOT));
    if(kvalot==NULL){
        return NULL;
    }
    kvalot->keypoolROM=init_ROM;//初始化池容量    
    kvalot->numof_tong=hash_tong_num;//初始化哈希表数量
    //kvalot->keynum=0;//初始化键数量
    kvalot->keypool=(KEY*)calloc( init_ROM , sizeof(KEY) );//创建一个键值对池
    kvalot->hash_table=(HASH_TONG*)calloc( kvalot->numof_tong , sizeof(HASH_TONG) );//创建一个哈希表    
    kvalot->kvalot_name=bcstr((uint8_t*)kvalot_name,strlen(kvalot_name));//创建一个键值对池名称
    if(!kvalot->keypool||!kvalot->hash_table||!kvalot->kvalot_name){
        free(kvalot->keypool);
        free(kvalot->hash_table);
        free(kvalot->kvalot_name);
        free(kvalot);
        return NULL;
    }
    strcpy(kvalot->kvalot_name,kvalot_name);//复制键值对池名称
    return kvalot;
}
KEY* kvalh_find_key(KVALOT* kvalot, str* key_name)
{
    /*
    警告：记得释放KEY的handle
    */
    uint32_t hash_index = murmurhash(key_name,bits(kvalot->numof_tong));
    HASH_TONG* hash_tong = &kvalot->hash_table[hash_index];
    for (uint32_t i = 0; i < hash_tong->numof_key; i++) {
        KEY* key = &kvalot->keypool[hash_tong->offsetof_key[i]];
        //比较长度
        if (key->name->len != key_name->len) {
            continue;
        }
        if (strncmp(key->name->string, key_name->string,key_name->len) == 0) {
            KEY *copy_key = (KEY *)malloc(sizeof(KEY));
            memcpy(copy_key, key, sizeof(KEY));
            return copy_key;//返回一个KEY的副本
        }
    }
    return NULL;//没有找到
}
int8_t kvalh_remove_key(KVALOT* kvalot, str* key_name)
{
    uint32_t hash_index = murmurhash(key_name,bits(kvalot->numof_tong));
    HASH_TONG* hash_tong = &kvalot->hash_table[hash_index];
    // 遍历哈希桶内的所有键
    for (uint32_t i = 0; i < hash_tong->numof_key; i++) {
        //i是当前键在哈希桶内的索引序号
        KEY* key = &kvalot->keypool[hash_tong->offsetof_key[i]];
        if(key->name->len==key_name->len)//比较长度
        {
            if (strncmp(key->name->string, key_name->string ,key_name->len) == 0) {
                /*
                typedef struct KEY{
                    void* handle;//指向任意数据结构描述符
                    char type;//描述符类型
                    str* name;//指向一个key名称存放的地址 
                    uint32_t* linkey_offset;//数组,存放与当前key连接的key的偏移量
                    uint32_t* linkey_coef;//数组,存放与当前key连接的key的连接系数
                    uint32_t linkey_num;//与当前key连接的key数量
                    uint32_t hash_index;//当前key在哈希表中的索引
                }KEY;
                */
                //根据type的类型为KEY的val_addr分别处理
                switch (key->type){
                    case M_STREAM:
                        freeSTREAM(key->handle);
                        break;
                    case M_LIST:
                        freeLIST(key->handle);
                        break;
                    case M_BITMAP:
                        freeBITMAP(key->handle);
                        break;
                    case M_STACK:
                        freeSTACK(key->handle);
                        break;
                    case M_QUEUE:
                        freeQUEUE(key->handle);
                        break;
                    case M_TABLE:
                        tblh_del_table(key->handle);
                        break;
                    case M_KEYLOT:
                        freeKVALOT(key->handle);
                        break;
                    default:
                        free(key->handle);
                        prinft("err:kvalh_remove_key:type error\n");
                        return err;
                }
                freeSTREAM(key->name);
                free(key->linkey_coef);
                free(key->linkey_offset);

                //清空对应的哈希桶内的数据

                //保存当前key的偏移量
                uint32_t offsetof_rmkey=hash_tong->offsetof_key[i];

                hash_tong->offsetof_key[i]=hash_tong->offsetof_key[hash_tong->numof_key-1];
                hash_tong->numof_key--;

                //将最后一个键移动到当前位置,使用memmove的好处：不用考虑本key是否是最后一个键
                memmove(key,&kvalot->keypool[kvalot->keynum-1],sizeof(KEY));
                //更新原来最后一个键对应的哈希桶内的数据
                HASH_TONG* hash_tong_tail = &kvalot->hash_table[key->hash_index];
                for(uint32_t i=0;i<hash_tong_tail->numof_key;i++){
                    if(hash_tong_tail->offsetof_key[i]==kvalot->keynum-1){
                        hash_tong_tail->offsetof_key[i]=offsetof_rmkey;
                        break;
                    }
                }
                kvalot->keynum--;
                return 0;
            }
        }
    }
    return err;
}
int8_t kvalh_add_keys(KVALOT* kvalot,KEY* key,uint32_t num)
{
    /*
    批量添加键值对
    增加的键是
    */
    int if_exist_same_key=0;//标记是否存在相同的键名
    if(key==NULL||num==0){
        return err;
    }
    for(uint32_t i=0;i<num;i++){
        KEY* key_temp=kvalh_find_key(kvalot,key[i].name);
        if(key_temp!=NULL){ //如果存在相同的键名
            //释放key_temp
            free(key_temp);
            //删除原来的键值对
            kvalh_remove_key(kvalot,key[i].name);
            //添加新的键值对
            kvalh_add_key(kvalot,key[i].name,key[i].type,key[i].handle);

            
            
        }
    }
    /*
    以key_name的形式向键值对池中加入一个键值对

    必须保证哈希桶的数量足够大，否则返回错误
    key_name:键名,以C字符串形式传入
    type:value类型
    parameter1:其它参数
    parameter2:其它参数

    M_BITMAP://使用第一个参数作为BITMAP的大小
    M_TABLE://使用第一个参数作为TABLE的字段信息，第二个参数作为TABLE的字段数量  
    */
    //先判断哈希桶是不是太少了
    if(kvalot->keynum+1 >= kvalot->numof_tong * hash_k ){
        /*
        自动对哈希桶数量进行扩容到下一个级别
        //...
        //...
        //...
        */
        return err;//如果键数量大于等于哈希表数量*加载因子,增加失败
    }
    //再判断键名池有没有满，满了就对keypool_ROM进行扩容
    if(kvalot->keynum+1>=kvalot->keypoolROM){//如果键值对池容量不足,增加keypool容量
        KEY* cc_keypool=kvalot->keypool;
        kvalot->keypool=(KEY*)realloc(kvalot->keypool,(kvalot->keypoolROM+add_ROM)*sizeof(KEY));
        if(kvalot->keypool==NULL){
            kvalot->keypool=cc_keypool;//恢复原来的键值对池
            return err;
        }
        memset(kvalot->keypool+kvalot->keypoolROM,0,add_ROM*sizeof(KEY));//初始化新增的键值对池
        kvalot->keypoolROM+=add_ROM;
    }
    //对键名进行哈希，找到存储对应的哈希桶hash_index并保存    
    uint32_t hash_index=murmurhash(key_name, bits(kvalot->numof_tong));//计算哈希表索引
    //寻找哈希桶内是否存在相同的键名
    if(kvalot->hash_table[hash_index].numof_key!=0){
        for(uint32_t i=0;i<kvalot->hash_table[hash_index].numof_key;i++){//遍历哈希桶内的所有键
            if(strcmp(kvalot->keypool[kvalot->hash_table[hash_index].offsetof_key[i]].name->string, key_name->string)==0){//如果存在相同的键名
                if_exist_same_key=1;
                break;
            }
        }
    }
    //hash_index先不要动，我们先试着把value和key都存放进去之后最后设置桶内参数
    //默认都是存放在键池keypool内的最后一个里
    //先为KEY成员keyname申请一段内存
    kvalot->keypool[kvalot->keynum].name = key_name; // 直接使用传入的str*类型
    //为KEY其他成员初始化
    kvalot->keypool[kvalot->keynum].linkey_num=0;//初始化键连接数量
    kvalot->keypool[kvalot->keynum].linkey_coef=NULL;//初始化键连接系数数组
    kvalot->keypool[kvalot->keynum].linkey_offset=NULL;//初始化键连接偏移量数组
    kvalot->keypool[kvalot->keynum].hash_index=hash_index;//复制哈希表索引
    kvalot->keypool[kvalot->keynum].type=type;//复制键类型

    //之后我们要根据type的类型为KEY的val_addr分别处理
    /*
    #define M_NULL       '0'
    #define M_KEYLOT     '1'
    #define M_STREAM     '2'
    #define M_LIST       '3'
    #define M_BITMAP     '4'
    #define M_STACK      '5'
    #define M_QUEUE      '6'
    #define M_HOOK       '7'
    #define M_TABLE      '8'
    */
    switch (type){
        case M_STREAM://这是最简单的数据类型
            kvalot->keypool[kvalot->keynum].handle=makeSTREAM();//创建一个STREAM
            break;
        case M_LIST:
            kvalot->keypool[kvalot->keynum].handle=makeLIST();//创建一个LIST
            break;
        case M_BITMAP://使用第一个参数作为BITMAP的大小
            kvalot->keypool[kvalot->keynum].handle
            = makeBITMAP(*(uint32_t*)parameter1);//创建一个BITMAP
            break;
        case M_STACK:
            kvalot->keypool[kvalot->keynum].handle=makeSTACK();//创建一个STACK
            break;
        case M_QUEUE:
            kvalot->keypool[kvalot->keynum].handle=makeQUEUE();//创建一个QUEUE
            break;
        case M_TABLE://使用第一个参数作为TABLE的字段信息，第二个参数作为TABLE的字段数量            
            kvalot->keypool[kvalot->keynum].handle
            = makeTABLE(    key_name,
                            (FIELD*)parameter1,
                            *(uint32_t*)parameter2
            );
            break;//创建一个TABLE
        case M_KEYLOT:
            kvalot->keypool[kvalot->keynum].handle=kvalh_make_kvalot(key_name,hash_tong_1024ge);//创建一个KEYLOT
            break;
        default:
            return err;
    }
    //接下来才是更新哈希桶内的数据
    kvalot->hash_table[hash_index].numof_key++;//哈希表中对应的哈希桶内键数量+1    
    //为哈希桶中键的偏移量数组申请更多内存
    uint32_t* cc_offsetof_key=kvalot->hash_table[hash_index].offsetof_key;
    kvalot->hash_table[hash_index].offsetof_key=
    (uint32_t*)realloc(kvalot->hash_table[hash_index].offsetof_key,
                        (kvalot->hash_table[hash_index].numof_key)*sizeof(uint32_t));//增加键连接系数数组    
    if(kvalot->hash_table[hash_index].offsetof_key==NULL){
        kvalot->hash_table[hash_index].offsetof_key=cc_offsetof_key;
        kvalot->hash_table[hash_index].numof_key--;//哈希表中对应的哈希桶内键数量还原
        goto ERR;
    }
    //记录这个KEY的偏移量 ————> keypool[偏移量]
    kvalot->hash_table[hash_index].offsetof_key[kvalot->hash_table[hash_index].numof_key-1]
    = kvalot->keynum;//真正的偏移量是sizeof(KEY)*kvalot->keynum <=> keypool[kvalot->keynum]

    kvalot->keynum++;//键数量+1

    return 0;
    
    ERR:
    {
        //清空新增KEY
        freeSTREAM(kvalot->keypool[kvalot->keynum].name);
        memset(&kvalot->keypool[kvalot->keynum],0,sizeof(KEY));
        return err;
    }

}
int8_t kvalh_add_newkey(KVALOT* kvalot, str* key_name, uint8_t type, void* parameter1, void* parameter2)//添加一个键值对
{
    int if_exist_same_key=0;//标记是否存在相同的键名
    /*
    以key_name的形式向键值对池中加入一个
    全新的键值对

    必须保证哈希桶的数量足够大，否则返回错误
    key_name:键名,以C字符串形式传入
    type:value类型
    parameter1:其它参数
    parameter2:其它参数

    M_BITMAP://使用第一个参数作为BITMAP的大小
    M_TABLE://使用第一个参数作为TABLE的字段信息，第二个参数作为TABLE的字段数量  
    */
    //先判断哈希桶是不是太少了
    if(kvalot->keynum+1 >= kvalot->numof_tong * hash_k ){
        /*
        自动对哈希桶数量进行扩容到下一个级别
        //...
        //...
        //...
        */
        return err;//如果键数量大于等于哈希表数量*加载因子,增加失败
    }
    //再判断键名池有没有满，满了就对keypool_ROM进行扩容
    if(kvalot->keynum+1>=kvalot->keypoolROM){//如果键值对池容量不足,增加keypool容量
        KEY* cc_keypool=kvalot->keypool;
        kvalot->keypool=(KEY*)realloc(kvalot->keypool,(kvalot->keypoolROM+add_ROM)*sizeof(KEY));
        if(kvalot->keypool==NULL){
            kvalot->keypool=cc_keypool;//恢复原来的键值对池
            return err;
        }
        memset(kvalot->keypool+kvalot->keypoolROM,0,add_ROM*sizeof(KEY));//初始化新增的键值对池
        kvalot->keypoolROM+=add_ROM;
    }
    //对键名进行哈希，找到存储对应的哈希桶hash_index并保存    
    uint32_t hash_index=murmurhash(key_name, bits(kvalot->numof_tong));//计算哈希表索引
    //寻找哈希桶内是否存在相同的键名
    if(kvalot->hash_table[hash_index].numof_key!=0){
        for(uint32_t i=0;i<kvalot->hash_table[hash_index].numof_key;i++){//遍历哈希桶内的所有键
            if(strcmp(kvalot->keypool[kvalot->hash_table[hash_index].offsetof_key[i]].name->string, key_name->string)==0){//如果存在相同的键名
                if_exist_same_key=1;
                break;
            }
        }
    }
    //hash_index先不要动，我们先试着把value和key都存放进去之后最后设置桶内参数
    //默认都是存放在键池keypool内的最后一个里
    //先为KEY成员keyname申请一段内存
    kvalot->keypool[kvalot->keynum].name = key_name; // 直接使用传入的str*类型
    //为KEY其他成员初始化
    kvalot->keypool[kvalot->keynum].linkey_num=0;//初始化键连接数量
    kvalot->keypool[kvalot->keynum].linkey_coef=NULL;//初始化键连接系数数组
    kvalot->keypool[kvalot->keynum].linkey_offset=NULL;//初始化键连接偏移量数组
    kvalot->keypool[kvalot->keynum].hash_index=hash_index;//复制哈希表索引
    kvalot->keypool[kvalot->keynum].type=type;//复制键类型

    //之后我们要根据type的类型为KEY的val_addr分别处理
    /*
    #define M_NULL       '0'
    #define M_KEYLOT     '1'
    #define M_STREAM     '2'
    #define M_LIST       '3'
    #define M_BITMAP     '4'
    #define M_STACK      '5'
    #define M_QUEUE      '6'
    #define M_HOOK       '7'
    #define M_TABLE      '8'
    */
    switch (type){
        case M_STREAM://这是最简单的数据类型
            kvalot->keypool[kvalot->keynum].handle=makeSTREAM();//创建一个STREAM
            break;
        case M_LIST:
            kvalot->keypool[kvalot->keynum].handle=makeLIST();//创建一个LIST
            break;
        case M_BITMAP://使用第一个参数作为BITMAP的大小
            kvalot->keypool[kvalot->keynum].handle
            = makeBITMAP(*(uint32_t*)parameter1);//创建一个BITMAP
            break;
        case M_STACK:
            kvalot->keypool[kvalot->keynum].handle=makeSTACK();//创建一个STACK
            break;
        case M_QUEUE:
            kvalot->keypool[kvalot->keynum].handle=makeQUEUE();//创建一个QUEUE
            break;
        case M_TABLE://使用第一个参数作为TABLE的字段信息，第二个参数作为TABLE的字段数量            
            kvalot->keypool[kvalot->keynum].handle
            = makeTABLE(    key_name,
                            (FIELD*)parameter1,
                            *(uint32_t*)parameter2
            );
            break;//创建一个TABLE
        case M_KEYLOT:
            kvalot->keypool[kvalot->keynum].handle=kvalh_make_kvalot(key_name,hash_tong_1024ge);//创建一个KEYLOT
            break;
        default:
            return err;
    }
    //接下来才是更新哈希桶内的数据
    kvalot->hash_table[hash_index].numof_key++;//哈希表中对应的哈希桶内键数量+1    
    //为哈希桶中键的偏移量数组申请更多内存
    uint32_t* cc_offsetof_key=kvalot->hash_table[hash_index].offsetof_key;
    kvalot->hash_table[hash_index].offsetof_key=
    (uint32_t*)realloc(kvalot->hash_table[hash_index].offsetof_key,
                        (kvalot->hash_table[hash_index].numof_key)*sizeof(uint32_t));//增加键连接系数数组    
    if(kvalot->hash_table[hash_index].offsetof_key==NULL){
        kvalot->hash_table[hash_index].offsetof_key=cc_offsetof_key;
        kvalot->hash_table[hash_index].numof_key--;//哈希表中对应的哈希桶内键数量还原
        goto ERR;
    }
    //记录这个KEY的偏移量 ————> keypool[偏移量]
    kvalot->hash_table[hash_index].offsetof_key[kvalot->hash_table[hash_index].numof_key-1]
    = kvalot->keynum;//真正的偏移量是sizeof(KEY)*kvalot->keynum <=> keypool[kvalot->keynum]

    kvalot->keynum++;//键数量+1

    return 0;
    
    ERR:
    {
        //清空新增KEY
        freeSTREAM(kvalot->keypool[kvalot->keynum].name);
        memset(&kvalot->keypool[kvalot->keynum],0,sizeof(KEY));
        return err;
    }
}
KEY* kvalh_find_key(KVALOT* kvalot, str* key_name)
{
    /*
    警告：记得释放KEY的handle
    */
    uint32_t hash_index = murmurhash(key_name,bits(kvalot->numof_tong));
    HASH_TONG* hash_tong = &kvalot->hash_table[hash_index];
    for (uint32_t i = 0; i < hash_tong->numof_key; i++) {
        KEY* key = &kvalot->keypool[hash_tong->offsetof_key[i]];
        //比较长度
        if (key->name->len != key_name->len) {
            continue;
        }
        if (strncmp(key->name->string, key_name->string,key_name->len) == 0) {
            KEY *copy_key = (KEY *)malloc(sizeof(KEY));
            memcpy(copy_key, key, sizeof(KEY));
            return copy_key;//返回一个KEY的副本
        }
    }
    return NULL;//没有找到
}
int kvalh_copy_kvalot(KVALOT* kvalot_tar,KVALOT* kvalot_src)
{
    /*
    功能：
    把kvalot_src的键值对复制到kvalot_tar中

    相同的键名：tar会被src中的键值覆盖
    */
    //先对两个键值对池进行信息检查
    if(kvalot_tar==NULL||kvalot_src==NULL){
        return err;
    }
    //计算两个键值对池合并后预计的键数量
    int key_num_sum = kvalot_tar->keynum;//原键值对池的键数量
    for(uint32_t i=0;i<kvalot_src->keynum;i++){
        KEY src_s_key = kvalot_src->keypool[i];
        KEY* tar_s_key = kvalh_find_key(kvalot_tar, src_s_key.name);
        if(tar_s_key==NULL){
            key_num_sum++;//键数量+1
        }
        free(tar_s_key);
    }
    if(key_num_sum>kvalot_tar->keypoolROM){//如果键值对池容量不足
        return err;
    }
    //开始复制
    for(uint32_t i=0;i<kvalot_src->keynum;i++){
        KEY src_s_key = kvalot_src->keypool[i];}

    


}




void freeKVALOT(KVALOT* kvalot)
{
    if(kvalot==NULL){
        return;
    }
    for(uint32_t i=0;i<kvalot->keynum;i++){
        freeSTREAM(kvalot->keypool[i].name);
        free(kvalot->keypool[i].linkey_coef);
        free(kvalot->keypool[i].linkey_offset);
        free(kvalot->keypool[i].handle);
    }
    free(kvalot->keypool);
    free(kvalot->hash_table);
    freeSTREAM(kvalot->kvalot_name);
    free(kvalot);
}
