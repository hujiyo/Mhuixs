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
//#include "Mhudef.h"
//#include "Cmhuix.h"

#define MAX_STATEMENTS 100 //一次性能够处理的最大语句数量
#define MAX_TOKENS_PER_STATEMENT 100 //单个语句的最大token数量

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

/*
下面是token的所有类型的枚举
*/
typedef enum {
    TOKEN_HOOK,         //引用：钩子
    TOKEN_KEY,          //引用：键

    TOKEN_TABLE_TYPE,        //表类型
    TOKEN_KVALOT_TYPE,       //键库类型
    TOKEN_LIST_TYPE,         //列表类型
    TOKEN_BITMAP_TYPE,       //位图类型
    TOKEN_STREAM_TYPE,       //流类型
    TOKEN_STACK_TYPE,        //栈类型
    TOKEN_QUEUE_TYPE,        //队列类型

    TOKEN_SET,          // 设置
    TOKEN_INSERT,       // 插入
    TOKEN_ADD,          // 添加
    
    TOKEN_FIELD,        // 字段
    TOKEN_LINE,         // 行
    TOKEN_RANK,         // 保护等级

    TOKEN_NAME,       // 引用
    TOKEN_HANDLE_NAME,       // 引用的名称
    TOKEN_OBJECT_NAME,       // 数据结构对象的名称

    TOKEN_AT,           
    TOKEN_ALTER,

    TOKEN_VALUES,       // 数字值
    TOKEN_STREAM,         // 字符信息

    TOKEN_END,          // 结束符：语句结束符，即';'

    TOKEN_EEROR,         // token错误,一般遇到这个token,则视为用户 语句错误
} TokType,toktype;

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
        //判断token的类型
        if(!strncmp(token->content->string,"SET",3)||!strncmp(token->content->string,"set",3)){
            token->type = TOKEN_SET;
        }
        else if(!strncmp(token->content->string,"INSERT",6)||!strncmp(token->content->string,"insert",6)){
            token->type = TOKEN_INSERT;
        }
        else if(!strncmp(token->content->string,"ADD",3)||!strncmp(token->content->string,"add",3)){
            token->type = TOKEN_ADD;
        }
        else if(!strncmp(token->content->string,"AT",2)||!strncmp(token->content->string,"at",2)){
            token->type = TOKEN_AT;
        }
        else if(!strncmp(token->content->string,"ALTER",5)||!strncmp(token->content->string,"alter",5)){
            token->type = TOKEN_ALTER;
        }
        else if(!strncmp(token->content->string,"TABLE",5)||!strncmp(token->content->string,"table",5)){
            token->type = TOKEN_TABLE_TYPE;
        }
        else if(!strncmp(token->content->string,"KEYLOT",6)||!strncmp(token->content->string,"kvalot",6)
                ||!strncmp(token->content->string,"KEYLOT",6)||!strncmp(token->content->string,"keylot",6)){
            token->type = TOKEN_KVALOT_TYPE;
        }
        else if(!strncmp(token->content->string,"LIST",4)||!strncmp(token->content->string,"list",4)){
            token->type = TOKEN_LIST_TYPE;
        }
        else if(!strncmp(token->content->string,"BITMAP",6)||!strncmp(token->content->string,"bitmap",6)){
            token->type = TOKEN_BITMAP_TYPE;
        }
        else if(!strncmp(token->content->string,"STREAM",6)||!strncmp(token->content->string,"stream",6)){
            token->type = TOKEN_STREAM_TYPE;
        }
        else if(!strncmp(token->content->string,"FIELD",5)||!strncmp(token->content->string,"field",5)){
            token->type = TOKEN_FIELD;
        }
        else if(!strncmp(token->content->string,"LINE",4)||!strncmp(token->content->string,"line",4)){
            token->type = TOKEN_LINE;
        }
        else if(!strncmp(token->content->string,"RANK",4)||!strncmp(token->content->string,"rank",4)){
            token->type = TOKEN_RANK;
        }
        else if(!strncmp(token->content->string,"HOOK",4)||!strncmp(token->content->string,"hook",4)){
            token->type = TOKEN_HOOK;
        }
        else if(!strncmp(token->content->string,"KEY",3)||!strncmp(token->content->string,"key",3)){
            token->type = TOKEN_KEY;
        }
        else{
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

str* lexer(str* Mhuixsentence)
{
    /*
    词法分析器
    将用户输入的字符串转换为多个stmt，之后再进行语法分析。
    Mhuixsentence:待处理的字符串：C语言字符串

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
    stmtObj last_obj = Obj_NULL;
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


int is_valid_statement(stmt curstmt) {
    /*
    is_valid_statement
    函数功能：检查当前语句是否符合语法规则

    返回值：
    0：语法错误 1：语法正确
    */
    
    for(int i = 0; i < curstmt.num; i++){

    }
    /*
    《Mhuixs查询语言设计草案》
    Mhuixs查询语言设计旨在提供一种全面兼容NoSQL与SQL语言，同时支持Mhuixs特性 
    基本思路：HOOK关键字用来指定某个数据结构对象，指定完对象后就可以使用对象类型对应的关键字了，比如使用HOOK指定完TABLE类型后就可以使用SQL语言对表进行操作，再比如使用HOOK指定完KVALOT类型后就可以使用类似redis的键值对操作命令来操作它了……
    我能想到的关键字如下：
    v.	HOOK
    n.	TABLE、KVALOT、LIST、BITMAP、STREAM、STACK、QUEUE
    n.	KEY、FIELD、LINE、RANK、NAME、- 、VALUES
    v.	SET、INSERT、ADD
    v.	
    m.	AT
    sql.	ALTER

    ####下面是特殊语法
    关键字【[quit,exit]】
    {
        quit/exit; #关闭与SQLite数据库的连接
    }

    ####下面是HOOK所有使用方法@MHU
    关键字【HOOK,GET,WHERE】【TABLE,KVALOT,LIST,BITMAP,STREAM】【DEL,TYPE,RANK,CLEAR】
    {
        #HOOK操作
        WHERE; #返回当前操作对象的信息，返回一个json格式的字符串
        HOOK; #回归HOOK根，此时无数据操作对象
        HOOK objtype objname1 objname2 ...; #使用钩子创建一个操作对象
        HOOK objname; #手动切换到一个已经存在的操作对象
        HOOK DEL WHERE; #删除本操作对象,退回HOOK根
        HOOK CLEAR objname1 objname2 ...;#清空当前操作对象的所有数据,但保留操作对象及其信息
        HOOK DEL objname1 objname2 ...; #删除指定的HOOK操作对象
        RANK WHERE rank; #设置当前对象的权限等级为rank
        RANK objname rank; #设置指定HOOK操作对象的权限等级为rank
        GET TYPE WHERE; #获取当前操作对象的类型
        GET RANK; #获取当前操作对象的权限等级
        GET RANK objname; #获取指定HOOK操作对象的权限等级
        GET TYPE objname; #获取指定HOOK操作对象的类型
    }

    ####下面是操作对象为TABLE时的所有语法@MHU
    关键字【TABLE】【HOOK】【ALTER,FIELD,SET,ADD,SWAP,REMOVE,RENAME,ATTRIBUTE,AT】
    {
        HOOK TABLE mytable; #创建一个名为mytable的表,此时操作对象为mytable
        #FILED操作
        FIELD ADD (field1_name datatype restraint,...);#从左向右添加字段
        FIELD INSERT (field1_name datatype restraint) AT field_number; #在指定位置插入字段
        FIELD SWAP field1_name field2_name; #交换两个字段
        FIELD REMOVE field1_name field2_name ...; #删除字段
        FIELD RENAME field1_name field2_name ...; #重命名字段
        FIELD SET field1_name ATTRIBUTE attribute; #重新设置字段约束性属性
        #LINE操作,不指明FIELD,默认就是整个LINE进行操作
        ADD (line1_data,NULL,...); #在表末尾添加一行数据，数据按照字段顺序依次给出
        INSERT (line1_data,NULL,...) AT line_number; #在指定行号处插入一行数据，line_number从0开始计数
        UPDATE line_number SET (field1_name = value1, field2_name = value2,...); #更新指定行号的部分数据
        REMOVE line_number; #删除指定行号的行数据
        SWAP line_number1 line_number2; #交换两行数据
        #GET简单查询操作
        GET FIELD field_name;#获取指定字段对应的列的数据
        GET line_number1 line_number2 ...; #获取指定行号的多行数据
        GET COORDINATE x1 y1 x2 y2 ...; #获取指定坐标范围内的多行数据
        GET ALL; #获取所有数据
    }

    ####下面是操作对象为KVALOT时的所有语法@MHU
    关键字【KVALOT】【HOOK】【SET,GET,DEL,INCR,DECR】
    {
        HOOK KVALOT mykvalot; #创建一个名为mykvalot的键值对存储对象，此时操作对象为mykvalot
        EXISTS key1 key2 ...; #判断存在几个键
        KEYS pattern; #查找所有符合给定模式的键
        SET key1 value1 key2 value2 ...; #设置键值对，若键已存在则覆盖
        APPEND key value;# 将value追加到key的值中。
        APPEND key value pos;# 将value追加到key的值中，从指定位置开始。若pos超出key值的长度，则从key值的末尾开始追加。
        GET key1 key2 ...; #获取指定键的值
        GET key0 FROM start end;#获取指定键的值的子字符串 
        DEL key1 key2 ...; #删除指定键
        INCR KEY num; #对指定键的值进行递增操作，num为可选参数，默认递增1
        DECR KEY num; #对指定键的值进行递减操作，num为可选参数，默认递减1
        TYPE key1 key2 ...; #获取指定键的值的数据类型
        LEN key1 key2 ...; #获取指定键的值的长度
    }

Key操作命令


哈希类型操作命令
• `hset hash key field value`：设置哈希表中字段的值。
• `hget key field`：获取哈希表中字段的值。
• `hmset key field value [field value…]`：同时设置哈希表中多个字段的值。
• `hmget key field [field…]`：获取哈希表中多个字段的值。
• `hgetall key`：获取哈希表中所有字段和值。
• `hdel key field [field…]`：删除哈希表中的字段。
• `hkeys key`：获取哈希表中所有字段名。
• `hvals key`：获取哈希表中所有字段值。
• `hexists key field`：判断哈希表中字段是否存在。

表类型操作命令
• `lpush key value [value…]`：将值插入到列表头部。
• `rpush key value [value…]`：将值插入到列表尾部。
• `lrange key start stop`：获取列表中指定范围的元素。
• `lindex key index`：获取列表中指定索引的元素。
• `llen key`：获取列表的长度。
• `lrem key count value`：移除列表中指定值的元素。
• `lset key index value`：设置列表中指定索引的值。
• `linsert key BEFORE|AFTER pivot value`：在列表中指定位置插入值。
• `lpop key`：移除并返回列表的第一个元素。
• `rpop key`：移除并返回列表的最后一个元素。

集合类型操作命令
• `sadd key member [member…]`：将成员添加到集合中。
• `smembers key`：获取集合中的所有成员。
• `sismember key member`：判断成员是否在集合中。
• `scard key`：获取集合的元素个数。
• `srem key member [member…]`：从集合中移除成员。
• `srandmember key [count]`：随机返回集合中的成员。
• `spop key [count]`：随机移除并返回集合中的成员。

有序集合类型操作命令
• `zadd key score member [score member…]`：将成员及其分数添加到有序集合中。
• `zrange key start stop [WITHSCORES]`：获取有序集合中指定范围的成员。
• `zrevrange key start stop [WITHSCORES]`：获取有序集合中指定范围的成员（逆序）。
• `zrem key member [member…]`：从有序集合中移除成员。
• `zcard key`：获取有序集合的元素个数。
• `zrangebyscore key min max [WITHSCORES] [LIMIT offset count]`：获取有序集合中指定分数范围的成员。
• `zrevrangebyscore key max min [WITHSCORES] [LIMIT offset count]`：获取有序集合中指定分数范围的成员（逆序）。
• `zcount key min max`：统计有序集合中指定分数范围的成员数量。
• `zrank key member`：获取成员在有序集合中的排名。
    }

    ####下面是操作对象为LIST时的所有语法@MHU
    关键字【LIST】【HOOK】【ADD,GET,DEL,LEN,INSERT,UPDATE】
    {
        HOOK LIST mylist; #创建一个名为mylist的列表对象，此时操作对象为mylist
        LPUSH value;#在列表开头添加一个值
        RPUSH/ADD value;#在列表末尾添加一个值
        LPOP/GET; #移除并返回列表开头的值
        RPOP; #移除并返回列表末尾的值

        GET index; #获取指定位置的值,index:1,2..为第1,2..个元素，-1,-2,为倒数第1,2..个元素,0表示获取列表长度
        DEL index; #删除指定索引位置的值
        LEN; #获取列表的长度
        INSERT value AT index; #在指定索引位置插入一个值
        SET value AT index; #更新列表中指定索引位置的值
    }

    ####下面是操作对象为BITMAP时的所有语法@MHU
    关键字【BITMAP】【HOOK】【SETBIT,GETBIT,COUNT,BITOP】
    {
        HOOK BITMAP mybitmap; #创建一个名为mybitmap的位图对象，此时操作对象为mybitmap
        SETBIT offset value; #设置位图中指定偏移量处的位值，value为0或1
        GETBIT offset; #获取位图中指定偏移量处的位值
        COUNT; #统计位图中值为1的位数
        BITOP operation mybitmap1 mybitmap2 [result_bitmap]; #对两个位图进行位运算，operation可以是AND、OR、XOR等，result_bitmap为可选参数，用于存储运算结果
    }

    ####下面是操作对象为STREAM时的所有语法@MHU
    关键字【STREAM】【HOOK】【ADD,GET,RANGE,DEL】
    {
        HOOK STREAM mystream; #创建一个名为mystream的流对象，此时操作对象为mystream
        ADD (field1: value1, field2: value2,...); #向流中添加一条消息，包含多个字段和对应值
        GET ID; #获取流中指定ID的消息
        RANGE start_id end_id; #获取流中指定ID范围的消息，start_id和end_id可以是具体ID或特殊符号表示开始和结束
        DEL ID; #删除流中指定ID的消息
    }

    ####下面是操作对象为TABLE时的所有语法（为了兼容SQL，故Mhuixs支持两套语法来操作表TABLE）@SQL
    关键字【TABLE】【[describe,desc],CREATE,INSERT,INTO,VALUES,SELECT,UPDATE,DELETE,ALTER
                    WHERE,ORDER,BY,LIMIT,OFFSET,ASC,DESC,INNER,JOIN,LEFT,RIGHT,OUTER,
                    FULL,ON,AND,OR,NOT,IN,】
    【[i1,int8_t],[i2,int16_t],[i4,int32_t,int],[i8,int64_t],
    [ui1,uint8_t],[ui2,uint16_t],[ui4,uint32_t],[ui8,uint64_t],
    [f4,float],[f8,double],
    [str,stream],
    [date],[time],[datetime]】
    【PKEY,FKEY,UNIQUE,NOTNULL,DEFAULT】//不填则默认为DEFAULT
    {
        describe/desc mytable; #查看表结构        
        CREATE TABLE table_name (field1_name datatype restraint,...);# 创建表第三个参数是约束性条件
        
        INSERT INTO table_name (column1, column2, column3,...)VALUES (value1, value2, value3,...);# 加入一条数据
        SELECT column1, column2,... FROM table_name WHERE condition;# 查询表
        UPDATE table_name SET column1 = value1, column2 = value2,... WHERE condition;# 更新数据
        DELETE FROM table_name WHERE condition;# 删除数据
        ALTER TABLE users ADD COLUMN age INT AFTER name;# 新列在name后面

        DROP TABLE [IF EXISTS] table_name;    
    }

    */
}
