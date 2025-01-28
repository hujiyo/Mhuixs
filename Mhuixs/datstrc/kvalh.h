#include <stdint.h>
/*
#版权所有 (c) Mhuixs-team 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2024.11
Email:hj18914255909@outlook.com
*/
#ifndef KVALH_H
#define KVALH_H

/*
这个库还没有完成
*/

#define hash_tong_1024ge        1024            //2^10
#define hash_tong_16384ge       16384           //2^14
#define hash_tong_65536ge       65536           //2^16
#define hash_tong_1048576ge     1048576         //2^20
#define hash_tong_16777216ge    16777216        //2^24

typedef struct KEY{
    void* handle;//指向任意数据结构描述符
    OBJECTYPE type;//描述符类型
    str* name;//指向一个key名称存放的地址 
    uint32_t hash_index;//当前key在哈希表中的索引
}KEY;

typedef struct KVALOT{
    HASH_TONG* hash_table;//哈希表--->索引
    uint32_t numof_tong;//哈希桶数量

    KEY* keypool;//键池
    uint32_t keynum;//key数量
    uint32_t keypoolROM;//池总容量

    str* kvalot_name;//键值对池名称
}KVALOT;

KVALOT*     makeKVALOT          (char* kvalot_name,uint8_t hash_tong_num);
KEY*        kvalh_find_key      (KVALOT* kvalot, str* key_name);
int8_t      kvalh_remove_key    (KVALOT* kvalot, str* key_name);
int8_t      kvalh_add_newkey    (KVALOT* kvalot, str* key_name, uint8_t type, 
                                void* parameter1, void* parameter2);
KEY*        kvalh_find_key      (KVALOT* kvalot, str* key_name);
int         kvalh_copy_kvalot   (KVALOT* kvalot_tar,KVALOT* kvalot_src);
void        freeKVALOT          (KVALOT* kvalot);
Obj         retvalue            (KVALOT* kvalot,str* key_name);

#endif