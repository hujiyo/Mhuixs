#ifndef FUNSEQ_H
#define FUNSEQ_H
/*
这个文件定义了HUJI协议的命令编号
用于在客户端和服务器之间进行通信
*/

#ifdef __cplusplus
extern "C" {
#endif

// HUJI协议命令编号定义
typedef enum {
    // 基础语法命令 (1-50)
    CMD_GET_OBJECT = 1,           // [GET objname;]
    CMD_WHERE = 2,                // [WHERE;]
    CMD_DESC = 3,                 // [DESC;]
    CMD_HOOK_ROOT = 4,            // [HOOK;]
    CMD_HOOK_CREATE = 5,          // [HOOK objtype objname1 objname2 ...;]
    CMD_HOOK_SWITCH = 6,          // [HOOK objname;]
    CMD_HOOK_DEL = 7,             // [HOOK DEL objname1 objname2 ...;]
    CMD_HOOK_CLEAR = 8,           // [HOOK CLEAR objname1 objname2 ...;]
    CMD_HOOK_COPY = 9,            // [HOOK COPY src_objname dst_objname;]
    CMD_HOOK_SWAP = 10,           // [HOOK SWAP src_objname dst_objname;]
    CMD_HOOK_MERGE = 11,          // [HOOK MERGE src_objname dst_objname;]
    CMD_HOOK_RENAME = 12,         // [HOOK RENAME old_key new_key;]
    CMD_RANK_SET = 13,            // [RANK objname rank;]
    CMD_GET_RANK = 14,            // [GET RANK objname;]
    CMD_GET_TYPE = 15,            // [GET TYPE objname;]
    CMD_LOCK = 16,                // [LOCK objname timeout;]
    CMD_UNLOCK = 17,              // [UNLOCK objname;]
    CMD_EXPORT = 18,              // [EXPORT objname format;]
    CMD_IMPORT = 19,              // [IMPORT objname format data;]
    CMD_CHMOD = 20,               // [CHMOD objname mode;]
    CMD_GET_CHMOD = 21,           // [GET CHMOD objname;]
    CMD_WAIT = 22,                // [WAIT timeout;]
    CMD_MULTI = 23,               // [MULTI;]
    CMD_EXEC = 24,                // [EXEC;]
    CMD_ASYNC = 25,               // [ASYNC command;]
    CMD_SYNC = 26,                // [SYNC;]
    
    // 添加INDEX命令编号
    CMD_INDEX_CREATE = 27,        // [INDEX CREATE field_name index_type;]
    CMD_INDEX_DEL = 28,           // [INDEX DEL field_name;]

    // 条件控制命令 (51-70) - 本地处理
    CMD_IF = 51,                  // [IF condition;]
    CMD_ELSE = 52,                // [ELSE;]
    CMD_ELIF = 53,                // [ELIF condition;]
    CMD_WHILE = 54,               // [WHILE condition;]
    CMD_FOR = 55,                 // [FOR var start end step;]
    CMD_END = 56,                 // [END;]
    CMD_BREAK = 57,               // [BREAK;]
    CMD_CONTINUE = 58,            // [CONTINUE;]
    
    // TABLE类语法命令 (101-150)
    CMD_FIELD_ADD = 101,          // [FIELD ADD field_name datatype restraint;]
    CMD_FIELD_INSERT = 102,       // [FIELD INSERT index field_name datatype restraint;]
    CMD_FIELD_SWAP = 103,         // [FIELD SWAP field1_index field2_index;]
    CMD_FIELD_DEL = 104,          // [FIELD DEL field1_index field2_index ...;]
    CMD_FIELD_SET_ATTR = 105,     // [FIELD SET ATTRIBUTE field_index attribute;]
    CMD_FIELD_GET_INFO = 106,     // [FIELD GET INFO field_index;]
    CMD_ADD_RECORD = 107,         // [ADD value1 value2...;]
    CMD_INSERT_RECORD = 108,      // [INSERT line_index value1 value2 ...;]
    CMD_SET_RECORD = 109,         // [SET line1_index field1_index value1 ...;]
    CMD_DEL_RECORD = 110,         // [DEL line1_index line2_index;]
    CMD_SWAP_RECORD = 111,        // [SWAP line1_index line2_index;]
    CMD_GET_RECORD = 112,         // [GET line1_index line2_index;]
    CMD_GET_FIELD = 113,          // [GET FIELD field_name;]
    CMD_GET_POS = 114,            // [GET POS x1 y1 x2 y2 ...;]
    CMD_GET_COUNT = 115,          // [GET COUNT;]
    CMD_GET_WHERE = 116,          // [GET WHERE condition;]
    CMD_SET_WHERE = 117,          // [SET WHERE condition field_name value;]
    CMD_DEL_WHERE = 118,          // [DEL WHERE condition;]
    CMD_SORT = 119,               // [SORT field_name order;]
    
    // KVALOT类语法命令 (151-200)
    CMD_EXISTS = 151,             // [EXISTS key1 key2 ...;]
    CMD_SELECT = 152,             // [SELECT pattern;]
    CMD_SET_TYPE = 153,           // [SET TYPE type key1 key2 key3 ...;]
    CMD_SET_KEY = 154,            // [SET key value;]
    CMD_GET_KEY = 155,            // [GET key;]
    CMD_DEL_KEY = 156,            // [DEL key1 key2 ...;]
    CMD_KEY_ENTER = 157,          // [KEY key;]
    CMD_COPY_KEY = 158,           // [COPY src_key dst_key;]
    CMD_SWAP_KEY = 159,           // [SWAP src_key dst_key;]
    CMD_RENAME_KEY = 160,         // [RENAME old_key new_key;]
    CMD_EXPIRE = 161,             // [EXPIRE key microseconds;]
    CMD_PERSIST = 162,            // [PERSIST key;]
    CMD_TTL = 163,                // [TTL key;]
    CMD_INCR = 164,               // [INCR key;]
    CMD_DECR = 165,               // [DECR key;]
    CMD_INCR_BY = 166,            // [INCR key value;]
    CMD_DECR_BY = 167,            // [DECR key value;]
    CMD_GET_KEYS = 168,           // [GET KEYS;]
    CMD_GET_KEY_COUNT = 169,      // [GET COUNT;]
    
    // STREAM类语法命令 (201-230)
    CMD_APPEND = 201,             // [APPEND value;]
    CMD_APPEND_POS = 202,         // [APPEND pos value;]
    CMD_GET_STREAM = 203,         // [GET pos len;]
    CMD_SET_STREAM = 204,         // [SET pos value;]
    CMD_SET_STREAM_LEN = 205,     // [SET pos len char;]
    CMD_GET_STREAM_LEN = 206,     // [GET LEN;]
    CMD_GET_STREAM_ALL = 207,     // [GET;]
    CMD_CLEAR_STREAM = 208,       // [CLEAR;]
    CMD_MERGE_STREAM = 209,       // [MERGE src_stream;]
    
    // LIST类语法命令 (231-280)
    CMD_LPUSH = 231,              // [LPUSH value;]
    CMD_RPUSH = 232,              // [RPUSH value;]
    CMD_LPOP = 233,               // [LPOP;]
    CMD_RPOP = 234,               // [RPOP;]
    CMD_GET_LIST = 235,           // [GET index;]
    CMD_DEL_LIST = 236,           // [DEL index;]
    CMD_GET_LIST_LEN = 237,       // [GET LEN index;]
    CMD_INSERT_LIST = 238,        // [INSERT index value;]
    CMD_SET_LIST = 239,           // [SET index value;]
    CMD_EXISTS_LIST = 240,        // [EXISTS value1 value2 ...;]
    CMD_GET_LIST_LENGTH = 241,    // [GET LEN;]
    CMD_SWAP_LIST = 242,          // [SWAP index1 index2;]
    CMD_COPY_LIST = 243,          // [COPY src_index dst_index;]
    CMD_CLEAR_LIST = 244,         // [CLEAR;]
    CMD_REVERSE_LIST = 245,       // [REVERSE;]
    CMD_SORT_LIST = 246,          // [SORT order;]
    CMD_UNIQUE_LIST = 247,        // [UNIQUE;]
    CMD_GET_LIST_SLICE = 248,     // [GET start end;]
    CMD_FIND_LIST = 249,          // [FIND value;]
    CMD_COUNT_LIST = 250,         // [COUNT value;]
    CMD_JOIN_LIST = 251,          // [JOIN src_list;]
    
    // BITMAP类语法命令 (281-330)
    CMD_SET_BIT = 281,            // [SET offset value;]
    CMD_SET_BIT_RANGE = 282,      // [SET offset1 offset2 value;]
    CMD_GET_BIT = 283,            // [GET offset;]
    CMD_GET_BIT_RANGE = 284,      // [GET offset1 offset2;]
    CMD_COUNT_BIT = 285,          // [COUNT offset1 offset2;]
    CMD_COUNT_BIT_ALL = 286,      // [COUNT;]
    CMD_FLIP_BIT = 287,           // [FLIP offset;]
    CMD_FLIP_BIT_RANGE = 288,     // [FLIP offset1 offset2;]
    CMD_CLEAR_BIT = 289,          // [CLEAR;]
    CMD_FILL_BIT = 290,           // [FILL value;]
    CMD_FIND_BIT = 291,           // [FIND value;]
    CMD_FIND_BIT_START = 292,     // [FIND value start;]
    CMD_AND_BIT = 293,            // [AND src_bitmap;]
    CMD_OR_BIT = 294,             // [OR src_bitmap;]
    CMD_XOR_BIT = 295,            // [XOR src_bitmap;]
    CMD_NOT_BIT = 296,            // [NOT;]
    CMD_SHIFT_BIT = 297,          // [SHIFT direction count;]
    CMD_SHIFT_BIT_MATH = 298,     // [SHIFT direction count MATHS;]
    CMD_GET_BIT_SIZE = 299,       // [GET SIZE;]
    CMD_RESIZE_BIT = 300,         // [RESIZE size;]
    CMD_IMPORT_BIT = 301,         // [IMPORT format data;]
    CMD_JOIN_BIT = 302,           // [JOIN src_bitmap;]
    
    // 跨HOOK互动操作命令 (331-340)
    CMD_HOOK_JOIN = 331,          // [HOOK JOIN hook1 hook2 new_hook;]
    CMD_KEY_JOIN = 332,           // [KEY JOIN key1 key2 new_key;]
    
    // 系统管理命令 (341-360)
    CMD_SYSTEM_INFO = 341,        // [SYSTEM INFO;]
    CMD_SYSTEM_STATUS = 342,      // [SYSTEM STATUS;]
    CMD_SYSTEM_REGISTER = 343,    // [SYSTEM REGISTER;]
    CMD_SYSTEM_CLEANUP = 344,     // [SYSTEM CLEANUP;]
    CMD_SYSTEM_BACKUP = 345,      // [SYSTEM BACKUP path;]
    CMD_SYSTEM_RESTORE = 346,     // [SYSTEM RESTORE path;]
    CMD_SYSTEM_LOG = 347,         // [SYSTEM LOG level;]
    
    // 宏和变量操作命令 (客户端本地处理 - 361-370)
    CMD_SET_VAR = 361,            // [$var_name value;]
    CMD_GET_VAR = 362,            // [GET $var_name;]
    CMD_SET_VAR_VALUE = 363,      // [SET $var_name value;]
    CMD_DEL_VAR = 364,            // [DEL $var_name;]
    CMD_SET_VAR_FROM_GET = 365,   // [$var_name = GET command;]
    
    // 持久性和压缩管理命令 (371-380)
    CMD_BACKUP_OBJ = 371,         // [BACKUP objname;]
    CMD_COMPRESS = 372,           // [ZS objname rank;]
    
    // 错误命令
    CMD_ERROR = 999               // 错误命令

    //新增
    CMD_SYSTEM_SHUTDOWN = 381,    // [SYSTEM SHUTDOWN;]
} CommandNumber;

#ifdef __cplusplus
}
#endif

#endif
