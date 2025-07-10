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
    
    // 检查对象状态
    if (state != success) {
        report(merr, kvalot_module, "KVALOT object is in error state");
        return merr;
    }
    
    // 验证键名合法性
    if (validate_key_name(key_name) != success) {
        return merr;
    }
    
    // 检查是否需要扩容
    if(keynum+1 >= numof_tong * hash_k ){        
        // 自动对哈希桶数量进行扩容到下一个级别
        if(rise_capacity() == merr){
            // 如果扩容失败，返回错误
            report(merr, kvalot_module, "Failed to expand capacity in add_key");
            state = merr;
            return merr; 
        }
    }
    
    // 检查type是否合法
    if(iserr_obj_type(type) || type == M_NULL){
        report(merr, kvalot_module, "Invalid object type");
        return merr;
    }

    // 对键名进行哈希，找到存储对应的哈希桶hash_index并保存
    uint32_t hash_index = murmurhash(*key_name, bits(numof_tong));
    HASH_TONG* current_tong = &hash_table[hash_index];
    
    // 检查是否存在同名键
    uint32_t existing_key_index = find_key_in_tong(current_tong, key_name);
    if (existing_key_index != UINT32_MAX) {
        report(merr, kvalot_module, "Key already exists");
        return merr;
    }
    
    // 确保桶容量足够
    if (tong_ensure_capacity(current_tong, current_tong->numof_key + 1) != success) {
        state = merr;
        return merr;
    }
    
    // keypool增加一个元素,默认添加新键都是存放在键池keypool内的末尾
    keypool.emplace_back();//创建一个空的KEY对象
    
    // 设置键名
    if(keypool[keynum].setname(*key_name) == merr) {
        report(merr, kvalot_module, "Failed to set key name");
        keypool.pop_back();//回滚
        state = merr;
        return merr;
    }

    // 创建对象
    int make_result = keypool[keynum].bhs.make_self(type, parameter1, parameter2, parameter3);
    if(make_result == merr) {
        report(merr, kvalot_module, "Failed to create object");
        keypool[keynum].clear_self();//回滚
        keypool.pop_back();//回滚
        state = merr;
        return merr;
    }
    keypool[keynum].hash_index = hash_index;//复制哈希表索引
    
    // 更新哈希桶内的数据    
    current_tong->offsetof_key[current_tong->numof_key] = keypool.size()-1;//桶内的键偏移量,从0开始
    current_tong->numof_key++;//桶内的键数量+1

    keynum++;//键数量+1
    return success;
}

int KVALOT::rmv_key(str* key_name)
{
    /*
    如果键类型为非HOOK,则直接删除键值对
    如果键类型为HOOK,则仅仅先断开链接,再删除KEY
    */
    
    // 检查对象状态
    if (state != success) {
        report(merr, kvalot_module, "KVALOT object is in error state");
        return merr;
    }
    
    // 验证键名合法性
    if (validate_key_name(key_name) != success) {
        return merr;
    }
    
    uint32_t hash_index = murmurhash(*key_name, bits(numof_tong));
    HASH_TONG* hash_tong = &hash_table[hash_index];
    
    // 查找要删除的键
    uint32_t found_index = find_key_in_tong(hash_tong, key_name);
    if (found_index == UINT32_MAX) {
        report(merr, kvalot_module, "Key not found for removal");
        return merr;
    }
    
    // 获取要删除的键
    uint32_t key_pool_index = hash_tong->offsetof_key[found_index];
    KEY* key_to_remove = &keypool[key_pool_index];
    
    // 清除键值对
    key_to_remove->clear_self();
    
    // 如果不是最后一个键，需要移动数据
    if (key_pool_index != keynum - 1) {
        // 将最后一个键移动到当前位置
        keypool[key_pool_index] = keypool[keynum - 1];
        
        // 更新被移动键在哈希桶中的索引
        KEY* moved_key = &keypool[key_pool_index];
        HASH_TONG* moved_key_tong = &hash_table[moved_key->hash_index];
        
        // 找到并更新移动键的索引
        for (uint32_t i = 0; i < moved_key_tong->numof_key; i++) {
            if (moved_key_tong->offsetof_key[i] == keynum - 1) {
                moved_key_tong->offsetof_key[i] = key_pool_index;
                break;
            }
        }
    }
    
    // 从keypool中删除最后一个元素
    keypool.pop_back();
    keynum--;
    
    // 从哈希桶中删除该键的索引
    // 将最后一个索引移动到当前位置
    hash_tong->offsetof_key[found_index] = hash_tong->offsetof_key[hash_tong->numof_key - 1];
    hash_tong->numof_key--;

    return success;
}









