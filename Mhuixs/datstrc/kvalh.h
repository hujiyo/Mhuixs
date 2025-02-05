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

typedef struct KEY KEY;
typedef struct HASH_TONG HASH_TONG;

typedef struct KVALOT KVALOT;

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