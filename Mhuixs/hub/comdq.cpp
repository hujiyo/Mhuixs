#include "comdq.h"

// 生成全局命令队列
command_queue_t* create_command_queue() {
    command_queue_t* queue = new(nothrow) command_queue_t();
    if (!queue) return NULL;
    queue->shutdown = 0;//显式赋值为0
    return queue;
}

void destroy_command_queue(command_queue_t* queue) {
    if (!queue) return;
    // 清空队列并释放所有命令
    command_t* cmd = NULL;
    while (queue->command_queue.try_dequeue(cmd)) {
        destroy_command(cmd);
    }
    delete queue;
}

int command_queue_push(command_queue_t* queue, command_t* cmd, const size_t num) {
    command_t* *temp = (command_t* *)malloc(sizeof(command_t*) * num);
    for (size_t i = 0; i < num;temp[i] = &cmd[i],i++)

    if (!queue || !cmd || queue->shutdown ||
        queue->command_queue.enqueue_bulk(temp,num) == false
        ) {
        printf("[error]command_queue_push::ERR!!!");
        free(temp);
        return 1;
    }
    free(temp);
    return 0;
}

command_t** command_queue_pop(command_queue_t* queue,command_t* *cmd,size_t max,size_t* num_back) {
    if (!queue||!queue->shutdown) return NULL;
    *num_back = queue->command_queue.try_dequeue_bulk(cmd,max);
    return cmd;
}

command_t* create_command(UID caller,const CommandNumber cmd_id,const uint32_t seq_num,
                         const uint8_t* data,const uint32_t data_len) {
    command_t* cmd = (command_t*)calloc(1,sizeof(command_t));
    if (!cmd) return NULL;

    cmd->caller=caller;
    cmd->command_id = cmd_id;
    cmd->sequence_num = seq_num;
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
void destroy_command(command_t* cmd) {
    if (!cmd) return;
    if (cmd->data) free(cmd->data);
    free(cmd);
}