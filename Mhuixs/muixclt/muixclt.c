/*
#版权所有 (c) HuJi 2025
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
Email:hj18914255909@outlook.com
*/

// 调试模式宏定义
#define DEBUG_MODE 1  // 设置为1启用调试模式，0禁用调试模式

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h> // 用于测试函数的随机数
#include <arpa/inet.h> // 用于ntohl函数

#define _MUIXCLT_ //Mhuixs客户端标志宏

// 定义strdup函数以避免编译警告
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

/*
Mhuxis客户端包括以下功能：
1.扩展语法解释器(用户可定制这部分,将用户喜欢的语法解释为NAQL,可选，暂时不实现)
2.标准基础NAQL解释器(标准NAQL语法解释器，用于解释标准NAQL语法)
3.本地控制器(将部分特殊命令本地执行,与数据相关的命令发送给服务器执行)
第3点我仔细简介一下，本地控制器主要执行一些特殊命令，
比如IF ELSE等类似的控制语句，SLEEP等类似的休眠命令，
这些命令不会发送给服务器执行，而是本地执行。
*/

/*
lexer需要将NAQL语句转换为：

编号 + 参数数目 + 参数1 + 参数2 + 参数3 + ...

使用HUJI命令传输协议发送给服务器：

每个语句转化为：[HUJI（四ASCII字节） + 编号(4字节网络字节序) + 文本流]


文本流：[参数数目(1字节0-255) + @ 
+ 参数1字符数（字符串数字） + @ + 参数1 + @
+ 参数2字符数（字符串数字） + @ + 参数2 + @
...
+ 参数n字符数（字符串数字） + @ + 参数n + @
语句之间无缝衔接。
*/

#include "netlink.h"
#include "lexer.h"
#include "logo.h"
#include "pkg.h"
#include "variable.h"
#include "controller.h"

// 前向声明

// 全局变量
#ifdef start_tls
static SSL_CTX *ssl_ctx = NULL;
#endif

// 全局流程控制器
static FlowController* global_flow_controller = NULL;

// 函数声明
void debug_print_packet_content(const uint8_t* protocol_data, int data_len);
void batch_mode(const char *filename);

// 调试模式下的脚本执行状态
typedef struct {
    char **lines;           // 脚本行数组
    int line_count;         // 总行数
    int current_line;       // 当前执行行
    int loop_var;           // 循环变量值
    int loop_start;         // 循环开始值
    int loop_end;           // 循环结束值
    int loop_step;          // 循环步长
    int loop_begin_line;    // 循环开始行
    int in_loop;            // 是否在循环中
    char loop_var_name[64]; // 循环变量名
} DebugScriptState;

static DebugScriptState debug_state = {0};

// 调试模式下执行单行脚本
int debug_execute_single_line(const char *line) {
    // 使用lexer解析NAQL语句
    str result = lexer((char*)line, strlen(line));
    
    // 注意：本地命令不会生成协议数据，所以result.len可能为0
    // 但这不意味着解析失败，需要检查result.state
    if (result.state != 0) {
        printf("错误: 无法解析命令: %s\n", line);
        str_free(&result);
        return -1;
    }
    
    // 打印解包后的内容（仅当有协议数据时）
    if(result.len > 0) {
        debug_print_packet_content(result.string, result.len);
    }
    
    str_free(&result);
    return 0;
}

// 调试模式下解析和执行多行脚本
int debug_execute_script(const char *script) {
    // 使用lexer解析整个脚本
    str result = lexer((char*)script, strlen(script));
    
    // 检查是否解析失败（通过state字段判断，而不是len）
    if (result.state != 0) {
        printf("错误: 无法解析脚本\n");
        str_free(&result);
        return -1;
    }
    
    // 如果没有协议数据（本地命令），直接返回成功
    if (result.len == 0) {
        printf("脚本执行完成（本地命令）\n");
        str_free(&result);
        return 0;
    }
    
    // 解析并打印每个数据包
    const uint8_t* data = result.string;
    int remaining = result.len;
    int offset = 0;
    
    while (offset < remaining) {
        // 检查是否有足够的数据读取基本头部
        if (offset + 8 > remaining) {
            break;
        }
        
        // 检查HUJI魔数
        if (strncmp((const char*)(data + offset), "HUJI", 4) != 0) {
            printf("错误: 无效的协议魔数在偏移 %d\n", offset);
            break;
        }
        
        // 读取命令编号
        uint32_t cmd_num;
        memcpy(&cmd_num, data + offset + 4, 4);
        cmd_num = ntohl(cmd_num);
        
        // 计算这个数据包的长度
        int packet_len = 8; // HUJI + cmd_num
        
        // 读取参数数目
        if (offset + 8 < remaining) {
            int param_count = (unsigned char)data[offset + 8];
            packet_len += 2; // param_count + @
            
            // 计算参数部分的长度
            int param_offset = offset + 10; // 跳过HUJI(4) + cmd_num(4) + param_count(1) + @(1)
            for (int i = 0; i < param_count && param_offset < remaining; i++) {
                // 读取参数长度
                int len_start = param_offset;
                while (param_offset < remaining && data[param_offset] != '@') {
                    param_offset++;
                }
                if (param_offset >= remaining) break;
                
                // 解析长度
                char len_str[32];
                int len_str_len = param_offset - len_start;
                if (len_str_len > 31) len_str_len = 31;
                memcpy(len_str, data + len_start, len_str_len);
                len_str[len_str_len] = '\0';
                int param_len = atoi(len_str);
                
                param_offset++; // 跳过@
                param_offset += param_len; // 跳过参数内容
                if (param_offset < remaining && data[param_offset] == '@') {
                    param_offset++; // 跳过结尾@
                }
            }
            
            packet_len = param_offset - offset;
        }
        
        // 确保不超出边界
        if (offset + packet_len > remaining) {
            packet_len = remaining - offset;
        }
        
        // 打印这个数据包
        debug_print_packet_content(data + offset, packet_len);
        
        offset += packet_len;
    }
    
    str_free(&result);
    return 0;
}

// 调试模式下的数据包解析和打印函数
void debug_print_packet_content(const uint8_t* protocol_data, int data_len) {
    if (data_len < 9) { // HUJI(4) + 编号(4) + 最少1字节数据
        printf("错误: 数据包太小\n");
        return;
    }
    
    // 检查HUJI魔数
    if (strncmp((const char*)protocol_data, "HUJI", 4) != 0) {
        printf("错误: 无效的协议魔数\n");
        return;
    }
    
    // 提取命令编号（网络字节序）
    uint32_t cmd_num;
    memcpy(&cmd_num, protocol_data + 4, 4);
    cmd_num = ntohl(cmd_num);
    
    // 解析参数流
    const char* param_stream = (const char*)(protocol_data + 8);
    int param_stream_len = data_len - 8;
    
    if (param_stream_len < 1) {
        printf("错误: 参数流为空\n");
        return;
    }
    
    // 获取参数数目
    int param_count = (unsigned char)param_stream[0];
    printf("%u %d", cmd_num, param_count);
    
    // 解析参数
    int offset = 2; // 跳过参数数目和@符号
    for (int i = 0; i < param_count && offset < param_stream_len; i++) {
        // 读取参数长度
        int param_len = 0;
        int len_start = offset;
        while (offset < param_stream_len && param_stream[offset] != '@') {
            offset++;
        }
        if (offset >= param_stream_len) break;
        
        // 解析长度字符串
        char len_str[32];
        int len_str_len = offset - len_start;
        if (len_str_len > 31) len_str_len = 31;
        memcpy(len_str, param_stream + len_start, len_str_len);
        len_str[len_str_len] = '\0';
        param_len = atoi(len_str);
        
        offset++; // 跳过@符号
        
        // 读取参数内容
        if (offset + param_len <= param_stream_len) {
            printf(" \"");
            for (int j = 0; j < param_len; j++) {
                char c = param_stream[offset + j];
                if (c == '"') {
                    printf("\\\"");
                } else if (c == '\\') {
                    printf("\\\\");
                } else if (c >= 32 && c <= 126) {
                    printf("%c", c);
                } else {
                    printf("\\x%02x", (unsigned char)c);
                }
            }
            printf("\"");
            offset += param_len;
            if (offset < param_stream_len && param_stream[offset] == '@') {
                offset++;
            }
        }
    }
    printf("\n");
}

// 多行脚本缓冲区
static char multiline_buffer[4096] = {0};
static int in_multiline = 0;

// 处理单条语句的函数，返回0=成功，-1=失败，1=多行
int deal_with_the_line(const char *line) {
    /*
    核心函数：
    封装了NAQL语句的执行，函数按行处理用户的输入
        
    返回值有3种类型
    0:表示这个语句执行成功了
    -1:表示执行失败
    1:表示这是一个多行语句,内部会临时保存这个语句
    */
    
#if DEBUG_MODE
    // 调试模式：使用新的流程控制器系统
    // 所有命令都通过单行执行器处理，流程控制由流程控制器管理
    return debug_execute_single_line(line);
    
#else
    // 正常模式：发送到服务器
    if (!connected) {
        printf("error: 未连接到服务器，请使用 \\c 命令连接\n");
        return -1;
    }
    if (send_query(line) > 0) {
        char *response = receive_response();
        if (response) {
            printf("%s\n", response);
            free(response);
            return 0;
        } else {
            return -1;
        }
    } else {
        return -1;
    }
#endif
}

// 信号处理函数,当接收到中断信号时，断开连接，清理资源，退出程序
void signal_handler(int signum) {
    (void)signum; // 避免未使用参数警告
    printf("\n\n接收到中断信号，正在清理资源...\n");
    disconnect_from_server();//断开连接
#ifdef start_tls
    cleanup_openssl();//清理资源
#endif
    exit(0);//退出程序
}

// 解析命令行参数
int parse_command_line(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_help();
            return 1;
        }
        else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            print_version();
            return 1;
        }
        else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--server") == 0) {
            if (i + 1 < argc) {
                strcpy(server_ip, argv[++i]);
            } else {
                fprintf(stderr, "错误: 缺少服务器IP地址\n");
                return -1;
            }
        }
        else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--port") == 0) {
            if (i + 1 < argc) {
                server_port = atoi(argv[++i]);
                if (server_port <= 0 || server_port > 65535) {
                    fprintf(stderr, "错误: 无效的端口号\n");
                    return -1;
                }
            } else {
                fprintf(stderr, "错误: 缺少端口号\n");
                return -1;
            }
        }
        else if (strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--file") == 0) {
            if (i + 1 < argc) {
                batch_mode(argv[++i]);
                return 1;
            } else {
                fprintf(stderr, "错误: 缺少文件名\n");
                return -1;
            }
        }
        else {
            fprintf(stderr, "错误: 未知选项 %s\n", argv[i]);
            return -1;
        }
    }
    return 0;
}



// 交互模式
void interactive_mode() {
    char *input;
    char *response;
    
    print_welcome(server_ip, server_port);
    
#if DEBUG_MODE
    printf("调试模式已启用 - 命令将在本地解析和模拟执行\n");
#else
    // 自动连接到服务器
    if (connect_to_server(server_ip, server_port) != 0) {
        printf("警告: auto连接服务器失败，可使用 \\c 命令手动连接\n");
    }
#endif
    int is_lines=0;//是否是多行命令

    while (1) {
        // 设置提示符
        char prompt[64];//提示符
        if(!is_lines)snprintf(prompt, sizeof(prompt), "Mhuixs> ");    
        else {       snprintf(prompt, sizeof(prompt), "     -> "); is_lines = 0; }
        input = readline(prompt);
        
        /*
        readline 是 C 语言库（如 GNU Readline），提供交互式命令行输入支持，包括：
        1.行编辑（光标移动、删除、历史记录等）
        2.自动补全（按 Tab 键）
        3.历史命令管理（上下箭头键浏览）

        特殊返回值：
        NULL：用户按 Ctrl+D 或输入 EOF（通常是 Ctrl+Z）
        */
        
        if (!input) {
            printf("\nUser exit.\n");
            break;
        }
        // 跳过空行
        if (strlen(input) == 0) {
            free(input);
            continue;
        }
        
        // 添加到历史记录
        add_history(input);
        
        // 处理内置命令
        if (strncmp(input, "\\q", 2) == 0 || strncmp(input, "\\quit", 5) == 0) {
            free(input);
            break;
        }
        else if (strncmp(input, "\\h", 2) == 0 || strncmp(input, "\\help", 5) == 0) {
            print_help();
        }
        else if (strncmp(input, "\\c", 2) == 0 || strncmp(input, "\\connect", 8) == 0) {
            if (connect_to_server(server_ip, server_port) != 0) {
                printf("连接失败\n");
            }
        }
        else if (strncmp(input, "\\d", 2) == 0 || strncmp(input, "\\disconnect", 11) == 0) {
            disconnect_from_server();
        }
        else if (strncmp(input, "\\s", 2) == 0 || strncmp(input, "\\status", 7) == 0) {
            printf("连接状态: %s\n", connected ? "已连接" : "未连接");
            if (connected) {
                printf("服务器: %s:%d\n", server_ip, server_port);
            }
        }
        else {
            int ret = deal_with_the_line(input);
            if (ret == -1) {
                printf("\n[error to run this sentence!]\n");
            } else if (ret == 1) {
                is_lines = 1;
            }
        }
        free(input);
    }
}

// 批处理模式
void batch_mode(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "错误: 无法打开文件 %s\n", filename);
        return;
    }
    printf("正在执行批处理文件: %s\n", filename);
    
#if DEBUG_MODE
    printf("调试模式已启用 - 命令将在本地解析和模拟执行\n");
#else
    if (connect_to_server(server_ip, server_port) != 0) {
        fprintf(stderr, "错误: 无法连接到服务器\n");
        fclose(file);
        return;
    }
#endif
    char line[MAX_QUERY_LENGTH];
    int line_number = 0;
    int in_multiline_batch = 0;  // 批处理模式下的多行状态
    
    while (fgets(line, sizeof(line), file)) {
        line_number++;
        char *newline = strchr(line, '\n');
        if (newline) *newline = '\0';
        if (strlen(line) == 0 || line[0] == '#') {
            continue;
        }
        
        // 在批处理模式中也需要处理多行状态
        if (!in_multiline_batch) {
            printf("执行第 %d 行: %s\n", line_number, line);
        } else {
            printf("     -> %s\n", line);
        }
        
        int ret = deal_with_the_line(line);
        if (ret == -1) {
            printf("[error to run this sentence!]\n");
        } else if (ret == 1) {
            in_multiline_batch = 1;  // 进入多行模式
        } else if (ret == 0 && in_multiline_batch) {
            in_multiline_batch = 0;  // 退出多行模式
        }
    }
    fclose(file);
    printf("批处理完成\n");
}

// 流程控制器感知的语句执行器
int flow_aware_statement_executor(const char* statement) {
    if(!statement) return -1;
    
    printf("流程控制器执行语句: %s\n", statement);
    
    // 这里可以添加简单的语句解析和执行
    // 暂时只打印，实际项目中可以调用lexer的简化版本
    
    return 0;
}

int main(int argc, char *argv[]) {
    // 设置信号处理
    signal(SIGINT, signal_handler);//Ctrl+C
    signal(SIGTERM, signal_handler);//Ctrl+Z
    
    // 初始化变量系统
    variable_system_init();
    
    // 初始化流程控制器
    global_flow_controller = flow_controller_create();
    if(!global_flow_controller) {
        fprintf(stderr, "错误: 无法创建流程控制器\n");
        return 1;
    }
    
    // 将流程控制器设置到lexer中
    set_flow_controller(global_flow_controller);
    
    // 设置语句执行函数指针
    execute_statement_function = simple_execute_statement;
    
#ifdef start_tls
    // 初始化OpenSSL
    init_openssl();
    
    // 创建SSL上下文
    ssl_ctx = create_ssl_context();
    if (!ssl_ctx) {
        cleanup_openssl();
        return 1;
    }
#endif
    
    // 解析命令行参数，批处理模式包含在这
    int parse_result = parse_command_line(argc, argv);
    if (parse_result != 0) {
#ifdef start_tls
        cleanup_openssl();
#endif
        return parse_result == 1 ? 0 : 1;
    }
    
    // 启动交互模式
    interactive_mode();
    
    // 清理资源
    disconnect_from_server();
    variable_system_cleanup();
    flow_controller_destroy(global_flow_controller);
#ifdef start_tls
    cleanup_openssl();
#endif
    return 0;
}
