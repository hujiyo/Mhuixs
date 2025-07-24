#include "fcall.hpp"
#include "registry.hpp"
#include "usergroup.hpp"
#include "funseq.h"
#include "tblh.hpp"
#include "kvalh.hpp"
#include "list.hpp"
#include "bitmap.hpp"
#include "stream.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern Registry Reg;

mrc Mhuixscall(UID caller, basic_handle_struct bhs, FunID funseq, int argc, void* argv[])
{
    // 根据命令类型调用相应的处理函数
    switch(funseq){
        // TABLE相关命令
        case CMD_ADD_RECORD:
            return handle_add_record(caller, bhs, argc, argv);
        case CMD_GET_RECORD:
            return handle_get_record(caller, bhs, argc, argv);
        case CMD_DEL_RECORD:
            return handle_del_record(caller, bhs, argc, argv);
            
        // KVALOT相关命令
        case CMD_SET_KEY:
            return handle_set_key(caller, bhs, argc, argv);
        case CMD_GET_KEY:
            return handle_get_key(caller, bhs, argc, argv);
        case CMD_DEL_KEY:
            return handle_del_key(caller, bhs, argc, argv);
            
        // LIST相关命令
        case CMD_LPUSH:
            return handle_lpush(caller, bhs, argc, argv);
        case CMD_RPUSH:
            return handle_rpush(caller, bhs, argc, argv);
        case CMD_LPOP:
            return handle_lpop(caller, bhs, argc, argv);
        case CMD_RPOP:
            return handle_rpop(caller, bhs, argc, argv);
            
        // BITMAP相关命令
        case CMD_SET_BIT:
            return handle_set_bit(caller, bhs, argc, argv);
        case CMD_GET_BIT:
            return handle_get_bit(caller, bhs, argc, argv);
            
        // STREAM相关命令
        case CMD_APPEND:
            return handle_append(caller, bhs, argc, argv);
        case CMD_GET_STREAM_ALL:
            return handle_get_stream(caller, bhs, argc, argv);
            
        // HOOK相关命令
        case CMD_HOOK_CREATE:
            return handle_hook_create(caller, argc, argv);
        case CMD_HOOK_SWITCH:
            return handle_hook_switch(caller, argc, argv);
            
        default:
            printf("Mhuixscall: Unknown command %d\n", funseq);
            return merr;
    }
}

// TABLE命令处理函数
mrc handle_add_record(UID caller, basic_handle_struct bhs, int argc, void* argv[]) {
    // 检查对象类型是否为TABLE
    if (bhs.type != M_TABLE) {
        printf("Error: Object is not a TABLE\n");
        return merr;
    }
    
    // 检查权限
    // 这里应该检查添加记录的权限
    
    // 获取TABLE对象
    TABLE* table = bhs.handle.table;
    
    // 调用TABLE的add_record方法
    if (argc > 0) {
        // 将参数转换为initializer_list
        std::initializer_list<char*> contents = {};
        // 这里需要实际处理参数
        int64_t result = table->add_record(argc, /* 参数列表 */);
        if (result < 0) {
            printf("Error: Failed to add record\n");
            return merr;
        }
        printf("Record added successfully at index %ld\n", result);
    } else {
        printf("Error: No values provided for record\n");
        return merr;
    }
    
    return success;
}

mrc handle_get_record(UID caller, basic_handle_struct bhs, int argc, void* argv[]) {
    // 检查对象类型是否为TABLE
    if (bhs.type != M_TABLE) {
        printf("Error: Object is not a TABLE\n");
        return merr;
    }
    
    // 获取TABLE对象
    TABLE* table = bhs.handle.table;
    
    // 根据参数获取记录
    if (argc == 0) {
        // 获取所有记录
        table->print_table(0);
    } else if (argc >= 1) {
        // 获取指定记录
        // 这里需要实际实现获取记录的逻辑
        printf("Getting records from TABLE\n");
    }
    
    return success;
}

mrc handle_del_record(UID caller, basic_handle_struct bhs, int argc, void* argv[]) {
    // 检查对象类型是否为TABLE
    if (bhs.type != M_TABLE) {
        printf("Error: Object is not a TABLE\n");
        return merr;
    }
    
    // 检查权限
    // 这里应该检查删除记录的权限
    
    // 获取TABLE对象
    TABLE* table = bhs.handle.table;
    
    // 删除记录
    if (argc >= 1) {
        // 这里需要实际实现删除记录的逻辑
        printf("Deleting records from TABLE\n");
    } else {
        printf("Error: No record index provided\n");
        return merr;
    }
    
    return success;
}

// KVALOT命令处理函数
mrc handle_set_key(UID caller, basic_handle_struct bhs, int argc, void* argv[]) {
    // 检查对象类型是否为KVALOT
    if (bhs.type != M_KVALOT) {
        printf("Error: Object is not a KVALOT\n");
        return merr;
    }
    
    // 检查权限
    // 这里应该检查设置键值的权限
    
    // 获取KVALOT对象
    KVALOT* kvalot = bhs.handle.kvalot;
    
    // 设置键值
    if (argc >= 2) {
        char* key_name = (char*)argv[0];
        char* value = (char*)argv[1];
        
        // 创建str对象
        str key_str;
        str_init(&key_str);
        str_append_string(&key_str, key_name);
        
        // 添加键值对
        // 这里需要根据value的类型创建相应对象
        int result = kvalot->add_key(&key_str, M_STREAM, &value, nullptr, nullptr);
        str_free(&key_str);
        
        if (result != success) {
            printf("Error: Failed to set key %s\n", key_name);
            return merr;
        }
        printf("Key %s set successfully\n", key_name);
    } else {
        printf("Error: Insufficient arguments for SET_KEY\n");
        return merr;
    }
    
    return success;
}

mrc handle_get_key(UID caller, basic_handle_struct bhs, int argc, void* argv[]) {
    // 检查对象类型是否为KVALOT
    if (bhs.type != M_KVALOT) {
        printf("Error: Object is not a KVALOT\n");
        return merr;
    }
    
    // 获取KVALOT对象
    KVALOT* kvalot = bhs.handle.kvalot;
    
    // 获取键值
    if (argc >= 1) {
        char* key_name = (char*)argv[0];
        
        // 创建str对象
        str key_str;
        str_init(&key_str);
        str_append_string(&key_str, key_name);
        
        // 查找键
        basic_handle_struct result = kvalot->find_key(&key_str);
        str_free(&key_str);
        
        if (result.type == M_NULL) {
            printf("Key %s not found\n", key_name);
            return merr;
        }
        
        printf("Key %s found\n", key_name);
        // 这里应该返回键的值
    } else {
        printf("Error: No key name provided\n");
        return merr;
    }
    
    return success;
}

mrc handle_del_key(UID caller, basic_handle_struct bhs, int argc, void* argv[]) {
    // 检查对象类型是否为KVALOT
    if (bhs.type != M_KVALOT) {
        printf("Error: Object is not a KVALOT\n");
        return merr;
    }
    
    // 检查权限
    // 这里应该检查删除键的权限
    
    // 获取KVALOT对象
    KVALOT* kvalot = bhs.handle.kvalot;
    
    // 删除键
    if (argc >= 1) {
        char* key_name = (char*)argv[0];
        
        // 创建str对象
        str key_str;
        str_init(&key_str);
        str_append_string(&key_str, key_name);
        
        // 删除键
        int result = kvalot->rmv_key(&key_str);
        str_free(&key_str);
        
        if (result != success) {
            printf("Error: Failed to delete key %s\n", key_name);
            return merr;
        }
        printf("Key %s deleted successfully\n", key_name);
    } else {
        printf("Error: No key name provided\n");
        return merr;
    }
    
    return success;
}

// LIST命令处理函数
mrc handle_lpush(UID caller, basic_handle_struct bhs, int argc, void* argv[]) {
    // 检查对象类型是否为LIST
    if (bhs.type != M_LIST) {
        printf("Error: Object is not a LIST\n");
        return merr;
    }
    
    // 检查权限
    // 这里应该检查添加元素的权限
    
    // 获取LIST对象
    LIST* list = bhs.handle.list;
    
    // 在列表头部添加元素
    if (argc >= 1) {
        char* value = (char*)argv[0];
        
        // 创建str对象
        str value_str;
        str_init(&value_str);
        str_append_string(&value_str, value);
        
        // 添加元素
        int result = list->lpush(value_str);
        str_free(&value_str);
        
        if (result != success) {
            printf("Error: Failed to push value to list\n");
            return merr;
        }
        printf("Value pushed to list head successfully\n");
    } else {
        printf("Error: No value provided\n");
        return merr;
    }
    
    return success;
}

mrc handle_rpush(UID caller, basic_handle_struct bhs, int argc, void* argv[]) {
    // 检查对象类型是否为LIST
    if (bhs.type != M_LIST) {
        printf("Error: Object is not a LIST\n");
        return merr;
    }
    
    // 检查权限
    // 这里应该检查添加元素的权限
    
    // 获取LIST对象
    LIST* list = bhs.handle.list;
    
    // 在列表尾部添加元素
    if (argc >= 1) {
        char* value = (char*)argv[0];
        
        // 创建str对象
        str value_str;
        str_init(&value_str);
        str_append_string(&value_str, value);
        
        // 添加元素
        int result = list->rpush(value_str);
        str_free(&value_str);
        
        if (result != success) {
            printf("Error: Failed to push value to list\n");
            return merr;
        }
        printf("Value pushed to list tail successfully\n");
    } else {
        printf("Error: No value provided\n");
        return merr;
    }
    
    return success;
}

mrc handle_lpop(UID caller, basic_handle_struct bhs, int argc, void* argv[]) {
    // 检查对象类型是否为LIST
    if (bhs.type != M_LIST) {
        printf("Error: Object is not a LIST\n");
        return merr;
    }
    
    // 检查权限
    // 这里应该检查删除元素的权限
    
    // 获取LIST对象
    LIST* list = bhs.handle.list;
    
    // 从列表头部弹出元素
    str result = list->lpop();
    if (result.len == 0) {
        printf("Error: List is empty\n");
        return merr;
    }
    
    printf("Value popped from list head: %.*s\n", result.len, result.string);
    str_free(&result);
    
    return success;
}

mrc handle_rpop(UID caller, basic_handle_struct bhs, int argc, void* argv[]) {
    // 检查对象类型是否为LIST
    if (bhs.type != M_LIST) {
        printf("Error: Object is not a LIST\n");
        return merr;
    }
    
    // 检查权限
    // 这里应该检查删除元素的权限
    
    // 获取LIST对象
    LIST* list = bhs.handle.list;
    
    // 从列表尾部弹出元素
    str result = list->rpop();
    if (result.len == 0) {
        printf("Error: List is empty\n");
        return merr;
    }
    
    printf("Value popped from list tail: %.*s\n", result.len, result.string);
    str_free(&result);
    
    return success;
}

// BITMAP命令处理函数
mrc handle_set_bit(UID caller, basic_handle_struct bhs, int argc, void* argv[]) {
    // 检查对象类型是否为BITMAP
    if (bhs.type != M_BITMAP) {
        printf("Error: Object is not a BITMAP\n");
        return merr;
    }
    
    // 检查权限
    // 这里应该检查设置位的权限
    
    // 获取BITMAP对象
    BITMAP* bitmap = bhs.handle.bitmap;
    
    // 设置位
    if (argc >= 2) {
        uint64_t offset = strtoull((char*)argv[0], nullptr, 10);
        uint8_t value = (uint8_t)strtoul((char*)argv[1], nullptr, 10);
        
        // 设置位值
        int result = bitmap->set(offset, value);
        if (result != success) {
            printf("Error: Failed to set bit at offset %lu\n", offset);
            return merr;
        }
        printf("Bit at offset %lu set to %u successfully\n", offset, value);
    } else {
        printf("Error: Insufficient arguments for SET_BIT\n");
        return merr;
    }
    
    return success;
}

mrc handle_get_bit(UID caller, basic_handle_struct bhs, int argc, void* argv[]) {
    // 检查对象类型是否为BITMAP
    if (bhs.type != M_BITMAP) {
        printf("Error: Object is not a BITMAP\n");
        return merr;
    }
    
    // 获取BITMAP对象
    BITMAP* bitmap = bhs.handle.bitmap;
    
    // 获取位值
    if (argc >= 1) {
        uint64_t offset = strtoull((char*)argv[0], nullptr, 10);
        
        // 获取位值
        BITMAP::BIT bit = (*bitmap)[offset];
        uint8_t value = (uint8_t)bit;
        
        printf("Bit at offset %lu has value %u\n", offset, value);
    } else {
        printf("Error: No offset provided\n");
        return merr;
    }
    
    return success;
}

// STREAM命令处理函数
mrc handle_append(UID caller, basic_handle_struct bhs, int argc, void* argv[]) {
    // 检查对象类型是否为STREAM
    if (bhs.type != M_STREAM) {
        printf("Error: Object is not a STREAM\n");
        return merr;
    }
    
    // 检查权限
    // 这里应该检查添加数据的权限
    
    // 获取STREAM对象
    STREAM* stream = bhs.handle.stream;
    
    // 追加数据
    if (argc >= 1) {
        char* data = (char*)argv[0];
        uint32_t len = strlen(data);
        
        // 追加数据
        int result = stream->append((const uint8_t*)data, len);
        if (result != success) {
            printf("Error: Failed to append data to stream\n");
            return merr;
        }
        printf("Data appended to stream successfully\n");
    } else {
        printf("Error: No data provided\n");
        return merr;
    }
    
    return success;
}

mrc handle_get_stream(UID caller, basic_handle_struct bhs, int argc, void* argv[]) {
    // 检查对象类型是否为STREAM
    if (bhs.type != M_STREAM) {
        printf("Error: Object is not a STREAM\n");
        return merr;
    }
    
    // 获取STREAM对象
    STREAM* stream = bhs.handle.stream;
    
    // 获取流长度
    uint32_t len = stream->len();
    if (len > 0) {
        // 分配缓冲区
        uint8_t* buffer = (uint8_t*)malloc(len);
        if (buffer) {
            // 获取数据
            int result = stream->get(0, len, buffer);
            if (result == success) {
                printf("Stream data: %.*s\n", len, buffer);
            } else {
                printf("Error: Failed to get stream data\n");
                free(buffer);
                return merr;
            }
            free(buffer);
        } else {
            printf("Error: Failed to allocate buffer\n");
            return merr;
        }
    } else {
        printf("Stream is empty\n");
    }
    
    return success;
}

// HOOK命令处理函数
mrc handle_hook_create(UID caller, int argc, void* argv[]) {
    if (argc < 2) {
        printf("Error: Insufficient arguments for HOOK_CREATE\n");
        return merr;
    }
    
    // 参数: [objtype, objname]
    obj_type type = *(obj_type*)argv[0];
    char* name = (char*)argv[1];
    
    // 创建HOOK
    HOOK* hook = new HOOK(caller, string(name));
    if (!hook) {
        printf("Error: Failed to create HOOK\n");
        return merr;
    }
    
    // 注册HOOK
    if (!Reg.register_hook(hook)) {
        delete hook;
        printf("Error: Failed to register HOOK\n");
        return merr;
    }
    
    printf("Hook %s created successfully\n", name);
    return success;
}

mrc handle_hook_switch(UID caller, int argc, void* argv[]) {
    if (argc < 1) {
        printf("Error: Insufficient arguments for HOOK_SWITCH\n");
        return merr;
    }
    
    // 参数: [objname]
    char* name = (char*)argv[0];
    
    // 查找HOOK
    HOOK* hook = Reg.find_hook(string(name));
    if (!hook) {
        printf("Error: HOOK %s not found\n", name);
        return merr;
    }
    
    printf("Hook switched to: %s\n", name);
    return success;
}