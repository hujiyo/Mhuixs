/*
#版权所有 (c) Mhuixs-team 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2025.1
Email:hj18914255909@outlook.com
*/
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#define err -1

typedef struct STREAM{
    uint8_t *string;
    uint32_t len;
} STREAM;

STREAM* makeSTREAM() {
    return (STREAM *)calloc(1,sizeof(STREAM));
}

void freeSTREAM(STREAM *stream) {
    if (stream == NULL){
        return;
    }
    free(stream->string);
    free(stream);
}

int readSTREAM(STREAM *stream, uint32_t pos, uint8_t *bitestream, uint32_t length)
{
    //从pos开始读取length个字节到bitestream中
    if (stream == NULL || bitestream == NULL || pos >= stream->len) {
        return err;
    }
    // 计算实际要读取的长度
    uint32_t available = stream->len - pos;
    uint32_t read_length = (length < available) ? length : available;
    // 复制数据到bitestream
    memcpy(bitestream, stream->string + pos, read_length);
    return read_length;
}
int writeSTREAM(STREAM *stream, uint32_t pos, uint8_t *bitestream, uint32_t length)
{
    if (stream == NULL || bitestream == NULL || length == 0) {
        return err;
    }
    if (pos > stream->len) pos = stream->len; // 如果位置超出范围，则追加到末尾
    // 如果超范围，则扩展内存以容纳新数据
    if (pos + length > stream->len) {
        uint8_t *new_string = (uint8_t *)realloc(stream->string, pos + length);
        if (new_string == NULL) return err;
        stream->string = new_string;
        stream->len = pos + length;
    }
    // 将新数据复制到指定位置
    memcpy(stream->string + pos, bitestream, length);
    return pos;
}
