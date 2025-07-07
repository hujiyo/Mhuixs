#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include "netlink.h"
/*
#版权所有 (c) Mhuixs-team 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2024.12
Email:hj18914255909@outlook.com
*/

// 打印帮助信息 (Client && Server)
void print_help() {
    #ifdef _MHUIXS_
    printf("Mhuixs - 用法:\n");
    printf("  Mhuixs [选项]\n\n");
    printf("选项:\n");
    printf("  -h, --help              显示此帮助信息\n");
    printf("  -v, --version           显示版本信息\n\n");

    #else //_MUIXCLT_
    printf("muixclt - 用法:\n");
    printf("  muixclt [选项] [命令]\n\n");
    printf("选项:\n");
    printf("  -h, --help              显示此帮助信息\n");
    printf("  -v, --version           显示版本信息\n");
    printf("  -s, --server <IP>       指定服务器IP地址 (默认: %s)\n", DEFAULT_SERVER_IP);
    printf("  -p, --port <端口>       指定服务器端口 (默认: %d)\n", PORT);
    printf("  -f, --file <文件>       从文件批量执行查询\n");
    printf("交互式命令:\n");
    printf("  \\q, \\quit              退出客户端\n");
    printf("  \\h, \\help              显示帮助信息\n");
    printf("  \\c, \\connect           连接到服务器\n");
    printf("  \\d, \\disconnect        断开连接\n");
    printf("  \\s, \\status            显示连接状态\n");
    printf("  \\v, \\verbose           切换详细模式\n\n");
    #endif
}

// 打印版本信息 (Client && Server)
void print_version() {
    #ifdef _MHUIXS_
    printf("Mhuixs v0.0.1  Mhuixs Server\n"
           "Copyright (c) HuJi 2024 Email:hj18914255909@outlook.com\n");
    #else //_MUIXCLT_
    printf("muixclt v0.0.1  Mhuixs client\n"
           "Copyright (c) HuJi 2024 Email:hj18914255909@outlook.com\n");
    #endif
}

//服务端开头介绍信息 (Server)
static const char* server_info_lines[] = {
"                         Mhuixs",
"        Memory-based Hybrid Universal Index System",
"",
"           Welcome to use Mhuixs Database Server",
"",
"                 Copyright (c) 2024 HuJi.",
"       Licensed under the Apache License,Version 2.0.",
""
};

//客户端开头介绍信息 (Client)
static const char* client_info_lines[] = {
"                      Mhuixs client",
"        Memory-based Hybrid Universal Index System",
"",
"          Welcome to use Mhuixs Database Client",
"",
"                 Copyright (c) 2024 HuJi.",
"       Licensed under the Apache License,Version 2.0.",
""
};

// 打印欢迎信息 (Client && Server)
void print_mhuixs_logo()
{
    char *logo_bitmap_inf0 = 
    "000000000000000000000000000000000000000000000000000000000000"
    "000000000000000011110000000000000000001110000000000000000000"
    "000111000011100001110000000000000000001110000000000000000000"
    "001111000011100001110000000000000000000000000000000000001111"
    "011111101111110001110000001111000111101110111000111100111111"
    "011101111101111001111111100111000011101110011101110001110000"
    "111100111100111001111001110111000011101110000111100001111110"
    "111000111100011101110001110111000111101110011111110000000111"
    "111000111100011101110001110111111111101110111100111000000111"
    "111000111100011101110001110001111101101110110000011101111111"
    "000000000000000000000000000000000000000000000000000011110000"; // "Mhuixs"艺术字logo

    char *logo_bitmap_inf1 =
    "000000000000000000000000000000000000000000000000000000000000"
    "000000000000000000000000000011110000000000001111111110000000"
    "000000000000000000000000000111111100000001111111111111100000"
    "000000000000000000000000000111111111011111111111111111110000"
    "000000000000000000000000000011111111111111111111111111110000"
    "000000000000000011110000000000111111111111110000011111111000"
    "000111111000000111111100000111111111111110000000011111111100"
    "001111111100000011111110011111111111111110000000011111111100"
    "001111111100000001111111111111111111111111100000011111111100"
    "011111111100000000011111111111100001111111110000001111111000"
    "011111111000000001111111111110000000111111100000000000000000"
    "011111111000001111111111111111000000000000000000000111110000"
    "011111111011111111111101111111100000000000000000011111111100"
    "001111111111111111110000011111100000000000000000011111111100"
    "000111111111111110000000000000000000000000000000001111111000"
    "000011111111110000000000000000000000000000000000000011100000"; // "HOOK"艺术字logo（彩蛋）

    srand(time(NULL));
    // 彩蛋概率判断
    char *logo_bitmap_inf;
    int logo_width;
    if ((rand() % 1000000) < 314) { // 0.0314% 概率
        logo_bitmap_inf = logo_bitmap_inf1;
        logo_width = 60;
    } else {
        logo_bitmap_inf = logo_bitmap_inf0;
        logo_width = 60;
    }
    int logo_len = strlen(logo_bitmap_inf);
    int logo_lines = logo_len / logo_width;
    for(int i = 0; i < logo_lines; i++) {
        for(int j = 0; j < logo_width; j++) {
            char c = logo_bitmap_inf[i * logo_width + j];
            if (c == '1') {
                printf("%c", (rand() % 100 > 50) ? ('A' + rand() % 26) : ('a' + rand() % 26));
            } else {
                printf(" ");
            }
        }
        printf("\n");
    }
    printf("\n");


    #ifdef _MHUIXS_
    // 打印 server_info_lines
    int logo_lines_count = sizeof(server_info_lines) / sizeof(server_info_lines[0]);
    for(int i = 0; i < logo_lines_count; i++){
        printf("%s\n", server_info_lines[i]);
    }
    #else //_MUIXCLT_
    // 打印 client_info_lines
    int logo_lines_count = sizeof(client_info_lines) / sizeof(client_info_lines[0]);
    for(int i = 0; i < logo_lines_count; i++){
        printf("%s\n", client_info_lines[i]);
    }
    #endif
}

// 打印欢迎信息 (Client && Server)
void print_welcome() {
    print_mhuixs_logo();

    #ifdef _MHUIXS_
    printf("\n服务已启动...\n");
    #else //_MUIXCLT_
    printf("\n服务器地址: %s:%d\n", server_ip, server_port);    
    #endif

    printf("输入 \\h 获取帮助信息，输入 \\q 退出\n");
    printf("----------------------------------------\n");
}


