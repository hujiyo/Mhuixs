#include "kvalh.hpp"

int KVALOT::add_key(str* key_name, obj_type type, void* parameter1, void* parameter2,void *parameter3)//添加一个键值对
{
    /*
    以key_name的形式向键值对池中创建一个  全新的键值对   
    key_name:键名,以str字符串形式传入
    type:obj_type类型
    parameter1:其它参数
    parameter2:其它参数
    parameter3:其它参数
    */
    //先判断键名是否合法
    if(key_name==NULL || key_name->string==NULL || key_name->len==0){
        return merr;
    }
    //先判断哈希桶是不是太少了
    if(keynum+1 >= numof_tong * hash_k ){        
        //自动对哈希桶数量进行扩容到下一个级别
        //rise_capacity()函数会自动对哈希桶数量进行扩容到下一个级别
        if(rise_capacity()==merr){
           //如果扩容失败，返回错误
            printf("KVALOT::add_key:Error: rise_capacity error.\n");
            return merr; 
        }
    }
    //检查type是否合法
    if(iserr_obj_type(type) || type == M_NULL){
        return merr;
    }

    //对键名进行哈希，找到存储对应的哈希桶hash_index并保存
    uint32_t hash_index=murmurhash(*key_name, bits(numof_tong));//计算哈希表索引
    //寻找哈希桶内是否存在相同的键名
    uint32_t numof_key_in_this_tong=hash_table[hash_index].numof_key;
    uint32_t* offsetof_key=hash_table[hash_index].offsetof_key;
    if(numof_key_in_this_tong!=0){
        for(uint32_t i=0;i<numof_key_in_this_tong;i++){//遍历哈希桶内的所有键
            //先比较长度
            if(*(uint32_t*)key_name_pool->addr(offsetof_key[i]) == key_name->len){
                //再比较内容
                if( memcmp( key_name_pool->addr(offsetof_key[i]) + sizeof(uint32_t),
                            key_name->string,
                            key_name->len) ==0 ){
                    //如果相同，返回错误
                    return merr;
                }
            }            
        }
    }
    //上面都是一些检查，下面开始正式添加键值对，首先要创建指定类型的对象，要根据type的类型为KEY的handle分别处理
    {
        uint32_t* new_offsetof_key =(uint32_t*)realloc(
            hash_table[hash_index].offsetof_key,
            sizeof(uint32_t)*(hash_table[hash_index].numof_key+1));
        if(new_offsetof_key==NULL){
            //概率很小
            printf("KVALOT::add_key:Error: Memory allocation failed for offsetof_key.\n");
            return merr;
        }
        hash_table[hash_index].offsetof_key=new_offsetof_key;
    }//把这个内存分配提前，因为如果下面出错了这个是不用回滚的
    //keypool增加一个元素,默认添加新键都是存放在键池keypool内的末尾
    keypool.emplace_back();//创建一个空的KEY对象
    if(keypool[keynum].setname(*key_name,*key_name_pool)==merr)//设置键名
    {
        //如果设置键名失败，返回错误
        printf("KVALOT::add_key:Error: setname error.\n");
        keypool.pop_back();//回滚
        return merr;
    }

    int flag = keypool[keynum].bhs.make_self(type,parameter1,parameter2,parameter3);//创建对象
    if(flag==merr){//如果创建对象失败，返回错误
        printf("KVALOT::add_key:Error: make_self error.\n");
        keypool[keynum].clear_self(*key_name_pool);//回滚
        keypool.pop_back();//回滚
        return merr;
    }
    keypool[keynum].hash_index=hash_index;//复制哈希表索引
    
    //接下来才是更新哈希桶内的数据    
    hash_table[hash_index].offsetof_key[hash_table[hash_index].numof_key]= keypool.size()-1;//桶内的键偏移量,从0开始
    hash_table[hash_index].numof_key++;//桶内的键数量+1


    keynum++;//键数量+1
    return 0;
}

int KVALOT::rmv_key(str* key_name)
{
    /*
    如果键类型为非HOOK,则直接删除键值对
    如果键类型为HOOK,则仅仅先断开链接,再删除KEY
    */
    uint32_t hash_index = murmurhash(*key_name,bits(numof_tong));
    HASH_TONG* hash_tong = &hash_table[hash_index];
    // 遍历哈希桶内的所有键
    for (uint32_t i = 0; i < hash_tong->numof_key; i++) {
        //i是当前键在哈希桶内的索引序号
        KEY* key = &keypool[hash_tong->offsetof_key[i]];
        if(*(uint32_t*)key_name_pool->addr(key->name) == key_name->len )//比较长度
        {
            if (memcmp(key_name_pool->addr(key->name)+sizeof(uint32_t),
                         key_name->string ,key_name->len) == 0) {
                //找到了要删除的键                
                key->clear_self(*key_name_pool);//清除键值对
            
                //清空对应的哈希桶内的数据
                //保存当前key的偏移量
                uint32_t offsetof_rmkey=hash_tong->offsetof_key[i];
                auto midit = keypool.begin()+offsetof_rmkey;
                *midit = keypool.back();//把最后一个键移动到当前位置
                //更新原来最后一个键对应的哈希桶内的数据
                HASH_TONG* hash_tong_tail = &hash_table[midit->hash_index];
                for(uint32_t i=0;i<hash_tong_tail->numof_key;i++){
                    if(hash_tong_tail->offsetof_key[i]==keynum-1){
                        hash_tong_tail->offsetof_key[i]=offsetof_rmkey;
                        break;
                    }
                }
                keypool.pop_back();//删除最后一个键
                keynum--;//键数量-1


                hash_tong->offsetof_key[i]=hash_tong->offsetof_key[hash_tong->numof_key-1];
                hash_tong->numof_key--;

                return 0;
            }
        }
    }
    return merr;
}









