#ifndef PKG_H
#define PKG_H
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
#include <arpa/inet.h>
#include <time.h>
#include <stdint.h>

#include "../lib/mstring.h"
#include "merr.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
这个文件将会对数据进行打包、解包操作。
MUIX包协议定义：

包头结构 (9字节):
- 魔数：4字节，分别为 'M' 'U' 'I' 'X'
- 数据长度：4字节（大端字节序，表示用户数据长度）
- 结束符：1字节，固定为 '$'

包体:
- 用户数据：可变长度（用户已经网络序列化的数据）
*/

#define MAX_PACKET_SIZE 4096 // 最大包大小 4KB
#define PACKET_HEADER_SIZE 9 // 包头大小：4(MUIX) + 4(长度) + 1($)

/*
用户需要自己释放返回的 mstring
*/
mstring packing(uint8_t *data, uint32_t data_length);//打包
mstring unpacking(const uint8_t *packet, uint32_t packet_length);//解包

int find_packet_boundary(const uint8_t *buffer, uint32_t buffer_size, uint32_t *start_index, uint32_t *packet_size);
/*
这个函数用于在流式数据buffer中查找第一个完整的包
最大寻找buffer_size个字节
返回1表示找到完整包，0表示未找到

start_index:包开始位置,为输出参数
packet_size:包大小,为输出参数
*/
#ifdef __cplusplus
}
#endif

#endif