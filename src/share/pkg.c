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

// 包头结构
typedef struct {
    uint8_t magic[4];      // 魔数 'M''U''I''X'
    uint32_t data_length;  // 数据长度（大端字节序）
    uint8_t delimiter;     // 结束符 '$'
} __attribute__((packed)) PacketHeader;


str packing(uint8_t *data,uint32_t data_length){
    // 检查数据长度是否合法
    if (data_length > MAX_PACKET_SIZE - PACKET_HEADER_SIZE ||
        !data || !data_length) {
        return end;
    }
    // 初始化包头
    PacketHeader header = {
        .magic[0] = MHUIXS_MAGIC0,
        .magic[1] = MHUIXS_MAGIC1,
        .magic[2] = MHUIXS_MAGIC2,
        .magic[3] = MHUIXS_MAGIC3,
        .data_length = htonl(data_length),
        .delimiter = MHUIXS_DELIMITER
    };

    //序列化数据包
    uint8_t *buffer = (uint8_t*)malloc(PACKET_HEADER_SIZE + data_length);
    if (!buffer) {
        printf("错误: 序列化缓冲区分配失败\n");
        return end;
    }
    memcpy(buffer, &header, PACKET_HEADER_SIZE);// 复制包头
    memcpy(buffer + PACKET_HEADER_SIZE, data, data_length);// 复制数据  

    str ret = {
        .string = buffer,
        .len = PACKET_HEADER_SIZE + data_length,
        .state = 0
    };
    return ret;
}

//解包：解包失败表示数据包非法
str unpacking(const uint8_t *packet, uint32_t packet_length) {
    str result = {
        .string = NULL,
        .len = 0,
        .state = 0
    };    
    // 检查基本条件
    if (!packet || packet_length < PACKET_HEADER_SIZE) {
        return result; // 返回空字符串表示错误
    }    
    // 检查魔数
    if (packet[0] != MHUIXS_MAGIC0 || 
        packet[1] != MHUIXS_MAGIC1 || 
        packet[2] != MHUIXS_MAGIC2 || 
        packet[3] != MHUIXS_MAGIC3) {
        return result;
    }
    // 检查分隔符
    if (packet[PACKET_HEADER_SIZE - 1] != MHUIXS_DELIMITER) {
        return result;
    }    
    // 提取数据长度
    uint32_t data_length;
    memcpy(&data_length, packet + 4, sizeof(uint32_t)); // 假设数据长度在魔数后的4字节
    data_length = ntohl(data_length);//转化为主机字节序
    
    // 检查数据长度是否合理
    if (data_length > MAX_PACKET_SIZE - PACKET_HEADER_SIZE || 
        PACKET_HEADER_SIZE + data_length != packet_length) {
        return result;
    }    
    // 分配内存并复制数据
    uint8_t *data = (uint8_t*)malloc(data_length); // +1 用于字符串终止符
    if (!data) {
        printf("错误: 分配内存失败\n");
        return result;
    }    
    memcpy(data, packet + PACKET_HEADER_SIZE, data_length);
    result.string = data;
    result.len = data_length;
    result.state = 0;
    return result;
}

int find_packet_boundary(const uint8_t *buffer, uint32_t buffer_size, uint32_t *start_index, uint32_t *packet_size) {
    /*
    这个函数用于在流式数据buffer中查找第一个完整的包
    最大寻找buffer_size个字节
    返回1表示找到完整包，0表示未找到

    start_index:包开始位置,为输出参数
    packet_size:包大小,为输出参数
    */
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

