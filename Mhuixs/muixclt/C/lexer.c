/*
#版权所有 (c) HuJi 2025.1
#保留所有权利
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
Email:hj18914255909@outlook.com
*/
#define err -1

#include <stdint.h>
#include <time.h>
#include "stdstr.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <arpa/inet.h>  // 用于htonl函数

// 添加缺失的字符串操作函数
static inline int str_append_string(str *s, const char *cstr) {
    if (!s || !cstr) return -1;
    return sappend(s, cstr, strlen(cstr));
}

static inline int str_append_data(str *s, const void *data, uint32_t len) {
    if (!s || !data) return -1;
    return sappend(s, data, len);
}

typedef enum {
    // 对象管理
    TOKEN_HOOK,         // 钩子

    // 对象类型
    TOKEN_TABLE,        // 表类型
    TOKEN_KVALOT,       // 键库类型
    TOKEN_LIST,         // 列表类型
    TOKEN_BITMAP,       // 位图类型
    TOKEN_STREAM,       // 流类型

    // 操作关键字
    TOKEN_DEL,          // 删除
    TOKEN_TYPE,         // 类型
    TOKEN_RANK,         // 权限等级
    TOKEN_CLEAR,        // 清空
    TOKEN_DESC,         // 描述
    TOKEN_GET,          // 获取
    TOKEN_WHERE,        // 条件
    TOKEN_TEMP,         // 临时
    TOKEN_SET,          // 设置
    TOKEN_EXISTS,       // 存在
    TOKEN_SELECT,       // 选择
    TOKEN_KEY,          // 键
    TOKEN_INSERT,       // 插入
    TOKEN_FIELD,        // 字段
    TOKEN_ADD,          // 添加
    TOKEN_SWAP,         // 交换
    TOKEN_ATTRIBUTE,    // 属性
    TOKEN_POS,          // 位置
    TOKEN_APPEND,       // 追加
    TOKEN_LEN,          // 长度
    TOKEN_LPUSH,        // 左压入
    TOKEN_RPUSH,        // 右压入
    TOKEN_LPOP,         // 左弹出
    TOKEN_RPOP,         // 右弹出
    TOKEN_COUNT,        // 计数
    TOKEN_COPY,         // 复制
    TOKEN_MOVE,         // 移动
    TOKEN_MERGE,        // 合并
    TOKEN_SPLIT,        // 分割
    TOKEN_EXPORT,       // 导出
    TOKEN_IMPORT,       // 导入
    TOKEN_SUBSCRIBE,    // 订阅
    TOKEN_UNSUBSCRIBE,  // 取消订阅
    TOKEN_NOTIFY,       // 通知
    TOKEN_LOCK,         // 锁定
    TOKEN_UNLOCK,       // 解锁
    TOKEN_COMMIT,       // 提交
    TOKEN_ROLLBACK,     // 回滚
    TOKEN_ASYNC,        // 异步
    TOKEN_SYNC,         // 同步
    TOKEN_WAIT,         // 等待
    TOKEN_RENAME,       // 重命名
    TOKEN_CHMOD,        // 权限
    TOKEN_MULTI,        // 开始事务
    TOKEN_EXEC,         // 执行事务
    TOKEN_EXPIRE,       // 过期
    TOKEN_PERSIST,      // 持久化
    TOKEN_TTL,          // 生存时间
    TOKEN_INCR,         // 递增
    TOKEN_DECR,         // 递减
    TOKEN_KEYS,         // 键集合
    TOKEN_FLIP,         // 翻转
    TOKEN_FILL,         // 填充
    TOKEN_FIND,         // 查找
    TOKEN_AND,          // 与操作
    TOKEN_OR,           // 或操作
    TOKEN_XOR,          // 异或操作
    TOKEN_NOT,          // 非操作
    TOKEN_SHIFT,        // 位移
    TOKEN_RESIZE,       // 调整大小
    TOKEN_JOIN,         // 连接
    TOKEN_REVERSE,      // 反转
    TOKEN_SORT,         // 排序
    TOKEN_UNIQUE,       // 去重
    TOKEN_SYSTEM,       // 系统
    TOKEN_INFO,         // 信息
    TOKEN_STATUS,       // 状态
    TOKEN_REGISTER,     // 注册表
    TOKEN_CLEANUP,      // 清理
    TOKEN_BACKUP,       // 备份
    TOKEN_RESTORE,      // 恢复
    TOKEN_LOG,          // 日志
    TOKEN_ZS,           // 压缩

    // 类型关键字
    TOKEN_i1,           // int8_t
    TOKEN_i2,           // int16_t
    TOKEN_i4,           // int32_t/int
    TOKEN_i8,           // int64_t
    TOKEN_ui1,          // uint8_t
    TOKEN_ui2,          // uint16_t
    TOKEN_ui4,          // uint32_t
    TOKEN_ui8,          // uint64_t
    TOKEN_f4,           // float
    TOKEN_f8,           // double
    TOKEN_str,          // string
    TOKEN_date,         // 日期
    TOKEN_time,         // 时间
    TOKEN_datetime,     // 日期时间
    TOKEN_bool,         // 布尔
    TOKEN_blob,         // 二进制大对象
    TOKEN_json,         // JSON类型

    // 键关键字
    TOKEN_PKEY,         // 主键
    TOKEN_FKEY,         // 外键
    TOKEN_NOTNULL,      // 非空
    TOKEN_DEFAULT,      // 默认值
    TOKEN_INDEX,        // 索引
    TOKEN_AUTO_INCREMENT, // 自动递增

    // 条件关键字
    TOKEN_IF,           // 如果
    TOKEN_ELSE,         // 否则
    TOKEN_ELIF,         // 否则如果
    TOKEN_ENDIF,        // 结束如果
    TOKEN_WHILE,        // 循环
    TOKEN_ENDWHILE,     // 结束循环
    TOKEN_FOR,          // for循环
    TOKEN_ENDFOR,       // 结束for循环
    TOKEN_END,          // 结束（通用）
    TOKEN_BREAK,        // 跳出
    TOKEN_CONTINUE,     // 继续

    // 逻辑关键字
    TOKEN_AND_LOGIC,    // 逻辑与
    TOKEN_OR_LOGIC,     // 逻辑或
    TOKEN_NOT_LOGIC,    // 逻辑非
    TOKEN_IN,           // 在...中
    TOKEN_BETWEEN,      // 在...之间
    TOKEN_LIKE,         // 模糊匹配
    TOKEN_IS,           // 是
    TOKEN_NULL,         // 空值

    // 其他关键字
    TOKEN_ASC,          // 升序
    TOKEN_DESC_ORDER,   // 降序
    TOKEN_LEFT,         // 左
    TOKEN_RIGHT,        // 右
    TOKEN_MATHS,        // 数学运算
    TOKEN_SIZE,         // 大小
    TOKEN_CREATE,       // 创建
    TOKEN_WARN,         // 警告
    TOKEN_ERROR,        // 错误
    TOKEN_JSON_FORMAT,  // JSON格式
    TOKEN_CSV,          // CSV格式
    TOKEN_BINARY,       // 二进制格式
    TOKEN_PATTERN,      // 模式
    TOKEN_MICROSECONDS, // 微秒
    TOKEN_DIRECTION,    // 方向
    TOKEN_FORMAT,       // 格式
    TOKEN_DATA,         // 数据
    TOKEN_PATH,         // 路径
    TOKEN_LEVEL,        // 级别
    TOKEN_TIMEOUT,      // 超时
    TOKEN_MODE,         // 模式
    TOKEN_STEP,         // 步长
    TOKEN_START,        // 开始
    TOKEN_OFFSET,       // 偏移
    TOKEN_VALUE,        // 值
    TOKEN_CONDITION,    // 条件
    TOKEN_VAR,          // 变量

    // 宏替换
    TOKEN_MACRO,        // 宏 ($)

    // 基本类型
    TOKEN_NAME,         // 名称
    TOKEN_VALUES,       // 数据值
    TOKEN_NUM,          // 数字

    // 特殊token
    TOKEN_END_STMT,     // 语句结束符 (;)
    TOKEN_EEROR,        // token错误
    TOKEN_UNKNOWN,      // 未识别

    // 符号
    TOKEN_SYMBOL,       // 符号
    TOKEN_DY,           // 大于号 >
    TOKEN_XY,           // 小于号 <
    TOKEN_DYDY,         // 大于等于号 >=
    TOKEN_XYDY,         // 小于等于号 <=
    TOKEN_DDY,          // 等于号 ==
    TOKEN_BDY,          // 不等于号 !=
    TOKEN_YDY,          // 模糊匹配 ~=
    TOKEN_BJ,           // 并集 <>
    TOKEN_JJ,           // 交集 ><

} TokType,toktype;

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
    CMD_RANK_SET = 12,            // [RANK objname rank;]
    CMD_GET_RANK = 13,            // [GET RANK objname;]
    CMD_GET_TYPE = 14,            // [GET TYPE objname;]
    CMD_LOCK = 15,                // [LOCK objname timeout;]
    CMD_UNLOCK = 16,              // [UNLOCK objname;]
    CMD_EXPORT = 17,              // [EXPORT objname format;]
    CMD_IMPORT = 18,              // [IMPORT objname format data;]
    CMD_HOOK_RENAME = 19,         // [HOOK RENAME old_key new_key;]
    CMD_CHMOD = 20,               // [CHMOD objname mode;]
    CMD_GET_CHMOD = 21,           // [GET CHMOD objname;]
    CMD_WAIT = 22,                // [WAIT timeout;]
    
    // 事务控制命令 (51-70)
    CMD_MULTI = 51,               // [MULTI;]
    CMD_EXEC = 52,                // [EXEC;]
    CMD_ASYNC = 53,               // [ASYNC command;]
    CMD_SYNC = 54,                // [SYNC;]
    
    // 条件控制命令 (客户端本地处理 - 71-90)
    CMD_IF = 71,                  // [IF condition;]
    CMD_ELSE = 72,                // [ELSE;]
    CMD_ELIF = 73,                // [ELIF condition;]
    CMD_WHILE = 74,               // [WHILE condition;]
    CMD_FOR = 75,                 // [FOR var start end step;]
    CMD_END = 76,                 // [END;]
    CMD_BREAK = 77,               // [BREAK;]
    CMD_CONTINUE = 78,            // [CONTINUE;]
    
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
    CMD_INDEX_CREATE = 120,       // [INDEX CREATE field_name index_type;]
    CMD_INDEX_DEL = 121,          // [INDEX DEL field_name;]
    
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
    
    // 持久性和压缩管理命令 (371-380)
    CMD_BACKUP_OBJ = 371,         // [BACKUP objname;]
    CMD_COMPRESS = 372,           // [ZS objname rank;]
    
    // 错误命令
    CMD_ERROR = 999               // 错误命令
} CommandNumber;

typedef struct {
    const char *keyword;
    toktype type;
} keyword,kw;



//令牌结构体
typedef struct Token{
    str content;//TOKEN的字面信息
    toktype type;//TOKEN的类型
} Token,tok;//符号（令牌）结构体

//用户输入的字符串将会用这个结构体来储存、处理和分析
typedef struct inputstr{
    uint8_t *string;//待处理的字符串
    uint32_t len;
    uint8_t *pos;//指向string中待处理的字符
}inputstr;

//下面是token分隔函数的实现
static tok* getoken(inputstr* instr)//返回的token记得释放
{
    /*
    起始字符总结:
    1.关键字:首字母为字符
    2.数字:首字母为数字
    3.字符串:首字母为"
    4.语句结束符:首字符为;或\n
    5.单字符:首字符为'
    6.变量名:首字母为_或字符或中文
    禁止'\'续行 

    结束字符总结:
    1.关键字、数字、变量名:空格或\n或';'
    2.字符串:只能是"
    3.单字符:只能是'
    */
   
    //判断是否是token的起始字符,不包括某些单独处理的字符
    #define is_std_token_start_char(c)  ( \
    (c>='a' && c<='z')      ||      \
    (c>='A' && c<='Z')      ||      \
    (c>='0' && c<='9')      ||      \
    (c>=128)                ||      \
    (c=='_')                ||      \
    (c=='\"')               ||      \
    (c=='\'')               ||      \
    (c==';')                ||      \
    (c=='\n')                     \
    )
    //判断是否是token的结束字符
    #define is_std_token_end_char(c)  ( \
    (c ==' ')  || \
    (c =='\n')  || \
    (c =='\'')  || \
    (c ==';')     \
    )

    tok *token = (tok*)calloc(1,sizeof(tok));
    str_init(&token->content);


    /*
    功能:从pos位置开始，截取下一个token并返回，基本不做token类型判断
    请保证instr合法,pos必须在string的合法范围内

    返回值:
    成功:返回token指针
    失败/字符串结束:返回TOKEN_EEROR
    instr结束:返回NULL

    返回的token部分有类型判断：
    1.TOKEN_EEROR:表示语法错误
    2.TOKEN_END:表示语句结束符
    3.TOKEN_UNKNOWN:表示未知token
    4.TOKEN_VALUES:表示值    
    5.TOKEN_SYMBOL:表示符号
    */
    //检查instr是否合法，pos是否在string的合法范围内
    if(instr == NULL || instr->string == NULL ||instr->len == 0 || instr->pos == NULL ||
        instr->pos < instr->string ||instr->pos >=  instr->string+instr->len){
        str_free(&token->content);
        free(token);
        return NULL;//返回NULL表示instr已经分割完毕了或者instr无法分割（即不合法）
    }

    //下面定义一些flags
    int is_string = 0;//函数全局变量：是否是字符串:"
    int is_char = 0;//函数全局变量：是否是单字节字符:'
    int is_word = 0;//函数全局变量：是否是名字:字母、下划线
    int is_number = 0;//函数全局变量：是否是数字:数字
    int is_symbol = 0;//函数全局变量：是否是符号:>(>,>=,><) <(<,<=,<>) =(==) ~(~=) !(!=)

    //在原来pos的位置基础上先后寻找首个token的起始字符
    for(; instr->pos < instr->string + instr->len ; instr->pos++){
        if(is_std_token_start_char(*instr->pos)){
            //找到token的起始字符了,开始分类处理
            if(*instr->pos == '\"'){
                is_string = 1;
                break;
            }
            if(*instr->pos == '\''){
                is_char = 1;
                break;
            }
            if(*instr->pos == ';' || *instr->pos == '\n'){
                token->type = TOKEN_END_STMT;
                instr->pos++;//跳过';'或'\n'
                return token;
            }
            if(*instr->pos >= '0' && *instr->pos <= '9'){
                is_number = 1;
                break;
            }
            if(*instr->pos >= 'a' && *instr->pos <= 'z' || *instr->pos >= 'A' && *instr->pos <= 'Z' || *instr->pos == '_' || *instr->pos >= 128){
                is_word = 1;
                break;
            }
        }
        // 添加对符号的识别
        else if(*instr->pos == '>' || *instr->pos == '<' || *instr->pos == '=' || *instr->pos == '~' || *instr->pos == '!') {
            is_symbol = 1;//>(>,>=,><) <(<,<=,<>) =(==) ~(~=) !(!=)
            break;
        }
    }

    if(is_number+is_word+is_char + is_string + is_symbol == 0){
        //说明没有找到token的起始字符(连结束符都没有找到)
        str_free(&token->content);
        free(token);
        return NULL;//返回NULL表示instr已经分割完毕了  
    }

    
    uint8_t* ed_pos = instr->pos+1;//建立一个临时的指针，用于寻找token的结束字符
    //推动ed_pos指针，截取完整的token
    if(is_char){
        *instr->pos++;//跳过'
        //单字节字符单独处理,注意要考虑到转义字符
        if(instr->pos+2 < instr->string+instr->len && *instr->pos == '\\' ){
            //说明是转义字符
            if(*(instr->pos+2) != '\''){
                token->type = TOKEN_EEROR;
                //不用管pos的位置，因为已经TOKEN_EEROR了
                return token;
            }
            instr->pos++;//跳过'\'
            switch(*instr->pos){
                case 'n':
                    swrite(&token->content,0,"\n",1);
                    break;
                case 't':
                    swrite(&token->content,0,"\t",1);
                    break;
                case 'r':
                    swrite(&token->content,0,"\r",1);
                    break;
                case '0':
                    swrite(&token->content,0,"\0",1);
                    break;
                case '\\':
                    swrite(&token->content,0,"\\",1);
                    break;
                default:
                    token->type = TOKEN_EEROR;
                    //不用管pos的位置，因为已经TOKEN_EEROR了
                    return token;
            }
            token->type =TOKEN_VALUES;
            instr->pos+=2;
            return token;
        }
        else if(instr->pos+1 < instr->string+instr->len){
            if(*(instr->pos+1) != '\''){
                token->type = TOKEN_EEROR;
                //不用管pos的位置，因为已经TOKEN_EEROR了
                return token;                            
            }
            swrite(&token->content,0,instr->pos,1);
            token->type =TOKEN_VALUES;
            instr->pos+=2;
            return token;
        }
        else{
            token->type = TOKEN_EEROR;
            //不用管pos的位置，因为已经TOKEN_EEROR了
            return token; 
        }
    }    
    else if(is_string){//字符串处理
        *instr->pos++;//跳过"
        for(;ed_pos < instr->string+instr->len;ed_pos++){
            //不断判断ed_pos指向的是否是string的结束字符
            if(*ed_pos == '\"' && *(ed_pos-1) != '\\'){
                //说明是字符串的结束字符
                swrite(&token->content,0,instr->pos,ed_pos-instr->pos);
                token->type = TOKEN_VALUES;
                instr->pos = ed_pos+1;//跳过"
                //这里不需要安全检查，因为每次调用getoken都会检查instr的合法性
                return token;
            }
        }
        //没找到字符串的结束字符
        token->type = TOKEN_EEROR;
        return token;
    }
    else if(is_word){//名字处理
        for(;ed_pos < instr->string+instr->len;ed_pos++){
            //不断判断ed_pos指向的是否是string的结束字符 ' ' ';' '\n'
            if(*ed_pos == ' ' || *ed_pos == ';' || *ed_pos == '\n' || *ed_pos == '>' 
                || *ed_pos == '<' || *ed_pos == '=' || *ed_pos == '~' || *ed_pos == '!'){
                //名字结束了
                swrite(&token->content,0,instr->pos,ed_pos-instr->pos);
                token->type = TOKEN_UNKNOWN;
                instr->pos = ed_pos;
                return token;
            }
        }
        token->type = TOKEN_EEROR;
        return token;
    }
    else if(is_number){//数字处理
        for(;ed_pos < instr->string+instr->len;ed_pos++){
            if(*ed_pos == ' ' || *ed_pos == ';' || *ed_pos == '\n' || *ed_pos == '>'
                || *ed_pos == '<' || *ed_pos == '=' || *ed_pos == '~' || *ed_pos == '!'){
                //数字结束了
                swrite(&token->content,0,instr->pos,ed_pos-instr->pos);
                token->type = TOKEN_NUM;
                instr->pos = ed_pos;
                return token;
            }
            else if((*ed_pos < '0' || *ed_pos > '9' )&& *ed_pos != '.'){
                //不是数字
                token->type = TOKEN_EEROR;
                return token;
            }           
        }
        //最后一个token是数字
        swrite(&token->content,0,instr->pos,ed_pos-instr->pos);
        token->type = TOKEN_NUM;
        instr->pos = ed_pos;
        return token;
    }
    else if(is_symbol){//符号处理
        //>(>,>=,><) <(<,<=,<>) =(==) ~(~=) !(!=)
        //先确定后面还有没有字符
        if(ed_pos >= instr->string+instr->len||(*ed_pos != '=' && *ed_pos != '<' && *ed_pos!= '>')){
            //说明是单字节的符号
            if(*instr->pos == '~' || *instr->pos== '!' || *instr->pos== '='){
                //错误
                token->type = TOKEN_EEROR;
                return token;
            }
            swrite(&token->content,0,instr->pos,1);
            token->type = TOKEN_SYMBOL;
            instr->pos++;
            return token;
        }
        //说明后面还有字符
        if(*instr->pos == '>') {
            if(*(instr->pos+1) == '=' ||*(instr->pos+1) == '<') {
                swrite(&token->content,0,instr->pos,2);
                token->type = TOKEN_SYMBOL;
                instr->pos+=2;
                return token;
            }
            //错误
            token->type = TOKEN_EEROR;
            return token;
        } 
        else if(*instr->pos == '<') {
            if(*(instr->pos+1) == '=' ||*(instr->pos+1) == '>') {
                swrite(&token->content,0,instr->pos,2);
                token->type = TOKEN_SYMBOL;
                instr->pos+=2;
                return token;
            }
            //错误
            token->type = TOKEN_EEROR;
            return token;
        } 
        else if(*instr->pos == '='||*instr->pos == '~'||*instr->pos == '!') {
            if(*(instr->pos+1) == '=') {
                swrite(&token->content,0,instr->pos,2);
                token->type = TOKEN_SYMBOL;
                instr->pos+=2;
                return token;
            }
            //错误
            token->type = TOKEN_EEROR;
            return token;
        }else{}
    }
}

const keyword keyword_map[] = {
    // 对象管理
    {"HOOK", TOKEN_HOOK}, {"hook", TOKEN_HOOK},
    
    // 对象类型
    {"TABLE", TOKEN_TABLE}, {"table", TOKEN_TABLE},
    {"KVALOT", TOKEN_KVALOT}, {"kvalot", TOKEN_KVALOT},
    {"LIST", TOKEN_LIST}, {"list", TOKEN_LIST},
    {"BITMAP", TOKEN_BITMAP}, {"bitmap", TOKEN_BITMAP},
    {"STREAM", TOKEN_STREAM}, {"stream", TOKEN_STREAM},
    
    // 操作关键字
    {"DEL", TOKEN_DEL}, {"del", TOKEN_DEL},
    {"TYPE", TOKEN_TYPE}, {"type", TOKEN_TYPE},
    {"RANK", TOKEN_RANK}, {"rank", TOKEN_RANK},
    {"CLEAR", TOKEN_CLEAR}, {"clear", TOKEN_CLEAR},
    {"DESC", TOKEN_DESC}, {"desc", TOKEN_DESC},
    {"GET", TOKEN_GET}, {"get", TOKEN_GET},
    {"WHERE", TOKEN_WHERE}, {"where", TOKEN_WHERE},
    {"TEMP", TOKEN_TEMP}, {"temp", TOKEN_TEMP},
    {"SET", TOKEN_SET}, {"set", TOKEN_SET},
    {"EXISTS", TOKEN_EXISTS}, {"exists", TOKEN_EXISTS},
    {"SELECT", TOKEN_SELECT}, {"select", TOKEN_SELECT},
    {"KEY", TOKEN_KEY}, {"key", TOKEN_KEY},
    {"INSERT", TOKEN_INSERT}, {"insert", TOKEN_INSERT},
    {"FIELD", TOKEN_FIELD}, {"field", TOKEN_FIELD},
    {"ADD", TOKEN_ADD}, {"add", TOKEN_ADD},
    {"SWAP", TOKEN_SWAP}, {"swap", TOKEN_SWAP},
    {"ATTRIBUTE", TOKEN_ATTRIBUTE}, {"attribute", TOKEN_ATTRIBUTE},
    {"POS", TOKEN_POS}, {"pos", TOKEN_POS},
    {"APPEND", TOKEN_APPEND}, {"append", TOKEN_APPEND},
    {"LEN", TOKEN_LEN}, {"len", TOKEN_LEN},
    {"LPUSH", TOKEN_LPUSH}, {"lpush", TOKEN_LPUSH},
    {"RPUSH", TOKEN_RPUSH}, {"rpush", TOKEN_RPUSH},
    {"LPOP", TOKEN_LPOP}, {"lpop", TOKEN_LPOP},
    {"RPOP", TOKEN_RPOP}, {"rpop", TOKEN_RPOP},
    {"COUNT", TOKEN_COUNT}, {"count", TOKEN_COUNT},
    {"COPY", TOKEN_COPY}, {"copy", TOKEN_COPY},
    {"MOVE", TOKEN_MOVE}, {"move", TOKEN_MOVE},
    {"MERGE", TOKEN_MERGE}, {"merge", TOKEN_MERGE},
    {"SPLIT", TOKEN_SPLIT}, {"split", TOKEN_SPLIT},
    {"EXPORT", TOKEN_EXPORT}, {"export", TOKEN_EXPORT},
    {"IMPORT", TOKEN_IMPORT}, {"import", TOKEN_IMPORT},
    {"SUBSCRIBE", TOKEN_SUBSCRIBE}, {"subscribe", TOKEN_SUBSCRIBE},
    {"UNSUBSCRIBE", TOKEN_UNSUBSCRIBE}, {"unsubscribe", TOKEN_UNSUBSCRIBE},
    {"NOTIFY", TOKEN_NOTIFY}, {"notify", TOKEN_NOTIFY},
    {"LOCK", TOKEN_LOCK}, {"lock", TOKEN_LOCK},
    {"UNLOCK", TOKEN_UNLOCK}, {"unlock", TOKEN_UNLOCK},
    {"COMMIT", TOKEN_COMMIT}, {"commit", TOKEN_COMMIT},
    {"ROLLBACK", TOKEN_ROLLBACK}, {"rollback", TOKEN_ROLLBACK},
    {"ASYNC", TOKEN_ASYNC}, {"async", TOKEN_ASYNC},
    {"SYNC", TOKEN_SYNC}, {"sync", TOKEN_SYNC},
    {"WAIT", TOKEN_WAIT}, {"wait", TOKEN_WAIT},
    {"RENAME", TOKEN_RENAME}, {"rename", TOKEN_RENAME},
    {"CHMOD", TOKEN_CHMOD}, {"chmod", TOKEN_CHMOD},
    {"MULTI", TOKEN_MULTI}, {"multi", TOKEN_MULTI},
    {"EXEC", TOKEN_EXEC}, {"exec", TOKEN_EXEC},
    {"EXPIRE", TOKEN_EXPIRE}, {"expire", TOKEN_EXPIRE},
    {"PERSIST", TOKEN_PERSIST}, {"persist", TOKEN_PERSIST},
    {"TTL", TOKEN_TTL}, {"ttl", TOKEN_TTL},
    {"INCR", TOKEN_INCR}, {"incr", TOKEN_INCR},
    {"DECR", TOKEN_DECR}, {"decr", TOKEN_DECR},
    {"KEYS", TOKEN_KEYS}, {"keys", TOKEN_KEYS},
    {"FLIP", TOKEN_FLIP}, {"flip", TOKEN_FLIP},
    {"FILL", TOKEN_FILL}, {"fill", TOKEN_FILL},
    {"FIND", TOKEN_FIND}, {"find", TOKEN_FIND},
    {"AND", TOKEN_AND}, {"and", TOKEN_AND},
    {"OR", TOKEN_OR}, {"or", TOKEN_OR},
    {"XOR", TOKEN_XOR}, {"xor", TOKEN_XOR},
    {"NOT", TOKEN_NOT}, {"not", TOKEN_NOT},
    {"SHIFT", TOKEN_SHIFT}, {"shift", TOKEN_SHIFT},
    {"RESIZE", TOKEN_RESIZE}, {"resize", TOKEN_RESIZE},
    {"JOIN", TOKEN_JOIN}, {"join", TOKEN_JOIN},
    {"REVERSE", TOKEN_REVERSE}, {"reverse", TOKEN_REVERSE},
    {"SORT", TOKEN_SORT}, {"sort", TOKEN_SORT},
    {"UNIQUE", TOKEN_UNIQUE}, {"unique", TOKEN_UNIQUE},
    {"SYSTEM", TOKEN_SYSTEM}, {"system", TOKEN_SYSTEM},
    {"INFO", TOKEN_INFO}, {"info", TOKEN_INFO},
    {"STATUS", TOKEN_STATUS}, {"status", TOKEN_STATUS},
    {"REGISTER", TOKEN_REGISTER}, {"register", TOKEN_REGISTER},
    {"CLEANUP", TOKEN_CLEANUP}, {"cleanup", TOKEN_CLEANUP},
    {"BACKUP", TOKEN_BACKUP}, {"backup", TOKEN_BACKUP},
    {"RESTORE", TOKEN_RESTORE}, {"restore", TOKEN_RESTORE},
    {"LOG", TOKEN_LOG}, {"log", TOKEN_LOG},
    {"ZS", TOKEN_ZS}, {"zs", TOKEN_ZS},
    
    // 类型关键字
    {"i1", TOKEN_i1}, {"int8_t", TOKEN_i1},
    {"i2", TOKEN_i2}, {"int16_t", TOKEN_i2},
    {"i4", TOKEN_i4}, {"int32_t", TOKEN_i4}, {"int", TOKEN_i4},
    {"i8", TOKEN_i8}, {"int64_t", TOKEN_i8},
    {"ui1", TOKEN_ui1}, {"uint8_t", TOKEN_ui1},
    {"ui2", TOKEN_ui2}, {"uint16_t", TOKEN_ui2},
    {"ui4", TOKEN_ui4}, {"uint32_t", TOKEN_ui4},
    {"ui8", TOKEN_ui8}, {"uint64_t", TOKEN_ui8},
    {"f4", TOKEN_f4}, {"float", TOKEN_f4},
    {"f8", TOKEN_f8}, {"double", TOKEN_f8},
    {"str", TOKEN_str}, {"string", TOKEN_str},
    {"date", TOKEN_date},
    {"time", TOKEN_time},
    {"datetime", TOKEN_datetime},
    {"bool", TOKEN_bool},
    {"blob", TOKEN_blob},
    {"json", TOKEN_json},
    
    // 键关键字
    {"PKEY", TOKEN_PKEY}, {"pkey", TOKEN_PKEY},
    {"FKEY", TOKEN_FKEY}, {"fkey", TOKEN_FKEY},
    {"NOTNULL", TOKEN_NOTNULL}, {"notnull", TOKEN_NOTNULL},
    {"DEFAULT", TOKEN_DEFAULT}, {"default", TOKEN_DEFAULT},
    {"INDEX", TOKEN_INDEX}, {"index", TOKEN_INDEX},
    {"AUTO_INCREMENT", TOKEN_AUTO_INCREMENT}, {"auto_increment", TOKEN_AUTO_INCREMENT},
    
    // 条件关键字
    {"IF", TOKEN_IF}, {"if", TOKEN_IF},
    {"ELSE", TOKEN_ELSE}, {"else", TOKEN_ELSE},
    {"ELIF", TOKEN_ELIF}, {"elif", TOKEN_ELIF},
    {"ENDIF", TOKEN_ENDIF}, {"endif", TOKEN_ENDIF},
    {"WHILE", TOKEN_WHILE}, {"while", TOKEN_WHILE},
    {"ENDWHILE", TOKEN_ENDWHILE}, {"endwhile", TOKEN_ENDWHILE},
    {"FOR", TOKEN_FOR}, {"for", TOKEN_FOR},
    {"ENDFOR", TOKEN_ENDFOR}, {"endfor", TOKEN_ENDFOR},
    {"END", TOKEN_END}, {"end", TOKEN_END},
    {"BREAK", TOKEN_BREAK}, {"break", TOKEN_BREAK},
    {"CONTINUE", TOKEN_CONTINUE}, {"continue", TOKEN_CONTINUE},
    
    // 逻辑关键字
    {"AND", TOKEN_AND_LOGIC}, {"and", TOKEN_AND_LOGIC},
    {"OR", TOKEN_OR_LOGIC}, {"or", TOKEN_OR_LOGIC},
    {"NOT", TOKEN_NOT_LOGIC}, {"not", TOKEN_NOT_LOGIC},
    {"IN", TOKEN_IN}, {"in", TOKEN_IN},
    {"BETWEEN", TOKEN_BETWEEN}, {"between", TOKEN_BETWEEN},
    {"LIKE", TOKEN_LIKE}, {"like", TOKEN_LIKE},
    {"IS", TOKEN_IS}, {"is", TOKEN_IS},
    {"NULL", TOKEN_NULL}, {"null", TOKEN_NULL},
    
    // 其他关键字
    {"ASC", TOKEN_ASC}, {"asc", TOKEN_ASC},
    {"DESC", TOKEN_DESC_ORDER}, {"desc", TOKEN_DESC_ORDER},
    {"LEFT", TOKEN_LEFT}, {"left", TOKEN_LEFT},
    {"RIGHT", TOKEN_RIGHT}, {"right", TOKEN_RIGHT},
    {"MATHS", TOKEN_MATHS}, {"maths", TOKEN_MATHS},
    {"SIZE", TOKEN_SIZE}, {"size", TOKEN_SIZE},
    {"CREATE", TOKEN_CREATE}, {"create", TOKEN_CREATE},
    {"WARN", TOKEN_WARN}, {"warn", TOKEN_WARN},
    {"ERROR", TOKEN_ERROR}, {"error", TOKEN_ERROR},
    {"JSON", TOKEN_JSON_FORMAT}, {"json", TOKEN_JSON_FORMAT},
    {"CSV", TOKEN_CSV}, {"csv", TOKEN_CSV},
    {"BINARY", TOKEN_BINARY}, {"binary", TOKEN_BINARY},
    {"PATTERN", TOKEN_PATTERN}, {"pattern", TOKEN_PATTERN},
    {"MICROSECONDS", TOKEN_MICROSECONDS}, {"microseconds", TOKEN_MICROSECONDS},
    {"DIRECTION", TOKEN_DIRECTION}, {"direction", TOKEN_DIRECTION},
    {"FORMAT", TOKEN_FORMAT}, {"format", TOKEN_FORMAT},
    {"DATA", TOKEN_DATA}, {"data", TOKEN_DATA},
    {"PATH", TOKEN_PATH}, {"path", TOKEN_PATH},
    {"LEVEL", TOKEN_LEVEL}, {"level", TOKEN_LEVEL},
    {"TIMEOUT", TOKEN_TIMEOUT}, {"timeout", TOKEN_TIMEOUT},
    {"MODE", TOKEN_MODE}, {"mode", TOKEN_MODE},
    {"STEP", TOKEN_STEP}, {"step", TOKEN_STEP},
    {"START", TOKEN_START}, {"start", TOKEN_START},
    {"OFFSET", TOKEN_OFFSET}, {"offset", TOKEN_OFFSET},
    {"VALUE", TOKEN_VALUE}, {"value", TOKEN_VALUE},
    {"CONDITION", TOKEN_CONDITION}, {"condition", TOKEN_CONDITION},
    {"VAR", TOKEN_VAR}, {"var", TOKEN_VAR},
    
    // 符号必须在关键字数组中连续排列,且">"放第一个
    {">", TOKEN_DY}, {"<", TOKEN_XY},
    {">=", TOKEN_DYDY}, {"<=", TOKEN_XYDY},
    {"==", TOKEN_DDY}, {"!=", TOKEN_BDY},
    {"~=", TOKEN_YDY}, {"<>", TOKEN_BJ},
    {"><", TOKEN_JJ}
};

// 修复符号数量定义
#define SYMBOL_NUM 9 // Mhuixs支持的符号总数量
// merr已在stdstr.h中定义

// 函数声明
static CommandNumber parse_get_statement(tok* token, int len, int* param_start);
static CommandNumber parse_hook_statement(tok* token, int len, int* param_start);
static CommandNumber parse_field_statement(tok* token, int len, int* param_start);
static CommandNumber parse_set_statement(tok* token, int len, int* param_start);
static CommandNumber parse_del_statement(tok* token, int len, int* param_start);
static CommandNumber parse_swap_statement(tok* token, int len, int* param_start);
static CommandNumber parse_key_statement(tok* token, int len, int* param_start);
static CommandNumber parse_append_statement(tok* token, int len, int* param_start);
static CommandNumber parse_sort_statement(tok* token, int len, int* param_start);
static CommandNumber parse_find_statement(tok* token, int len, int* param_start);
static CommandNumber parse_count_statement(tok* token, int len, int* param_start);
static CommandNumber parse_flip_statement(tok* token, int len, int* param_start);
static CommandNumber parse_shift_statement(tok* token, int len, int* param_start);
static CommandNumber parse_system_statement(tok* token, int len, int* param_start);
static CommandNumber parse_macro_statement(tok* token, int len, int* param_start);
static int is_local_command(CommandNumber cmd);
static int process_local_command(CommandNumber cmd, tok* params, int param_count);

static int distinguish_token_type(tok* token){
    /*
    根据token的content判断token的类型
    getoken函数只会进行基本的token类型判断,返回的token部分有类型判断：
    1.TOKEN_EEROR:表示语法错误-->保持原样,但返回err
    2.TOKEN_END_STMT:表示语句结束符-->保持原样,返回0
    3.TOKEN_UNKNOWN:表示未知token-->判断是否是关键字,如果是关键字,则写入token->type。否则,写入TOKEN_NAME
    4.TOKEN_VALUES:表示值-->不做处理,返回0 
    5.TOKEN_SYMBOL:表示符号-->判断符号类型
    */
    if(token == NULL || token->type == TOKEN_EEROR){
        return merr;
    }
    if(token->type == TOKEN_END_STMT){
        str_free(&token->content);
        return 0;
    }
    if(token->type == TOKEN_VALUES || token->type == TOKEN_NUM){
        return 0;
    }
    if(token->type == TOKEN_SYMBOL){
       // 由于关键字地图中的符号是连续的,所以先扫描整个关键字表找到第一个符号,然后再往后对比是否有匹配的符号
        for(int i = 0;i < sizeof(keyword_map)/sizeof(keyword_map[0]);i++){
            // 上面已经规定">"符号是关键字地图中的第一个符号
            if(keyword_map[i].type == TOKEN_DY){
                // 说明找到了第一个">"符号
                for(int j = 0;j < SYMBOL_NUM;j++){
                    if(strncmp(token->content.string,keyword_map[i+j].keyword,token->content.len) == 0){
                        // 说明找到了匹配的符号
                        token->type = keyword_map[i+j].type;
                        return 0;
                    }
                }
                // 说明没有找到匹配的符号
                token->type = TOKEN_EEROR;
                str_free(&token->content);
                return merr;
            }
        }
    }
    if(token->type == TOKEN_UNKNOWN){
        // 判断是否是关键字
        for(int i = 0;i < sizeof(keyword_map)/sizeof(keyword_map[0]);i++){
            if(strncmp(token->content.string,keyword_map[i].keyword,token->content.len) == 0){
                // 说明是关键字
                token->type = keyword_map[i].type;
                return 0; 
            }  
        }
        // 说明不是关键字
        token->type = TOKEN_NAME;
        return 0;
    }
    return 0;
}

static tok* get_token(inputstr* instr){
    /*
    功能:从pos位置开始，截取下一个token并返回，并做token类型判断
    请保证instr合法,pos必须在string的合法范围内

    返回值:
    成功:返回token指针
    失败/字符串结束:返回TOKEN_EEROR
    instr结束:返回NULL
    */
    tok *token = getoken(instr);
    if(token == NULL){
        return NULL;
    }
    distinguish_token_type(token);
    return token;
}

// 新的语句类型识别函数
CommandNumber distinguish_stmt_type(tok* token, const int len, int* param_start){
    if(token == NULL || param_start == NULL || len < 1){
        return CMD_ERROR;
    }
    
    *param_start = -1; // 默认无参数
    
    // 基础语法识别
    switch(token[0].type){
        case TOKEN_GET:
            return parse_get_statement(token, len, param_start);
        case TOKEN_WHERE:
            if(len == 1){
                return CMD_WHERE;
            }
            break;
        case TOKEN_DESC:
            if(len == 1){
                return CMD_DESC;
            }
            break;
        case TOKEN_HOOK:
            return parse_hook_statement(token, len, param_start);
        case TOKEN_RANK:
            if(len == 3 && token[1].type == TOKEN_NAME && token[2].type == TOKEN_NUM){
                *param_start = 1;
                return CMD_RANK_SET;
            }
            break;
        case TOKEN_LOCK:
            if(len == 3 && token[1].type == TOKEN_NAME && token[2].type == TOKEN_NUM){
                *param_start = 1;
                return CMD_LOCK;
            }
            break;
        case TOKEN_UNLOCK:
            if(len == 2 && token[1].type == TOKEN_NAME){
                *param_start = 1;
                return CMD_UNLOCK;
            }
            break;
        case TOKEN_EXPORT:
            if(len == 3 && token[1].type == TOKEN_NAME && token[2].type == TOKEN_NAME){
                *param_start = 1;
                return CMD_EXPORT;
            }
            break;
        case TOKEN_IMPORT:
            if(len == 4 && token[1].type == TOKEN_NAME && token[2].type == TOKEN_NAME && token[3].type == TOKEN_VALUES){
                *param_start = 1;
                return CMD_IMPORT;
            }
            break;
        case TOKEN_CHMOD:
            if(len == 3 && token[1].type == TOKEN_NAME && token[2].type == TOKEN_NAME){
                *param_start = 1;
                return CMD_CHMOD;
            }
            break;
        case TOKEN_WAIT:
            if(len == 2 && token[1].type == TOKEN_NUM){
                *param_start = 1;
                return CMD_WAIT;
            }
            break;
        case TOKEN_MULTI:
            if(len == 1){
                return CMD_MULTI;
            }
            break;
        case TOKEN_EXEC:
            if(len == 1){
                return CMD_EXEC;
            }
            break;
        case TOKEN_ASYNC:
            if(len >= 2){
                *param_start = 1;
                return CMD_ASYNC;
            }
            break;
        case TOKEN_SYNC:
            if(len == 1){
                return CMD_SYNC;
            }
            break;
        case TOKEN_FIELD:
            return parse_field_statement(token, len, param_start);
        case TOKEN_ADD:
            if(len >= 2){
                *param_start = 1;
                return CMD_ADD_RECORD;
            }
            break;
        case TOKEN_INSERT:
            if(len >= 3 && token[1].type == TOKEN_NUM){
                *param_start = 1;
                return CMD_INSERT_RECORD;
            }
            break;
        case TOKEN_SET:
            return parse_set_statement(token, len, param_start);
        case TOKEN_DEL:
            return parse_del_statement(token, len, param_start);
        case TOKEN_SWAP:
            return parse_swap_statement(token, len, param_start);
        case TOKEN_EXISTS:
            if(len >= 2){
                *param_start = 1;
                return CMD_EXISTS;
            }
            break;
        case TOKEN_SELECT:
            if(len == 2 && token[1].type == TOKEN_VALUES){
                *param_start = 1;
                return CMD_SELECT;
            }
            break;
        case TOKEN_KEY:
            return parse_key_statement(token, len, param_start);
        case TOKEN_COPY:
            if(len == 3 && token[1].type == TOKEN_NAME && token[2].type == TOKEN_NAME){
                *param_start = 1;
                return CMD_COPY_KEY;
            }
            break;
        case TOKEN_RENAME:
            if(len == 3 && token[1].type == TOKEN_NAME && token[2].type == TOKEN_NAME){
                *param_start = 1;
                return CMD_RENAME_KEY;
            }
            break;
        case TOKEN_EXPIRE:
            if(len == 3 && token[1].type == TOKEN_NAME && token[2].type == TOKEN_NUM){
                *param_start = 1;
                return CMD_EXPIRE;
            }
            break;
        case TOKEN_PERSIST:
            if(len == 2 && token[1].type == TOKEN_NAME){
                *param_start = 1;
                return CMD_PERSIST;
            }
            break;
        case TOKEN_TTL:
            if(len == 2 && token[1].type == TOKEN_NAME){
                *param_start = 1;
                return CMD_TTL;
            }
            break;
        case TOKEN_INCR:
            if(len == 2 && token[1].type == TOKEN_NAME){
                *param_start = 1;
                return CMD_INCR;
            } else if(len == 3 && token[1].type == TOKEN_NAME && token[2].type == TOKEN_NUM){
                *param_start = 1;
                return CMD_INCR_BY;
            }
            break;
        case TOKEN_DECR:
            if(len == 2 && token[1].type == TOKEN_NAME){
                *param_start = 1;
                return CMD_DECR;
            } else if(len == 3 && token[1].type == TOKEN_NAME && token[2].type == TOKEN_NUM){
                *param_start = 1;
                return CMD_DECR_BY;
            }
            break;
        case TOKEN_APPEND:
            return parse_append_statement(token, len, param_start);
        case TOKEN_LPUSH:
            if(len == 2 && token[1].type == TOKEN_VALUES){
                *param_start = 1;
                return CMD_LPUSH;
            }
            break;
        case TOKEN_RPUSH:
            if(len == 2 && token[1].type == TOKEN_VALUES){
                *param_start = 1;
                return CMD_RPUSH;
            }
            break;
        case TOKEN_LPOP:
            if(len == 1){
                return CMD_LPOP;
            }
            break;
        case TOKEN_RPOP:
            if(len == 1){
                return CMD_RPOP;
            }
            break;
        case TOKEN_CLEAR:
            if(len == 1){
                return CMD_CLEAR_STREAM; // 根据上下文确定具体类型
            }
            break;
        case TOKEN_MERGE:
            if(len == 2 && token[1].type == TOKEN_NAME){
                *param_start = 1;
                return CMD_MERGE_STREAM;
            }
            break;
        case TOKEN_REVERSE:
            if(len == 1){
                return CMD_REVERSE_LIST;
            }
            break;
        case TOKEN_SORT:
            return parse_sort_statement(token, len, param_start);
        case TOKEN_UNIQUE:
            if(len == 1){
                return CMD_UNIQUE_LIST;
            }
            break;
        case TOKEN_FIND:
            return parse_find_statement(token, len, param_start);
        case TOKEN_COUNT:
            return parse_count_statement(token, len, param_start);
        case TOKEN_JOIN:
            if(len == 2 && token[1].type == TOKEN_NAME){
                *param_start = 1;
                return CMD_JOIN_LIST; // 根据上下文确定具体类型
            }
            break;
        case TOKEN_FLIP:
            return parse_flip_statement(token, len, param_start);
        case TOKEN_FILL:
            if(len == 2 && token[1].type == TOKEN_VALUES){
                *param_start = 1;
                return CMD_FILL_BIT;
            }
            break;
        case TOKEN_AND:
            if(len == 2 && token[1].type == TOKEN_NAME){
                *param_start = 1;
                return CMD_AND_BIT;
            }
            break;
        case TOKEN_OR:
            if(len == 2 && token[1].type == TOKEN_NAME){
                *param_start = 1;
                return CMD_OR_BIT;
            }
            break;
        case TOKEN_XOR:
            if(len == 2 && token[1].type == TOKEN_NAME){
                *param_start = 1;
                return CMD_XOR_BIT;
            }
            break;
        case TOKEN_NOT:
            if(len == 1){
                return CMD_NOT_BIT;
            }
            break;
        case TOKEN_SHIFT:
            return parse_shift_statement(token, len, param_start);
        case TOKEN_RESIZE:
            if(len == 2 && token[1].type == TOKEN_NUM){
                *param_start = 1;
                return CMD_RESIZE_BIT;
            }
            break;
        case TOKEN_SYSTEM:
            return parse_system_statement(token, len, param_start);
        case TOKEN_BACKUP:
            if(len == 2 && token[1].type == TOKEN_NAME){
                *param_start = 1;
                return CMD_BACKUP_OBJ;
            }
            break;
        case TOKEN_ZS:
            if(len == 3 && token[1].type == TOKEN_NAME && token[2].type == TOKEN_NUM){
                *param_start = 1;
                return CMD_COMPRESS;
            }
            break;
        case TOKEN_MACRO:
            return parse_macro_statement(token, len, param_start);
        case TOKEN_IF:
            return CMD_IF; // 本地处理
        case TOKEN_ELSE:
            return CMD_ELSE; // 本地处理
        case TOKEN_ELIF:
            return CMD_ELIF; // 本地处理
        case TOKEN_WHILE:
            return CMD_WHILE; // 本地处理
        case TOKEN_FOR:
            return CMD_FOR; // 本地处理
        case TOKEN_END:
            return CMD_END; // 本地处理
        case TOKEN_BREAK:
            return CMD_BREAK; // 本地处理
        case TOKEN_CONTINUE:
            return CMD_CONTINUE; // 本地处理
        default:
            return CMD_ERROR;
    }
    
    return CMD_ERROR;
}

// 辅助解析函数实现

// HUJI协议格式生成函数
str generate_huji_protocol(CommandNumber cmd, tok* params, int param_count) {
    str result;
    str_init(&result);
    
    // 添加HUJI协议头
    str_append_string(&result, "HUJI");
    
    // 添加命令编号（4字节网络字节序）
    uint32_t cmd_net = htonl((uint32_t)cmd);
    str_append_data(&result, (char*)&cmd_net, 4);
    
    // 构建参数数据流
    str param_stream;
    str_init(&param_stream);
    
    // 参数数目（1字节）
    uint8_t param_count_byte = (uint8_t)param_count;
    str_append_data(&param_stream, (char*)&param_count_byte, 1);
    str_append_string(&param_stream, "@");
    
    // 添加每个参数
    for(int i = 0; i < param_count; i++) {
        if(params[i].type == TOKEN_VALUES || params[i].type == TOKEN_NAME || 
           params[i].type == TOKEN_NUM || params[i].type >= TOKEN_TABLE) {
            
            // 参数长度（字符串数字）
            char len_str[32];
            snprintf(len_str, sizeof(len_str), "%d", params[i].content.len);
            str_append_string(&param_stream, len_str);
            str_append_string(&param_stream, "@");
            
            // 参数内容
            str_append_data(&param_stream, params[i].content.string, params[i].content.len);
            str_append_string(&param_stream, "@");
        }
    }
    
    // 添加参数流到结果
    str_append_data(&result, param_stream.string, param_stream.len);
    
    str_free(&param_stream);
    return result;
}

// 修复并完善lexer函数
str lexer(char* string, int len) {
    str result;
    str_init(&result);
    
    // 先把string转化为inputstr
    inputstr instr;
    instr.string = (uint8_t*)string;
    instr.len = len;
    instr.pos = (uint8_t*)string;

    tok *tokens = NULL;
    uint32_t token_count = 0;
    tok* current_token = NULL;
    
    // 第一步：解析所有token
    while((current_token = getoken(&instr)) != NULL) {
        if(current_token->type == TOKEN_EEROR) {
            // 语法错误，释放内存并返回空结果
            for(uint32_t i = 0; i < token_count; i++) {
                str_free(&tokens[i].content);
            }
            free(tokens);
            str_free(&current_token->content);
            free(current_token);
            str_free(&result);
            str_init(&result);
            return result;
        }
        
        // 扩展token数组
        tok* new_tokens = realloc(tokens, (token_count + 1) * sizeof(tok));
        if(new_tokens == NULL) {
            // 内存分配失败
            for(uint32_t i = 0; i < token_count; i++) {
                str_free(&tokens[i].content);
            }
            free(tokens);
            str_free(&current_token->content);
            free(current_token);
            str_free(&result);
            str_init(&result);
            return result;
        }
        
        tokens = new_tokens;
        distinguish_token_type(current_token); // 识别token类型
        tokens[token_count] = *current_token;
        token_count++;
        free(current_token); // 释放临时token结构
    }
    
    // 检查最后一个token是否是语句结束符
    if(token_count == 0 || tokens[token_count-1].type != TOKEN_END_STMT) {
        // 语法错误
        for(uint32_t i = 0; i < token_count; i++) {
            str_free(&tokens[i].content);
        }
        free(tokens);
        str_free(&result);
        str_init(&result);
        return result;
    }
    
    // 第二步：按语句分析并生成HUJI协议
    int stmt_start = 0;
    while(stmt_start < token_count) {
        // 找到当前语句的结束位置
        int stmt_end = stmt_start;
        while(stmt_end < token_count && tokens[stmt_end].type != TOKEN_END_STMT) {
            stmt_end++;
        }
        
        if(stmt_end >= token_count) {
            // 没有找到语句结束符
            break;
        }
        
        int stmt_len = stmt_end - stmt_start;
        if(stmt_len > 0) {
            // 识别语句类型
            int param_start = -1;
            CommandNumber cmd = distinguish_stmt_type(&tokens[stmt_start], stmt_len, &param_start);
            
            if(cmd == CMD_ERROR) {
                // 语法错误，跳过这个语句
                stmt_start = stmt_end + 1;
                continue;
            }
            
            // 检查是否是本地处理的命令
            if(is_local_command(cmd)) {
                // 提取参数
                tok* params = NULL;
                int param_count = 0;
                
                if(param_start >= 0 && param_start < stmt_len) {
                    param_count = stmt_len - param_start;
                    params = &tokens[stmt_start + param_start];
                }
                
                // 本地处理
                process_local_command(cmd, params, param_count);
                stmt_start = stmt_end + 1;
                continue;
            }
            
            // 提取参数
            tok* params = NULL;
            int param_count = 0;
            
            if(param_start >= 0 && param_start < stmt_len) {
                param_count = stmt_len - param_start;
                params = &tokens[stmt_start + param_start];
            }
            
            // 生成HUJI协议数据
            str protocol_data = generate_huji_protocol(cmd, params, param_count);
            
            // 添加到结果中
            str_append_data(&result, protocol_data.string, protocol_data.len);
            
            str_free(&protocol_data);
        }
        
        stmt_start = stmt_end + 1;
    }
    
    // 释放所有token
    for(uint32_t i = 0; i < token_count; i++) {
        str_free(&tokens[i].content);
    }
    free(tokens);
    
    return result;
}

// 修复getoken函数中的TOKEN_END处理
// 需要修改getoken函数中的TOKEN_END为TOKEN_END_STMT
// 在getoken函数中查找并替换所有TOKEN_END为TOKEN_END_STMT

// 辅助解析函数实现
static CommandNumber parse_get_statement(tok* token, int len, int* param_start) {
    if(len == 1) {
        return CMD_GET_OBJECT;
    }
    
    if(len == 2) {
        if(token[1].type == TOKEN_LEN) {
            return CMD_GET_STREAM_LEN;
        } else if(token[1].type == TOKEN_COUNT) {
            return CMD_GET_COUNT;
        } else if(token[1].type == TOKEN_KEYS) {
            return CMD_GET_KEYS;
        } else if(token[1].type == TOKEN_NAME) {
            *param_start = 1;
            return CMD_GET_KEY;
        } else if(token[1].type == TOKEN_NUM) {
            *param_start = 1;
            return CMD_GET_LIST;
        }
    }
    
    if(len == 3) {
        if(token[1].type == TOKEN_RANK && token[2].type == TOKEN_NAME) {
            *param_start = 2;
            return CMD_GET_RANK;
        } else if(token[1].type == TOKEN_TYPE && token[2].type == TOKEN_NAME) {
            *param_start = 2;
            return CMD_GET_TYPE;
        } else if(token[1].type == TOKEN_CHMOD && token[2].type == TOKEN_NAME) {
            *param_start = 2;
            return CMD_GET_CHMOD;
        } else if(token[1].type == TOKEN_FIELD && token[2].type == TOKEN_NAME) {
            *param_start = 2;
            return CMD_GET_FIELD;
        } else if(token[1].type == TOKEN_NUM && token[2].type == TOKEN_NUM) {
            *param_start = 1;
            return CMD_GET_STREAM;
        }
    }
    
    if(len >= 4 && token[1].type == TOKEN_POS) {
        // [GET POS x1 y1 x2 y2 ...]
        *param_start = 2;
        return CMD_GET_POS;
    }
    
    if(len >= 3 && token[1].type == TOKEN_WHERE) {
        *param_start = 2;
        return CMD_GET_WHERE;
    }
    
    return CMD_ERROR;
}

static CommandNumber parse_hook_statement(tok* token, int len, int* param_start) {
    if(len == 1) {
        return CMD_HOOK_ROOT;
    }
    
    if(len == 2 && token[1].type == TOKEN_NAME) {
        *param_start = 1;
        return CMD_HOOK_SWITCH;
    }
    
    if(len >= 3) {
        if(token[1].type == TOKEN_TABLE || token[1].type == TOKEN_KVALOT ||
           token[1].type == TOKEN_LIST || token[1].type == TOKEN_BITMAP ||
           token[1].type == TOKEN_STREAM) {
            *param_start = 1;
            return CMD_HOOK_CREATE;
        } else if(token[1].type == TOKEN_DEL) {
            *param_start = 2;
            return CMD_HOOK_DEL;
        } else if(token[1].type == TOKEN_CLEAR) {
            *param_start = 2;
            return CMD_HOOK_CLEAR;
        } else if(token[1].type == TOKEN_COPY && len == 3) {
            *param_start = 2;
            return CMD_HOOK_COPY;
        } else if(token[1].type == TOKEN_SWAP && len == 3) {
            *param_start = 2;
            return CMD_HOOK_SWAP;
        } else if(token[1].type == TOKEN_MERGE && len == 3) {
            *param_start = 2;
            return CMD_HOOK_MERGE;
        } else if(token[1].type == TOKEN_RENAME && len == 3) {
            *param_start = 2;
            return CMD_HOOK_RENAME;
        } else if(token[1].type == TOKEN_JOIN && len == 4) {
            *param_start = 2;
            return CMD_HOOK_JOIN;
        }
    }
    
    return CMD_ERROR;
}

static CommandNumber parse_field_statement(tok* token, int len, int* param_start) {
    if(len < 2) return CMD_ERROR;
    
    if(token[1].type == TOKEN_ADD && len >= 4) {
        *param_start = 2;
        return CMD_FIELD_ADD;
    } else if(token[1].type == TOKEN_INSERT && len >= 5) {
        *param_start = 2;
        return CMD_FIELD_INSERT;
    } else if(token[1].type == TOKEN_SWAP && len == 4) {
        *param_start = 2;
        return CMD_FIELD_SWAP;
    } else if(token[1].type == TOKEN_DEL && len >= 3) {
        *param_start = 2;
        return CMD_FIELD_DEL;
    } else if(token[1].type == TOKEN_SET && len >= 3) {
        if(token[2].type == TOKEN_ATTRIBUTE && len >= 4) {
            *param_start = 3;
            return CMD_FIELD_SET_ATTR;
        }
    } else if(token[1].type == TOKEN_GET && len >= 3) {
        if(token[2].type == TOKEN_INFO && len >= 4) {
            *param_start = 3;
            return CMD_FIELD_GET_INFO;
        }
    }
    
    return CMD_ERROR;
}

static CommandNumber parse_set_statement(tok* token, int len, int* param_start) {
    if(len < 2) return CMD_ERROR;
    
    if(token[1].type == TOKEN_TYPE && len >= 4) {
        *param_start = 2;
        return CMD_SET_TYPE;
    } else if(len == 3 && token[1].type == TOKEN_NAME && token[2].type == TOKEN_VALUES) {
        *param_start = 1;
        return CMD_SET_KEY;
    } else if(len >= 4 && token[1].type == TOKEN_WHERE) {
        *param_start = 2;
        return CMD_SET_WHERE;
    } else if(len >= 3) {
        // 其他SET语句，如设置记录、流、位图等
        if(token[1].type == TOKEN_NUM) {
            *param_start = 1;
            if(len == 3) {
                return CMD_SET_STREAM; // [SET pos value]
            } else if(len == 4) {
                return CMD_SET_STREAM_LEN; // [SET pos len char]
            } else {
                return CMD_SET_RECORD; // [SET line_index field_index value ...]
            }
        }
    }
    
    return CMD_ERROR;
}

static CommandNumber parse_del_statement(tok* token, int len, int* param_start) {
    if(len < 2) return CMD_ERROR;
    
    if(token[1].type == TOKEN_WHERE) {
        *param_start = 2;
        return CMD_DEL_WHERE;
    } else if(len >= 2) {
        *param_start = 1;
        if(token[1].type == TOKEN_NUM) {
            return CMD_DEL_RECORD; // TABLE类或其他数值删除
        } else {
            return CMD_DEL_KEY; // 键删除
        }
    }
    
    return CMD_ERROR;
}

static CommandNumber parse_swap_statement(tok* token, int len, int* param_start) {
    if(len == 3) {
        *param_start = 1;
        if(token[1].type == TOKEN_NUM && token[2].type == TOKEN_NUM) {
            return CMD_SWAP_RECORD; // TABLE类记录交换
        } else if(token[1].type == TOKEN_NAME && token[2].type == TOKEN_NAME) {
            return CMD_SWAP_KEY; // 键交换
        }
    }
    
    return CMD_ERROR;
}

static CommandNumber parse_key_statement(tok* token, int len, int* param_start) {
    if(len < 2) return CMD_ERROR;
    
    if(token[1].type == TOKEN_NAME) {
        *param_start = 1;
        return CMD_KEY_ENTER;
    } else if(len >= 4 && token[1].type == TOKEN_JOIN) {
        *param_start = 2;
        return CMD_KEY_JOIN;
    }
    
    return CMD_ERROR;
}

static CommandNumber parse_append_statement(tok* token, int len, int* param_start) {
    if(len < 2) return CMD_ERROR;
    
    if(len == 2) {
        *param_start = 1;
        return CMD_APPEND;
    } else if(len == 3) {
        *param_start = 1;
        return CMD_APPEND_POS;
    }
    
    return CMD_ERROR;
}

static CommandNumber parse_sort_statement(tok* token, int len, int* param_start) {
    if(len < 2) return CMD_ERROR;
    
    if(len == 2) {
        *param_start = 1;
        return CMD_SORT_LIST; // LIST类排序
    } else if(len == 3) {
        *param_start = 1;
        return CMD_SORT; // TABLE类排序
    }
    
    return CMD_ERROR;
}

static CommandNumber parse_find_statement(tok* token, int len, int* param_start) {
    if(len < 2) return CMD_ERROR;
    
    if(len == 2) {
        *param_start = 1;
        return CMD_FIND_LIST; // LIST类查找或BITMAP查找
    } else if(len == 3) {
        *param_start = 1;
        return CMD_FIND_BIT_START; // BITMAP从指定位置查找
    }
    
    return CMD_ERROR;
}

static CommandNumber parse_count_statement(tok* token, int len, int* param_start) {
    if(len == 1) {
        return CMD_COUNT_BIT_ALL; // 统计所有位
    } else if(len == 2) {
        *param_start = 1;
        return CMD_COUNT_LIST; // LIST类统计
    } else if(len == 3) {
        *param_start = 1;
        return CMD_COUNT_BIT; // BITMAP范围统计
    }
    
    return CMD_ERROR;
}

static CommandNumber parse_flip_statement(tok* token, int len, int* param_start) {
    if(len < 2) return CMD_ERROR;
    
    if(len == 2) {
        *param_start = 1;
        return CMD_FLIP_BIT;
    } else if(len == 3) {
        *param_start = 1;
        return CMD_FLIP_BIT_RANGE;
    }
    
    return CMD_ERROR;
}

static CommandNumber parse_shift_statement(tok* token, int len, int* param_start) {
    if(len < 3) return CMD_ERROR;
    
    if(len == 3) {
        *param_start = 1;
        return CMD_SHIFT_BIT;
    } else if(len == 4 && token[3].type == TOKEN_MATHS) {
        *param_start = 1;
        return CMD_SHIFT_BIT_MATH;
    }
    
    return CMD_ERROR;
}

static CommandNumber parse_system_statement(tok* token, int len, int* param_start) {
    if(len < 2) return CMD_ERROR;
    
    if(token[1].type == TOKEN_INFO && len == 2) {
        return CMD_SYSTEM_INFO;
    } else if(token[1].type == TOKEN_STATUS && len == 2) {
        return CMD_SYSTEM_STATUS;
    } else if(token[1].type == TOKEN_REGISTER && len == 2) {
        return CMD_SYSTEM_REGISTER;
    } else if(token[1].type == TOKEN_CLEANUP && len == 2) {
        return CMD_SYSTEM_CLEANUP;
    } else if(token[1].type == TOKEN_BACKUP && len == 3) {
        *param_start = 2;
        return CMD_SYSTEM_BACKUP;
    } else if(token[1].type == TOKEN_RESTORE && len == 3) {
        *param_start = 2;
        return CMD_SYSTEM_RESTORE;
    } else if(token[1].type == TOKEN_LOG && len == 3) {
        *param_start = 2;
        return CMD_SYSTEM_LOG;
    }
    
    return CMD_ERROR;
}

static CommandNumber parse_macro_statement(tok* token, int len, int* param_start) {
    if(len < 2) return CMD_ERROR;
    
    // 处理宏变量语法 [$var_name value;]
    if(token[0].type == TOKEN_MACRO) {
        if(len == 3) {
            *param_start = 1;
            return CMD_SET_VAR;
        }
    }
    
    return CMD_ERROR;
}

// 本地变量管理
typedef struct {
    str name;
    str value;
} Variable;

static Variable* local_vars = NULL;
static int var_count = 0;
static int var_capacity = 0;

// 条件控制状态管理
typedef struct {
    int condition_depth;
    int loop_depth;
    int if_stack[32];     // IF语句栈
    int loop_stack[32];   // 循环语句栈
} ControlState;

static ControlState control_state = {0, 0, {0}, {0}};

// 本地变量操作函数
int set_local_variable(const char* name, const char* value) {
    // 查找是否已存在
    for(int i = 0; i < var_count; i++) {
        if(strncmp(local_vars[i].name.string, name, local_vars[i].name.len) == 0) {
            // 更新现有变量
            str_free(&local_vars[i].value);
            str_init(&local_vars[i].value);
            str_append_string(&local_vars[i].value, value);
            return 0;
        }
    }
    
    // 添加新变量
    if(var_count >= var_capacity) {
        var_capacity = var_capacity == 0 ? 8 : var_capacity * 2;
        Variable* new_vars = realloc(local_vars, var_capacity * sizeof(Variable));
        if(!new_vars) return -1;
        local_vars = new_vars;
    }
    
    str_init(&local_vars[var_count].name);
    str_init(&local_vars[var_count].value);
    str_append_string(&local_vars[var_count].name, name);
    str_append_string(&local_vars[var_count].value, value);
    var_count++;
    
    return 0;
}

const char* get_local_variable(const char* name) {
    for(int i = 0; i < var_count; i++) {
        if(strncmp(local_vars[i].name.string, name, local_vars[i].name.len) == 0) {
            return local_vars[i].value.string;
        }
    }
    return NULL;
}

int delete_local_variable(const char* name) {
    for(int i = 0; i < var_count; i++) {
        if(strncmp(local_vars[i].name.string, name, local_vars[i].name.len) == 0) {
            str_free(&local_vars[i].name);
            str_free(&local_vars[i].value);
            // 移动后面的变量
            for(int j = i; j < var_count - 1; j++) {
                local_vars[j] = local_vars[j + 1];
            }
            var_count--;
            return 0;
        }
    }
    return -1; // 变量不存在
}

// 本地命令处理函数
int process_local_command(CommandNumber cmd, tok* params, int param_count) {
    switch(cmd) {
        case CMD_SET_VAR:
            if(param_count >= 2) {
                return set_local_variable(params[0].content.string, params[1].content.string);
            }
            break;
            
        case CMD_GET_VAR:
            if(param_count >= 1) {
                const char* value = get_local_variable(params[0].content.string);
                if(value) {
                    printf("%s\n", value);
                    return 0;
                }
            }
            break;
            
        case CMD_DEL_VAR:
            if(param_count >= 1) {
                return delete_local_variable(params[0].content.string);
            }
            break;
            
        case CMD_IF:
            // IF条件处理 - 这里简化处理，实际应该解析条件表达式
            control_state.if_stack[control_state.condition_depth] = 1; // 假设条件为真
            control_state.condition_depth++;
            return 0;
            
        case CMD_ELSE:
            if(control_state.condition_depth > 0) {
                control_state.if_stack[control_state.condition_depth - 1] = 
                    !control_state.if_stack[control_state.condition_depth - 1];
            }
            return 0;
            
        case CMD_ELIF:
            // ELIF处理 - 简化处理
            if(control_state.condition_depth > 0) {
                control_state.if_stack[control_state.condition_depth - 1] = 0; // 简化为假
            }
            return 0;
            
        case CMD_END:
            if(control_state.condition_depth > 0) {
                control_state.condition_depth--;
            } else if(control_state.loop_depth > 0) {
                control_state.loop_depth--;
            }
            return 0;
            
        case CMD_WHILE:
            // WHILE循环处理 - 简化处理
            control_state.loop_stack[control_state.loop_depth] = 1;
            control_state.loop_depth++;
            return 0;
            
        case CMD_FOR:
            // FOR循环处理 - 简化处理
            control_state.loop_stack[control_state.loop_depth] = 1;
            control_state.loop_depth++;
            return 0;
            
        case CMD_BREAK:
            // 跳出循环
            if(control_state.loop_depth > 0) {
                control_state.loop_stack[control_state.loop_depth - 1] = 0;
            }
            return 0;
            
        case CMD_CONTINUE:
            // 继续循环
            return 0;
            
        default:
            return -1; // 不是本地命令
    }
    
    return -1;
}

// 检查命令是否需要本地处理
int is_local_command(CommandNumber cmd) {
    return (cmd >= CMD_IF && cmd <= CMD_CONTINUE) || 
           (cmd >= CMD_SET_VAR && cmd <= CMD_DEL_VAR);
}

// 资源清理函数
void cleanup_local_resources() {
    for(int i = 0; i < var_count; i++) {
        str_free(&local_vars[i].name);
        str_free(&local_vars[i].value);
    }
    free(local_vars);
    local_vars = NULL;
    var_count = 0;
    var_capacity = 0;
    
    // 重置控制状态
    control_state.condition_depth = 0;
    control_state.loop_depth = 0;
}