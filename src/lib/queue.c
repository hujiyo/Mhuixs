#include "list_queue.h"
#include <stdlib.h>
#include <string.h>

int list_queue_init(list_queue_t *queue) {
    if (!queue) return -1;

    memset(queue, 0, sizeof(list_queue_t));
    
    // 创建 list
    queue->list = list_create();
    if (!queue->list) return -1;
    
    // 初始化互斥锁
    int ret = uv_mutex_init(&queue->mutex);
    if (ret != 0) {
        free_list(queue->list);
        return ret;
    }
    
    // 初始化条件变量
    ret = uv_cond_init(&queue->cond);
    if (ret != 0) {
        uv_mutex_destroy(&queue->mutex);
        free_list(queue->list);
        return ret;
    }
    
    queue->shutdown = false;
    
    return 0;
}

void list_queue_destroy(list_queue_t *queue) {
    if (!queue) return;
    
    // 清空队列
    uv_mutex_lock(&queue->mutex);
    if (queue->list) {
        list_clear(queue->list);
        free_list(queue->list);
        queue->list = NULL;
    }
    uv_mutex_unlock(&queue->mutex);
    
    uv_cond_destroy(&queue->cond);
    uv_mutex_destroy(&queue->mutex);
}

bool list_queue_enqueue(list_queue_t *queue, void *data) {
    if (!queue || !data) return false;
    
    uv_mutex_lock(&queue->mutex);
    
    if (queue->shutdown) {
        uv_mutex_unlock(&queue->mutex);
        return false;
    }
    
    // 使用 list_rpush 在尾部插入
    int ret = list_rpush(queue->list, data);
    
    if (ret == 0) {
        // 唤醒一个等待的消费者
        uv_cond_signal(&queue->cond);
    }
    
    uv_mutex_unlock(&queue->mutex);
    
    return ret == 0;
}

bool list_queue_dequeue(list_queue_t *queue, void **data) {
    if (!queue || !data) return false;
    
    uv_mutex_lock(&queue->mutex);
    
    // 等待直到有数据或队列关闭
    while (list_size(queue->list) == 0 && !queue->shutdown) {
        uv_cond_wait(&queue->cond, &queue->mutex);
    }
    
    // 如果队列关闭且没有数据，返回失败
    if (queue->shutdown && list_size(queue->list) == 0) {
        uv_mutex_unlock(&queue->mutex);
        return false;
    }
    
    // 使用 list_lpop 从头部取出
    *data = list_lpop(queue->list);
    
    uv_mutex_unlock(&queue->mutex);
    
    // list_lpop 返回 (Obj)(intptr_t)(-1) 表示失败
    return *data != (void*)(intptr_t)(-1);
}

bool list_queue_try_dequeue(list_queue_t *queue, void **data) {
    if (!queue || !data) return false;
    
    uv_mutex_lock(&queue->mutex);
    
    if (list_size(queue->list) == 0) {
        uv_mutex_unlock(&queue->mutex);
        return false;
    }
    
    *data = list_lpop(queue->list);
    
    uv_mutex_unlock(&queue->mutex);
    
    return *data != (void*)(intptr_t)(-1);
}

uint32_t list_queue_size(list_queue_t *queue) {
    if (!queue) return 0;
    
    uv_mutex_lock(&queue->mutex);
    uint32_t size = (uint32_t)list_size(queue->list);
    uv_mutex_unlock(&queue->mutex);
    
    return size;
}

void list_queue_shutdown(list_queue_t *queue) {
    if (!queue) return;
    
    uv_mutex_lock(&queue->mutex);
    queue->shutdown = true;
    // 唤醒所有等待的线程
    uv_cond_broadcast(&queue->cond);
    uv_mutex_unlock(&queue->mutex);
}
