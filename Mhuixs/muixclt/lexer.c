/*
#版权所有 (c) HuJi 2025.1
#保留所有权利
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
Email:hj18914255909@outlook.com
*/
#include "lexer.h"

// 静态流程控制器变量
static FlowController* g_flow_controller = NULL;

// 流程控制器设置函数
void set_flow_controller(FlowController* controller) {
    g_flow_controller = controller;
}

// 添加缺失的字符串操作函数
static inline int str_append_data(str *s, const void *data, uint32_t len) {
    if (!s || !data) return -1;
    return sappend(s, data, len);
}

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
    (c=='\n')               ||      \
    (c=='$')                      \
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
    int is_macro = 0;//函数全局变量：是否是宏变量:$

    //在原来pos的位置基础上先后寻找首个token的起始字符
    for(; instr->pos < instr->string + instr->len ; instr->pos++){
        // 处理注释行：如果遇到#，跳过整行
        if(*instr->pos == '#'){
            // 跳过注释行到行尾
            while(instr->pos < instr->string + instr->len && *instr->pos != '\n'){
                instr->pos++;
            }
            // 如果找到\n，跳过它
            if(instr->pos < instr->string + instr->len && *instr->pos == '\n'){
                instr->pos++;
            }
            continue;
        }
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
            if(*instr->pos == '$'){
                is_macro = 1;
                break;
            }
        }
        // 添加对符号的识别
        else if(*instr->pos == '>' || *instr->pos == '<' || *instr->pos == '=' || *instr->pos == '~' || *instr->pos == '!') {
            is_symbol = 1;//>(>,>=,><) <(<,<=,<>) =(==) ~(~=) !(!=)
            break;
        }
    }

    if(is_number+is_word+is_char + is_string + is_symbol + is_macro == 0){
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
        instr->pos++;//跳过"
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
                token->type = TOKEN_VALUES; // 将数字视为字符串值
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
        token->type = TOKEN_VALUES; // 将数字视为字符串值
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
    else if(is_macro){//宏变量处理 $var_name
        instr->pos++;//跳过$
        
        // 检查第一个字符必须是字母或下划线
        if(instr->pos < instr->string+instr->len) {
            uint8_t first_char = *instr->pos;
            // 检查是否是有效的变量名首字符：字母、下划线或Unicode字符(>=128)
            int is_valid_first_char = 
                (first_char >= 'a' && first_char <= 'z') ||
                (first_char >= 'A' && first_char <= 'Z') ||
                (first_char == '_') ||
                (first_char >= 128);
                
            if(!is_valid_first_char) {
                // 第一个字符不是有效的变量名首字符
                token->type = TOKEN_EEROR;
                return token;
            }
        }
        
        for(;ed_pos < instr->string+instr->len;ed_pos++){
            //不断判断ed_pos指向的是否是宏变量名的结束字符 ' ' ';' '\n'
            if(*ed_pos == ' ' || *ed_pos == ';' || *ed_pos == '\n' || *ed_pos == '>' 
                || *ed_pos == '<' || *ed_pos == '=' || *ed_pos == '~' || *ed_pos == '!'){
                //宏变量名结束了
                swrite(&token->content,0,instr->pos,ed_pos-instr->pos);
                token->type = TOKEN_MACRO;
                instr->pos = ed_pos;
                return token;
            }
            else if((*ed_pos < 'a' || *ed_pos > 'z') && (*ed_pos < 'A' || *ed_pos > 'Z') 
                && (*ed_pos < '0' || *ed_pos > '9') && *ed_pos != '_' && *ed_pos < 128){
                //不是合法的变量名字符（不包括Unicode字符）
                token->type = TOKEN_EEROR;
                return token;
            }           
        }
        //最后一个token是宏变量
        swrite(&token->content,0,instr->pos,ed_pos-instr->pos);
        token->type = TOKEN_MACRO;
        instr->pos = ed_pos;
        return token;
    }
    
    // 如果到达这里，说明没有找到有效的token
    str_free(&token->content);
    free(token);
    return NULL;
}


// 修复符号数量定义
#define SYMBOL_NUM 9 // Mhuixs支持的符号总数量

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
static CommandNumber parse_index_statement(tok* token, int len, int* param_start);
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
    if(token->type == TOKEN_VALUES){
        return 1;
    }
    if(token->type == TOKEN_SYMBOL){
       // 由于关键字地图中的符号是连续的,所以先扫描整个关键字表找到第一个符号,然后再往后对比是否有匹配的符号
        for(int i = 0;i < KEYWORD_MAP_SIZE;i++){
            // 上面已经规定">"符号是关键字地图中的第一个符号
            if(keyword_map[i].type == TOKEN_DY){
                // 说明找到了第一个">"符号
                for(int j = 0;j < SYMBOL_NUM;j++){
                    if(strncmp((const char*)token->content.string,keyword_map[i+j].keyword,token->content.len) == 0 && 
                       strlen(keyword_map[i+j].keyword) == token->content.len){
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
        for(int i = 0;i < KEYWORD_MAP_SIZE;i++){
            if(strncmp((const char*)token->content.string,keyword_map[i].keyword,token->content.len) == 0 && 
               strlen(keyword_map[i].keyword) == token->content.len){
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
            if(len == 3 && token[1].type == TOKEN_NAME && token[2].type == TOKEN_VALUES){
                *param_start = 1;
                return CMD_RANK_SET;
            }
            break;
        case TOKEN_LOCK:
            if(len == 3 && token[1].type == TOKEN_NAME && token[2].type == TOKEN_VALUES){
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
            if(len == 3 && token[1].type == TOKEN_NAME && 
               (token[2].type == TOKEN_NAME || token[2].type == TOKEN_JSON_FORMAT || 
                token[2].type == TOKEN_json || token[2].type == TOKEN_CSV || token[2].type == TOKEN_BINARY)){
                *param_start = 1;
                return CMD_EXPORT;
            }
            break;
        case TOKEN_IMPORT:
            if(len == 4 && token[1].type == TOKEN_NAME && 
               (token[2].type == TOKEN_NAME || token[2].type == TOKEN_JSON_FORMAT || 
                token[2].type == TOKEN_json || token[2].type == TOKEN_CSV || token[2].type == TOKEN_BINARY) && 
               token[3].type == TOKEN_VALUES){
                *param_start = 1;
                return CMD_IMPORT;
            }
            break;
        case TOKEN_CHMOD:
            if(len == 3 && token[1].type == TOKEN_NAME && (token[2].type == TOKEN_NAME || token[2].type == TOKEN_VALUES)){
                *param_start = 1;
                return CMD_CHMOD;
            }
            break;
        case TOKEN_WAIT:
            if(len == 2 && token[1].type == TOKEN_VALUES){
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
            if(len >= 3 && token[1].type == TOKEN_VALUES){
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
            if(len == 3 && (token[1].type == TOKEN_NAME || token[1].type == TOKEN_VALUES) && (token[2].type == TOKEN_NAME || token[2].type == TOKEN_VALUES)){
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
            if(len == 2 && (token[1].type == TOKEN_NAME || token[1].type == TOKEN_MACRO)){
                *param_start = 1;
                return CMD_INCR;
            } else if(len == 3 && (token[1].type == TOKEN_NAME || token[1].type == TOKEN_MACRO) && token[2].type == TOKEN_NUM){
                *param_start = 1;
                return CMD_INCR_BY;
            }
            break;
        case TOKEN_DECR:
            if(len == 2 && (token[1].type == TOKEN_NAME || token[1].type == TOKEN_MACRO)){
                *param_start = 1;
                return CMD_DECR;
            } else if(len == 3 && (token[1].type == TOKEN_NAME || token[1].type == TOKEN_MACRO) && token[2].type == TOKEN_NUM){
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
            if(len == 2 && (token[1].type == TOKEN_VALUES || token[1].type == TOKEN_NUM)){
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
            *param_start = 1;
            return CMD_IF; // 本地处理
        case TOKEN_ELSE:
            return CMD_ELSE; // 本地处理
        case TOKEN_ELIF:
            *param_start = 1;
            return CMD_ELIF; // 本地处理
        case TOKEN_WHILE:
            *param_start = 1;
            return CMD_WHILE; // 本地处理
        case TOKEN_FOR:
            *param_start = 1;
            return CMD_FOR; // 本地处理
        case TOKEN_END:
            if(len == 1){
                return CMD_END; // 本地处理
            }
            break;
        case TOKEN_BREAK:
            return CMD_BREAK; // 本地处理
        case TOKEN_CONTINUE:
            return CMD_CONTINUE; // 本地处理
        case TOKEN_INDEX:
            return parse_index_statement(token, len, param_start);
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
    
    str_append_string(&result, "HUJI");// 添加HUJI协议头
    
    uint32_t cmd_net = htonl((uint32_t)cmd);// 添加命令编号（4字节网络字节序）
    str_append_data(&result, (char*)&cmd_net, 4);
    
    
    str param_stream;// 构建参数数据流
    str_init(&param_stream);
    
    // 参数数目（1字节）
    uint8_t param_count_byte = (uint8_t)param_count;
    str_append_data(&param_stream, (char*)&param_count_byte, 1);
    str_append_string(&param_stream, "@");
    
    // 添加每个参数
    for(int i = 0; i < param_count; i++) {
        if(params[i].type == TOKEN_VALUES || params[i].type == TOKEN_NAME || 
           params[i].type == TOKEN_NUM || params[i].type == TOKEN_MACRO || params[i].type >= TOKEN_TABLE) {
            
            // 处理变量替换
            str param_content;
            str_init(&param_content);
            
            if(params[i].type == TOKEN_MACRO) {
                // 这是一个变量，需要获取其值
                char* var_name = str_to_cstr(&params[i].content);
                const char* var_value = get_local_variable(var_name);
                if(var_value) {
                    str_append_string(&param_content, var_value);
                } else {
                    // 变量不存在，使用原始变量名（包含$）
                    str_append_string(&param_content, "$");
                    str_append_data(&param_content, params[i].content.string, params[i].content.len);
                }
                free(var_name);
            } else {
                // 直接使用原始内容
                str_append_data(&param_content, params[i].content.string, params[i].content.len);
            }
            
            // 参数长度（字符串数字）
            char len_str[32];
            snprintf(len_str, sizeof(len_str), "%d", param_content.len);
            str_append_string(&param_stream, len_str);
            str_append_string(&param_stream, "@");
            
            // 参数内容
            str_append_data(&param_stream, param_content.string, param_content.len);
            str_append_string(&param_stream, "@");
            
            str_free(&param_content);
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
                // 语法错误，设置错误状态
                result.state = -1;
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
                
                // 检查是否在控制块中且不是控制命令本身
                if(flow_controller_should_cache(g_flow_controller) && 
                   cmd != CMD_IF && cmd != CMD_ELSE && cmd != CMD_ELIF && 
                   cmd != CMD_WHILE && cmd != CMD_FOR && cmd != CMD_END && cmd != CMD_BREAK && cmd != CMD_CONTINUE) {
                    // 在控制块中，缓存语句而不是执行
                    str stmt_str;
                    str_init(&stmt_str);
                    
                    // 重建语句字符串
                    for(int i = stmt_start; i < stmt_end; i++) {
                        str_append_data(&stmt_str, tokens[i].content.string, tokens[i].content.len);
                        if(i < stmt_end - 1) {
                            str_append_string(&stmt_str, " ");
                        }
                    }
                    str_append_string(&stmt_str, ";");
                    
                    char* stmt_cstr = str_to_cstr(&stmt_str);
                    flow_controller_cache_statement(g_flow_controller, stmt_cstr);
                    free(stmt_cstr);
                    str_free(&stmt_str);
                } else {
                    // 本地处理（控制命令或不在控制块中）
                    process_local_command(cmd, params, param_count);
                }
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
            
            // 检查是否在控制块中
            if(flow_controller_should_cache(g_flow_controller)) {
                // 在控制块中，缓存语句而不是生成协议
                str stmt_str;
                str_init(&stmt_str);
                
                // 重建语句字符串
                for(int i = stmt_start; i < stmt_end; i++) {
                    str_append_data(&stmt_str, tokens[i].content.string, tokens[i].content.len);
                    if(i < stmt_end - 1) {
                        str_append_string(&stmt_str, " ");
                    }
                }
                str_append_string(&stmt_str, ";");
                
                char* stmt_cstr = str_to_cstr(&stmt_str);
                flow_controller_cache_statement(g_flow_controller, stmt_cstr);
                free(stmt_cstr);
                str_free(&stmt_str);
            } else {
                // 生成HUJI协议数据
                str protocol_data = generate_huji_protocol(cmd, params, param_count);
                
                // 添加到结果中
                str_append_data(&result, protocol_data.string, protocol_data.len);
                
                str_free(&protocol_data);
            }
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
        } else if(token[1].type == TOKEN_SIZE) {
            return CMD_GET_BIT_SIZE;
        } else if(token[1].type == TOKEN_NAME || token[1].type == TOKEN_VALUES) {
            *param_start = 1;
            return CMD_GET_KEY;
        } else if(token[1].type == TOKEN_NUM) {
            *param_start = 1;
            return CMD_GET_LIST;
        } else if(token[1].type == TOKEN_MACRO) {
            // GET $variable 应该被解释为使用变量值作为hook名的GET命令
            // 而不是本地的GET_VAR命令
            *param_start = 1;
            return CMD_GET_OBJECT;  // 改为CMD_GET_OBJECT，让变量替换机制处理
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
    
    if(len == 2 && (token[1].type == TOKEN_NAME || token[1].type == TOKEN_VALUES)) {
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
        } else if(token[1].type == TOKEN_COPY && len == 4) {
            *param_start = 2;
            return CMD_HOOK_COPY;
        } else if(token[1].type == TOKEN_SWAP && len == 4) {
            *param_start = 2;
            return CMD_HOOK_SWAP;
        } else if(token[1].type == TOKEN_MERGE && len == 4) {
            *param_start = 2;
            return CMD_HOOK_MERGE;
        } else if(token[1].type == TOKEN_RENAME && len == 4) {
            *param_start = 2;
            return CMD_HOOK_RENAME;
        } else if(token[1].type == TOKEN_JOIN && len == 5) {
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
    } else if(len >= 3 && token[1].type == TOKEN_MACRO) {
        *param_start = 1;
        return CMD_SET_VAR_VALUE;
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
        } else if(token[1].type == TOKEN_MACRO) {
            return CMD_DEL_VAR; // 变量删除，支持多个变量
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
        } else if((token[1].type == TOKEN_NAME || token[1].type == TOKEN_VALUES) && 
                  (token[2].type == TOKEN_NAME || token[2].type == TOKEN_VALUES)) {
            return CMD_SWAP_KEY; // 键交换
        }
    }
    
    return CMD_ERROR;
}

static CommandNumber parse_key_statement(tok* token, int len, int* param_start) {
    if(len < 2) return CMD_ERROR;
    
    if(token[1].type == TOKEN_NAME || token[1].type == TOKEN_VALUES) {
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
    if(len < 1) return CMD_ERROR;
    
    // 处理宏变量语法 [$var_name value;]
    if(token[0].type == TOKEN_MACRO) {
        if(len == 1) {
            // [$var_name;] - 只有变量名，可能是获取变量值
            *param_start = 0;
            return CMD_GET_VAR;
        } else if(len >= 2) {
            // [$var_name value;] 或 [$var_name value1 value2 ...;]
            *param_start = 0;
            return CMD_SET_VAR;
        }
    }
    
    return CMD_ERROR;
}

// 变量系统已在文件开头包含



// 本地命令处理函数
int process_local_command(CommandNumber cmd, tok* params, int param_count) {
    switch(cmd) {
        case CMD_SET_VAR:
            if(param_count >= 2) {
                // 第一个参数是$var_name（TOKEN_MACRO），第二个参数是value
                char* var_name = str_to_cstr(&params[0].content);
                char* var_value = str_to_cstr(&params[1].content);
                int result = set_local_variable(var_name, var_value);
                free(var_name);
                free(var_value);
                return result;
            }
            break;
            
        case CMD_GET_VAR:
            if(param_count >= 1) {
                // 参数是$var_name（TOKEN_MACRO）
                char* var_name = str_to_cstr(&params[0].content);
                print_variable_info(var_name);
                free(var_name);
                return 0;
            }
            break;
            
        case CMD_DEL_VAR:
            if(param_count >= 1) {
                // 参数是$var_name（TOKEN_MACRO）
                char* var_name = str_to_cstr(&params[0].content);
                int result = delete_local_variable(var_name);
                free(var_name);
                return result;
            }
            break;
            
        case CMD_INCR:
        case CMD_INCR_BY:
            if(param_count >= 1 && params[0].type == TOKEN_MACRO) {
                // 本地变量递增
                char* var_name = str_to_cstr(&params[0].content);
                const char* current_val = get_local_variable(var_name);
                if(current_val) {
                    int val = atoi(current_val);
                    int increment = 1;
                    if(cmd == CMD_INCR_BY && param_count >= 2) {
                        char* inc_str = str_to_cstr(&params[1].content);
                        increment = atoi(inc_str);
                        free(inc_str);
                    }
                    val += increment;
                    char new_val[32];
                    snprintf(new_val, sizeof(new_val), "%d", val);
                    set_local_variable(var_name, new_val);
                    printf("变量 %s 递增后值为: %d\n", var_name, val);
                } else {
                    // 变量不存在，初始化为increment值
                    int increment = 1;
                    if(cmd == CMD_INCR_BY && param_count >= 2) {
                        char* inc_str = str_to_cstr(&params[1].content);
                        increment = atoi(inc_str);
                        free(inc_str);
                    }
                    char new_val[32];
                    snprintf(new_val, sizeof(new_val), "%d", increment);
                    set_local_variable(var_name, new_val);
                    printf("变量 %s 初始化为: %d\n", var_name, increment);
                }
                free(var_name);
                return 0;
            }
            break;
            
        case CMD_DECR:
        case CMD_DECR_BY:
            if(param_count >= 1 && params[0].type == TOKEN_MACRO) {
                // 本地变量递减
                char* var_name = str_to_cstr(&params[0].content);
                const char* current_val = get_local_variable(var_name);
                if(current_val) {
                    int val = atoi(current_val);
                    int decrement = 1;
                    if(cmd == CMD_DECR_BY && param_count >= 2) {
                        char* dec_str = str_to_cstr(&params[1].content);
                        decrement = atoi(dec_str);
                        free(dec_str);
                    }
                    val -= decrement;
                    char new_val[32];
                    snprintf(new_val, sizeof(new_val), "%d", val);
                    set_local_variable(var_name, new_val);
                    printf("变量 %s 递减后值为: %d\n", var_name, val);
                } else {
                    // 变量不存在，初始化为-decrement值
                    int decrement = 1;
                    if(cmd == CMD_DECR_BY && param_count >= 2) {
                        char* dec_str = str_to_cstr(&params[1].content);
                        decrement = atoi(dec_str);
                        free(dec_str);
                    }
                    char new_val[32];
                    snprintf(new_val, sizeof(new_val), "%d", -decrement);
                    set_local_variable(var_name, new_val);
                    printf("变量 %s 初始化为: %d\n", var_name, -decrement);
                }
                free(var_name);
                return 0;
            }
            break;
            
        case CMD_IF:
            // IF条件处理 - 使用新的流程控制器
            if(param_count >= 1) {
                char* condition_expr = str_to_cstr(&params[0].content);
                int result = flow_controller_push_if(g_flow_controller, condition_expr);
                free(condition_expr);
                return result;
            }
            return flow_controller_push_if(g_flow_controller, "1"); // 默认条件为真
            
        case CMD_ELSE:
            return flow_controller_push_else(g_flow_controller);
            
        case CMD_ELIF:
            // ELIF处理 - 使用新的流程控制器
            if(param_count >= 1) {
                char* condition_expr = str_to_cstr(&params[0].content);
                int result = flow_controller_push_elif(g_flow_controller, condition_expr);
                free(condition_expr);
                return result;
            }
            return flow_controller_push_elif(g_flow_controller, "0"); // 默认条件为假
            
        case CMD_WHILE:
            // WHILE循环处理 - 使用新的流程控制器
            if(param_count >= 1) {
                char* condition_expr = str_to_cstr(&params[0].content);
                int result = flow_controller_push_while(g_flow_controller, condition_expr);
                free(condition_expr);
                return result;
            }
            return flow_controller_push_while(g_flow_controller, "1"); // 默认条件为真
            
        case CMD_FOR:
            // FOR循环处理 - 使用新的流程控制器
            if(param_count >= 4) {
                char* var_name = str_to_cstr(&params[0].content);
                
                // 解析开始值（可能是变量）
                int start_val = 0;
                if(params[1].type == TOKEN_MACRO) {
                    char* start_var = str_to_cstr(&params[1].content);
                    const char* start_str = get_local_variable(start_var);
                    start_val = start_str ? atoi(start_str) : 0;
                    free(start_var);
                } else {
                    char* start_str = str_to_cstr(&params[1].content);
                    start_val = atoi(start_str);
                    free(start_str);
                }
                
                // 解析结束值（可能是变量）
                int end_val = 0;
                if(params[2].type == TOKEN_MACRO) {
                    char* end_var = str_to_cstr(&params[2].content);
                    const char* end_str = get_local_variable(end_var);
                    end_val = end_str ? atoi(end_str) : 0;
                    free(end_var);
                } else {
                    char* end_str = str_to_cstr(&params[2].content);
                    end_val = atoi(end_str);
                    free(end_str);
                }
                
                // 解析步长（可能是变量）
                int step_val = 1;
                if(params[3].type == TOKEN_MACRO) {
                    char* step_var = str_to_cstr(&params[3].content);
                    const char* step_str = get_local_variable(step_var);
                    step_val = step_str ? atoi(step_str) : 1;
                    free(step_var);
                } else {
                    char* step_str = str_to_cstr(&params[3].content);
                    step_val = atoi(step_str);
                    free(step_str);
                }
                
                // 使用新的流程控制器推入FOR循环
                int result = flow_controller_push_for(g_flow_controller, var_name, start_val, end_val, step_val);
                free(var_name);
                return result;
            }
            return -1; // 参数不足
        case CMD_BREAK:
            // 跳出循环 - 使用新的流程控制器
            return flow_controller_break(g_flow_controller);
            
        case CMD_CONTINUE:
            // 继续循环 - 使用新的流程控制器
            return flow_controller_continue(g_flow_controller);
            
        case CMD_END:
            // END命令 - 使用新的流程控制器
            if(flow_controller_is_in_block(g_flow_controller)) {
                // 使用外部定义的语句执行器
                extern int flow_aware_statement_executor(const char* statement);
                return flow_controller_end_block(g_flow_controller, flow_aware_statement_executor);
            }
            return 0;
            
        default:
            return -1; // 不是本地命令
    }
    
    return -1;
}

// 检查命令是否需要本地处理
int is_local_command(CommandNumber cmd) {
    return (cmd >= CMD_IF && cmd <= CMD_CONTINUE) || 
           (cmd >= CMD_SET_VAR && cmd <= CMD_DEL_VAR) ||
           (cmd == CMD_INCR || cmd == CMD_DECR || cmd == CMD_INCR_BY || cmd == CMD_DECR_BY);
}

// 资源清理函数
void cleanup_local_resources() {
    variable_system_cleanup();
}

// 添加parse_index_statement函数
static CommandNumber parse_index_statement(tok* token, int len, int* param_start) {
    if(len < 2) return CMD_ERROR;
    
    if(token[1].type == TOKEN_CREATE && len >= 4) {
        // [INDEX CREATE field_name index_type;]
        *param_start = 2;
        return CMD_INDEX_CREATE;
    } else if(token[1].type == TOKEN_DEL && len >= 3) {
        // [INDEX DEL field_name;]
        *param_start = 2;
        return CMD_INDEX_DEL;
    }
    
    return CMD_ERROR;
}

// 执行语句的函数指针
int (*execute_statement_function)(const char* statement) = NULL;

// 简单的语句执行函数（避免递归）
int simple_execute_statement(const char* statement) {
    if(!statement) return -1;
    // 避免递归调用lexer，使用简化的解析和执行
    printf("直接执行语句: %s\n", statement);
    return 0;
}