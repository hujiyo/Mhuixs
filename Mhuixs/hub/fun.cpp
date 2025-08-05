#include "fun.h"

#include <nlohmann/json.hpp>

response_t* handle_get_object(basic_handle_struct bhs, command_t* cmd) {
    //获得对象所有信息
    if (!cmd || !cmd->param1) {
        return nullptr; // Or handle error appropriately
    }

    nlohmann::json j;

    switch (bhs.type) {
        case M_KVALOT: {
            if (bhs.handle.kvalot) {
                j = bhs.handle.kvalot->get_all_info();
            }
            break;
        }
        case M_LIST: {
            if (bhs.handle.list) {
                j = bhs.handle.list->get_all_info();
            }
            break;
        }
        case M_BITMAP: {
            if (bhs.handle.bitmap) {
                j = bhs.handle.bitmap->get_all_info();
            }
            break;
        }
        case M_TABLE: {
            if (bhs.handle.table) {
                j = bhs.handle.table->get_all_info();
            }
            break;
        }
        case M_STREAM: {
            if (bhs.handle.stream) {
                j = bhs.handle.stream->get_all_info();
            }
            break;
        }
        default: {
            // Handle unknown type
            j = {{"error", "Unsupported object type"}};
            break;
        }
    }

    std::string json_str = j.dump();
    response_t* res = (response_t*)malloc(sizeof(response_t) + json_str.length() + 1);
    if (!res) {
        return nullptr;
    }

    res->data_len = json_str.length();
    strcpy((char*)(res + 1), json_str.c_str());

    return res;
}

response_t* handle_where(basic_handle_struct bhs, command_t* cmd) {
    // 仅支持 TABLE 类型的 where 筛选
    if (bhs.type != M_TABLE || !bhs.handle.table || !cmd || !cmd->param1 || !cmd->param2 || !cmd->param3) {
        return nullptr;
    }
    TABLE* table = bhs.handle.table;
    const char* field_name = static_cast<const char*>(cmd->param1); // 字段名
    const char* op = static_cast<const char*>(cmd->param2);         // 操作符，如 "==", ">", "<", "!=", ">=", "<="
    const char* value = static_cast<const char*>(cmd->param3);      // 比较值

    // 查找字段索引
    int field_idx = -1;
    for (uint32_t i = 0; i < table->field_num; ++i) {
        if (strncmp(reinterpret_cast<const char*>(table->p_field[i].name), field_name, format_name_length) == 0) {
            field_idx = i;
            break;
        }
    }
    if (field_idx == -1) {
        // 字段不存在
        nlohmann::json j = {{"error", "Field not found"}};
        std::string json_str = j.dump();
        response_t* res = (response_t*)malloc(sizeof(response_t) + json_str.length() + 1);
        if (!res) return nullptr;
        res->response_len = json_str.length();
        strcpy((char*)(res + 1), json_str.c_str());
        return res;
    }
    char field_type = table->p_field[field_idx].type;
    nlohmann::json result = nlohmann::json::array();
    for (uint32_t j = 0; j < table->record_num; ++j) {
        uint8_t* addr = table->address_of_i_j(field_idx, j);
        bool match = false;
        switch (field_type) {
            case I1: case I2: case I4: case DATE: case TIME: {
                int32_t v = 0;
                memcpy(&v, addr, sizeof(int32_t));
                int32_t cmp = atoi(value);
                if (strcmp(op, "==") == 0) match = (v == cmp);
                else if (strcmp(op, "!=") == 0) match = (v != cmp);
                else if (strcmp(op, ">") == 0) match = (v > cmp);
                else if (strcmp(op, "<") == 0) match = (v < cmp);
                else if (strcmp(op, ">=") == 0) match = (v >= cmp);
                else if (strcmp(op, "<=") == 0) match = (v <= cmp);
                break;
            }
            case UI1: case UI2: case UI4: {
                uint32_t v = 0;
                memcpy(&v, addr, sizeof(uint32_t));
                uint32_t cmp = (uint32_t)strtoul(value, nullptr, 10);
                if (strcmp(op, "==") == 0) match = (v == cmp);
                else if (strcmp(op, "!=") == 0) match = (v != cmp);
                else if (strcmp(op, ">") == 0) match = (v > cmp);
                else if (strcmp(op, "<") == 0) match = (v < cmp);
                else if (strcmp(op, ">=") == 0) match = (v >= cmp);
                else if (strcmp(op, "<=") == 0) match = (v <= cmp);
                break;
            }
            case I8: {
                int64_t v = 0;
                memcpy(&v, addr, sizeof(int64_t));
                int64_t cmp = strtoll(value, nullptr, 10);
                if (strcmp(op, "==") == 0) match = (v == cmp);
                else if (strcmp(op, "!=") == 0) match = (v != cmp);
                else if (strcmp(op, ">") == 0) match = (v > cmp);
                else if (strcmp(op, "<") == 0) match = (v < cmp);
                else if (strcmp(op, ">=") == 0) match = (v >= cmp);
                else if (strcmp(op, "<=") == 0) match = (v <= cmp);
                break;
            }
            case UI8: {
                uint64_t v = 0;
                memcpy(&v, addr, sizeof(uint64_t));
                uint64_t cmp = strtoull(value, nullptr, 10);
                if (strcmp(op, "==") == 0) match = (v == cmp);
                else if (strcmp(op, "!=") == 0) match = (v != cmp);
                else if (strcmp(op, ">") == 0) match = (v > cmp);
                else if (strcmp(op, "<") == 0) match = (v < cmp);
                else if (strcmp(op, ">=") == 0) match = (v >= cmp);
                else if (strcmp(op, "<=") == 0) match = (v <= cmp);
                break;
            }
            case F4: {
                float v = 0;
                memcpy(&v, addr, sizeof(float));
                float cmp = strtof(value, nullptr);
                if (strcmp(op, "==") == 0) match = (v == cmp);
                else if (strcmp(op, "!=") == 0) match = (v != cmp);
                else if (strcmp(op, ">") == 0) match = (v > cmp);
                else if (strcmp(op, "<") == 0) match = (v < cmp);
                else if (strcmp(op, ">=") == 0) match = (v >= cmp);
                else if (strcmp(op, "<=") == 0) match = (v <= cmp);
                break;
            }
            case F8: {
                double v = 0;
                memcpy(&v, addr, sizeof(double));
                double cmp = strtod(value, nullptr);
                if (strcmp(op, "==") == 0) match = (v == cmp);
                else if (strcmp(op, "!=") == 0) match = (v != cmp);
                else if (strcmp(op, ">") == 0) match = (v > cmp);
                else if (strcmp(op, "<") == 0) match = (v < cmp);
                else if (strcmp(op, ">=") == 0) match = (v >= cmp);
                else if (strcmp(op, "<=") == 0) match = (v <= cmp);
                break;
            }
            case STR: {
                std::string* v = nullptr;
                memcpy(&v, addr, sizeof(std::string*));
                if (!v) break;
                if (strcmp(op, "==") == 0) match = (*v == value);
                else if (strcmp(op, "!=") == 0) match = (*v != value);
                else if (strcmp(op, "~=") == 0) match = (v->find(value) != std::string::npos); // 包含
                break;
            }
            default:
                break;
        }
        if (match) {
            // 收集整条记录
            nlohmann::json row = nlohmann::json::object();
            for (uint32_t k = 0; k < table->field_num; ++k) {
                uint8_t* faddr = table->address_of_i_j(k, j);
                char ftype = table->p_field[k].type;
                const char* fname = reinterpret_cast<const char*>(table->p_field[k].name);
                switch (ftype) {
                    case I1: case I2: case I4: case DATE: case TIME: {
                        int32_t v = 0;
                        memcpy(&v, faddr, sizeof(int32_t));
                        row[fname] = v;
                        break;
                    }
                    case UI1: case UI2: case UI4: {
                        uint32_t v = 0;
                        memcpy(&v, faddr, sizeof(uint32_t));
                        row[fname] = v;
                        break;
                    }
                    case I8: {
                        int64_t v = 0;
                        memcpy(&v, faddr, sizeof(int64_t));
                        row[fname] = v;
                        break;
                    }
                    case UI8: {
                        uint64_t v = 0;
                        memcpy(&v, faddr, sizeof(uint64_t));
                        row[fname] = v;
                        break;
                    }
                    case F4: {
                        float v = 0;
                        memcpy(&v, faddr, sizeof(float));
                        row[fname] = v;
                        break;
                    }
                    case F8: {
                        double v = 0;
                        memcpy(&v, faddr, sizeof(double));
                        row[fname] = v;
                        break;
                    }
                    case STR: {
                        std::string* v = nullptr;
                        memcpy(&v, faddr, sizeof(std::string*));
                        row[fname] = v ? *v : "";
                        break;
                    }
                    default:
                        row[fname] = nullptr;
                        break;
                }
            }
            result.push_back(row);
        }
    }
    std::string json_str = result.dump();
    response_t* res = (response_t*)malloc(sizeof(response_t) + json_str.length() + 1);
    if (!res) return nullptr;
    res->response_len = json_str.length();
    strcpy((char*)(res + 1), json_str.c_str());
    return res;
}

response_t* handle_desc(basic_handle_struct bhs, command_t* cmd) {
    // 返回对象的结构描述信息，优先调用 get_all_info 或类似接口
    nlohmann::json j;
    switch (bhs.type) {
        case M_KVALOT:
            if (bhs.handle.kvalot) {
                j = bhs.handle.kvalot->get_all_info();
            }
            break;
        case M_LIST:
            if (bhs.handle.list) {
                j = bhs.handle.list->get_all_info();
            }
            break;
        case M_BITMAP:
            if (bhs.handle.bitmap) {
                j = bhs.handle.bitmap->get_all_info();
            }
            break;
        case M_TABLE:
            if (bhs.handle.table) {
                j = bhs.handle.table->get_all_info();
            }
            break;
        case M_STREAM:
            if (bhs.handle.stream) {
                j = bhs.handle.stream->get_all_info();
            }
            break;
        default:
            j = {{"error", "Unsupported object type"}};
            break;
    }
    std::string json_str = j.dump();
    response_t* res = (response_t*)malloc(sizeof(response_t) + json_str.length() + 1);
    if (!res) return nullptr;
    res->response_len = json_str.length();
    strcpy((char*)(res + 1), json_str.c_str());
    return res;
}

response_t* handle_hook_root(basic_handle_struct bhs, command_t* cmd) {
    // 返回所有已注册HOOK的基本信息
    nlohmann::json j = nlohmann::json::array();
    extern Registry Reg;
    // 由于hook_map为private，只能通过Registry类增加一个遍历接口，临时方案：只返回空数组或错误
    // 推荐后续在Registry类中增加遍历所有HOOK的接口
    std::string json_str = j.dump();
    response_t* res = (response_t*)malloc(sizeof(response_t) + json_str.length() + 1);
    if (!res) return nullptr;
    res->response_len = json_str.length();
    strcpy((char*)(res + 1), json_str.c_str());
    return res;
}
//response_t* handle_hook_create(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_hook_switch(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_hook_del(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_hook_clear(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_hook_copy(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_hook_swap(basic_handle_struct bhs, command_t* cmd);

//response_t* handle_hook_merge(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_hook_rename(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_rank_set(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_get_rank(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_get_type(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_lock(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_unlock(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_export(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_import(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_chmod(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_get_chmod(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_wait(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_multi(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_exec(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_async(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_sync(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_index_create(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_index_del(basic_handle_struct bhs, command_t* cmd);

// TABLE类语法命令处理函数声明
//response_t* handle_field_add(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_field_insert(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_field_swap(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_field_del(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_field_set_attr(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_field_get_info(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_add_record(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_insert_record(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_set_record(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_del_record(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_swap_record(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_get_record(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_get_field(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_get_pos(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_get_count(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_get_where(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_set_where(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_del_where(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_sort(basic_handle_struct bhs, command_t* cmd);

// KVALOT类语法命令处理函数声明
//response_t* handle_exists(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_select(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_set_type(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_set_key(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_get_key(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_del_key(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_key_enter(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_copy_key(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_swap_key(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_rename_key(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_expire(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_persist(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_ttl(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_incr(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_decr(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_incr_by(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_decr_by(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_get_keys(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_get_key_count(basic_handle_struct bhs, command_t* cmd);

// STREAM类语法命令处理函数声明
//response_t* handle_append(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_append_pos(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_get_stream(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_set_stream(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_set_stream_len(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_get_stream_len(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_get_stream_all(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_clear_stream(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_merge_stream(basic_handle_struct bhs, command_t* cmd);

// LIST类语法命令处理函数声明
//response_t* handle_lpush(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_rpush(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_lpop(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_rpop(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_get_list(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_del_list(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_get_list_len(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_insert_list(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_set_list(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_exists_list(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_get_list_length(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_swap_list(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_copy_list(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_clear_list(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_reverse_list(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_sort_list(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_unique_list(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_get_list_slice(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_find_list(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_count_list(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_join_list(basic_handle_struct bhs, command_t* cmd)////;

// BITMAP类语法命令处理函数声明
//response_t* handle_set_bit(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_set_bit_range(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_get_bit(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_get_bit_range(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_count_bit(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_count_bit_all(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_flip_bit(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_flip_bit_range(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_clear_bit(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_fill_bit(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_find_bit(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_find_bit_start(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_and_bit(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_or_bit(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_xor_bit(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_not_bit(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_shift_bit(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_shift_bit_math(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_get_bit_size(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_resize_bit(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_import_bit(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_join_bit(basic_handle_struct bhs, command_t* cmd);

// 跨HOOK互动操作命令处理函数声明
//response_t* handle_hook_join(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_key_join(basic_handle_struct bhs, command_t* cmd);

// 系统管理命令处理函数声明
//response_t* handle_system_info(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_system_status(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_system_register(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_system_cleanup(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_system_backup(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_system_restore(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_system_log(basic_handle_struct bhs, command_t* cmd);

// 持久性和压缩管理命令处理函数声明
//response_t* handle_backup_obj(basic_handle_struct bhs, command_t* cmd);
//response_t* handle_compress(basic_handle_struct bhs, command_t* cmd);