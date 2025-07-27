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
            return merr;
        }
        tong->capacity = 4;
    } else if (needed_capacity > tong->capacity) {
        // 需要扩容
        uint32_t new_capacity = tong->capacity;
        while (new_capacity < needed_capacity) {
            new_capacity *= 2;
        }
        uint32_t* new_offsetof_key = (uint32_t*)realloc(tong->offsetof_key, sizeof(uint32_t) * new_capacity);
        if (new_offsetof_key == NULL) {
            report(merr, kvalot_module, "Memory reallocation failed for hash bucket");
            return merr;
        }
        tong->offsetof_key = new_offsetof_key;
        tong->capacity = new_capacity;
    }
    return success;
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