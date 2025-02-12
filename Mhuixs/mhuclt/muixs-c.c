/*
#版权所有 (c) HuJi 2025.1
#保留所有权利
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
Email:hj18914255909@outlook.com
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include "stdstr.h"

#define err -1

#define NOTKEYWORD -1

#define MAX_STATEMENTS 100 //一次性能够处理的最大语句数量
#define MAX_TOKENS_PER_STATEMENT 100 //单个语句的最大token数量

/*
下面是token的所有类型的枚举
*/
typedef enum TokType toktype;
typedef int commendID;//命令ID
typedef enum {
    Obj_NULL,            //空对象
    Obj_TABLE,           //表对象
    Obj_KVALOT,          //键库对象
    Obj_LIST,            //列表对象
    Obj_BITMAP,          //位图对象
    Obj_STREAM,          //流类对象
} stmtObj;//每个语句都要有自己的操作对象

//用户输入的字符串将会用这个结构体来储存、处理和分析
typedef struct inputstr{
    uint8_t *string;//待处理的字符串
    uint32_t len;
    uint8_t *pos;//指向string中待处理的字符
}inputstr;

//令牌结构体
typedef struct Token{
    str* content;//TOKEN的字面信息
    toktype type;//TOKEN的类型
} Token,tok;//符号（令牌）结构体

//单语句结构体,由多个符合语法顺序的token组成
typedef struct statement{
    tok *tokens;
    int num;//token的数量
    stmtObj obj;//语句的操作对象
} statement,stmt;//单语句操作结构体

//stmt将会被加入到stmts中进行统一处理
typedef struct stmts{
    stmt *stmt;
    int num;//语句的数量
} statements,stmts;//多语句操作结构体;

Token* getoken(inputstr* instr)//返回的token记得释放
{

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

    //判断是否是token的结束字符，'不算是token的结束字符，因为'将单独处理
    #define is_std_token_end_char(c)  ( \
    (c ==' ')  || \
    (c ==';')     \
    )


    Token *token = (Token*)malloc(sizeof(Token));
    /*
    返回值:
    成功:返回token
    失败/字符串结束:返回NULL
    */
    /*
    功能:从pos位置开始，获取下一个token并返回
    */

    /*
    请保证instr合法
    pos必须在string的合法范围内    
    */

    /*
    特殊的字符:
    语句结束:    ;
    字符串:  "   '      '只有再""内才有效
    */

    int is_string = 0;//函数全局变量：是否是字符串
    

    //在原来pos的位置基础上先后寻找首个token的起始字符
    for(uint32_t i=0;;i++){
        //判断st_pos是否是string的结束字符
        if(instr->pos > instr->string+instr->len){
            return NULL;//用户输入的字符串已经结束
        }
        if(is_std_token_start_char(*instr->pos)){
            //找到token的起始字符了
            if(*instr->pos == '\"'){
                is_string = 1;
                instr->pos++;//跳过"
                break;
            }
            if(*instr->pos == '\''){
                //单字节字符
                instr->pos++;//跳过'
                /*单字节处理模块*/{                    
                    if(*instr->pos == '\\'){
                        //说明是转义字符
                        if(*(instr->pos+2) != '\''){
                            token->content = NULL;
                            token->type = TOKEN_EEROR;
                            //不用管pos的位置，因为已经TOKEN_EEROR了
                            return token;
                        }
                        instr->pos++;//跳过'\'
                        switch(*instr->pos){
                            case 'n':
                                token->content = stostr("\n",1);
                                break;
                            case 't':
                                token->content = stostr("\t",1);
                                break;
                            case 'r':
                                token->content = stostr("\r",1);
                                break;
                            case '0':
                                token->content = stostr("\0",1);
                                break;
                            case '\\':
                                token->content = stostr("\\",1);
                                break;
                            default:
                                token->content = NULL;
                                token->type = TOKEN_EEROR;
                                //不用管pos的位置，因为已经TOKEN_EEROR了
                                return token;
                        }
                        token->type =TOKEN_STREAM;
                        instr->pos+=2;
                        return token;
                    }
                    else{
                        if(*(instr->pos+1) != '\''){
                            token->content = NULL;
                            token->type = TOKEN_EEROR;
                            //不用管pos的位置，因为已经TOKEN_EEROR了
                            return token;                            
                        }
                        token->content = stostr(instr->pos,1);
                        token->type =TOKEN_STREAM;
                        instr->pos+=2;
                        return token;
                    }

                }

            }
            if(*instr->pos == ';'){
                token->content = NULL;
                token->type = TOKEN_END;
                instr->pos++;
                return token;
            }
            break;
        }
        instr->pos++;//推动指针
    }

    //推动ed_pos指针，截取完整的token
    uint8_t* ed_pos = instr->pos+1;
    if(is_string){//字符串单独处理
        for(;;){
            //判断ed_pos是否是string的结束字符
            if(ed_pos > instr->string+instr->len){
                token->content = NULL;
                token->type = TOKEN_EEROR;
                return token;
            }
            if(*ed_pos == '\"' && *(ed_pos-1) != '\\'){
                break;
            }
            ed_pos++;
        }
    }
    else{//非字符串非单字符
        for(;;){
            //判断ed_pos是否是string的结束字符
            if(ed_pos > instr->string+instr->len){
                token->content = NULL;
                token->type = TOKEN_EEROR;
                return token;
            }
            if(is_std_token_end_char(*ed_pos)){                
                break;
            }
        }
    }

    //已经截取到了完整的token了
    //将token的内容复制到token->content中
    token->content = stostr(instr->pos,ed_pos - instr->pos);
    //将instr->pos移动到ed_pos的位置
    instr->pos = ed_pos;
    //判断token的类型
    if(is_string){
        token->type = TOKEN_STREAM;
    }
    else{
        /*
        callitkeyword将会自动尝试为token匹配关键字，如果失败则返回NOTKEYWORD
        */
        if(callitkeyword(token->content->string,token->content->len,&token->type) == NOTKEYWORD ){
            //判断第一个字符是否是数字，如果是数字，则检查是否是规范的数字
            if(token->content->string[0] >= '0' && token->content->string[0] <= '9'){
                //检查是否是规范的数字
                for(uint32_t i=0;i<token->content->len;i++){
                    if((token->content->string[i] < '0' || token->content->string[i] > '9')&& token->content->string[i] != '.'){
                        token->type = TOKEN_EEROR;
                        sfree(token->content);
                        token->content = NULL;
                        return token;
                    }
                }
                //是规范的数字
                token->type = TOKEN_VALUES;
                return token;
            }
            //既不是关键字，也不是数字，也不是字符串，也不是结束符，也不是错误，那么就是名称
            token->type = TOKEN_NAME;
        }
    }

    return token;
}

str* lexer(str* Mhuixsentence,stmtObj last_obj,uint8_t* info)
{
    /*
    词法分析器
    将用户输入的字符串转换为多个stmt，之后再进行语法分析。
    Mhuixsentence:待处理的字符串：C语言字符串
    last_obj:首个语句的操作对象类型
    info:对象信息

    返回值：str*:处理后的字符串：C语言字符串
    NULL：出错了
    */

    //########################################################
    //分词成句模块：将用户语句拆分为token后再重新组装为stmt
    //########################################################

    //初始化inputstr用来存储用户输入的Mhuixsentence字符串
    inputstr instr;
    instr.string = (uint8_t*)malloc(Mhuixsentence->len);//str不计入'\0'
    if(instr.string == NULL){
        printf("Malloc Error\n");
        return NULL;
    }
    instr.len = Mhuixsentence->len;
    instr.pos = instr.string;
    memcpy(instr.string,Mhuixsentence->string,Mhuixsentence->len);//将Mhuixsentence的内容复制到instr.string中

    //初始化stmts
    stmts stmts;
    stmts.stmt = (stmt*)malloc(sizeof(stmt)*MAX_STATEMENTS);
    if(stmts.stmt == NULL){
        printf("Malloc Error\n");
        free(instr.string);
        return NULL;
    }
    stmts.num = 0;

    //初始化当前语句
    stmt current_stmt;
    current_stmt.tokens = (tok*)malloc(sizeof(tok)*MAX_TOKENS_PER_STATEMENT);
    if(current_stmt.tokens == NULL){
        printf("Malloc Error\n");
        free(instr.string);
        free(stmts.stmt);
        return NULL;
    }
    current_stmt.num = 0;

    int error = 1;
    
    //获得语句
    for(int i=0,j=0; i < MAX_TOKENS_PER_STATEMENT, j < MAX_STATEMENTS; i++){
        //获取token,每次结束循环前记得释放token->content、token本身
        Token* token = getoken(&instr);

        if(token->type == TOKEN_EEROR){
            //用户输入的字符串错误,则抛出“令牌错误”
            printf("Token Error\n");

            //释放token
            sfree(token->content);
            free(token);

            goto ERR;
        }
        else if(token == NULL){

            //释放token
            free(token->content);
            free(token);

            //用户输入的字符串已经读取结束
            break;//跳出循环
        }
        else if(token->type == TOKEN_END){
            i=-1;//重新开始循环
            j++;//语句数量+1

            /*
            TOKEN_END本身不会被添加到current_stmt中，只作为语句的结束标志
            */

            //一个语句解析结束,将当前语句添加到stmts中
            //先分配内存
            stmts.stmt = (stmt*)realloc(stmts.stmt, (stmts.num + 1) * sizeof(stmt));
            if(stmts.stmt == NULL){
                //内存不足
                printf("Realloc Error\n");

                //释放token
                sfree(token->content);
                free(token);

                goto ERR;
            }
            //将当前语句添加到stmts中
            stmts.stmt[stmts.num].tokens = (tok*)calloc(current_stmt.num, sizeof(tok));
            if(stmts.stmt[stmts.num].tokens == NULL){
                //内存不足
                printf("Realloc Error\n");

                //释放token
                sfree(token->content);
                free(token);

                goto ERR;
            }
            stmts.stmt[stmts.num].num = current_stmt.num;
            for(int i = 0; i < current_stmt.num; i++){
                //复制token
                stmts.stmt[stmts.num].tokens[i] = current_stmt.tokens[i];
            }
           
            stmts.num++;

            //重置当前语句
            for(int i = 0; i < current_stmt.num; i++){
                //释放token
                sfree(current_stmt.tokens[i].content);
                current_stmt.tokens[i].content = NULL;
            }
            current_stmt.num = 0; 
        }
        else{
            //将token添加到当前语句中
            tok* new_current_stmt_tokens 
            = (tok*)realloc(current_stmt.tokens, (current_stmt.num + 1) * sizeof(tok*));
            if(new_current_stmt_tokens == NULL){
                //内存分配失败
                printf("Realloc Error\n");

                //释放token
                sfree(token->content);
                free(token);

                goto ERR;
            }
            current_stmt.tokens = new_current_stmt_tokens;

            current_stmt.tokens[current_stmt.num] = *token;
            current_stmt.num++;            
        }
        //释放token
        sfree(token->content);
        free(token);
        
        continue;
    }
    
    //如果可以执行到这里，说明整个循环结束都没有发生错误
    //如果发生ERR跳转，那么error=1，在下面释放内存模块末尾会离开函数
    error = 0;

    //模块的结束，在这里统一释放掉本模块的所有内存
    //释放内存模块+错误处理
    ERR:{
        //先释放当前语句的stmt
        for(uint32_t i=0;i<current_stmt.num;i++)
        {
            //释放第i个token的内容
            sfree(current_stmt.tokens[i].content);
        }
        free(current_stmt.tokens);//释放当前语句的stmt

        //释放stmts
        for(int i = 0; i < stmts.num; i++)
        {
            //释放第i个stmt的内容
            stmt *c_stmt = &stmts.stmt[i];

            for(int j = 0; j < c_stmt->num; j++)
            {
                sfree((c_stmt->tokens[j]).content);
            }
            free(c_stmt->tokens);
        }
        free(stmts.stmt);//释放stmts

        //释放instr.string
        free(instr.string);

        if(error){
            return NULL;
        }        
    }    
    
    //########################################################
    //对象识别模块：识别每个stmt的操作对象类型，需要识别出隐性操作对象
    //########################################################

    /*
    下一句语言如果没有明确指定操作对象，
    那么默认操作对象为当前stmt的上一句的操作对象
    */
    
    for(int i = 0; i < stmts.num; i++){
        //获取当前语句
        stmt* curstmt = &stmts.stmt[i];

        //开始寻找当前语句的操作对象
        //switch()

        
    }


    //########################################################
    //语法检查模块：分析每个stmt的语法是否正确
    //########################################################
    
    //////////////////////////////////////////////////////////

    //########################################################
    //命令转化模块：将每个stmt转换为Mhuixs命令
    //########################################################

    //////////////////////////////////////////////////////////
}

/*
##下面是HOOK所有使用方法
###关键字【HOOK】【TABLE,KVALOT,LIST,BITMAP,STREAM】【DEL,TYPE,RANK,CLEAR,DESC,GET,WHERE,TEMP】【$:预处理联系符】
{
    #HOOK基础操作

    [GET;] #标准 GET_OBJ 语句,获取当前操作对象的所有数据

    [WHERE;] #标准 WHERE 语句，返回当前操作对象的基本信息，返回一个json格式的字符串

    [DESC;] #标准 DESC 语句，返回当前操作对象的视图信息

    [HOOK;] #标准 HOOK 语句，回归HOOK根，此时无数据操作对象

    [HOOK objtype objname1 objname2 ...;] #标准 HOOK_MAKE 语句，使用钩子创建一个操作对象

    [HOOK objname;] #标准 HOOK_CHECKOUT 语句，手动切换到一个已经存在的操作对象

    [HOOK DEL objname1 objname2 ...;] #标准 HOOK_DEL_OGJ 语句，删除指定的HOOK操作对象,回归HOOK根，此时无数据操作对象
        衍生&&:HOOK DEL $WHERE;#删除当前操作对象            

    [HOOK CLEAR objname1 objname2 ...;]#标准 HOOK_CLEAR 语句，清空当前操作对象的所有数据,但保留操作对象及其信息

    [RANK objname rank;]#标准 RANK_OBJ 语句，设置指定HOOK操作对象的权限等级为rank
        衍生&&:RANK $WHERE rank;#设置当前对象的权限等级为rank
    
    [GET RANK objname;] #标准 GET_RANK 语句，获取指定HOOK操作对象的权限等级
        衍生&&:GET RANK $WHERE; #获取当前操作对象的权限等级

    [GET TYPE objname;] #标准 GET_TYPE 语句，获取指定HOOK操作对象的类型
        衍生&&:GET TYPE $WHERE; #获取当前操作对象的类型

    [TEMP GET id;]#标准 TEMP_GET 语句,创建临时数组,将这个临时数组取名为id,id<0 - 65535>

    [TEMP DEL id;]#标准 TEMP_DEL 语句,删除临时数组

    [TEMP WHERE id1 ==/-- id2 id3;] #标准 TEMP_WHERE 语句,取 id1 和 id2 的交集/并集放入临时数组 id3 中

    [WHERE field_index/list_index >/</==/>=/!=/~= value/pattern id;] #标准 WHERE_CONDITION_TEMP 语句 查询指定字段的行数据,放入临时数组 id 中,然后再除去重复项,相当于取并集

    [GET TEMP id;]#标准 GET_TEMP 语句,获取临时数组id对应的索引对应的数据,自动根据数据类型判断函数序列
    #表：对应行 #列表：对应元素
}
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
/*
##下面是操作对象为TABLE时的所有语法@MHU
###关键字【INSERT,GET,FIELD,SET,ADD,SWAP,DEL,RENAME,ATTRIBUTE,POS,WHERE】
【[i1,int8_t],[i2,int16_t],[i4,int32_t,int],[i8,int64_t],[ui1,uint8_t],[ui2,uint16_t],[ui4,uint32_t],[ui8,uint64_t],
[f4,float],[f8,double],[str,stream],date,time,datetime】
【PKEY,FKEY,UNIQUE,NOTNULL,DEFAULT】
【$:将字段名预处理为索引】$field_name <==> field_index(数字索引从0开始)
{
    #FILED操作,不指明FIELD默认就是LINE操作

    [FIELD ADD field1_name datatype restraint field2_name datatype restraint...;]#标准 FIELD_ADD 语句
        衍生&&:FIELD ADD (field1_name datatype restraint,...);#从左向右添加字段,初处理将会对命令进行预处理，即‘,’替换为空格,()替换为空格
              预处理=> FIELD ADD  field1_name datatype restraint field2_name datatype restraint...;

    [FIELD INSERT field_index field_name datatype restraint;]#标准 FIELD_INSERT 语句只支持单次插入一个字段
        衍生&&:FIELD INSERT (field_name datatype restraint,...) AT field_index; #在指定位置插入多个字段
              预处理=> FIELD INSERT field_index field1_name datatype restraint;...#多个标准语句的组合

    [FIELD SWAP field1_index field2_index field3_index ...;]#标准 FIELD_SWAP 语句,1先后分别与2,3,...交换  

    [FIELD DEL field1_index field2_index ...;]#标准 FIELD_DEL 语句,删除指定字段

    [FIELD RENAME field_index field_name;]#标准 FIELD_RENAME 语句,重命名字段,只支持一次操作一个字段
        衍生&&:FIELD RENAME field_index field_name ...; #重命名字段 
              预处理=> FIELD RENAME field_index field_name;...#多个标准语句的组合

    [FIELD SET field_index ATTRIBUTE attribute;]#标准 FIELD_SET 语句,设置字段约束性属性
        
    [ADD value1 value2...;]#标准 LINE_ADD 语句
        衍生&&:ADD (line1_data,NULL,...)(line2_data,3,...)...; #在表末尾添加几行数据，数据按字段顺序依次给出，必须包含所有字段，NULL表示字段为NULL
              预处理=> ADD  line1_data NULL ...;ADD line2_data 3 ...;... #分解为多个ADD语句
        衍生&&:ADD field_index=value1 field_index=value2 ...;#表末添加一行数据，部分数据直接给出，其它部分自动补全为NULL
              预处理=> ADD  NULL NULL value1 value2 ...;#转变为ADD普通语句

    [INSERT line_index value1 value2 ...]#标准 LINE_INSERT 语句
        衍生&&:INSERT (line1_data,NULL,...) AT line_index; #在指定行号处插入一行数据，line_number从0开始计数
              预处理=> INSERT line1_index line1_data NULL...;INSERT line2_index line2_data 3...;... #分解为多个INSERT语句
        衍生&&:INSERT field_index=value1 field_index=value2... AT line_index;#表末添加一行数据，部分数据直接给出，其它部分自动补全为NULL
              预处理=> INSERT line_index  NULL NULL value1 value2...;#转变为ADD普通语句

    [SET line1_index field1_index value1 line2_index field2_index value2 ...;]#标准 LINE_SET 语句
        衍生&&:SET line_number field1_index = value1, field2_index = value2,...;#(,)等在第一步预处理时就会被替换为‘ ’
              预处理=> SET line_index field1_index value1 line_index field2_index value2...;#转变为SET普通语句
        衍生&&:SET POS (x1,y1)= value1 (x2,y2)=value2 ...;#(,)等在第一步预处理时就会被替换为‘ ’
              预处理=> SET line_index field1_index value1 line_index field2_index value2...;#转变为SET普通语句

    [DEL line1_index line2_index...;]#标准 LINE_DEL 语句

    [SWAP line1_index line2_index ...;]#标准 LINE_SWAP 语句

    [GET line1_index line2_index ...;]#标准 LINE_GET 语句,获取指定行号的多行数据
        GET FIELD field_name;#获取指定字段对应的列的数据

    [GET POS x1 y1 x2 y2 ...;]#标准 POS_GET 语句,获取指定坐标的数据
}
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
#define stmtype_LINE_GET 28
#define stmtype_POS_GET 29
/*
##下面是操作对象为KVALOT时的所有语法@MHU
###关键字【SET,DEL,EXISTS,SELECT,KEY,TYPE】
{
    [EXISTS key1 key2 ...;] #标准 KEY_EXISTS 语句,判断存在几个键

    [SELECT pattern;] #标准 KEY_SELECT 语句，查找所有符合给定模式的键,注意查询的是键，不是值
        衍生&&:SELECT ALL;#查找所有键
              预处理=> SELECT *; #查找所有键

    [SET TYPE type key1 key2 key3 ... ;] #标准 KEY_SET 语句，不赋值，type为KVALOT,STREAM,TABLE,LIST,BITMAP

    [DEL key1 key2 ...;] #标准 KEY_DEL 语句,删除指定键
    
    [KEY key;]# 标准 KEY_CHECKOUT 语句,进入键对象操作
}
*/
#define stmtype_KEY_EXISTS 30
#define stmtype_KEY_SELECT 31
#define stmtype_KEY_SET 32
#define stmtype_KEY_DEL 33
#define stmtype_KEY_CHECKOUT 34
/*
##下面是操作对象为STREAM时的所有语法@MHU
###关键字【APPEND,SET,GET,LEN】
{
    [APPEND value;]#标准 STREAM_APPEND 语句，将数据追加向流中附加数据
    
    [APPEND pos value;]#标准 STREAM_APPEND_POS 将value追加到流中，从指定位置开始。若pos超出流的长度，则从流的末尾开始追加。

    [GET pos len;]#标准 STREAM_GET 语句，获取流中指定长度的数据

    [SET pos value;]#标准 STREAM_SET 语句，从pos处设置流中指定位置的值

    [SET pos len char;]#标准 STREAM_SET_CHAR 语句，从pos处设置流中指定位置的值,并将其长度设置为len

    [GET LEN;] #标准 STREAM_GET_LEN 语句，获取流的长度
}
*/
#define stmtype_STREAM_APPEND 35
#define stmtype_STREAM_APPEND_POS 36
#define stmtype_STREAM_GET 37
#define stmtype_STREAM_GET_LEN 38
#define stmtype_STREAM_SET 39
#define stmtype_STREAM_SET_CHAR 40
/* 
##下面是操作对象为LIST时的所有语法
###关键字【GET,DEL,LEN,INSERT,LPUSH,RPUSH,LPOP,RPOP,SET,EXISTS】
{
    [LPUSH value;]#标准 LIST_LPUSH 语句，在列表开头添加一个值

    [RPUSH value;]#标准 LIST_RPUSH 语句，在列表末尾添加一个值

    [LPOP;]#标准 LIST_LPOP 语句，移除并返回列表开头的值

    [RPOP;]#标准 LIST_RPOP 语句，移除并返回列表末尾的值


    [GET index;] #标准 LIST_GET 语句,获取指定位置的值,index:1,2..为第1,2..个元素，-1,-2,为倒数第1,2..个元素，0:所有元素
        衍生&&:GET ALL;#获取所有值

    [DEL index;] #标准 LIST_DEL 语句，删除指定索引位置的值

    [GET LEN index;]#标准 LIST_GET_INDEX_LEN 语句，获取列表中指定索引位置的值的长度

    [INSERT index value;] #标准 LIST_INSERT 语句，在指定索引位置插入一个值

    [SET index value;]#标准 LIST_SET 语句，更新列表中指定索引位置的值

    [EXISTS value1 value2 ...;]#标准 LIST_EXISTS 语句，判断列表是否存在指定值

    [GET LEN;] #标准 LIST_GET_LEN 语句，获取列表的长度
}
*/
#define stmtype_LIST_LPUSH 41
#define stmtype_LIST_RPUSH 42
#define stmtype_LIST_LPOP 43
#define stmtype_LIST_RPOP 44
#define stmtype_LIST_GET 45
#define stmtype_LIST_DEL 46
#define stmtype_LIST_GET_INDEX_LEN 47
#define stmtype_LIST_INSERT 48
#define stmtype_LIST_SET 49
#define stmtype_LIST_EXISTS 50
/*
##下面是操作对象为BITMAP时的所有语法@MHU
###关键【SET,GET,COUNT】
{
    [SET offset value;] #标准 BITMAP_SET 语句，设置位图中指定偏移量处的位值，value为0或1

    [SET offset1 offset2 value;] #标准 BITMAP_SET_RANGE 语句，设置位图中指定偏移量范围内的位值，value为0或1

    [GET offset;] #标准 BITMAP_GET 语句，获取位图中指定偏移量处的位值

    [GET offset1 offset2;]#标准 BITMAP_GET_RANGE 语句，获取位图中指定偏移量范围内的位值

    [COUNT;] #标准 BITMAP_COUNT 语句，统计位图中值为1的位数

    [COUNT offset1 offset2;] #标准 BITMAP_COUNT_RANGE 语句，统计位图中指定偏移量范围内值为1的位数        
}
*/
#define stmtype_BITMAP_SET 51
#define stmtype_BITMAP_SET_RANGE 52
#define stmtype_BITMAP_GET 53
#define stmtype_BITMAP_GET_RANGE 54
#define stmtype_BITMAP_COUNT 55
#define stmtype_BITMAP_COUNT_RANGE 56

commendID is_valid_statement(stmt curstmt,stmtObj* lastobj,stmtObj* curobj) {
    /*
    is_valid_statement
    函数功能：检查当前语句是否符合语法规则,返回当前语句的操作对象类型
    用法：
    is_valid_statement(对象语句,传入指针,传出指针);

    返回值：
    err：语法错误 正整数：语法正确
    */
    if(curstmt.num >  MAX_TOKENS_PER_STATEMENT){
        return err;
    }
    int tokenseq = 1;
for(;;)//循环问询每一个token
{
    switch (curstmt.tokens[0].type) 
    {
        case TOKEN_WHERE:
            if(tokenseq == 1){
                if(curstmt.num != 1){
                    return err;
                }
                *curobj = *lastobj;//操作对象保持不变
                //WHERE;#返回当前操作对象的信息，返回一个json格式的字符串
                return WHERE_E;
            }
            //
            
            break;
    }
}
    return 1;
}
/*
【HOOK】【TABLE,KVALOT,LIST,BITMAP,STREAM】
【DEL,TYPE,RANK,CLEAR,DESC,GET,TEMP,APPEND,LEN,LPUSH,RPUSH,LPOP,RPOP,EXISTS,COUNT】
【INSERT,FIELD,SET,ADD,SWAP,RENAME,ATTRIBUTE,POS,WHERE,SELECT,KEY】
【[i1,int8_t],[i2,int16_t],[i4,int32_t,int],[i8,int64_t],
[ui1,uint8_t],[ui2,uint16_t],[ui4,uint32_t],[ui8,uint64_t],
[f4,float],[f8,double],[str,stream],date,time,datetime】
【PKEY,FKEY,UNIQUE,NOTNULL,DEFAULT】
*/
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

    TOKEN_NAME,       // 引用
    TOKEN_HANDLE_NAME,       // 引用的名称
    TOKEN_OBJECT_NAME,       // 数据结构对象的名称

    TOKEN_VALUES,       // 数字值

    TOKEN_END,          // 结束符：语句结束符，即';'

    TOKEN_EEROR,         // token错误,一般遇到这个token,则视为用户 语句错误
} TokType,toktype;

int callitkeyword(uint8_t* str,int len,toktype *type)
{   
    if(len==2){
        if(!strncmp(str,"i1",2)){
            *type=TOKEN_i1;
            return 0;
        }
        if(!strncmp(str,"i2",2)){
            *type=TOKEN_i2;
            return 0;
        }
        if(!strncmp(str,"i4",2)){
            *type=TOKEN_i4;
            return 0;
        }
        if(!strncmp(str,"i8",2)){
            *type=TOKEN_i8;
            return 0;
        }
        if(!strncmp(str,"f4",2)){
            *type=TOKEN_f4;
            return 0;
        }
        if(!strncmp(str,"f8",2)){
            *type=TOKEN_f8;
            return 0;
        }
        return NOTKEYWORD;
    }
    if(len==3){
        if(!strncmp(str,"ui1",3)){
            *type=TOKEN_ui1;
            return 0;
        }
        if(!strncmp(str,"ui2",3)){
            *type=TOKEN_ui2;
            return 0;
        }
        if(!strncmp(str,"ui4",3)){
            *type=TOKEN_ui4;
            return 0;
        }
        if(!strncmp(str,"ui8",3)){
            *type=TOKEN_ui8;
            return 0;
        }
        if(!strncmp(str,"KEY",3) || !strncmp(str,"key",3)){
            *type=TOKEN_KEY;
            return 0;
        }
        if(!strncmp(str,"DEL",3) ||!strncmp(str,"del",3)){
            *type=TOKEN_DEL;
            return 0;
        }
        if(!strncmp(str,"GET",3) ||!strncmp(str,"get",3)){
            *type=TOKEN_GET;
            return 0;
        }
        if(!strncmp(str,"SET",3) ||!strncmp(str,"set",3)){
            *type=TOKEN_SET;
            return 0;
        }
        if(!strncmp(str,"ADD",3) ||!strncmp(str,"add",3)){
            *type=TOKEN_ADD;
            return 0;
        }
        if(!strncmp(str,"LEN",3) ||!strncmp(str,"len",3)){
            *type=TOKEN_LEN;
            return 0; 
        }
        if(!strncmp(str,"POS",3) ||!strncmp(str,"pos",3)){
            *type=TOKEN_POS;
            return 0;
        }
        return NOTKEYWORD;
    }
    if(len==4){
        if(!strncmp(str,"TYPE",4) ||!strncmp(str,"type",4)){
            *type=TOKEN_TYPE;
            return 0;
        }
        if(!strncmp(str,"HOOK",4) ||!strncmp(str,"hook",4)){
            *type=TOKEN_HOOK;
            return 0;
        }
        if(!strncmp(str,"LIST",4) ||!strncmp(str,"List",4)){
            *type=TOKEN_LIST;
            return 0;
        }
        if(!strncmp(str,"SWAP",4) ||!strncmp(str,"swap",4)){
            *type=TOKEN_SWAP;
            return 0;
        }
        if(!strncmp(str,"LPOP",4) ||!strncmp(str,"lpop",4)){
            *type=TOKEN_LPOP;
            return 0;
        }
        if(!strncmp(str,"RPOP",4) ||!strncmp(str,"rpop",4)){
            *type=TOKEN_RPOP;
            return 0;
        }
        if(!strncmp(str,"DESC",4) ||!strncmp(str,"desc",4)){
            *type=TOKEN_DESC;
            return 0;
        }
        if(!strncmp(str,"TYPE",4) ||!strncmp(str,"type",4)){
            *type=TOKEN_TYPE;
            return 0;
        }
        if(!strncmp(str,"PKEY",4) ||!strncmp(str,"pkey",4)){
            *type=TOKEN_PKEY;
            return 0;
        }
        if(!strncmp(str,"FKEY",4) ||!strncmp(str,"fkey",4)){
            *type=TOKEN_FKEY;
            return 0;
        }
        if(!strncmp(str,"RANK",4) ||!strncmp(str,"rank",4)){
            *type=TOKEN_RANK;
            return 0;
        }
        if(!strncmp(str,"TEMP",4) ||!strncmp(str,"temp",4)){
            *type=TOKEN_TEMP;
            return 0;
        }
        return NOTKEYWORD;
    }
    if(len==5){
        if(!strncmp(str,"TABLE",5) ||!strncmp(str,"table",5)){
            *type=TOKEN_TABLE;
            return 0;
        }
        if(!strncmp(str,"COUNT",5) ||!strncmp(str,"count",5)){
            *type=TOKEN_COUNT;
            return 0;
        }
        if(!strncmp(str,"CLEAR",5) ||!strncmp(str,"clear",5)){
            *type=TOKEN_CLEAR;
            return 0;
        }
        if(!strncmp(str,"WHERE",5) ||!strncmp(str,"where",5)){
            *type=TOKEN_WHERE;
            return 0;
        }
        if(!strncmp(str,"LPUSH",5) ||!strncmp(str,"lpush",5)){
            *type=TOKEN_LPUSH;
            return 0;
        }
        if(!strncmp(str,"RPUSH",5) ||!strncmp(str,"rpush",5)){
            *type=TOKEN_RPUSH;
            return 0;
        }
        if(!strncmp(str,"FIELD",5) ||!strncmp(str,"field",5)){
            *type=TOKEN_FIELD;
            return 0;
        }
        if(!strncmp(str,"FLOAT",5) ||!strncmp(str,"float",5)){
            *type=TOKEN_f4;
            return 0;
        }
        return NOTKEYWORD;
    }
    if(len==6){
        if(!strncmp(str,"BITMAP",6) ||!strncmp(str,"bitmap",6)){
            *type=TOKEN_BITMAP;
            return 0;
        }
        if(!strncmp(str,"STREAM",6) ||!strncmp(str,"stream",6)){
            *type=TOKEN_STREAM;
            return 0;
        }
        if(!strncmp(str,"EXISTS",6) ||!strncmp(str,"exists",6)){
            *type=TOKEN_EXISTS;
            return 0;
        }
        if(!strncmp(str,"SELECT",6) ||!strncmp(str,"select",6)){
            *type=TOKEN_SELECT;
            return 0;
        }
        if(!strncmp(str,"APPEND",6) ||!strncmp(str,"append",6)){
            *type=TOKEN_APPEND;
            return 0;
        }
        if(!strncmp(str,"UNIQUE",6) ||!strncmp(str,"unique",6)){
            *type=TOKEN_UNIQUE;
            return 0;
        }
        if(!strncmp(str,"KVALOT",6) ||!strncmp(str,"kvalot",6)){
            *type=TOKEN_KVALOT;
            return 0;
        }
        if(!strncmp(str,"INSERT",6) ||!strncmp(str,"insert",6)){
            *type=TOKEN_INSERT;
            return 0;
        }
        if(!strncmp(str,"RENAME",6) ||!strncmp(str,"rename",6)){
            *type=TOKEN_RENAME;
            return 0;
        }
        if(!strncmp(str,"int8_t",6)){
            *type=TOKEN_i1;
            return 0;
        }
        if(!strncmp(str,"double",6)){
            *type=TOKEN_f8;
            return 0;
        }
        return NOTKEYWORD;
    }
    if(len==7){
        if(!strncmp(str,"DEFAULT",7) ||!strncmp(str,"default",7)){
            *type=TOKEN_DEFAULT;
            return 0;
        }
        if(!strncmp(str,"NOTNULL",7) ||!strncmp(str,"notnull",7)){
            *type=TOKEN_NOTNULL;
            return 0;
        }
        if(!strncmp(str,"uint8_t",7)){
            *type=TOKEN_ui1;
            return 0;
        }
        if(!strncmp(str,"int16_t",7)){
            *type=TOKEN_i2;
            return 0;
        }
        if(!strncmp(str,"int32_t",7)){
            *type=TOKEN_i4;
            return 0;
        }
        if(!strncmp(str,"int64_t",7)){
            *type=TOKEN_i8;
            return 0;
        }
        return NOTKEYWORD;
    }
    if(len==8){
        if(!strncmp(str,"uint16_t",8)){
            *type=TOKEN_ui2;
            return 0;
        }
        if(!strncmp(str,"uint32_t",8)){
            *type=TOKEN_ui4;
            return 0;
        }
        if(!strncmp(str,"uint64_t",8)){
            *type=TOKEN_ui8;
            return 0;
        }
        return NOTKEYWORD;
    }
    if(len==9){
        if(!strncmp(str,"ATTRIBUTE",9) ||!strncmp(str,"attribute",9)){
            *type=TOKEN_ATTRIBUTE;
            return 0;
        }
        return NOTKEYWORD;
    }
    return NOTKEYWORD;
}
