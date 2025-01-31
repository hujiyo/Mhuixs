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
#include "stream.h"
#include "Mhudef.h"
#include "Cmhuix.h"


//判断是否是token的起始字符
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

typedef struct inputstr{
    uint8_t *string;//待处理的字符串
    uint32_t len;
    uint8_t *pos;//指向string中待处理的字符
}inputstr;

typedef struct Token{
    str* content;//TOKEN的字面信息
    toktype type;//TOKEN的类型
} Token,tok;//符号（令牌）结构体

typedef struct statement{
    tok** tokens;
    int num;//token的数量
} statement,stmt;//单语句操作结构体

typedef struct stmts{
    stmt** stmt;
    int num;//语句的数量
} statements,stmts;//多语句操作结构体;

Token* getoken(inputstr* instr)//返回的token记得释放
{
    Token *token = (Token*)malloc(sizeof(Token));

    /*
    请保证instr合法
    pos必须在string的合法范围内
    
    */

    /*
    特殊的字符:
    语句结束:    ;
    字符串:  "   '      '只有再""内才有效
    */
    int is_string = 0;//是否是字符串
    

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
                                token->content = bcpstr("\n",1);
                                break;
                            case 't':
                                token->content = bcpstr("\t",1);
                                break;
                            case 'r':
                                token->content = bcpstr("\r",1);
                                break;
                            case '0':
                                token->content = bcpstr("\0",1);
                                break;
                            case '\\':
                                token->content = bcpstr("\\",1);
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
                        token->content = bcpstr(instr->pos,1);
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
    token->content = bcpstr(instr->pos,ed_pos - instr->pos);
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
        else if(!strncmp(token->content->string,"STACK",5)||!strncmp(token->content->string,"stack",5)){
            token->type = TOKEN_STACK_TYPE;
        }
        else if(!strncmp(token->content->string,"QUEUE",5)||!strncmp(token->content->string,"queue",5)){
            token->type = TOKEN_QUEUE_TYPE;
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
                        freeSTREAM(token->content);
                        token->content = NULL;
                        return token;
                    }
                }
                //是规范的数字
                token->type = TOKEN_VALUES;
                return token;
            }
            token->type = TOKEN_NAME;
        }
    }

    return token;
}

str* lexer(str* Mhuixsentence)
{
    /*
    词法分析器
    lexer会接管Mhuixsentence的内存并自动释放
    Mhuixsentence:待处理的字符串：C语言字符串
    */

    //初始化inputstr
    inputstr instr;
    instr.string = (uint8_t*)malloc(Mhuixsentence->len);//str不计入'\0'
    if(instr.string == NULL){
        return NULL;
    }
    instr.len = Mhuixsentence->len;
    instr.pos = instr.string;
    //将Mhuixsentence的内容复制到instr.string中
    memcpy(instr.string,Mhuixsentence->string,Mhuixsentence->len);
    freeSTREAM(Mhuixsentence);//释放Mhuixsentence的内存

    //初始化stmts
    stmts stmts;
    
    //获得语句
    for(;;){
        //获取token
        Token* token = getoken(&instr);
        if(token == NULL){
            //用户输入的字符串已经读取结束
            goto end;
        }
        else if(token->type == TOKEN_EEROR){
            //用户输入的字符串错误
            //...
            //...
            //...
            return NULL;
        }
        else if(token->type == TOKEN_END){
            //一个语句解析结束
            //解析语句是否符合规范
            //加入stmts中

        }
        //将token添加到stmts中
        stmts.stmt = (stmt*)realloc(stmts.stmt,stmts.num+1);
        if(stmts.stmt == NULL){
            return NULL;
        }
    }
    
    end://没有语法错误，已经解析为多个语句，每个语句语法正确



    
}