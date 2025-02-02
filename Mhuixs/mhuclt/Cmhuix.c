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
    lexer会接管Mhuixsentence的内存并自动释放！！！
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
    stmts.stmt = NULL;
    stmts.num = 0;

    //初始化当前语句
    stmt current_stmt;
    current_stmt.tokens = NULL;
    current_stmt.num = 0;

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
            free(current_stmt.tokens);
            for(int i = 0; i < stmts.num; i++){
                for(int j = 0; j < stmts.stmt[i]->num; j++){
                    freeSTREAM(stmts.stmt[i]->tokens[j]->content);
                    free(stmts.stmt[i]->tokens[j]);
                }
                free(stmts.stmt[i]->tokens);
                free(stmts.stmt[i]);
            }
            free(stmts.stmt);
            return NULL;
        }
        else if(token->type == TOKEN_END){
            //一个语句解析结束
            //语法检查
            if (!is_valid_statement(current_stmt)) {
                //语法错误
                free(current_stmt.tokens);
                for(int i = 0; i < stmts.num; i++){
                    for(int j = 0; j < stmts.stmt[i]->num; j++){
                        freeSTREAM(stmts.stmt[i]->tokens[j]->content);
                        free(stmts.stmt[i]->tokens[j]);
                    }
                    free(stmts.stmt[i]->tokens);
                    free(stmts.stmt[i]);
                }
                free(stmts.stmt);
                return NULL;
            }

            //将当前语句添加到stmts中
            stmts.stmt = (stmt*)realloc(stmts.stmt, (stmts.num + 1) * sizeof(stmt));
            if(stmts.stmt == NULL){
                free(current_stmt.tokens);
                for(int i = 0; i < stmts.num; i++){
                    for(int j = 0; j < stmts.stmt[i]->num; j++){
                        freeSTREAM(stmts.stmt[i]->tokens[j]->content);
                        free(stmts.stmt[i]->tokens[j]);
                    }
                    free(stmts.stmt[i]->tokens);
                    free(stmts.stmt[i]);
                }
                free(stmts.stmt);
                return NULL;
            }
            stmts.stmt[stmts.num] = (stmt*)malloc(sizeof(stmt));
            if(stmts.stmt[stmts.num] == NULL){
                free(current_stmt.tokens);
                for(int i = 0; i < stmts.num; i++){
                    for(int j = 0; j < stmts.stmt[i]->num; j++){
                        freeSTREAM(stmts.stmt[i]->tokens[j]->content);
                        free(stmts.stmt[i]->tokens[j]);
                    }
                    free(stmts.stmt[i]->tokens);
                    free(stmts.stmt[i]);
                }
                free(stmts.stmt);
                return NULL;
            }
            stmts.stmt[stmts.num]->tokens = current_stmt.tokens;
            stmts.stmt[stmts.num]->num = current_stmt.num;
            stmts.num++;

            //重置当前语句
            current_stmt.tokens = NULL;
            current_stmt.num = 0;
        }
        else{
            //将token添加到当前语句中
            current_stmt.tokens = (tok**)realloc(current_stmt.tokens, (current_stmt.num + 1) * sizeof(tok*));
            if(current_stmt.tokens == NULL){
                free(token->content);
                free(token);
                free(current_stmt.tokens);
                for(int i = 0; i < stmts.num; i++){
                    for(int j = 0; j < stmts.stmt[i]->num; j++){
                        freeSTREAM(stmts.stmt[i]->tokens[j]->content);
                        free(stmts.stmt[i]->tokens[j]);
                    }
                    free(stmts.stmt[i]->tokens);
                    free(stmts.stmt[i]);
                }
                free(stmts.stmt);
                return NULL;
            }
            current_stmt.tokens[current_stmt.num] = token;
            current_stmt.num++;
        }
    }
    
    end://没有语法错误，已经解析为多个语句，每个语句语法正确
    //将stmts转换为str并返回
    str* result = (str*)malloc(sizeof(str));
    if(result == NULL){
        for(int i = 0; i < stmts.num; i++){
            for(int j = 0; j < stmts.stmt[i]->num; j++){
                freeSTREAM(stmts.stmt[i]->tokens[j]->content);
                free(stmts.stmt[i]->tokens[j]);
            }
            free(stmts.stmt[i]->tokens);
            free(stmts.stmt[i]);
        }
        free(stmts.stmt);
        return NULL;
    }
    result->string = (uint8_t*)malloc(1);
    result->len = 0;
    if(result->string == NULL){
        free(result);
        for(int i = 0; i < stmts.num; i++){
            for(int j = 0; j < stmts.stmt[i]->num; j++){
                freeSTREAM(stmts.stmt[i]->tokens[j]->content);
                free(stmts.stmt[i]->tokens[j]);
            }
            free(stmts.stmt[i]->tokens);
            free(stmts.stmt[i]);
        }
        free(stmts.stmt);
        return NULL;
    }
    result->string[0] = '\0';

    for(int i = 0; i < stmts.num; i++){
        for(int j = 0; j < stmts.stmt[i]->num; j++){
            result = str_append(result, stmts.stmt[i]->tokens[j]->content);
            if(result == NULL){
                for(int k = 0; k < stmts.stmt[i]->num; k++){
                    freeSTREAM(stmts.stmt[i]->tokens[k]->content);
                    free(stmts.stmt[i]->tokens[k]);
                }
                free(stmts.stmt[i]->tokens);
                free(stmts.stmt[i]);
            }
            free(stmts.stmt[i]->tokens[j]->content);
            free(stmts.stmt[i]->tokens[j]);
        }
        free(stmts.stmt[i]->tokens);
        free(stmts.stmt[i]);
    }
    free(stmts.stmt);

    return result;
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

表操作语法
示例一：
HOOK TABLE table1 #使用钩子创建一个表,权限默认为执行者权限
#之后Mhuixs默认的执行对象就是table1，表可以使用SQL语言进行操作

此处SQL语言操作略

HOOK TABLE table2 #再次使用HOOK，会自动切换回HOOK指向的新对象
HOOK table1 #切回table1
HOOK RANK 200 #上一句为创建table2，则默认对象为table2，使用-占位
HOOK table2 RANK 200 #HOOK后紧跟表名表示切回table2，之后设置钩子权限等级为200,则相当于table1的权限等级为200

*/
// 新增语法检查函数
int is_valid_statement(stmt current_stmt) {
    // 这里可以添加具体的语法检查逻辑
    // 例如，检查是否包含必要的关键字，检查关键字的顺序等
    // 这里仅提供一个简单的示例，实际应用中需要更复杂的逻辑

    // 语句至少需要两个token
    if (current_stmt.num < 2) {
        return 0;
    }

    // 第一个token必须是HOOK
    if (current_stmt.tokens[0]->type != TOKEN_HOOK) {
        return 0;
    }

    // 第二个token必须是有效的数据结构类型
    toktype second_token_type = current_stmt.tokens[1]->type;
    if (second_token_type != TOKEN_TABLE_TYPE &&
        second_token_type != TOKEN_KVALOT_TYPE &&
        second_token_type != TOKEN_LIST_TYPE &&
        second_token_type != TOKEN_BITMAP_TYPE &&
        second_token_type != TOKEN_STREAM_TYPE &&
        second_token_type != TOKEN_STACK_TYPE &&
        second_token_type != TOKEN_QUEUE_TYPE) {
        return 0;
    }

    // 如果有第三个token，检查其是否是TOKEN_NAME或TOKEN_RANK
    if (current_stmt.num > 2) {
        toktype third_token_type = current_stmt.tokens[2]->type;
        if (third_token_type != TOKEN_NAME && third_token_type != TOKEN_RANK) {
            return 0;
        }
    }

    // 更多的语法检查逻辑...
    return 1; // 语法正确
}
