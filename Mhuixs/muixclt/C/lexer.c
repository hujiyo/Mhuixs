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

typedef enum {
    TOKEN_HOOK,         //引用：钩子
    TOKEN_KEY,          //引用：键

    TOKEN_TABLE,        //表类型
    TOKEN_KVALOT,       //键库类型
    TOKEN_LIST,         //列表类型
    TOKEN_BITMAP,       //位图类型
    TOKEN_STREAM,       //流类型

    TOKEN_DEL,       // 删除
    TOKEN_GET,          // 获取
    TOKEN_RENAME,       // 重命名
    TOKEN_CLEAR,       // 清空
    TOKEN_INSERT,       // 插入
    TOKEN_SELECT,       // 选择
    TOKEN_SWAP,       // 交换
    TOKEN_SET,          // 设置
    TOKEN_ADD,          // 添加
    TOKEN_LPOP,       // 左弹出
    TOKEN_RPOP,       // 右弹出
    TOKEN_LPUSH,       // 左压入
    TOKEN_RPUSH,       // 右压入
    TOKEN_APPEND,       // 追加
    TOKEN_COUNT,       // 计数

    TOKEN_POS,       // 位置
    TOKEN_DESC,       // 描述
    TOKEN_WHERE,       // 条件
    TOKEN_TYPE,       // 类型
    TOKEN_EXISTS,       // 存在
    TOKEN_TEMP,       // 临时
    TOKEN_BACK,

    TOKEN_i1,
    TOKEN_i2,
    TOKEN_i4,
    TOKEN_i8,
    TOKEN_ui1,
    TOKEN_ui2,
    TOKEN_ui4,
    TOKEN_ui8,
    TOKEN_f4,
    TOKEN_f8,
    TOKEN_str,
    TOKEN_date,
    TOKEN_time,
    TOKEN_datetime,

    TOKEN_PKEY,         // 主键
    TOKEN_FKEY,         // 外键
    TOKEN_UNIQUE,       // 唯一
    TOKEN_NOTNULL,       // 非空
    TOKEN_DEFAULT,       // 默认值


    TOKEN_ATTRIBUTE,       // 属性
    
    TOKEN_FIELD,        // 字段
    TOKEN_RANK,         // 保护等级
    TOKEN_LEN,         // 长度

    TOKEN_NAME,       // 名字

    TOKEN_VALUES,       // 数据值
    TOKEN_NUM,         // 连接

    TOKEN_END,          // 结束符：语句结束符，即';'

    TOKEN_EEROR,         // token错误,一般遇到这个token,则视为用户 语句错误

    TOKEN_UNKNOWN,         // 未判断

    TOKEN_SYMBOL,         // 符号
    TOKEN_DY,         // 符号：大于号
    TOKEN_XY,         // 符号：小于号
    TOKEN_DYDY,         // 符号：大于等于号
    TOKEN_XYDY,         // 符号：小于等于号
    TOKEN_DDY,         // 符号：等于号
    TOKEN_BDY,         // 符号：不等于号
    TOKEN_YDY,         // 符号：模糊匹配
    TOKEN_BJ,         // 符号:并集
    TOKEN_JJ,         // 符号:交集
} TokType,toktype;

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

    /*
    typedef struct inputstr{
        uint8_t *string;//待处理的字符串
        uint32_t len;
        uint8_t *pos;//指向string中待处理的字符
    }inputstr;
    */
   
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
                token->type = TOKEN_END;
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
    {"i1", TOKEN_i1},{"i2", TOKEN_i2},{"i4", TOKEN_i4},{"i8", TOKEN_i8},
    {"int8_t", TOKEN_i1},{"int16_t", TOKEN_i2},{"int32_t", TOKEN_i4},{"int64_t", TOKEN_i8},

    {"ui1", TOKEN_ui1},{"ui2", TOKEN_ui2},{"ui4", TOKEN_ui4},{"ui8", TOKEN_ui8},
    {"uint8_t", TOKEN_ui1},{"uint16_t", TOKEN_ui2},{"uint32_t", TOKEN_ui4},{"uint64_t", TOKEN_ui8},

    {"f4", TOKEN_f4},{"FLOAT", TOKEN_f4},{"float", TOKEN_f4}, 
    {"f8", TOKEN_f8},{"DOUBLE", TOKEN_f8},{"double", TOKEN_f8},

    {"KEY", TOKEN_KEY}, {"key", TOKEN_KEY},
    {"DEL", TOKEN_DEL}, {"del", TOKEN_DEL},
    {"GET", TOKEN_GET}, {"get", TOKEN_GET},
    {"SET", TOKEN_SET}, {"set", TOKEN_SET},
    {"ADD", TOKEN_ADD}, {"add", TOKEN_ADD},
    {"LEN", TOKEN_LEN}, {"len", TOKEN_LEN},
    {"POS", TOKEN_POS}, {"pos", TOKEN_POS},
    {"BACK", TOKEN_BACK}, {"back", TOKEN_BACK},
    {"TYPE", TOKEN_TYPE}, {"type", TOKEN_TYPE},
    {"HOOK", TOKEN_HOOK}, {"hook", TOKEN_HOOK},
    {"LIST", TOKEN_LIST}, {"List", TOKEN_LIST},
    {"SWAP", TOKEN_SWAP}, {"swap", TOKEN_SWAP},
    {"LPOP", TOKEN_LPOP}, {"lpop", TOKEN_LPOP},
    {"RPOP", TOKEN_RPOP}, {"rpop", TOKEN_RPOP},
    {"DESC", TOKEN_DESC}, {"desc", TOKEN_DESC},
    {"PKEY", TOKEN_PKEY}, {"pkey", TOKEN_PKEY},
    {"FKEY", TOKEN_FKEY}, {"fkey", TOKEN_FKEY},
    {"RANK", TOKEN_RANK}, {"rank", TOKEN_RANK},
    {"TEMP", TOKEN_TEMP}, {"temp", TOKEN_TEMP},
    {"TABLE", TOKEN_TABLE}, {"table", TOKEN_TABLE},
    {"COUNT", TOKEN_COUNT}, {"count", TOKEN_COUNT},
    {"CLEAR", TOKEN_CLEAR}, {"clear", TOKEN_CLEAR},
    {"WHERE", TOKEN_WHERE}, {"where", TOKEN_WHERE},
    {"LPUSH", TOKEN_LPUSH}, {"lpush", TOKEN_LPUSH},
    {"RPUSH", TOKEN_RPUSH}, {"rpush", TOKEN_RPUSH},
    {"FIELD", TOKEN_FIELD}, {"field", TOKEN_FIELD},    
    {"BITMAP", TOKEN_BITMAP}, {"bitmap", TOKEN_BITMAP},
    {"STREAM", TOKEN_STREAM}, {"stream", TOKEN_STREAM},
    {"EXISTS", TOKEN_EXISTS}, {"exists", TOKEN_EXISTS},
    {"SELECT", TOKEN_SELECT}, {"select", TOKEN_SELECT},
    {"APPEND", TOKEN_APPEND}, {"append", TOKEN_APPEND},
    {"UNIQUE", TOKEN_UNIQUE}, {"unique", TOKEN_UNIQUE},
    {"KVALOT", TOKEN_KVALOT}, {"kvalot", TOKEN_KVALOT},
    {"INSERT", TOKEN_INSERT}, {"insert", TOKEN_INSERT},
    {"RENAME", TOKEN_RENAME}, {"rename", TOKEN_RENAME},   
    {"DEFAULT", TOKEN_DEFAULT}, {"default", TOKEN_DEFAULT},
    {"NOTNULL", TOKEN_NOTNULL}, {"notnull", TOKEN_NOTNULL},    
    {"ATTRIBUTE", TOKEN_ATTRIBUTE}, {"attribute", TOKEN_ATTRIBUTE},
    /*规定:符号必须在关键字数组中连续排列,且">"放第一个,这对distinguish_token_type函数很重要*/
    {">", TOKEN_DY}, {"<", TOKEN_XY},
    {">=", TOKEN_DYDY}, {"<=", TOKEN_XYDY},    
    {"==", TOKEN_DDY}, {"!=", TOKEN_BDY},
    {"~=", TOKEN_YDY}, {"<>", TOKEN_BJ},
    {"><", TOKEN_JJ}
};

static int distinguish_token_type(tok* token){
    /*
    根据token的content判断token的类型
    getoken函数只会进行基本的token类型判断,返回的token部分有类型判断：
    1.TOKEN_EEROR:表示语法错误-->保持原样,但返回err
    2.TOKEN_END:表示语句结束符-->保持原样,返回0
    3.TOKEN_UNKNOWN:表示未知token-->判断是否是关键字,如果是关键字,则写入token->type。否则,写入TOKEN_NAME
    4.TOKEN_VALUES:表示值-->不做处理,返回0 
    5.TOKEN_SYMBOL:表示符号-->判断符号类型
    */
    if(token == NULL || token->type == TOKEN_EEROR){//函数返回值其实无所谓
        return merr;
    }
    if(token->type == TOKEN_END){
                    str_free(&token->content);//释放token->content
        return 0;
    }
    if(token->type == TOKEN_VALUES || token->type == TOKEN_NUM){
        return 0;
    }
    if(token->type == TOKEN_SYMBOL){
       //由于关键字地图中的符号是连续的,所以先扫描整个关键字表找到第一个符号,然后再往后对比是否有匹配的符号
        #define SYMBOL_NUM 9 //Mhuixs支持的符号总数量
        for(int i = 0;i < sizeof(keyword_map)/sizeof(keyword_map[0]);i++){
            //上面已经规定">"符号是关键字地图中的第一个符号
            if(keyword_map[i].type == TOKEN_DY){
                //说明找到了第一个">"符号
                for(int j = 0;j < SYMBOL_NUM;j++){
                    if(strncmp(token->content.string,keyword_map[i+j].keyword,token->content.len) == 0){
                        //说明找到了匹配的符号
                        token->type = keyword_map[i+j].type;
                        return 0;
                    }
                }
                //说明没有找到匹配的符号
                token->type = TOKEN_EEROR;
                str_free(&token->content);//释放token->content
                return merr;
            }
        }
    }
    if(token->type == TOKEN_UNKNOWN){
        //判断是否是关键字
        for(int i = 0;i < sizeof(keyword_map)/sizeof(keyword_map[0]);i++){
            if(strncmp(token->content.string,keyword_map[i].keyword,token->content.len) == 0){
                //说明是关键字
                token->type = keyword_map[i].type;
                return 0; 
            }  
        }
        //说明不是关键字
        token->type = TOKEN_NAME;
        return 0;
    }
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
    if(token == NULL){//说明instr已经分割完毕了或者instr无法分割（即不合法）
        return NULL;
    }
    distinguish_token_type(token);
    return token;
}

/*
[GET;] #标准 GET_OBJ 语句,获取当前操作对象的所有数据
[WHERE;] #标准 WHERE 语句，返回当前操作对象的基本信息，返回一个json格式的字符串
[DESC;] #标准 DESC 语句，返回当前操作对象的视图信息
[HOOK;] #标准 HOOK 语句，回归HOOK根，此时无数据操作对象
[HOOK objtype objname1 objname2 ...;] #标准 HOOK_MAKE 语句，使用钩子创建一个操作对象
[HOOK objname;] #标准 HOOK_CHECKOUT 语句，手动切换到一个已经存在的操作对象
[HOOK DEL objname1 objname2 ...;] #标准 HOOK_DEL_OGJ 语句，删除指定的HOOK操作对象,回归HOOK根，此时无数据操作对象
[HOOK CLEAR objname1 objname2 ...;]#标准 HOOK_CLEAR 语句，清空当前操作对象的所有数据,但保留操作对象及其信息
[RANK objname rank;]#标准 RANK_OBJ 语句，设置指定HOOK操作对象的权限等级为rank
[GET RANK objname;] #标准 GET_RANK 语句，获取指定HOOK操作对象的权限等级
[GET TYPE objname;] #标准 GET_TYPE 语句，获取指定HOOK操作对象的类型
[TEMP GET id;]#标准 TEMP_GET 语句,创建临时数组,将这个临时数组取名为id,id是0 - 65535之间的整数
[TEMP DEL id;]#标准 TEMP_DEL 语句,删除临时数组
[TEMP WHERE id1 ></<> id2 id3;] #标准 TEMP_WHERE 语句,取 id1 和 id2 的交集/并集放入临时数组 id3 中
// 交集符号:>< 并集符号:<>
[WHERE field_index/list_index >/</==/>=/<=/!=/~= value/pattern id;] #标准 WHERE_CONDITION_TEMP 语句 查询指定字段的行数据,放入临时数组 id 中,然后再除去重复项,相当于取并集
// 其中~= 为pattern模糊匹配,使用双引号引出,pattern为正则表达式
[GET TEMP id;]#标准 GET_TEMP 语句,获取临时数组id对应的索引对应的数据,自动根据数据类型判断函数序列#表：对应行 #列表：对应元素
[BACK;]# 标准 BACK 语句,返回前一个操作对象
[GET LEN;] #标准 GET_LEN 语句，获取流/列表的长度
*/
#define stmtype_GET_OBJ 1
#define stmtype_WHERE 2
#define stmtype_DESC 3
#define stmtype_HOOK 4
#define stmtype_HOOK_MAKE 5
#define stmtype_HOOK_CHECKOUT 6
#define stmtype_HOOK_DEL_OBJ 7
#define stmtype_HOOK_CLEAR 8
#define stmtype_RANK_OBJ 9
#define stmtype_GET_RANK 10
#define stmtype_GET_TYPE 11
#define stmtype_TEMP_GET 12
#define stmtype_TEMP_DEL 13
#define stmtype_TEMP_WHERE 14
#define stmtype_WHERE_CONDITION_TEMP 15
#define stmtype_GET_TEMP 16
#define stmtype_BACK 17
#define stmtype_GET_LEN 18
/*
[FIELD ADD field1_name datatype restraint field2_name datatype restraint...;]#标准 FIELD_ADD 语句
[FIELD INSERT field_index field_name datatype restraint;]#标准 FIELD_INSERT 语句只支持单次插入一个字段
[FIELD SWAP field1_index field2_index field3_index ...;]#标准 FIELD_SWAP 语句,1先后分别与2,3,...交换  
[FIELD DEL field1_index field2_index ...;]#标准 FIELD_DEL 语句,删除指定字段
[FIELD RENAME field_index field_name;]#标准 FIELD_RENAME 语句,重命名字段,只支持一次操作一个字段
[FIELD SET field_index ATTRIBUTE attribute;]#标准 FIELD_SET 语句,设置字段约束性属性     
[ADD value1 value2...;]#标准 LINE_ADD 语句
[INSERT line_index value1 value2 ...]#标准 LINE_INSERT 语句
[SET line1_index field1_index value1 line2_index field2_index value2 ...;]#标准 LINE_SET 语句
[DEL line1_index line2_index...;]#标准 LINE_DEL 语句
[SWAP line1_index line2_index ...;]#标准 LINE_SWAP 语句
[GET line1_index line2_index ...;]#标准 LINE_GET 语句,获取指定行号的多行数据
[GET POS x1 y1 x2 y2 ...;]#标准 POS_GET 语句,获取指定坐标的数据
*/
#define stmtype_FIELD_ADD 17
#define stmtype_FIELD_INSERT 18
#define stmtype_FIELD_SWAP 19
#define stmtype_FIELD_DEL 20
#define stmtype_FIELD_RENAME 21
#define stmtype_FIELD_SET 22
#define stmtype_LINE_ADD 23
#define stmtype_LINE_INSERT 24
#define stmtype_LINE_SET 25
#define stmtype_LINE_DEL 26
#define stmtype_LINE_SWAP 27
#define stmtype_GET 28
#define stmtype_POS_GET 29
/*
[EXISTS key1 key2 ...;] #标准 KEY_EXISTS 语句,判断存在几个键
[SELECT pattern;] #标准 KEY_SELECT 语句，查找所有符合给定模式的键,注意查询的是键，不是值
[SET TYPE type key1 key2 key3 ... ;] #标准 KEY_SET 语句，不赋值，type为KVALOT,STREAM,TABLE,LIST,BITMAP
[DEL key1 key2 ...;] #标准 KEY_DEL 语句,删除指定键
[KEY key;]# 标准 KEY_CHECKOUT 语句,进入键对象操作
*/
#define stmtype_KEY_EXISTS 30
#define stmtype_KEY_SELECT 31
#define stmtype_KEY_SET 32
#define stmtype_KEY_DEL 33
#define stmtype_KEY_CHECKOUT 34
/*
[APPEND value;]#标准 STREAM_APPEND 语句，将数据追加向流中附加数据
[APPEND pos value;]#标准 STREAM_APPEND_POS 将value追加到流中，从指定位置开始。若pos超出流的长度，则从流的末尾开始追加。
[GET pos len;]#标准 STREAM_GET 语句，获取流中指定长度的数据
[SET pos value;]#标准 STREAM_SET 语句，从pos处设置流中指定位置的值
[SET pos len char;]#标准 STREAM_SET_CHAR 语句，从pos处设置流中指定位置的值,并将其长度设置为len
*/
#define stmtype_STREAM_APPEND 36
#define stmtype_STREAM_APPEND_POS 37
#define stmtype_STREAM_GET 38
#define stmtype_STREAM_GET_LEN 39
#define stmtype_STREAM_SET 40
#define stmtype_STREAM_SET_CHAR 41
#define stmtype_KEY_MAKE 42
#define TOKEN_CHECKOUT TOKEN_GET  // 临时定义，可能需要根据实际需要调整
/* 
[LPUSH value;]#标准 LIST_LPUSH 语句，在列表开头添加一个值
[RPUSH value;]#标准 LIST_RPUSH 语句，在列表末尾添加一个值
[LPOP;]#标准 LIST_LPOP 语句，移除并返回列表开头的值
[RPOP;]#标准 LIST_RPOP 语句，移除并返回列表末尾的值
[GET index;] #标准 LIST_GET 语句,获取指定位置的值,index:1,2..为第1,2..个元素，-1,-2,为倒数第1,2..个元素，0:所有元素
[DEL index;] #标准 LIST_DEL 语句，删除指定索引位置的值
[GET LEN index;]#标准 LIST_GET_INDEX_LEN 语句，获取列表中指定索引位置的值的长度
[INSERT index value;] #标准 LIST_INSERT 语句，在指定索引位置插入一个值
[SET index value;]#标准 LIST_SET 语句，更新列表中指定索引位置的值
[EXISTS value1 value2 ...;]#标准 LIST_EXISTS 语句，判断列表是否存在指定值
*/
/*
#define stmtype_LIST_LPUSH 42
#define stmtype_LIST_RPUSH 43
#define stmtype_LIST_LPOP 44
#define stmtype_LIST_RPOP 45
#define stmtype_LIST_GET 46
#define stmtype_LIST_DEL 47
#define stmtype_LIST_GET_INDEX_LEN 48
#define stmtype_LIST_INSERT 49
#define stmtype_LIST_SET 50
#define stmtype_LIST_EXISTS 51
*/
/*
[SET index value;] #标准 BITMAP_SET 语句，设置位图中指定偏移量处的位值，value为0或1
[SET index1 index2 value;] #标准 BITMAP_SET_RANGE 语句，设置位图中指定偏移量范围内的位值，value为0或1
[GET index;] #标准 BITMAP_GET 语句，获取位图中指定偏移量处的位值
[GET index1 index2;]#标准 BITMAP_GET_RANGE 语句，获取位图中指定偏移量范围内的位值
[COUNT index1 index2;] #标准 BITMAP_COUNT_RANGE 语句，统计位图中指定偏移量范围内值为1的位数        
*/
/*
#define stmtype_BITMAP_SET 52
#define stmtype_BITMAP_SET_RANGE 53
#define stmtype_BITMAP_GET 54
#define stmtype_BITMAP_GET_RANGE 55
#define stmtype_BITMAP_COUNT_RANGE 56
*/

int distinguish_stmt_type(tok* token,const int len,int* where){
    /*
    功能:根据第一个token到最后一个token判断语句类型,where:返回首参数位置,如果没有参数,则返回-1
    返回值:
    成功:返回语句类型
    失败:返回-1
    */
    if(token == NULL || where == NULL ||len < 1){
        return -1;
    }
    int result = -1;

    int flag=0;


    switch(token[0].type){
        /*
        
        */
        case TOKEN_GET:{
            /*
            多义语句：
            [GET POS x1 y1 x2 y2 ...;]#标准 POS_GET 语句,获取指定坐标的数据
            [GET line1_index line2_index ...;]#标准 LINE的GET 语句,获取指定行号的多行数据
            [GET pos len;]#标准 STREAM的GET 语句，获取流中指定长度的数据
            */
            if(token[1].type == TOKEN_POS ){
                //[GET POS x1 y1 x2 y2...;]#标准 POS_GET 语句,获取指定坐标的数据
                //检查参数是否合法
                for(int i = 2;i < len;i++){
                    if(token[i].type != TOKEN_NUM){
                        flag = 1;
                    }
                }
                if(len%2 == 0 && len >= 4 && flag == 0){
                    result = stmtype_POS_GET;
                    *where = 2;//首参数位置为2
                }                
                goto back;
            }
            else if(token[1].type == TOKEN_NUM){
                //[GET line1_index line2_index...;]#标准 LINE的GET 语句,获取指定行号的多行数据
                //[GET pos len;]#标准 STREAM的GET 语句，获取流中指定长度的数据
                //检查参数是否合法
                for(int i = 1;i < len;i++){
                    if(token[i].type!= TOKEN_NUM){
                        flag = 1;
                    }
                }
                if(flag == 0){
                    result = stmtype_GET;
                    *where = 1;//首参数位置为1
                }                
                goto back;
            }            
            switch(len){
                case 1:
                    //[GET;] #标准 GET_OBJ 语句,获取当前操作对象的所有数据
                    result = stmtype_GET_OBJ;
                    *where = -1;//没有参数
                    goto back;
                case 2:
                    //[GET LEN;] #标准 GET_LEN 语句，获取流/列表的长度
                    if(token[1].type == TOKEN_LEN){
                        result = stmtype_GET_LEN;
                        *where = -1;//没有参数
                    }
                    goto back;
                case 3:
                    //[GET RANK objname;] #标准 GET_RANK 语句，获取指定HOOK操作对象的权限等级
                    //[GET TYPE objname;] #标准 GET_TYPE 语句，获取指定HOOK操作对象的类型
                    //[GET TEMP id;]#标准 GET_TEMP 语句,获取临时数组id对应的索引对应的数据,自动根据数据类型判断函数序列#表：对应行 #列表：对应元素
                    switch(token[1].type){
                        case TOKEN_RANK:
                            if(token[2].type == TOKEN_NAME){
                                result = stmtype_GET_RANK;
                                *where = 2;//首参数位置为2
                            }
                            goto back;
                        case TOKEN_TYPE:
                            if(token[2].type == TOKEN_NAME){
                                result = stmtype_GET_TYPE;
                                *where = 2;//首参数位置为2
                            }
                            goto back;
                        case TOKEN_TEMP:
                            if(token[2].type == TOKEN_VALUES){
                                result = stmtype_GET_TEMP;
                                *where = 2;
                            }                            
                            goto back;
                        default:
                            goto back;
                    }
                default:
                    goto back;
            }
        }  
        case TOKEN_WHERE:{
            switch(len){
                case 1:
                    //[WHERE;] #标准 WHERE 语句，返回当前操作对象的基本信息，返回一个json格式的字符串
                    result = stmtype_WHERE;
                    *where = -1;//没有参数
                    goto back;
                case 4:
                    //[WHERE field_index/list_index >/</==/>=/<=/!=/~= value/pattern id;] #标准 WHERE_CONDITION_TEMP 语句 查询指定字段的行数据,放入临时数组 id 中,然后再除去重复项,相当于取并集
                    // 其中~= 为pattern模糊匹配,使用双引号引出,pattern为正则表达式
                    if(token[1].type == TOKEN_NUM && 
                        (token[2].type == TOKEN_DY ||
                        token[2].type == TOKEN_XY ||
                        token[2].type == TOKEN_DYDY ||
                        token[2].type == TOKEN_XYDY ||
                        token[2].type == TOKEN_DDY ||
                        token[2].type == TOKEN_BDY ||
                        token[2].type == TOKEN_YDY ||
                        token[2].type == TOKEN_JJ ||
                        token[2].type == TOKEN_BJ) &&
                        token[3].type == TOKEN_VALUES &&
                         token[4].type == TOKEN_NUM){
                        result = stmtype_WHERE_CONDITION_TEMP;
                        *where = 1;//首参数位置为1
                    }
                    goto back;
                default:
                    goto back;
            }
        }
        case TOKEN_DESC:{
            //[DESC;]#标准 DESC 语句，返回当前操作对象的视图信息
            if(len == 1){
                result = stmtype_DESC;
                *where = -1;//没有参数
            }
            goto back;
        }
        case TOKEN_HOOK:{
            /*
            多义语句：
            [HOOK objtype objname1 objname2 ...;] #标准 HOOK_MAKE 语句，使用钩子创建一个操作对象
            [HOOK DEL objname1 objname2 ...;] #标准 HOOK_DEL_OGJ 语句，删除指定的HOOK操作对象,回归HOOK根，此时无数据操作对象
            [HOOK CLEAR objname1 objname2 ...;]#标准 HOOK_CLEAR 语句，清空当前操作对象的所有数据,但保留操作对象及其信息
            */
            if(token[1].type == TOKEN_TABLE||
                token[1].type == TOKEN_KVALOT||
                token[1].type == TOKEN_BITMAP||
                token[1].type == TOKEN_STREAM||
                token[1].type == TOKEN_LIST){
                //[HOOK objtype objname1 objname2...;] #标准 HOOK_MAKE 语句，使用钩子创建一个操作对象
                //检查参数是否合法
                for(int i = 2;i < len;i++){
                    if(token[i].type!= TOKEN_NAME){
                        flag = 1;
                    }
                }
                if(flag == 0){
                    result = stmtype_HOOK_MAKE;
                    *where = 2;//首参数位置为2
                }
                goto back;                
            }
            else if(token[1].type == TOKEN_DEL || token[1].type == TOKEN_CLEAR){
                //[HOOK DEL objname1 objname2...;] #标准 HOOK_DEL_OGJ 语句，删除指定的HOOK操作对象,回归HOOK根，此时无数据操作对象
                //[HOOK CLEAR objname1 objname2...;]#标准 HOOK_CLEAR 语句，清空当前操作对象的所有数据,但保留操作对象及其信息
                //检查参数是否合法
                for(int i = 2;i < len;i++){
                    if(token[i].type!= TOKEN_NAME){
                        flag = 1;
                    }
                }
                if(flag == 0){
                    if(token[1].type == TOKEN_DEL){
                        result = stmtype_HOOK_DEL_OBJ;
                    }
                    else if(token[1].type == TOKEN_CLEAR){
                        result = stmtype_HOOK_CLEAR;
                    }
                    *where = 2;//首参数位置为2
                }
                goto back;
            }
            switch(len){
                case 1:
                    //[HOOK;] #标准 HOOK 语句，回归HOOK根，此时无数据操作对象
                    result = stmtype_HOOK;
                    *where = -1;//没有参数
                    goto back;
                case 2:
                    //[HOOK objname;] #标准 HOOK_CHECKOUT 语句，手动切换到一个已经存在的操作对象
                    if(token[1].type == TOKEN_NAME){
                        result = stmtype_HOOK_CHECKOUT;
                        *where = 2;//首参数位置为2
                    }
                    goto back;
                default:
                    goto back;                    
            }
        }
        case TOKEN_RANK:{
            //[RANK objname rank;]#标准 RANK_OBJ 语句，设置指定HOOK操作对象的权限等级为rank
            if(len == 3 && token[1].type == TOKEN_NAME && token[2].type == TOKEN_NUM){
                result = stmtype_RANK_OBJ;
                *where = 2;//首参数位置为2
            }
            goto back;
        }
        case TOKEN_TEMP:{
            /*
            [TEMP GET id;]#标准 TEMP_GET 语句,创建临时数组,将这个临时数组取名为id,id是0 - 65535之间的整数
            [TEMP DEL id;]#标准 TEMP_DEL 语句,删除临时数组
            [TEMP WHERE id1 ></<> id2 id3;] #标准 TEMP_WHERE 语句,取 id1 和 id2 的交集/并集放入临时数组 id3 中
            // 交集符号:>< 并集符号:<>
            */
            if((token[1].type == TOKEN_GET || token[1].type == TOKEN_DEL) && token[2].type == TOKEN_NUM){
                if(token[1].type == TOKEN_GET){
                    result = stmtype_TEMP_GET;
                }
                else if(token[1].type == TOKEN_DEL){
                    result = stmtype_TEMP_DEL;
                }
                *where = 2;//首参数位置为2 
            }
            else if(token[1].type == TOKEN_WHERE && token[2].type == TOKEN_NUM &&
                 (token[3].type == TOKEN_JJ ||token[3].type == TOKEN_BJ)
                    && token[4].type == TOKEN_NUM && token[5].type == TOKEN_NUM){
                result = stmtype_TEMP_WHERE;
                *where = 2;//首参数位置为2
            }
            goto back;
        }
        case TOKEN_BACK:{
            //[BACK;]# 标准 BACK 语句,返回前一个操作对象
            if(len == 1){
                result = stmtype_BACK;
                *where = -1;//没有参数
            }
            goto back;
        }
        case TOKEN_EXISTS:{
            //[EXISTS objname1 objname2...;] #标准 EXISTS 语句,判断存在几个HOOK或者键对象
            for(int i = 1;i < len;i++){
                if(token[i].type!= TOKEN_NAME || token[i].type != TOKEN_VALUES){
                    flag = 1;
                }
            }
            if(len >= 2 && flag == 0){
                result = stmtype_KEY_EXISTS;
                *where = 2;//首参数位置为2
            }
            goto back;
        }
        case TOKEN_SELECT:{
            //[SELECT pattern;] #标准 SELECT 语句，查找所有符合给定模式的键,注意查询的是键，不是值
            if(len == 2 && token[1].type == TOKEN_VALUES){
                result = stmtype_KEY_SELECT;
                *where = 2;//首参数位置为2
            }
            goto back;
        }
        case TOKEN_KEY:{
            /*
            [KEY type objname1 objname2 objname3 ... ;] #标准 KEY_MAKE 语句，不赋值，type为KVALOT,STREAM,TABLE,LIST,BITMAP
            [KEY DEL objname1 objname2 ...;] #标准 KEY_DEL 语句,删除指定键
            [KEY key;]# 标准 KEY_CHECKOUT 语句,进入键对象操作
            */
            if(token[1].type == TOKEN_TABLE||
                token[1].type == TOKEN_KVALOT||
                token[1].type == TOKEN_BITMAP||
                token[1].type == TOKEN_STREAM||
                token[1].type == TOKEN_LIST){
                //[KEY type objname1 objname2 objname3... ;] #标准 KEY_MAKE 语句，不赋值，type为KVALOT,STREAM,TABLE,LIST,BITMAP
                //检查参数是否合法
                for(int i = 2;i < len;i++){
                    if(token[i].type!= TOKEN_NAME){
                        flag = 1;
                    }
                }
                if(flag == 0 && len >= 2){
                    result = stmtype_KEY_MAKE;
                    *where = 2;//首参数位置为2
                }
                goto back;
            }
            else if(token[1].type == TOKEN_DEL){
                //[KEY DEL objname1 objname2...;] #标准 KEY_DEL 语句,删除指定键
                //检查参数是否合法
                for(int i = 2;i < len;i++){
                    if(token[i].type!= TOKEN_NAME){
                        flag = 1;
                    }
                }
                if(flag == 0){
                    result = stmtype_KEY_DEL;
                    *where = 2;//首参数位置为2
                }
                goto back;
            }
            else if(token[1].type == TOKEN_CHECKOUT){
                //[KEY key;]# 标准 KEY_CHECKOUT 语句,进入键对象操作
                if(len == 2 && token[1].type == TOKEN_NAME){
                    result = stmtype_KEY_CHECKOUT;
                    *where = 2;//首参数位置为2
                }
                goto back;
            }
            goto back;            
        }
        /*
        [KEY type objname1 objname2 objname3 ... ;] #标准 KEY_MAKE 语句，不赋值，type为KVALOT,STREAM,TABLE,LIST,BITMAP
        [KEY DEL objname1 objname2 ...;] #标准 KEY_DEL 语句,删除指定键
        [KEY key;]# 标准 KEY_CHECKOUT 语句,进入键对象操作

        [FIELD ADD field1_name datatype restraint field2_name datatype restraint...;]#标准 FIELD_ADD 语句
        [FIELD INSERT field_index field_name datatype restraint;]#标准 FIELD_INSERT 语句只支持单次插入一个字段
        [FIELD SWAP field1_index field2_index field3_index ...;]#标准 FIELD_SWAP 语句,1先后分别与2,3,...交换  
        [FIELD DEL field1_index field2_index ...;]#标准 FIELD_DEL 语句,删除指定字段
        [FIELD RENAME field_index field_name;]#标准 FIELD_RENAME 语句,重命名字段,只支持一次操作一个字段
        [FIELD SET field_index ATTRIBUTE attribute;]#标准 FIELD_SET 语句,设置字段约束性属性   

        [ADD value1 value2...;]#标准 LINE_ADD 语句

        [INSERT line_index value1 value2 ...]#标准 LINE_INSERT 语句

        [SET line1_index field1_index value1 line2_index field2_index value2 ...;]#标准 LINE_SET 语句
        [SET index value;]#标准 STREAM_SET 语句，从index处设置流中指定位置的值
        [SET pos len char;]#标准 STREAM_SET_CHAR 语句，从pos处设置流中指定位置的值,并将其长度设置为len

        [DEL line1_index line2_index...;]#标准 LINE_DEL 语句

        [SWAP line1_index line2_index ...;]#标准 LINE_SWAP 语句

        [APPEND value;]#标准 STREAM_APPEND 语句，将数据追加向流中附加数据
        [APPEND pos value;]#标准 STREAM_APPEND_POS 将value追加到流中，从指定位置开始。若pos超出流的长度，则从流的末尾开始追加。
        */
        
        case TOKEN_NUM:
            {
                int i = 0;  // 定义局部变量 i
                if(i == 0){
                    result = stmtype_BACK;
                    break;
                }
                else{
                    result = -1;
                    break;
                }
            }
        default:
            result = -1;
            break;
    }

    back:
    return result;
}







/*
//测试一下getoken函数
int main(){
    inputstr instr;
    instr.string = "SELECT HOOK<>1234567890 \"hauigiuuia\";";
    instr.len = strlen(instr.string);
    instr.pos = instr.string;
    tok *token = NULL;
    while((token = getoken(&instr)) != NULL){
        sprint(token->content,end);
        str_free(&token->content);//释放token->content
        free(token);
        token = NULL;
        printf("#\n");
    }
    system("pause");
}
*/


str lexer(char* string,int len){
    /*
    单个语句的字节码格式:[$~~$:语句确认符(4)][语句总长度(4)][语句类型(4)][参数1长度(4)][参数1类型(4)][参数2长度(4)][参数2类型(4)][...]
    多个语句:[stm1][stm2][...]
    */
    
    //先把string转化为inputstr
    inputstr instr;
    instr.string = string;
    instr.len = len;
    instr.pos = string;

    tok *token = NULL;
    uint32_t token_num=0;

    tok* cc_token = NULL;//临时存储
    
    //第一步开始解析,把所有的token都放入token数组中
    while((cc_token = getoken(&instr))!= NULL){
        tok* new_token = realloc(token,(token_num+1)*sizeof(tok));//拓展内存
        if(cc_token->type == TOKEN_EEROR || new_token == NULL ){
            l:
            //说明语法错误或内存分配失败,先释放内存
            for(uint32_t i = 0;i<token_num;i++){
                str_free(&token[i].content);//释放每一个token的content
            }
            free(token);//释放token数组
            str_free(&cc_token->content);//释放cc_token的content
            free(cc_token);//释放cc_token
                          str empty_str;
              str_init(&empty_str);
              return empty_str; 
        }
        token = new_token;
        token[token_num] = *cc_token;
        token_num++;
    }
    //如果结尾不是TOKEN_END,就说明语法错误
    if(token[token_num-1].type != TOKEN_END) goto l;

    // 第二步：识别语句类型并生成字节码
    str stream;
    str_init(&stream); // 初始化输出流
    for(int i=0;i<token_num;){
        int st=i,ed=i;
        while(token[i].type !=TOKEN_END){
            ed++;
        }
        //截取到一个语句
        int stmt_len=ed-st+1;
        int where=-1;//初始化参数位置
        int stmt_type=distinguish_stmt_type(&token[st],ed-st+1,&where);
        if(stmt_type == -1){
            //说明语法错误
            str_free(&stream);
            goto l;
        }

        //....



        i=ed+2;//跳过TOKEN_END
    }
    
    

}
/*
[GET;] #标准 GET_OBJ 语句,获取当前操作对象的所有数据
[GET RANK objname;] #标准 GET_RANK 语句，获取指定HOOK操作对象的权限等级
[GET TYPE objname;] #标准 GET_TYPE 语句，获取指定HOOK操作对象的类型
[GET TEMP id;]#标准 GET_TEMP 语句,获取临时数组id对应的索引对应的数据,自动根据数据类型判断函数序列#表：对应行 #列表：对应元素
[GET LEN;] #标准 GET_LEN 语句，获取流/列表的长度
[GET line1_index line2_index ...;]#标准 LINE_GET 语句,获取指定行号的多行数据
[GET POS x1 y1 x2 y2 ...;]#标准 POS_GET 语句,获取指定坐标的数据
[GET pos len;]#标准 STREAM_GET 语句，获取流中指定长度的数据

[WHERE;] #标准 WHERE 语句，返回当前操作对象的基本信息，返回一个json格式的字符串
[WHERE field_index/list_index >/</==/>=/<=/!=/~= value/pattern id;] #标准 WHERE_CONDITION_TEMP 语句 查询指定字段的行数据,放入临时数组 id 中,然后再除去重复项,相当于取并集
// 其中~= 为pattern模糊匹配,使用双引号引出,pattern为正则表达式

[DESC;] #标准 DESC 语句，返回当前操作对象的视图信息

[HOOK;] #标准 HOOK 语句，回归HOOK根，此时无数据操作对象
[HOOK objtype objname1 objname2 ...;] #标准 HOOK_MAKE 语句，使用钩子创建一个操作对象
[HOOK objname;] #标准 HOOK_CHECKOUT 语句，手动切换到一个已经存在的操作对象
[HOOK DEL objname1 objname2 ...;] #标准 HOOK_DEL_OGJ 语句，删除指定的HOOK操作对象,回归HOOK根，此时无数据操作对象
[HOOK CLEAR objname1 objname2 ...;]#标准 HOOK_CLEAR 语句，清空当前操作对象的所有数据,但保留操作对象及其信息

[RANK objname rank;]#标准 RANK_OBJ 语句，设置指定HOOK操作对象的权限等级为rank

[TEMP GET id;]#标准 TEMP_GET 语句,创建临时数组,将这个临时数组取名为id,id是0 - 65535之间的整数
[TEMP DEL id;]#标准 TEMP_DEL 语句,删除临时数组
[TEMP WHERE id1 ></<> id2 id3;] #标准 TEMP_WHERE 语句,取 id1 和 id2 的交集/并集放入临时数组 id3 中
// 交集符号:>< 并集符号:<>

[BACK;]# 标准 BACK 语句,返回前一个操作对象

[EXISTS objname1 objname2...;] #标准 EXISTS 语句,判断存在几个HOOK或者键对象

[SELECT pattern;] #标准 SELECT 语句，查找所有符合给定模式的键名或HOOK名

[KEY type objname1 objname2 objname3 ... ;] #标准 KEY_MAKE 语句，不赋值，type为KVALOT,STREAM,TABLE,LIST,BITMAP
[KEY DEL objname1 objname2 ...;] #标准 KEY_DEL 语句,删除指定键
[KEY key;]# 标准 KEY_CHECKOUT 语句,进入键对象操作

[FIELD ADD field1_name datatype restraint field2_name datatype restraint...;]#标准 FIELD_ADD 语句
[FIELD INSERT field_index field_name datatype restraint;]#标准 FIELD_INSERT 语句只支持单次插入一个字段
[FIELD SWAP field1_index field2_index field3_index ...;]#标准 FIELD_SWAP 语句,1先后分别与2,3,...交换  
[FIELD DEL field1_index field2_index ...;]#标准 FIELD_DEL 语句,删除指定字段
[FIELD RENAME field_index field_name;]#标准 FIELD_RENAME 语句,重命名字段,只支持一次操作一个字段
[FIELD SET field_index ATTRIBUTE attribute;]#标准 FIELD_SET 语句,设置字段约束性属性   

[ADD value1 value2...;]#标准 LINE_ADD 语句

[INSERT line_index value1 value2 ...]#标准 LINE_INSERT 语句

[SET line1_index field1_index value1 line2_index field2_index value2 ...;]#标准 LINE_SET 语句
[SET index value;]#标准 STREAM_SET 语句，从index处设置流中指定位置的值
[SET pos len char;]#标准 STREAM_SET_CHAR 语句，从pos处设置流中指定位置的值,并将其长度设置为len

[DEL line1_index line2_index...;]#标准 LINE_DEL 语句

[SWAP line1_index line2_index ...;]#标准 LINE_SWAP 语句

[APPEND value;]#标准 STREAM_APPEND 语句，将数据追加向流中附加数据
[APPEND pos value;]#标准 STREAM_APPEND_POS 将value追加到流中，从指定位置开始。若pos超出流的长度，则从流的末尾开始追加。


*/
