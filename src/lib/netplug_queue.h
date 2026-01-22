#ifndef NETPLUG_QUEUE_H
#define NETPLUG_QUEUE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "queue.h"

typedef list_queue_t netplug_queue_t;

static inline int netplug_queue_init(netplug_queue_t *q) {
    return list_queue_init(q);
}

static inline void netplug_queue_destroy(netplug_queue_t *q) {
    list_queue_destroy(q);
}

static inline bool netplug_queue_enqueue(netplug_queue_t *q, void *data) {
    return list_queue_enqueue(q, data);
}

static inline bool netplug_queue_dequeue(netplug_queue_t *q, void **data) {
    return list_queue_dequeue(q, data);
}

static inline bool netplug_queue_try_dequeue(netplug_queue_t *q, void **data) {
    return list_queue_try_dequeue(q, data);
}

static inline uint32_t netplug_queue_size(netplug_queue_t *q) {
    return list_queue_size(q);
}

static inline void netplug_queue_shutdown(netplug_queue_t *q) {
    list_queue_shutdown(q);
}

#endif

#ifdef __cplusplus
}
#endif

