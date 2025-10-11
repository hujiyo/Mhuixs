#include "kvalh.hpp"

KVALOT::KVALOT(str* kvalot_name)//只能是规定的数量
:numof_tong(hash_tong_1024ge),keynum(0),
kvalot_name((char*)kvalot_name->string,kvalot_name->len),
state(success)
{
    hash_table.resize(numof_tong);//初始化哈希桶表大小
    if (hash_table.empty()) {
        state = merr;
        report(merr, kvalot_module, "Failed to initialize hash table");
    }
}

KVALOT::KVALOT(const KVALOT& other)
{
    // 初始化为一个有效的空状态
    numof_tong = 0;
    keynum = 0;
    state = success;
    
    // 复制简单成员
    kvalot_name = other.kvalot_name;
    
    // 检查源对象的状态
    if (other.state != success) {
        state = other.state;
        report(state, kvalot_module, "Source object for copy is in an error state.");
        return;
    }

    // 预分配内存，避免多次重新分配
    try {
        keypool.reserve(other.keynum);
        hash_table.resize(other.numof_tong);
    } catch (...) {
        report(merr, kvalot_module, "Memory allocation failed during copy construction");
        state = merr;
        goto error_cleanup;
    }
    numof_tong = other.numof_tong;
    
    // 深拷贝 keypool
    for (uint32_t i = 0; i < other.keypool.size(); ++i) {
        const KEY& src_key = other.keypool[i];
        
        keypool.emplace_back();
        KEY& dest_key = keypool.back();

        // 复制键名
        if (src_key.name != NULL_OFFSET) {
            uint32_t name_len = *(uint32_t*)g_memap.addr(src_key.name);
            str name_str((uint8_t*)g_memap.addr(src_key.name) + sizeof(uint32_t), name_len);
            if (dest_key.setname(name_str) != success) {
                state = merr;
                goto error_cleanup;
            }
        } else {
            dest_key.name = NULL_OFFSET;
        }

        // 复制 bhs
        if (copy_bhs(&dest_key.bhs, &src_key.bhs) != success) {
            state = merr;
            goto error_cleanup;
        }

        // 复制哈希索引
        dest_key.hash_index = src_key.hash_index;
    }
    keynum = other.keynum;

    // 深拷贝 hash_table
    for (uint32_t i = 0; i < numof_tong; i++) {
        const HASH_TONG& src_tong = other.hash_table[i];
        HASH_TONG& dest_tong = hash_table[i];

        dest_tong.numof_key = src_tong.numof_key;
        dest_tong.capacity = src_tong.capacity;
        
        if (src_tong.numof_key > 0) {
            dest_tong.offsetof_key = (uint32_t*)malloc(sizeof(uint32_t) * dest_tong.capacity);
            if (dest_tong.offsetof_key == NULL) {
                state = merr;
                goto error_cleanup;
            }
            memcpy(dest_tong.offsetof_key, src_tong.offsetof_key, src_tong.numof_key * sizeof(uint32_t));
        } else {
            dest_tong.offsetof_key = NULL;
        }
    }

    return; // 成功

error_cleanup:
    // 如果发生错误，清理部分构造的对象
    for (auto& tong : hash_table) {
        if(tong.offsetof_key) free(tong.offsetof_key);
    }
    hash_table.clear();

    for (auto& key : keypool) {
        key.clear_self();
    }
    keypool.clear();
    
    // 重置为已知的错误状态
    numof_tong = 0;
    keynum = 0;
    report(merr, kvalot_module, "Copy construction failed, object is in an error state.");
}

int KVALOT::copy_bhs(basic_handle_struct* dest, const basic_handle_struct* src) {
    if (!dest || !src) return merr;
    
    dest->type = src->type;
    if (src->type == M_NULL) {
        memset(&dest->handle, 0, sizeof(dest->handle));
        return success;
    }

    if(iserr_obj_type(src->type)){
        report(merr, kvalot_module, "Invalid object type for copying");
        return merr;
    }
    
    void* new_obj_mem = NULL;

    switch (src->type){
        #define COPY_BHS_OBJECT(TYPE, member) \
            case M_##TYPE: { \
                if (src->handle.member == NULL) break; \
                new_obj_mem = calloc(1, sizeof(TYPE)); \
                if (!new_obj_mem) { \
                    report(merr, kvalot_module, "Memory allocation failed for " #TYPE " object copy"); \
                    return merr; \
                } \
                auto* new_obj = new (new_obj_mem) TYPE(*src->handle.member); \
                if (new_obj->iserr()) { \
                    new_obj->~TYPE(); \
                    free(new_obj); \
                    return merr; \
                } \
                dest->handle.member = new_obj; \
                break; \
            }

        COPY_BHS_OBJECT(STREAM, stream)
        COPY_BHS_OBJECT(LIST, list)
        COPY_BHS_OBJECT(BITMAP, bitmap)
        COPY_BHS_OBJECT(TABLE, table)
        COPY_BHS_OBJECT(KVALOT, kvalot)

        // HOOK类型通常是引用的，浅拷贝即可
        case M_HOOK:
            dest->handle.hook = src->handle.hook;
            break;

        default:
            report(merr, kvalot_module, "Unsupported object type for copying in KVALOT");
            return merr;
    }
    return success;
}

// 辅助函数：验证键名合法性
int KVALOT::validate_key_name(str* key_name) {
    if (key_name == NULL || key_name->string == NULL || key_name->len == 0) {
        report(merr, kvalot_module, "Invalid key name: null or empty");
        return merr;
    }
    return success;
}

// 辅助函数：在哈希桶中查找键
uint32_t KVALOT::find_key_in_tong(HASH_TONG* tong, str* key_name) {
    if (tong == NULL || key_name == NULL) return UINT32_MAX;
    
    for (uint32_t i = 0; i < tong->numof_key; i++) {
        KEY* key = &keypool[tong->offsetof_key[i]];
        //比较长度
        if (*(uint32_t*)g_memap.addr(key->name) == key_name->len &&
                memcmp(g_memap.addr(key->name)+sizeof(uint32_t),
                key_name->string,key_name->len) == 0 ){
            return i; // 返回在桶中的索引
        }
    }
    return UINT32_MAX; // 未找到
}

// 辅助函数：确保桶容量足够
int KVALOT::tong_ensure_capacity(HASH_TONG* tong, uint32_t needed_capacity) {
    if (tong == NULL) return merr;
    
    if (tong->numof_key == 0) {
        // 初次分配
        tong->offsetof_key = (uint32_t*)malloc(sizeof(uint32_t) * 4);
        if (tong->offsetof_key == NULL) {
            report(merr, kvalot_module, "Memory allocation failed for hash bucket");
    }
}


basic_handle_struct KVALOT::find_key(str* key_name){
    /*
    警告：记得释放KEY的handle
    */
    if (state != success) {
        return {NULL, M_NULL};
    }
    
    if (validate_key_name(key_name) != success) {
        return {NULL, M_NULL};
    }
    
    uint32_t hash_index = murmurhash(*key_name,bits(numof_tong));
    HASH_TONG* hash_tong = &hash_table[hash_index];
    
    uint32_t found_index = find_key_in_tong(hash_tong, key_name);
    if (found_index != UINT32_MAX) {
        KEY* key = &keypool[hash_tong->offsetof_key[found_index]];
        return key->bhs;
    }
    
    return {NULL,M_NULL};
}

KVALOT::~KVALOT(){
    //释放哈希桶内的key
    for(uint32_t i = 0; i < numof_tong; i++){
        if (hash_table[i].offsetof_key != NULL) {
            free(hash_table[i].offsetof_key);
            hash_table[i].offsetof_key = NULL;
        }
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

mrc KVALOT::iserr() {
    return state;
}

const char* KVALOT::get_error_msg() {
    switch (state) {
        case success: return "No error";
        case merr: return "General error";
        case merr_open_file: return "File open error";
        case init_failed: return "Initialization failed";
        case permission_denied: return "Permission denied";
        default: return "Unknown error";
    }
}

// 辅助函数：统一的内存分配失败处理
static void handle_memory_error(const char* operation) {
    char error_msg[256];
    snprintf(error_msg, sizeof(error_msg), "Memory allocation failed during %s", operation);
    report(merr, kvalot_module, error_msg);
}

// 辅助函数：验证哈希表完整性
int kvalot_validate_integrity(KVALOT* kvalot) {
    if (kvalot == NULL) {
        report(merr, kvalot_module, "NULL KVALOT pointer");
        return merr;
    }
    
    if (kvalot->iserr() != success) {
        return merr;
    }
    
    // 可以在这里添加更多的完整性检查
    return success;
}

// C风格的便利函数：创建KVALOT对象
KVALOT* kvalot_create(const char* name) {
    if (name == NULL) {
        report(merr, kvalot_module, "NULL name provided to kvalot_create");
        return NULL;
    }
    
    str name_str;
    if (str_from_cstr(&name_str, name) != 0) {
        report(merr, kvalot_module, "Failed to create str from name");
        return NULL;
    }
    
    KVALOT* kvalot = (KVALOT*)malloc(sizeof(KVALOT));
    if (kvalot == NULL) {
        handle_memory_error("kvalot creation");
        str_free(&name_str);
        return NULL;
    }
    
    // 使用placement new创建对象
    new (kvalot) KVALOT(&name_str);
    
    str_free(&name_str);
    
    if (kvalot->iserr() != success) {
        kvalot->~KVALOT();
        free(kvalot);
        return NULL;
    }
    
    return kvalot;
}

// C风格的便利函数：销毁KVALOT对象
void kvalot_destroy(KVALOT* kvalot) {
    if (kvalot != NULL) {
        kvalot->~KVALOT();
        free(kvalot);
    }
}

// 调试和统计功能实现
uint32_t KVALOT::get_key_count() const {
    return keynum;
}

uint32_t KVALOT::get_bucket_count() const {
    return numof_tong;
}

float KVALOT::get_load_factor() const {
    if (numof_tong == 0) return 0.0f;
    return (float)keynum / (float)numof_tong;
}

void KVALOT::print_statistics() const {
    printf("\n=== KVALOT Statistics ===\n");
    printf("Name: %s\n", kvalot_name.c_str());
    printf("Total keys: %u\n", keynum);
    printf("Total buckets: %u\n", numof_tong);
    printf("Load factor: %.3f (target: %.2f)\n", get_load_factor(), hash_k);
    printf("State: %s\n", get_error_msg());
    
    // 桶分布统计
    uint32_t empty_buckets = 0;
    uint32_t max_bucket_size = 0;
    uint32_t total_entries = 0;
    
    for (uint32_t i = 0; i < numof_tong; i++) {
        uint32_t bucket_size = hash_table[i].numof_key;
        if (bucket_size == 0) {
            empty_buckets++;
        } else {
            total_entries += bucket_size;
            if (bucket_size > max_bucket_size) {
                max_bucket_size = bucket_size;
            }
        }
    }
    
    printf("Empty buckets: %u (%.1f%%)\n", empty_buckets, 
           100.0f * empty_buckets / numof_tong);
    printf("Max bucket size: %u\n", max_bucket_size);
    printf("Average bucket size: %.2f\n", 
           numof_tong > 0 ? (float)total_entries / (numof_tong - empty_buckets) : 0.0f);
    printf("========================\n\n");
}

#include "kvalh.hpp"

int KVALOT::rise_capacity() {
    // 1. 确定桶数量
    uint32_t new_numof_tong = 0;
    if (numof_tong < hash_tong_4096ge) new_numof_tong = hash_tong_4096ge;
    else if (numof_tong < hash_tong_16384ge) new_numof_tong = hash_tong_16384ge;
    else if (numof_tong < hash_tong_65536ge) new_numof_tong = hash_tong_65536ge;
    else if (numof_tong < hash_tong_262144ge) new_numof_tong = hash_tong_262144ge;
    else if (numof_tong < hash_tong_1048576ge) new_numof_tong = hash_tong_1048576ge;
    else if (numof_tong < hash_tong_4194304ge) new_numof_tong = hash_tong_4194304ge;
    else if (numof_tong < hash_tong_16777216ge) new_numof_tong = hash_tong_16777216ge;
    else {
        report(merr, kvalot_module, "Hash table already at maximum capacity");
        state = merr;
        return merr;// 已经到最大桶数量
    }

    // 2. 新建哈希桶表
    std::vector<HASH_TONG> new_hash_table;
    try {
        new_hash_table.resize(new_numof_tong);//已经保证初始化了
    } catch (...) {
        report(merr, kvalot_module, "Failed to allocate new hash table");
        state = merr;
        return merr;
    }

    // 3. 重新分配所有 key 到新桶
    for (uint32_t i = 0; i < keypool.size(); ++i) {
        KEY& key = keypool[i];
        
        // 检查key的有效性
        if (key.name == NULL_OFFSET) {
            continue; // 跳过无效的key
        }
        
        // 获取key名称用于重新计算hash
        uint32_t key_len = *(uint32_t*)g_memap.addr(key.name);
        str key_str((uint8_t*)g_memap.addr(key.name) + sizeof(uint32_t), key_len);
        
        // 重新计算 hash_index
        uint32_t new_hash_index = murmurhash(key_str, bits(new_numof_tong));
        HASH_TONG* tong = &new_hash_table[new_hash_index];
        
        // 使用辅助函数确保桶容量
        if (tong_ensure_capacity(tong, tong->numof_key + 1) != success) {
            // 释放已分配内存
            for (uint32_t j = 0; j < new_numof_tong; ++j) {
                if (new_hash_table[j].offsetof_key != NULL) {
                    free(new_hash_table[j].offsetof_key);
                }
            }
            report(merr, kvalot_module, "Failed to ensure bucket capacity during rehashing");
            state = merr;
            return merr;
        }
        
        // 添加key到桶中
        tong->offsetof_key[tong->numof_key] = i;
        tong->numof_key++;
        key.hash_index = new_hash_index;
    }

    // 4. 释放旧桶内存
    for (uint32_t i = 0; i < hash_table.size(); ++i) {
        if (hash_table[i].offsetof_key != NULL) {
            free(hash_table[i].offsetof_key);
        }
    }

    // 5. 替换哈希表和桶数量
    hash_table = std::move(new_hash_table);
    numof_tong = new_numof_tong;

    return success;
}

uint32_t KVALOT::bits(uint32_t X){
    /*
    功能：
    1.把哈希桶的数量转化为二进制位数
    无效输入返回0
    */
    switch (X)    {
    //哈希桶数量映射到二进制位数
    case hash_tong_1024ge:return 10;
    case hash_tong_4096ge:return 12;
    case hash_tong_16384ge:return 14;
    case hash_tong_65536ge:return 16;
    case hash_tong_262144ge:return 18;
    case hash_tong_1048576ge:return 20;
    case hash_tong_4194304ge:return 22;
    case hash_tong_16777216ge:return 24;
    default:
        report(merr, kvalot_module, "Invalid hash table size, using default");
        return bits(hash_tong_65536ge);//默认返回65536的位数
    }
}

uint32_t KVALOT::murmurhash(str& stream, uint32_t result_bits) 
{
    int len = stream.len;
    const uint8_t *data = (const uint8_t*)stream.string;
    
    // 参数验证
    if (data == NULL || len < 0 || result_bits > 32) {
        report(merr, kvalot_module, "Invalid parameters for murmurhash");
        return 0;
    }
    
    /*
    这个哈希算法叫murmurhash,这个算法我是找的网上的，并和几个大模型联合完成
    由Austin Appleby在2008年发明的。
    result_bits是指哈希值的位数，这个位数对应哈希表的大小。最大可以设置为32位。
    */
    uint32_t seed=0x9747b28c;
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

    return seed & ((1 << result_bits) - 1);
}

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









