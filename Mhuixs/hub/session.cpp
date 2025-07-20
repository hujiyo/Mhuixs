#include "manager.h"
#include "pkg.h"
#include "unistd.h"
#include "errno.h"
#include "string.h"
#include "stdlib.h"

////本地函数////
void compact_buffer_(buffer_struct* buffer);//压缩网络缓冲区
uint32_t get_buffer_available_size(buffer_struct* buffer);//获取网络缓冲区可用空间
// 网络缓冲区操作
int buffer_write_(buffer_struct* buffer, const uint8_t* data, uint32_t len);//将数据写入网络缓冲区
int buffer_write_(buffer_struct* buffer, const uint8_t* data, uint32_t len);//将数据写入网络缓冲区
uint32_t buffer_read_(buffer_struct* buffer, uint8_t* data, uint32_t max_len);//从网络缓冲区中读取数据
uint32_t buffer_peek_(buffer_struct* buffer, uint8_t* data, uint32_t max_len);//查看网络缓冲区数据
// 会话网络操作
int session_process_incoming_packets_(session_t* session);//处理入站数据包

//缓冲区操作函数
int buffer_write_(buffer_struct* buffer, const uint8_t* data, uint32_t len) {
    if (!buffer || !data || len == 0) return SESS_INVALID;
int buffer_write_(buffer_struct* buffer, const uint8_t* data, uint32_t len) {
    if (!buffer || !data || len == 0) return SESS_INVALID;
    
    pthread_mutex_lock(&buffer->mutex);
    pthread_mutex_lock(&buffer->mutex);
    
    // 检查是否需要扩容
    if (buffer->write_pos + len > buffer->capacity) {
        if (buffer->read_pos > 0) {
    if (buffer->write_pos + len > buffer->capacity) {
        if (buffer->read_pos > 0) {
            // 先尝试压缩
            uint32_t data_size = buffer->write_pos - buffer->read_pos;
            uint32_t data_size = buffer->write_pos - buffer->read_pos;
            if (data_size > 0) {
                memmove(buffer->buffer, buffer->buffer + buffer->read_pos, data_size);
                memmove(buffer->buffer, buffer->buffer + buffer->read_pos, data_size);
            }
            buffer->write_pos = data_size;
            buffer->read_pos = 0;
            buffer->write_pos = data_size;
            buffer->read_pos = 0;
        }
        
        if (buffer->write_pos + len > buffer->capacity) {
        if (buffer->write_pos + len > buffer->capacity) {
            // 仍然不够，需要扩容
            uint32_t new_capacity = (buffer->capacity * 2 > buffer->write_pos + len) ?
                                   buffer->capacity * 2 : buffer->write_pos + len;
            uint8_t* new_buffer = (uint8_t*)realloc(buffer->buffer, new_capacity);
            uint32_t new_capacity = (buffer->capacity * 2 > buffer->write_pos + len) ?
                                   buffer->capacity * 2 : buffer->write_pos + len;
            uint8_t* new_buffer = (uint8_t*)realloc(buffer->buffer, new_capacity);
            if (!new_buffer) {
                pthread_mutex_unlock(&buffer->mutex);
                pthread_mutex_unlock(&buffer->mutex);
                return SESS_ERR;
            }
            buffer->buffer = new_buffer;
            buffer->capacity = new_capacity;
            buffer->buffer = new_buffer;
            buffer->capacity = new_capacity;
        }
    }
    
    memcpy(buffer->buffer + buffer->write_pos, data, len);
    buffer->write_pos += len;
    memcpy(buffer->buffer + buffer->write_pos, data, len);
    buffer->write_pos += len;
    
    pthread_mutex_unlock(&buffer->mutex);
    pthread_mutex_unlock(&buffer->mutex);
    return SESS_OK;
}


uint32_t buffer_read_(buffer_struct* buffer, uint8_t* data, uint32_t max_len) {
    if (!buffer || !data || max_len == 0) return 0;
    
    pthread_mutex_lock(&buffer->mutex);
    
    uint32_t available_data = buffer->write_pos - buffer->read_pos;
    uint32_t to_read = (max_len < available_data) ? max_len : available_data;
    
    if (to_read > 0) {
        memcpy(data, buffer->buffer + buffer->read_pos, to_read);
        buffer->read_pos += to_read;
    }
    
    pthread_mutex_unlock(&buffer->mutex);
    return to_read;
}


uint32_t buffer_peek_(buffer_struct* buffer, uint8_t* data, uint32_t max_len) {
    if (!buffer || !data || max_len == 0) return 0;
    
    pthread_mutex_lock(&buffer->mutex);
    
    uint32_t available_data = buffer->write_pos - buffer->read_pos;
    uint32_t to_peek = (max_len < available_data) ? max_len : available_data;
    
    if (to_peek > 0) {
        memcpy(data, buffer->buffer + buffer->read_pos, to_peek);
    }
    
    pthread_mutex_unlock(&buffer->mutex);
    return to_peek;
}


uint32_t get_buffer_available_size(buffer_struct* buffer) {
    if (!buffer) return 0;
    
    pthread_mutex_lock(&buffer->mutex);
    uint32_t available = buffer->write_pos - buffer->read_pos;
    pthread_mutex_unlock(&buffer->mutex);
    
    return available;
}

void compact_buffer_(buffer_struct* buffer) {
    if (!buffer) return;
    
    pthread_mutex_lock(&buffer->mutex);
    
    if (buffer->read_pos > 0) {
        uint32_t data_size = buffer->write_pos - buffer->read_pos;
        if (data_size > 0) {
            memmove(buffer->buffer, buffer->buffer + buffer->read_pos, data_size);
        }
        buffer->write_pos = data_size;
        buffer->read_pos = 0;
    }
    
    pthread_mutex_unlock(&buffer->mutex);
}


//会话相关函数
void shutdown_session_on_(session_t* session) {
    pthread_mutex_lock(&session->session_mutex);
    if (session->state == SESS_IDLE) {
        pthread_mutex_unlock(&session->session_mutex);
        return;
    }

    // 关闭套接字 客户端断开连接
    if (session->socket_fd >= 0) {
        close(session->socket_fd);
        session->socket_fd = -1;
    }

    session->state = SESS_IDLE;//标记为空闲，内存清理的事情交给下一个使用者
    pthread_mutex_unlock(&session->session_mutex);
}

int session_receive_data_(session_t* session) {
    if (!session || session->socket_fd < 0) return SESS_INVALID;

    uint8_t buffer[8192];
    const ssize_t bytes_received = recv(session->socket_fd, buffer, sizeof(buffer), MSG_DONTWAIT);

    if (bytes_received > 0) {
        // 写入接收缓冲区
        if (buffer_write_(&session->recv_buffer, buffer, bytes_received) == SESS_OK) {
        if (buffer_write_(&session->recv_buffer, buffer, bytes_received) == SESS_OK) {
            session->last_activity = time(NULL);
            return bytes_received;
        }
        return SESS_ERR;
        return SESS_ERR;
    }
    else if (bytes_received == 0) {
        // 连接关闭
        return SESS_ERR;
        return SESS_ERR;
    }
    else {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0; // 暂时无数据
        }
        return SESS_ERR;
        return SESS_ERR;
    }
}

int session_send_data_(session_t* session) {
    if (!session || session->socket_fd < 0) return SESS_INVALID;

    // 这个函数现在只负责处理会话的发送缓冲区
    // 实际的响应发送由响应线程处理
    return 0; // 暂时返回0，因为发送逻辑在响应线程中
    // 这个函数现在只负责处理会话的发送缓冲区
    // 实际的响应发送由响应线程处理
    return 0; // 暂时返回0，因为发送逻辑在响应线程中
}

int session_process_incoming_packets_(session_t* session) {
    if (!session) return SESS_INVALID;

    // 使用pkg库处理数据包
    // 使用pkg库处理数据包
    uint32_t available = get_buffer_available_size(&session->recv_buffer);
    if (available < PACKET_HEADER_SIZE) return 0;

    uint8_t buffer[8192];
    uint32_t bytes_read = buffer_peek_(&session->recv_buffer, buffer, sizeof(buffer));

    // 查找完整的数据包
    uint32_t start_index, packet_size;
    if (find_packet_boundary(buffer, bytes_read, &start_index, &packet_size)) {
        // 解包
        str unpacked_data = unpacking(buffer + start_index, packet_size);
        
        if (unpacked_data.string && unpacked_data.len > 0) {
            // 从解包后的数据中提取命令信息
            // 假设数据格式为：command_id(4字节) + sequence(4字节) + 用户数据
            if (unpacked_data.len >= 8) {
                uint32_t command_id, sequence;
                memcpy(&command_id, unpacked_data.string, 4);
                memcpy(&sequence, unpacked_data.string + 4, 4);
                
                // 转换为主机字节序
                command_id = ntohl(command_id);
                sequence = ntohl(sequence);
                
                // 创建命令结构体
                command_t* cmd = (command_t*)calloc(1, sizeof(command_t));
                // 创建命令结构体
                command_t* cmd = (command_t*)calloc(1, sizeof(command_t));
                if (cmd) {
                    cmd->session = session;
                    cmd->command_id = (CommandNumber)command_id;
                    cmd->session = session;
                    cmd->command_id = (CommandNumber)command_id;
                    // 将命令加入队列
                    command_queue.enqueue(cmd);
                    
                    // 从缓冲区移除已处理的数据
                    uint8_t temp_buffer[8192];
                    buffer_read_(&session->recv_buffer, temp_buffer, start_index + packet_size);
                    
                    // 释放解包后的数据
                    free(unpacked_data.string);
                    return 1;
                    command_queue.enqueue(cmd);
                    
                    // 从缓冲区移除已处理的数据
                    uint8_t temp_buffer[8192];
                    buffer_read_(&session->recv_buffer, temp_buffer, start_index + packet_size);
                    
                    // 释放解包后的数据
                    free(unpacked_data.string);
                    return 1;
                }
            }
            
            // 释放解包后的数据
            free(unpacked_data.string);
        }
    }

    return 0;
}

