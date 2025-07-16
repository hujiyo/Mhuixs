#include "comdq.h"

//生成全局命令队列
command_queue_t* create_command_queue() {
    command_queue_t* queue =(command_queue_t*)calloc(1,sizeof(command_queue_t));
    if (!queue) return NULL;

    if (pthread_mutex_init(&queue->mutex, NULL) != 0) {
        free(queue);
        return NULL;
    }

    if (pthread_cond_init(&queue->cond, NULL) != 0) {
        pthread_mutex_destroy(&queue->mutex);
        free(queue);
        return NULL;
    }
    return queue;
}
void destroy_command_queue(command_queue_t* queue) {
    /*
     *确保：
     *没有线程会再使用这个队列
     *没有线程正阻塞在条件变量 `queue->cond`
     *
     *所以销毁前必须先调用command_queue_shutdown函数
     */
    if (!queue) return;
    //先清空队列
    pthread_mutex_lock(&queue->mutex);
    queue->command_queue.clear();
    pthread_mutex_unlock(&queue->mutex);
    pthread_mutex_destroy(&queue->mutex);//删除锁之前先解锁
    pthread_cond_destroy(&queue->cond);
    free(queue);
}
size_t command_queue_push(command_queue_t* queue,const command_t* cmd,const size_t num) {
    if (!queue || !cmd) return -1;
    pthread_mutex_lock(&queue->mutex);
    if (queue->shutdown) {
        pthread_mutex_unlock(&queue->mutex);
        return -1;
    }
    size_t ret_num = 0 ;
    for (size_t i = 0; i < num; i++) {
        queue->command_queue.push_back(cmd[i]);
        ret_num++;
    }

    pthread_cond_signal(&queue->cond);//提示正在等待的执行线程
    pthread_mutex_unlock(&queue->mutex);
    return ret_num;
}
command_t* command_queue_pop(command_queue_t* queue, uint32_t timeout_ms,size_t* num_back) {
    if (!queue) return NULL;

    pthread_mutex_lock(&queue->mutex);

    timespec ts;
    if (timeout_ms > 0) {
        timeval tv;
        gettimeofday(&tv, NULL);
        ts.tv_sec = tv.tv_sec + timeout_ms / 1000;
        ts.tv_nsec = (tv.tv_usec + (timeout_ms % 1000) * 1000) * 1000;
        if (ts.tv_nsec >= 1000000000) {
            ts.tv_sec++;
            ts.tv_nsec -= 1000000000;
        }
    }

    while (!queue->head && !queue->shutdown) {
        if (timeout_ms > 0) {
            if (pthread_cond_timedwait(&queue->cond, &queue->mutex, &ts) != 0) {
                break; // 超时
            }
        } else {
            pthread_cond_wait(&queue->cond, &queue->mutex);
        }
    }

    command_t* cmd = NULL;
    if (queue->head) {
        cmd = queue->head;
        queue->head = cmd->next;
        if (!queue->head) queue->tail = NULL;
        cmd->next = NULL;
        queue->count--;
    }

    pthread_mutex_unlock(&queue->mutex);
    return cmd;
}
command_t* command_queue_try_pop(command_queue_t* queue) {
    if (!queue) return NULL;

    pthread_mutex_lock(&queue->mutex);

    command_t* cmd = NULL;
    if (queue->head) {
        cmd = queue->head;
        queue->head = cmd->next;
        if (!queue->head) queue->tail = NULL;
        cmd->next = NULL;
        queue->count--;
    }

    pthread_mutex_unlock(&queue->mutex);
    return cmd;
}
void shutdown_command_queue(command_queue_t* queue) {
    if (!queue) return;

    pthread_mutex_lock(&queue->mutex);
    queue->shutdown = 1;
    pthread_cond_broadcast(&queue->cond);
    pthread_mutex_unlock(&queue->mutex);
}
uint32_t command_queue_size(command_queue_t* queue) {
    if (!queue) return 0;

    pthread_mutex_lock(&queue->mutex);
    uint32_t size = queue->count;
    pthread_mutex_unlock(&queue->mutex);

    return size;
}
command_t* command_create(UID caller,const CommandNumber cmd_id,const uint32_t seq_num,const uint32_t priority,
                         const uint8_t* data,const uint32_t data_len) {
    command_t* cmd = (command_t*)malloc(sizeof(command_t));
    if (!cmd) return NULL;

    memset(cmd, 0, sizeof(command_t));
    cmd->caller=caller;
    cmd->command_id = cmd_id;
    cmd->sequence_num = seq_num;
    cmd->priority = priority;
    cmd->data_len = data_len;

    if (data_len > 0 && data) {
        cmd->data = (uint8_t*)malloc(data_len);
        if (!cmd->data) {
            free(cmd);
            return NULL;
        }
        memcpy(cmd->data, data, data_len);
    }

    return cmd;
}
void command_destroy(command_t* cmd) {
    if (!cmd) return;

    if (cmd->data) {
        free(cmd->data);
    }
    free(cmd);
}