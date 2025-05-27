#include "kvalh.hpp"


KVALOT::KVALOT(str* kvalot_name)//只能是规定的数量
:numof_tong(hash_tong_1024ge),keynum(0),
kvalot_name((char*)kvalot_name->string,kvalot_name->len)
{
    hash_table.resize(numof_tong);//初始化哈希桶表大小
}
/*
KVALOT::KVALOT(const KVALOT& kvalot){
    hash_table.resize(kvalot.numof_tong);//初始化哈希桶表大小
    numof_tong = kvalot.numof_tong;
    for(uint32_t i = 0; i < kvalot.numof_tong; i++){//复制哈希桶表
        hash_table[i].numof_key = kvalot.hash_table[i].numof_key;
        hash_table[i].offsetof_key = 
        (uint32_t*)calloc(hash_table[i].numof_key,sizeof(uint32_t));//分配偏移量数组
        memcpy(hash_table[i].offsetof_key,kvalot.hash_table[i].offsetof_key,hash_table[i].numof_key*sizeof(uint32_t));//复制偏移量数组
    }


}*/

basic_handle_struct KVALOT::find_key(str* key_name){
    /*
    警告：记得释放KEY的handle
    */
    uint32_t hash_index = murmurhash(*key_name,bits(numof_tong));
    HASH_TONG* hash_tong = &hash_table[hash_index];
    for (uint32_t i = 0; i < hash_tong->numof_key; i++) {
        KEY* key = &keypool[hash_tong->offsetof_key[i]];
        //比较长度
        if (*(uint32_t*)g_memap.addr(key->name) == key_name->len &&
                memcmp(g_memap.addr(key->name)+sizeof(uint32_t),
                key_name->string,key_name->len) == 0 ){
            return key->bhs;
        }
    }
    return {NULL,M_NULL};
}

KVALOT::~KVALOT(){
    //释放哈希桶内的key
    for(uint32_t i = 0; i < numof_tong; i++){
        free(hash_table[i].offsetof_key);
    }
    //释放key池
    for(uint32_t i = 0; i < keynum; i++){
        keypool[i].clear_self();
    }
}

str KVALOT::get_name(){
    str s((uint8_t*)kvalot_name.c_str(),kvalot_name.size());
    return s;
}