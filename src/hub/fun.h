#ifndef FUN_H
#define FUN_H

#include "fcall.hpp"
#include "tblh.h"
#include "kvalh.hpp"
#include "list.h"
#include "stream.hpp"
#include "bitmap.h"
#include "hook.hpp"
#include "usergroup.hpp"
#include "hook.hpp"

// 基础语法命令处理函数声明
response_t* handle_get_object(basic_handle_struct bhs, command_t* cmd);
response_t* handle_where(basic_handle_struct bhs, command_t* cmd);
response_t* handle_desc(basic_handle_struct bhs, command_t* cmd);
response_t* handle_hook_root(basic_handle_struct bhs, command_t* cmd);
response_t* handle_hook_create(basic_handle_struct bhs, command_t* cmd);
response_t* handle_hook_switch(basic_handle_struct bhs, command_t* cmd);
response_t* handle_hook_del(basic_handle_struct bhs, command_t* cmd);
response_t* handle_hook_clear(basic_handle_struct bhs, command_t* cmd);
response_t* handle_hook_copy(basic_handle_struct bhs, command_t* cmd);
response_t* handle_hook_swap(basic_handle_struct bhs, command_t* cmd);
response_t* handle_hook_merge(basic_handle_struct bhs, command_t* cmd);
response_t* handle_hook_rename(basic_handle_struct bhs, command_t* cmd);
response_t* handle_rank_set(basic_handle_struct bhs, command_t* cmd);
response_t* handle_get_rank(basic_handle_struct bhs, command_t* cmd);
response_t* handle_get_type(basic_handle_struct bhs, command_t* cmd);
response_t* handle_lock(basic_handle_struct bhs, command_t* cmd);
response_t* handle_unlock(basic_handle_struct bhs, command_t* cmd);
response_t* handle_export(basic_handle_struct bhs, command_t* cmd);
response_t* handle_import(basic_handle_struct bhs, command_t* cmd);
response_t* handle_chmod(basic_handle_struct bhs, command_t* cmd);
response_t* handle_get_chmod(basic_handle_struct bhs, command_t* cmd);
response_t* handle_wait(basic_handle_struct bhs, command_t* cmd);
response_t* handle_multi(basic_handle_struct bhs, command_t* cmd);
response_t* handle_exec(basic_handle_struct bhs, command_t* cmd);
response_t* handle_async(basic_handle_struct bhs, command_t* cmd);
response_t* handle_sync(basic_handle_struct bhs, command_t* cmd);
response_t* handle_index_create(basic_handle_struct bhs, command_t* cmd);
response_t* handle_index_del(basic_handle_struct bhs, command_t* cmd);

// TABLE类语法命令处理函数声明
response_t* handle_field_add(basic_handle_struct bhs, command_t* cmd);
response_t* handle_field_insert(basic_handle_struct bhs, command_t* cmd);
response_t* handle_field_swap(basic_handle_struct bhs, command_t* cmd);
response_t* handle_field_del(basic_handle_struct bhs, command_t* cmd);
response_t* handle_field_set_attr(basic_handle_struct bhs, command_t* cmd);
response_t* handle_field_get_info(basic_handle_struct bhs, command_t* cmd);
response_t* handle_add_record(basic_handle_struct bhs, command_t* cmd);
response_t* handle_insert_record(basic_handle_struct bhs, command_t* cmd);
response_t* handle_set_record(basic_handle_struct bhs, command_t* cmd);
response_t* handle_del_record(basic_handle_struct bhs, command_t* cmd);
response_t* handle_swap_record(basic_handle_struct bhs, command_t* cmd);
response_t* handle_get_record(basic_handle_struct bhs, command_t* cmd);
response_t* handle_get_field(basic_handle_struct bhs, command_t* cmd);
response_t* handle_get_pos(basic_handle_struct bhs, command_t* cmd);
response_t* handle_get_count(basic_handle_struct bhs, command_t* cmd);
response_t* handle_get_where(basic_handle_struct bhs, command_t* cmd);
response_t* handle_set_where(basic_handle_struct bhs, command_t* cmd);
response_t* handle_del_where(basic_handle_struct bhs, command_t* cmd);
response_t* handle_sort(basic_handle_struct bhs, command_t* cmd);

// KVALOT类语法命令处理函数声明
response_t* handle_exists(basic_handle_struct bhs, command_t* cmd);
response_t* handle_select(basic_handle_struct bhs, command_t* cmd);
response_t* handle_set_type(basic_handle_struct bhs, command_t* cmd);
response_t* handle_set_key(basic_handle_struct bhs, command_t* cmd);
response_t* handle_get_key(basic_handle_struct bhs, command_t* cmd);
response_t* handle_del_key(basic_handle_struct bhs, command_t* cmd);
response_t* handle_key_enter(basic_handle_struct bhs, command_t* cmd);
response_t* handle_copy_key(basic_handle_struct bhs, command_t* cmd);
response_t* handle_swap_key(basic_handle_struct bhs, command_t* cmd);
response_t* handle_rename_key(basic_handle_struct bhs, command_t* cmd);
response_t* handle_expire(basic_handle_struct bhs, command_t* cmd);
response_t* handle_persist(basic_handle_struct bhs, command_t* cmd);
response_t* handle_ttl(basic_handle_struct bhs, command_t* cmd);
response_t* handle_incr(basic_handle_struct bhs, command_t* cmd);
response_t* handle_decr(basic_handle_struct bhs, command_t* cmd);
response_t* handle_incr_by(basic_handle_struct bhs, command_t* cmd);
response_t* handle_decr_by(basic_handle_struct bhs, command_t* cmd);
response_t* handle_get_keys(basic_handle_struct bhs, command_t* cmd);
response_t* handle_get_key_count(basic_handle_struct bhs, command_t* cmd);

// STREAM类语法命令处理函数声明
response_t* handle_append(basic_handle_struct bhs, command_t* cmd);
response_t* handle_append_pos(basic_handle_struct bhs, command_t* cmd);
response_t* handle_get_stream(basic_handle_struct bhs, command_t* cmd);
response_t* handle_set_stream(basic_handle_struct bhs, command_t* cmd);
response_t* handle_set_stream_len(basic_handle_struct bhs, command_t* cmd);
response_t* handle_get_stream_len(basic_handle_struct bhs, command_t* cmd);
response_t* handle_get_stream_all(basic_handle_struct bhs, command_t* cmd);
response_t* handle_clear_stream(basic_handle_struct bhs, command_t* cmd);
response_t* handle_merge_stream(basic_handle_struct bhs, command_t* cmd);

// LIST类语法命令处理函数声明
response_t* handle_lpush(basic_handle_struct bhs, command_t* cmd);
response_t* handle_rpush(basic_handle_struct bhs, command_t* cmd);
response_t* handle_lpop(basic_handle_struct bhs, command_t* cmd);
response_t* handle_rpop(basic_handle_struct bhs, command_t* cmd);
response_t* handle_get_list(basic_handle_struct bhs, command_t* cmd);
response_t* handle_del_list(basic_handle_struct bhs, command_t* cmd);
response_t* handle_get_list_len(basic_handle_struct bhs, command_t* cmd);
response_t* handle_insert_list(basic_handle_struct bhs, command_t* cmd);
response_t* handle_set_list(basic_handle_struct bhs, command_t* cmd);
response_t* handle_exists_list(basic_handle_struct bhs, command_t* cmd);
response_t* handle_get_list_length(basic_handle_struct bhs, command_t* cmd);
response_t* handle_swap_list(basic_handle_struct bhs, command_t* cmd);
response_t* handle_copy_list(basic_handle_struct bhs, command_t* cmd);
response_t* handle_clear_list(basic_handle_struct bhs, command_t* cmd);
response_t* handle_reverse_list(basic_handle_struct bhs, command_t* cmd);
response_t* handle_sort_list(basic_handle_struct bhs, command_t* cmd);
response_t* handle_unique_list(basic_handle_struct bhs, command_t* cmd);
response_t* handle_get_list_slice(basic_handle_struct bhs, command_t* cmd);
response_t* handle_find_list(basic_handle_struct bhs, command_t* cmd);
response_t* handle_count_list(basic_handle_struct bhs, command_t* cmd);
response_t* handle_join_list(basic_handle_struct bhs, command_t* cmd);

// BITMAP类语法命令处理函数声明
response_t* handle_set_bit(basic_handle_struct bhs, command_t* cmd);
response_t* handle_set_bit_range(basic_handle_struct bhs, command_t* cmd);
response_t* handle_get_bit(basic_handle_struct bhs, command_t* cmd);
response_t* handle_get_bit_range(basic_handle_struct bhs, command_t* cmd);
response_t* handle_count_bit(basic_handle_struct bhs, command_t* cmd);
response_t* handle_count_bit_all(basic_handle_struct bhs, command_t* cmd);
response_t* handle_flip_bit(basic_handle_struct bhs, command_t* cmd);
response_t* handle_flip_bit_range(basic_handle_struct bhs, command_t* cmd);
response_t* handle_clear_bit(basic_handle_struct bhs, command_t* cmd);
response_t* handle_fill_bit(basic_handle_struct bhs, command_t* cmd);
response_t* handle_find_bit(basic_handle_struct bhs, command_t* cmd);
response_t* handle_find_bit_start(basic_handle_struct bhs, command_t* cmd);
response_t* handle_and_bit(basic_handle_struct bhs, command_t* cmd);
response_t* handle_or_bit(basic_handle_struct bhs, command_t* cmd);
response_t* handle_xor_bit(basic_handle_struct bhs, command_t* cmd);
response_t* handle_not_bit(basic_handle_struct bhs, command_t* cmd);
response_t* handle_shift_bit(basic_handle_struct bhs, command_t* cmd);
response_t* handle_shift_bit_math(basic_handle_struct bhs, command_t* cmd);
response_t* handle_get_bit_size(basic_handle_struct bhs, command_t* cmd);
response_t* handle_resize_bit(basic_handle_struct bhs, command_t* cmd);
response_t* handle_import_bit(basic_handle_struct bhs, command_t* cmd);
response_t* handle_join_bit(basic_handle_struct bhs, command_t* cmd);

// 跨HOOK互动操作命令处理函数声明
response_t* handle_hook_join(basic_handle_struct bhs, command_t* cmd);
response_t* handle_key_join(basic_handle_struct bhs, command_t* cmd);

// 系统管理命令处理函数声明
response_t* handle_system_info(basic_handle_struct bhs, command_t* cmd);
response_t* handle_system_status(basic_handle_struct bhs, command_t* cmd);
response_t* handle_system_register(basic_handle_struct bhs, command_t* cmd);
response_t* handle_system_cleanup(basic_handle_struct bhs, command_t* cmd);
response_t* handle_system_backup(basic_handle_struct bhs, command_t* cmd);
response_t* handle_system_restore(basic_handle_struct bhs, command_t* cmd);
response_t* handle_system_log(basic_handle_struct bhs, command_t* cmd);

// 持久性和压缩管理命令处理函数声明
response_t* handle_backup_obj(basic_handle_struct bhs, command_t* cmd);
response_t* handle_compress(basic_handle_struct bhs, command_t* cmd);

#endif
