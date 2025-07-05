/*
#版权所有 (c) HuJi 2025
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
Email:hj18914255909@outlook.com
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h> // 用于测试函数的随机数

/*
Mhuxis不采用redis的瘦客户端模式，
而是采用全功能客户端模式，客户端Mshell是一个解释器，
包括以下功能：
1.扩展语法解释器(用户可定制这部分,将用户喜欢的语法解释为NAQL,可选，暂时不实现)
2.标准基础NAQL解释器(标准NAQL语法解释器，用于解释标准NAQL语法)
3.本地执行器(将部分特殊命令本地执行,与数据相关的命令发送给服务器执行)
*/
#include "netlink.h"
#include "lexer.h"
#include "logo.h"

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

}


// 信号处理函数,当接收到中断信号时，断开连接，清理资源，退出程序
void signal_handler(int signum) {
    printf("\n\n接收到中断信号，正在清理资源...\n");
    disconnect_from_server();//断开连接
    cleanup_openssl();//清理资源
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
    
    print_welcome();
    
    // 自动连接到服务器
    if (connect_to_server(server_ip, server_port) != 0) {
        printf("警告: auto连接服务器失败，可使用 \\c 命令手动连接\n");
    }
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
    if (connect_to_server(server_ip, server_port) != 0) {
        fprintf(stderr, "错误: 无法连接到服务器\n");
        fclose(file);
        return;
    }
    char line[MAX_QUERY_LENGTH];
    int line_number = 0;
    while (fgets(line, sizeof(line), file)) {
        line_number++;
        char *newline = strchr(line, '\n');
        if (newline) *newline = '\0';
        if (strlen(line) == 0 || line[0] == '#') {
            continue;
        }
        printf("执行第 %d 行: %s\n", line_number, line);
        int ret = deal_with_the_line(line);
        if (ret == -1) {
            printf("[error to run this sentence!]\n");
        }
    }
    fclose(file);
    printf("批处理完成\n");
}

int main(int argc, char *argv[]) {
    // 设置信号处理
    signal(SIGINT, signal_handler);//Ctrl+C
    signal(SIGTERM, signal_handler);//Ctrl+Z
    
    // 初始化OpenSSL
    init_openssl();
    
    // 创建SSL上下文
    ssl_ctx = create_ssl_context();
    if (!ssl_ctx) {
        cleanup_openssl();
        return 1;
    }
    
    // 解析命令行参数，批处理模式包含在这
    int parse_result = parse_command_line(argc, argv);
    if (parse_result != 0) {
        cleanup_openssl();
        return parse_result == 1 ? 0 : 1;
    }
    
    // 启动交互模式
    interactive_mode();
    
    // 清理资源
    disconnect_from_server();
    cleanup_openssl();    
    return 0;
}
