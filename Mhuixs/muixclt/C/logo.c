#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
/*
#版权所有 (c) Mhuixs-team 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2024.12
Email:hj18914255909@outlook.com
*/

// 简化的 ASCII logo，避免多字节字符和长字符串警告
static const char* logo_lines[] = {
    "                       Mhuixs",
    "",
    "   ###    ### ##   ## ##   ## ## ##   ## #######",
    "   ####  #### ##   ## ##   ## ##  ## ##  ##",
    "   ## #### ## ####### ##   ## ##   ###   #######",
    "   ##  ##  ## ##   ## ##   ## ##  ## ##        ##",
    "   ##      ## ##   ##  #####  ## ##   ## #######",
    "",
    "        Memory-based Hybrid Universal Index System",
    "",
    "   ##################################################",
    "   ##                                            ##",
    "   ##    High Performance Memory Database        ##",
    "   ##                                            ##",
    "   ##    Supports NAQL Natural Query Language    ##",
    "   ##                                            ##",
    "   ##    Hybrid Relational + NoSQL Architecture  ##",
    "   ##                                            ##",
    "   ##    Distributed Design & High Concurrency   ##",
    "   ##                                            ##",
    "   ##################################################",
    "",
    "             Welcome to Mhuixs Database Client",
    "",
    "                  Version 1.0.0 | 2025",
    ""
};

void print_mhuixs_logo()
{
    srand(time(NULL));
    
    // 打印 logo
    int logo_lines_count = sizeof(logo_lines) / sizeof(logo_lines[0]);
    for(int i = 0; i < logo_lines_count; i++){
        const char* line = logo_lines[i];
        int len = strlen(line);
        for(int j = 0; j < len; j++){
            char c = line[j];
            if(c == '#'){
                // 随机化字符显示
                printf("%c", (rand() % 100 > 50) ? ('A' + rand() % 26) : ('a' + rand() % 26));
            } else {
                printf("%c", c);
            }
        }
        printf("\n");
    }
}