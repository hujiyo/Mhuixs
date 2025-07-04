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
#include <time.h>

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

// 函数声明

// 基础函数
uint32_t calculate_crc32(const uint8_t *data, size_t length);
uint32_t get_next_sequence();
int validate_packet_header(const PacketHeader *header);
void print_packet_info(const Packet *packet);

// 包管理函数
Packet* create_packet(PacketType type, PacketStatus status, const uint8_t *data, uint32_t data_length);
void destroy_packet(Packet *packet);
uint8_t* serialize_packet(const Packet *packet, uint32_t *total_size);
Packet* deserialize_packet(const uint8_t *buffer, uint32_t buffer_size);

// 特定类型包创建函数
Packet* create_query_packet(const char *query);
Packet* create_response_packet(const char *response, PacketStatus status);
Packet* create_error_packet(const char *error_message);
Packet* create_heartbeat_packet();
Packet* create_auth_packet(const char *username, const char *password);

// 工具函数
const char* get_packet_type_string(PacketType type);
const char* get_packet_status_string(PacketStatus status);

#endif // PKG_H 