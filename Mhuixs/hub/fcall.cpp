#include "fcall.hpp"
#include "fun.h"

Mhuixsfun_t funtable[FUNTABLE_SIZE]={};

void funcall_init() {
    //注册函数
    funtable[CMD_GET_OBJECT] = handle_get_object;
    funtable[CMD_WHERE] = handle_where;
    funtable[CMD_DESC] = handle_desc;
    funtable[CMD_HOOK_ROOT] = handle_hook_root;
    funtable[CMD_HOOK_CREATE] = handle_hook_create;
    funtable[CMD_HOOK_SWITCH] = handle_hook_switch;
    funtable[CMD_HOOK_DEL] = handle_hook_del;
    funtable[CMD_HOOK_CLEAR] = handle_hook_clear;
    funtable[CMD_HOOK_COPY] = handle_hook_copy;
    funtable[CMD_HOOK_SWAP] = handle_hook_swap;
    funtable[CMD_HOOK_MERGE] = handle_hook_merge;
    funtable[CMD_HOOK_RENAME] = handle_hook_rename;
    funtable[CMD_RANK_SET] = handle_rank_set;
    funtable[CMD_GET_RANK] = handle_get_rank;
    funtable[CMD_GET_TYPE] = handle_get_type;
    funtable[CMD_LOCK] = handle_lock;
    funtable[CMD_UNLOCK] = handle_unlock;
    funtable[CMD_EXPORT] = handle_export;
    funtable[CMD_IMPORT] = handle_import;
    funtable[CMD_CHMOD] = handle_chmod;
    funtable[CMD_GET_CHMOD] = handle_get_chmod;
    funtable[CMD_WAIT] = handle_wait;
    funtable[CMD_MULTI] = handle_multi;
    funtable[CMD_EXEC] = handle_exec;
    funtable[CMD_ASYNC] = handle_async;
    funtable[CMD_SYNC] = handle_sync;

    funtable[CMD_INDEX_CREATE] = handle_index_create;
    funtable[CMD_INDEX_DEL] = handle_index_del;

    funtable[CMD_FIELD_ADD] = handle_field_add;
    funtable[CMD_FIELD_INSERT] = handle_field_insert;
    funtable[CMD_FIELD_SWAP] = handle_field_swap;
    funtable[CMD_FIELD_DEL] = handle_field_del;
    funtable[CMD_FIELD_SET_ATTR] = handle_field_set_attr;
    funtable[CMD_FIELD_GET_INFO] = handle_field_get_info;
    funtable[CMD_ADD_RECORD] = handle_add_record;
    funtable[CMD_INSERT_RECORD] = handle_insert_record;
    funtable[CMD_SET_RECORD] = handle_set_record;
    funtable[CMD_DEL_RECORD] = handle_del_record;
    funtable[CMD_SWAP_RECORD] = handle_swap_record;
    funtable[CMD_GET_RECORD] = handle_get_record;
    funtable[CMD_GET_FIELD] = handle_get_field;
    funtable[CMD_GET_POS] = handle_get_pos;
    funtable[CMD_GET_COUNT] = handle_get_count;
    funtable[CMD_GET_WHERE] = handle_get_where;
    funtable[CMD_SET_WHERE] = handle_set_where;
    funtable[CMD_DEL_WHERE] = handle_del_where;
    funtable[CMD_SORT] = handle_sort;

    funtable[CMD_EXISTS] = handle_exists;
    funtable[CMD_SELECT] = handle_select;
    funtable[CMD_SET_TYPE] = handle_set_type;
    funtable[CMD_SET_KEY] = handle_set_key;
    funtable[CMD_GET_KEY] = handle_get_key;
    funtable[CMD_DEL_KEY] = handle_del_key;
    funtable[CMD_KEY_ENTER] = handle_key_enter;
    funtable[CMD_COPY_KEY] = handle_copy_key;
    funtable[CMD_SWAP_KEY] = handle_swap_key;
    funtable[CMD_RENAME_KEY] = handle_rename_key;
    funtable[CMD_EXPIRE] = handle_expire;
    funtable[CMD_PERSIST] = handle_persist;
    funtable[CMD_TTL] = handle_ttl;
    funtable[CMD_INCR] = handle_incr;
    funtable[CMD_DECR] = handle_decr;
    funtable[CMD_INCR_BY] = handle_incr_by;
    funtable[CMD_DECR_BY] = handle_decr_by;
    funtable[CMD_GET_KEYS] = handle_get_keys;
    funtable[CMD_GET_KEY_COUNT] = handle_get_key_count;

    funtable[CMD_APPEND] = handle_append;
    funtable[CMD_APPEND_POS] = handle_append_pos;
    funtable[CMD_GET_STREAM] = handle_get_stream;
    funtable[CMD_SET_STREAM] = handle_set_stream;
    funtable[CMD_SET_STREAM_LEN] = handle_set_stream_len;
    funtable[CMD_GET_STREAM_LEN] = handle_get_stream_len;
    funtable[CMD_GET_STREAM_ALL] = handle_get_stream_all;
    funtable[CMD_CLEAR_STREAM] = handle_clear_stream;
    funtable[CMD_MERGE_STREAM] = handle_merge_stream;

    funtable[CMD_LPUSH] = handle_lpush;
    funtable[CMD_RPUSH] = handle_rpush;
    funtable[CMD_LPOP] = handle_lpop;
    funtable[CMD_RPOP] = handle_rpop;
    funtable[CMD_GET_LIST] = handle_get_list;
    funtable[CMD_DEL_LIST] = handle_del_list;
    funtable[CMD_GET_LIST_LEN] = handle_get_list_len;
    funtable[CMD_INSERT_LIST] = handle_insert_list;
    funtable[CMD_SET_LIST] = handle_set_list;
    funtable[CMD_EXISTS_LIST] = handle_exists_list;
    funtable[CMD_GET_LIST_LENGTH] = handle_get_list_length;
    funtable[CMD_SWAP_LIST] = handle_swap_list;
    funtable[CMD_COPY_LIST] = handle_copy_list;
    funtable[CMD_CLEAR_LIST] = handle_clear_list;
    funtable[CMD_REVERSE_LIST] = handle_reverse_list;
    funtable[CMD_SORT_LIST] = handle_sort_list;
    funtable[CMD_UNIQUE_LIST] = handle_unique_list;
    funtable[CMD_GET_LIST_SLICE] = handle_get_list_slice;
    funtable[CMD_FIND_LIST] = handle_find_list;
    funtable[CMD_COUNT_LIST] = handle_count_list;
    funtable[CMD_JOIN_LIST] = handle_join_list;

    funtable[CMD_SET_BIT] = handle_set_bit;
    funtable[CMD_SET_BIT_RANGE] = handle_set_bit_range;
    funtable[CMD_GET_BIT] = handle_get_bit;
    funtable[CMD_GET_BIT_RANGE] = handle_get_bit_range;
    funtable[CMD_COUNT_BIT] = handle_count_bit;
    funtable[CMD_COUNT_BIT_ALL] = handle_count_bit_all;
    funtable[CMD_FLIP_BIT] = handle_flip_bit;
    funtable[CMD_FLIP_BIT_RANGE] = handle_flip_bit_range;
    funtable[CMD_CLEAR_BIT] = handle_clear_bit;
    funtable[CMD_FILL_BIT] = handle_fill_bit;
    funtable[CMD_FIND_BIT] = handle_find_bit;
    funtable[CMD_FIND_BIT_START] = handle_find_bit_start;
    funtable[CMD_AND_BIT] = handle_and_bit;
    funtable[CMD_OR_BIT] = handle_or_bit;
    funtable[CMD_XOR_BIT] = handle_xor_bit;
    funtable[CMD_NOT_BIT] = handle_not_bit;
    funtable[CMD_SHIFT_BIT] = handle_shift_bit;
    funtable[CMD_SHIFT_BIT_MATH] = handle_shift_bit_math;
    funtable[CMD_GET_BIT_SIZE] = handle_get_bit_size;
    funtable[CMD_RESIZE_BIT] = handle_resize_bit;
    funtable[CMD_IMPORT_BIT] = handle_import_bit;
    funtable[CMD_JOIN_BIT] = handle_join_bit;

    funtable[CMD_HOOK_JOIN] = handle_hook_join;
    funtable[CMD_KEY_JOIN] = handle_key_join;

    funtable[CMD_SYSTEM_INFO] = handle_system_info;
    funtable[CMD_SYSTEM_STATUS] = handle_system_status;
    funtable[CMD_SYSTEM_REGISTER] = handle_system_register;
    funtable[CMD_SYSTEM_CLEANUP] = handle_system_cleanup;
    funtable[CMD_SYSTEM_BACKUP] = handle_system_backup;
    funtable[CMD_SYSTEM_RESTORE] = handle_system_restore;
    funtable[CMD_SYSTEM_LOG] = handle_system_log;

    funtable[CMD_BACKUP_OBJ] = handle_backup_obj;
    funtable[CMD_COMPRESS] = handle_compress;
}


