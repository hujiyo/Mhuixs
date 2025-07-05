/*
#版权所有 (c) HuJi 2025.1
#保留所有权利
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
Email:hj18914255909@outlook.com
*/
#ifndef PKG_H
#define PKG_H

#include <stdint.h>

/*
这个文件将会对数据进行打包、解包操作。
包协议定义：

包头结构 (9字节):
- 魔数：4字节，分别为 'M' 'U' 'I' 'X'
- 数据长度：4字节（大端字节序，表示用户数据长度）
- 结束符：1字节，固定为 '$'

包体:
- 用户数据：可变长度（用户已经网络序列化的数据）
*/

#define MAX_PACKET_SIZE 4096 // 最大包大小 4KB
#define PACKET_HEADER_SIZE 9 // 包头大小：4(MUIX) + 4(长度) + 1($)

// 包头结构
typedef struct {
    uint8_t magic[4];      // 魔数 'M''U''I''X'
    uint32_t data_length;  // 数据长度（大端字节序）
    uint8_t delimiter;     // 结束符 '$'
} __attribute__((packed)) PacketHeader;

// 包结构
typedef struct {
    PacketHeader header;    // 包头
    uint8_t *data;         // 用户数据
} Packet;

// 用户API
Packet* create_packet(const void *data, uint32_t data_length);  // 创建包
void destroy_packet(Packet *packet);                           // 销毁包
uint8_t* serialize_packet(const Packet *packet, uint32_t *total_size);  // 序列化包
Packet* deserialize_packet(const uint8_t *buffer, uint32_t buffer_size); // 反序列化包

// 获取包信息的函数
uint32_t get_packet_data_length(const Packet *packet);
const uint8_t* get_packet_data(const Packet *packet);

int is_valid_packet(const Packet *packet);// 验证函数
// 这个函数用于在流式数据中查找第一个完整的包
// 返回1表示找到完整包，0表示未找到
// start_index: 包开始位置的索引
// packet_size: 完整包的大小
int find_packet_boundary(const uint8_t *buffer, uint32_t buffer_size, uint32_t *start_index, uint32_t *packet_size);

#endif