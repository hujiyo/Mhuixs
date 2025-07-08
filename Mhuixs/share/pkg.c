/*
#版权所有 (c) HuJi 2025.1
#保留所有权利
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
Email:hj18914255909@outlook.com
*/
/*
这个文件将会对数据进行打包、解包操作。
采用简化的包结构：MUIX + 数据长度 + $ + 用户数据
*/


#include "pkg.h"

// 协议常量定义
#define MHUIXS_MAGIC0 'M'
#define MHUIXS_MAGIC1 'U'
#define MHUIXS_MAGIC2 'I'
#define MHUIXS_MAGIC3 'X'
#define MHUIXS_DELIMITER '$'

// 内部函数：验证包头
static int validate_packet_header(const PacketHeader *header) {
    if (!header) return 0;
    
    // 检查魔数
    if (header->magic[0] != MHUIXS_MAGIC0 ||
        header->magic[1] != MHUIXS_MAGIC1 ||
        header->magic[2] != MHUIXS_MAGIC2 ||
        header->magic[3] != MHUIXS_MAGIC3) {
        printf("错误: 无效的魔数 %c%c%c%c\n", 
               header->magic[0], header->magic[1], 
               header->magic[2], header->magic[3]);
        return 0;
    }
    
    // 检查结束符
    if (header->delimiter != MHUIXS_DELIMITER) {
        printf("错误: 无效的结束符 '%c'，期望 '$'\n", header->delimiter);
        return 0;
    }
    
    // 检查数据长度（需要转换为主机字节序）
    uint32_t data_length = ntohl(header->data_length);
    if (data_length > MAX_PACKET_SIZE - PACKET_HEADER_SIZE) {
        printf("错误: 数据长度过大 %u\n", data_length);
        return 0;
    }
    
    return 1;
}

// 用户API：创建数据包
Packet* create_packet(const void *data, uint32_t data_length) {
    // 检查数据长度
    if (data_length > MAX_PACKET_SIZE - PACKET_HEADER_SIZE) {
        printf("错误: 数据长度过大 %u，最大支持 %u 字节\n", 
               data_length, MAX_PACKET_SIZE - PACKET_HEADER_SIZE);
        return NULL;
    }
    
    // 分配内存
    Packet *packet = (Packet*)malloc(sizeof(Packet));
    if (!packet) {
        printf("错误: 内存分配失败\n");
        return NULL;
    }
    
    // 初始化包头
    packet->header.magic[0] = MHUIXS_MAGIC0;
    packet->header.magic[1] = MHUIXS_MAGIC1;
    packet->header.magic[2] = MHUIXS_MAGIC2;
    packet->header.magic[3] = MHUIXS_MAGIC3;
    packet->header.data_length = htonl(data_length);  // 转换为大端字节序
    packet->header.delimiter = MHUIXS_DELIMITER;
    
    // 复制数据
    if (data_length > 0 && data) {
        packet->data = (uint8_t*)malloc(data_length);
        if (!packet->data) {
            printf("错误: 数据内存分配失败\n");
            free(packet);
            return NULL;
        }
        memcpy(packet->data, data, data_length);
    } else {
        packet->data = NULL;
    }
    
    return packet;
}

// 用户API：销毁数据包
void destroy_packet(Packet *packet) {
    if (!packet) return;
    
    if (packet->data) {
        free(packet->data);
    }
    free(packet);
}

// 用户API：序列化数据包
uint8_t* serialize_packet(const Packet *packet, uint32_t *total_size) {
    if (!packet || !total_size) return NULL;
    
    uint32_t data_length = ntohl(packet->header.data_length);
    *total_size = PACKET_HEADER_SIZE + data_length;
    
    uint8_t *buffer = (uint8_t*)malloc(*total_size);
    if (!buffer) {
        printf("错误: 序列化缓冲区分配失败\n");
        return NULL;
    }
    
    // 复制包头
    memcpy(buffer, &packet->header, PACKET_HEADER_SIZE);
    
    // 复制数据
    if (data_length > 0 && packet->data) {
        memcpy(buffer + PACKET_HEADER_SIZE, packet->data, data_length);
    }
    
    return buffer;
}

// 用户API：反序列化数据包
Packet* deserialize_packet(const uint8_t *buffer, uint32_t buffer_size) {
    if (!buffer || buffer_size < PACKET_HEADER_SIZE) {
        printf("错误: 缓冲区无效或过小\n");
        return NULL;
    }
    
    Packet *packet = (Packet*)malloc(sizeof(Packet));
    if (!packet) {
        printf("错误: 包内存分配失败\n");
        return NULL;
    }
    
    // 解析包头
    memcpy(&packet->header, buffer, PACKET_HEADER_SIZE);
    
    // 验证包头
    if (!validate_packet_header(&packet->header)) {
        free(packet);
        return NULL;
    }
    
    uint32_t data_length = ntohl(packet->header.data_length);
    uint32_t expected_size = PACKET_HEADER_SIZE + data_length;
    
    if (buffer_size < expected_size) {
        printf("错误: 缓冲区大小不足，期望 %u，实际 %u\n", expected_size, buffer_size);
        free(packet);
        return NULL;
    }
    
    // 解析数据
    if (data_length > 0) {
        packet->data = (uint8_t*)malloc(data_length);
        if (!packet->data) {
            printf("错误: 数据内存分配失败\n");
            free(packet);
            return NULL;
        }
        memcpy(packet->data, buffer + PACKET_HEADER_SIZE, data_length);
    } else {
        packet->data = NULL;
    }
    
    return packet;
}

// 获取包数据长度
uint32_t get_packet_data_length(const Packet *packet) {
    if (!packet) return 0;
    return ntohl(packet->header.data_length);
}

// 获取包数据
const uint8_t* get_packet_data(const Packet *packet) {
    if (!packet) return NULL;
    return packet->data;
}

// 验证数据包
int is_valid_packet(const Packet *packet) {
    if (!packet) return 0;
    
    // 验证包头
    if (!validate_packet_header(&packet->header)) {
        return 0;
    }
    
    // 验证数据部分
    uint32_t data_length = ntohl(packet->header.data_length);
    if (data_length > 0 && !packet->data) {
        printf("错误: 数据长度不为0但数据指针为空\n");
        return 0;
    }
    
    return 1;
}

int find_packet_boundary(const uint8_t *buffer, uint32_t buffer_size, uint32_t *start_index, uint32_t *packet_size) {
    if (!buffer || !start_index || !packet_size || buffer_size < PACKET_HEADER_SIZE) {
        return 0;
    }
    
    // 查找MUIX魔数
    for (uint32_t i = 0; i <= buffer_size - PACKET_HEADER_SIZE; i++) {
        if (buffer[i] == MHUIXS_MAGIC0 && 
            buffer[i + 1] == MHUIXS_MAGIC1 && 
            buffer[i + 2] == MHUIXS_MAGIC2 && 
            buffer[i + 3] == MHUIXS_MAGIC3) {
            
            // 找到魔数，解析数据长度
            uint32_t data_length;
            memcpy(&data_length, &buffer[i + 4], sizeof(uint32_t));
            data_length = ntohl(data_length);
            
            // 检查结束符
            if (buffer[i + 8] != MHUIXS_DELIMITER) {
                continue;  // 不是有效的包头，继续查找
            }
            
            // 验证数据长度合理性
            if (data_length > MAX_PACKET_SIZE - PACKET_HEADER_SIZE) {
                continue;  // 数据长度过大，继续查找
            }
            
            // 计算完整包大小
            uint32_t total_packet_size = PACKET_HEADER_SIZE + data_length;
            
            // 检查是否有足够的数据构成完整包
            if (buffer_size >= i + total_packet_size) {
                *start_index = i;           // 包开始位置
                *packet_size = total_packet_size;  // 完整包大小
                return 1;  // 找到完整的包
            } else {
                return 0;  // 包不完整，没有足够的数据
            }
        }
    }
    
    return 0;  // 没有找到有效的包
}

