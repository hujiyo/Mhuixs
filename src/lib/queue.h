#ifndef LIST_QUEUE_H
#define LIST_QUEUE_H

/**
 * 基于 list.c 的高性能线程安全队列
 * 
 * 特点：
 * - 使用分块存储，减少内存分配次数
 * - 缓存友好，性能优异
 * - 线程安全，支持阻塞等待
 * - 适合大量元素的队列场景
 */

#include <stdint.h>
#include <stdbool.h>
#include <uv.h>
#include "list.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    LIST* list;
    uv_mutex_t mutex;
    uv_cond_t cond;
    bool shutdown;
} list_queue_t;

// 初始化队列
int list_queue_init(list_queue_t *queue);

// 销毁队列
void list_queue_destroy(list_queue_t *queue);

// 入队（非阻塞）
bool list_queue_enqueue(list_queue_t *queue, void *data);

// 出队（阻塞，直到有数据或队列关闭）
bool list_queue_dequeue(list_queue_t *queue, void **data);

// 尝试出队（非阻塞）
bool list_queue_try_dequeue(list_queue_t *queue, void **data);

// 获取队列大小
uint32_t list_queue_size(list_queue_t *queue);

// 关闭队列（唤醒所有等待的线程）
void list_queue_shutdown(list_queue_t *queue);

#ifdef __cplusplus
}
#endif

#endif // LIST_QUEUE_H
