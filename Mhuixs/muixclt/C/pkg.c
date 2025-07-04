/*
#版权所有 (c) HuJi 2025.1
#保留所有权利
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
Email:hj18914255909@outlook.com
*/
/*
这个文件将会对数据进行打包、解包操作。
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <time.h>

/*
这个文件将会对数据进行打包、解包操作。
Mhuixs 通信协议定义：

包头结构 (16字节):
+--------+--------+--------+--------+--------+--------+--------+--------+
|  魔数  |  版本  |  类型  |  状态  |    序列号     |    数据长度    |
| 4字节  | 2字节  | 2字节  | 2字节  |    4字节     |    4字节      |
+--------+--------+--------+--------+--------+--------+--------+--------+

包体结构:
+--------+--------+--------+--------+
|            数据内容              |
|          (可变长度)              |
+--------+--------+--------+--------+

包尾结构 (4字节):
+--------+--------+--------+--------+
|           校验和(CRC32)         |
|             4字节               |
+--------+--------+--------+--------+
*/

// 协议常量定义
#define MHUIXS_MAGIC 0x4D485558     // "MHUX"
#define MHUIXS_VERSION 0x0100       // 版本 1.0
#define MAX_PACKET_SIZE 65536       // 最大包大小 64KB
#define PACKET_HEADER_SIZE 16       // 包头大小
#define PACKET_FOOTER_SIZE 4        // 包尾大小
#define MIN_PACKET_SIZE (PACKET_HEADER_SIZE + PACKET_FOOTER_SIZE) // 最小包大小

// 包类型定义
typedef enum {
    PKT_QUERY = 0x0001,         // 查询请求
    PKT_RESPONSE = 0x0002,      // 查询响应
    PKT_ERROR = 0x0003,         // 错误响应
    PKT_HEARTBEAT = 0x0004,     // 心跳包
    PKT_AUTH = 0x0005,          // 认证请求
    PKT_AUTH_RESP = 0x0006,     // 认证响应
    PKT_DISCONNECT = 0x0007,    // 断开连接
    PKT_KEEPALIVE = 0x0008      // 保活包
} PacketType;

// 包状态定义
typedef enum {
    PKT_STATUS_OK = 0x0000,     // 成功
    PKT_STATUS_ERROR = 0x0001,  // 错误
    PKT_STATUS_PENDING = 0x0002, // 待处理
    PKT_STATUS_TIMEOUT = 0x0003, // 超时
    PKT_STATUS_AUTH_FAILED = 0x0004, // 认证失败
    PKT_STATUS_PERMISSION_DENIED = 0x0005, // 权限不足
    PKT_STATUS_INVALID_QUERY = 0x0006, // 无效查询
    PKT_STATUS_SERVER_ERROR = 0x0007  // 服务器错误
} PacketStatus;

// 包头结构
typedef struct {
    uint32_t magic;         // 魔数
    uint16_t version;       // 版本
    uint16_t type;          // 类型
    uint16_t status;        // 状态
    uint32_t sequence;      // 序列号
    uint32_t data_length;   // 数据长度
} __attribute__((packed)) PacketHeader;

// 包结构
typedef struct {
    PacketHeader header;    // 包头
    uint8_t *data;         // 数据
    uint32_t checksum;     // 校验和
} Packet;

// 全局序列号
static uint32_t g_sequence = 0;

// 函数声明
uint32_t calculate_crc32(const uint8_t *data, size_t length);
uint32_t get_next_sequence();
int validate_packet_header(const PacketHeader *header);
void print_packet_info(const Packet *packet);

// CRC32校验和计算
uint32_t calculate_crc32(const uint8_t *data, size_t length) {
    uint32_t crc = 0xFFFFFFFF;
    static const uint32_t crc_table[256] = {
        0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F,
        0xE963A535, 0x9E6495A3, 0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
        0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91, 0x1DB71064, 0x6AB020F2,
        0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
        0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9,
        0xFA0F3D63, 0x8D080DF5, 0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
        0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B, 0x35B5A8FA, 0x42B2986C,
        0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
        0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423,
        0xCFBA9599, 0xB8BDA50F, 0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
        0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D, 0x76DC4190, 0x01DB7106,
        0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
        0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D,
        0x91646C97, 0xE6635C01, 0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
        0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457, 0x65B0D9C6, 0x12B7E950,
        0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
        0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7,
        0xA4D1C46D, 0xD3D6F4FB, 0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
        0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9, 0x5005713C, 0x270241AA,
        0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
        0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81,
        0xB7BD5C3B, 0xC0BA6CAD, 0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
        0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683, 0xE3630B12, 0x94643B84,
        0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
        0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB,
        0x196C3671, 0x6E6B06E7, 0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
        0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5, 0xD6D6A3E8, 0xA1D1937E,
        0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
        0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55,
        0x316E8EEF, 0x4669BE79, 0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
        0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F, 0xC5BA3BBE, 0xB2BD0B28,
        0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
        0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F,
        0x72076785, 0x05005713, 0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
        0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21, 0x86D3D2D4, 0xF1D4E242,
        0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
        0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69,
        0x616BFFD3, 0x166CCF45, 0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
        0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB, 0xAED16A4A, 0xD9D65ADC,
        0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
        0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD70693,
        0x54DE5729, 0x23D967BF, 0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
        0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
    };
    
    for (size_t i = 0; i < length; i++) {
        crc = crc_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    
    return crc ^ 0xFFFFFFFF;
}

// 获取下一个序列号
uint32_t get_next_sequence() {
    return ++g_sequence;
}

// 验证包头
int validate_packet_header(const PacketHeader *header) {
    if (!header) return 0;
    
    if (ntohl(header->magic) != MHUIXS_MAGIC) {
        printf("错误: 无效的魔数 0x%08X\n", ntohl(header->magic));
        return 0;
    }
    
    if (ntohs(header->version) != MHUIXS_VERSION) {
        printf("错误: 不支持的版本 0x%04X\n", ntohs(header->version));
        return 0;
    }
    
    uint32_t data_length = ntohl(header->data_length);
    if (data_length > MAX_PACKET_SIZE - MIN_PACKET_SIZE) {
        printf("错误: 数据长度过大 %u\n", data_length);
        return 0;
    }
    
    return 1;
}

// 打印包信息
void print_packet_info(const Packet *packet) {
    if (!packet) return;
    
    printf("包信息:\n");
    printf("  魔数: 0x%08X\n", ntohl(packet->header.magic));
    printf("  版本: 0x%04X\n", ntohs(packet->header.version));
    printf("  类型: 0x%04X\n", ntohs(packet->header.type));
    printf("  状态: 0x%04X\n", ntohs(packet->header.status));
    printf("  序列号: %u\n", ntohl(packet->header.sequence));
    printf("  数据长度: %u\n", ntohl(packet->header.data_length));
    printf("  校验和: 0x%08X\n", packet->checksum);
}

// 创建数据包
Packet* create_packet(PacketType type, PacketStatus status, const uint8_t *data, uint32_t data_length) {
    if (data_length > MAX_PACKET_SIZE - MIN_PACKET_SIZE) {
        printf("错误: 数据长度过大\n");
        return NULL;
    }
    
    Packet *packet = (Packet*)malloc(sizeof(Packet));
    if (!packet) {
        printf("错误: 内存分配失败\n");
        return NULL;
    }
    
    // 初始化包头
    packet->header.magic = htonl(MHUIXS_MAGIC);
    packet->header.version = htons(MHUIXS_VERSION);
    packet->header.type = htons(type);
    packet->header.status = htons(status);
    packet->header.sequence = htonl(get_next_sequence());
    packet->header.data_length = htonl(data_length);
    
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
    
    // 计算校验和
    uint8_t *temp_buffer = (uint8_t*)malloc(PACKET_HEADER_SIZE + data_length);
    if (!temp_buffer) {
        printf("错误: 临时缓冲区分配失败\n");
        free(packet->data);
        free(packet);
        return NULL;
    }
    
    memcpy(temp_buffer, &packet->header, PACKET_HEADER_SIZE);
    if (data_length > 0) {
        memcpy(temp_buffer + PACKET_HEADER_SIZE, packet->data, data_length);
    }
    
    packet->checksum = calculate_crc32(temp_buffer, PACKET_HEADER_SIZE + data_length);
    free(temp_buffer);
    
    return packet;
}

// 销毁数据包
void destroy_packet(Packet *packet) {
    if (!packet) return;
    
    if (packet->data) {
        free(packet->data);
    }
    free(packet);
}

// 序列化数据包
uint8_t* serialize_packet(const Packet *packet, uint32_t *total_size) {
    if (!packet || !total_size) return NULL;
    
    uint32_t data_length = ntohl(packet->header.data_length);
    *total_size = PACKET_HEADER_SIZE + data_length + PACKET_FOOTER_SIZE;
    
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
    
    // 复制校验和
    uint32_t checksum_net = htonl(packet->checksum);
    memcpy(buffer + PACKET_HEADER_SIZE + data_length, &checksum_net, PACKET_FOOTER_SIZE);
    
    return buffer;
}

// 反序列化数据包
Packet* deserialize_packet(const uint8_t *buffer, uint32_t buffer_size) {
    if (!buffer || buffer_size < MIN_PACKET_SIZE) {
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
    uint32_t expected_size = PACKET_HEADER_SIZE + data_length + PACKET_FOOTER_SIZE;
    
    if (buffer_size < expected_size) {
        printf("错误: 缓冲区大小不足\n");
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
    
    // 解析校验和
    uint32_t checksum_net;
    memcpy(&checksum_net, buffer + PACKET_HEADER_SIZE + data_length, PACKET_FOOTER_SIZE);
    packet->checksum = ntohl(checksum_net);
    
    // 验证校验和
    uint32_t calculated_checksum = calculate_crc32(buffer, PACKET_HEADER_SIZE + data_length);
    if (packet->checksum != calculated_checksum) {
        printf("错误: 校验和不匹配 (期望: 0x%08X, 实际: 0x%08X)\n", 
               packet->checksum, calculated_checksum);
        free(packet->data);
        free(packet);
        return NULL;
    }
    
    return packet;
}

// 创建查询包
Packet* create_query_packet(const char *query) {
    if (!query) return NULL;
    
    uint32_t query_length = strlen(query);
    return create_packet(PKT_QUERY, PKT_STATUS_PENDING, (const uint8_t*)query, query_length);
}

// 创建响应包
Packet* create_response_packet(const char *response, PacketStatus status) {
    if (!response) return NULL;
    
    uint32_t response_length = strlen(response);
    return create_packet(PKT_RESPONSE, status, (const uint8_t*)response, response_length);
}

// 创建错误包
Packet* create_error_packet(const char *error_message) {
    if (!error_message) return NULL;
    
    uint32_t error_length = strlen(error_message);
    return create_packet(PKT_ERROR, PKT_STATUS_ERROR, (const uint8_t*)error_message, error_length);
}

// 创建心跳包
Packet* create_heartbeat_packet() {
    time_t current_time = time(NULL);
    return create_packet(PKT_HEARTBEAT, PKT_STATUS_OK, (const uint8_t*)&current_time, sizeof(time_t));
}

// 创建认证包
Packet* create_auth_packet(const char *username, const char *password) {
    if (!username || !password) return NULL;
    
    char auth_data[256];
    snprintf(auth_data, sizeof(auth_data), "%s:%s", username, password);
    
    uint32_t auth_length = strlen(auth_data);
    return create_packet(PKT_AUTH, PKT_STATUS_PENDING, (const uint8_t*)auth_data, auth_length);
}

// 获取包类型字符串
const char* get_packet_type_string(PacketType type) {
    switch (type) {
        case PKT_QUERY: return "查询请求";
        case PKT_RESPONSE: return "查询响应";
        case PKT_ERROR: return "错误响应";
        case PKT_HEARTBEAT: return "心跳包";
        case PKT_AUTH: return "认证请求";
        case PKT_AUTH_RESP: return "认证响应";
        case PKT_DISCONNECT: return "断开连接";
        case PKT_KEEPALIVE: return "保活包";
        default: return "未知类型";
    }
}

// 获取包状态字符串
const char* get_packet_status_string(PacketStatus status) {
    switch (status) {
        case PKT_STATUS_OK: return "成功";
        case PKT_STATUS_ERROR: return "错误";
        case PKT_STATUS_PENDING: return "待处理";
        case PKT_STATUS_TIMEOUT: return "超时";
        case PKT_STATUS_AUTH_FAILED: return "认证失败";
        case PKT_STATUS_PERMISSION_DENIED: return "权限不足";
        case PKT_STATUS_INVALID_QUERY: return "无效查询";
        case PKT_STATUS_SERVER_ERROR: return "服务器错误";
        default: return "未知状态";
    }
}
