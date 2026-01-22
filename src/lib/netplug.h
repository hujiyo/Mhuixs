#ifndef NETPLUG_H
#define NETPLUG_H

#include "funseq.h"
#include "getid.h"
#include "lib/pkg.h"
#include "concurrentqueue.h"
#include <atomic>
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <time.h>
#include <unistd.h>
#include <uv.h>
#include <sys/resource.h>

using namespace std;
using namespace moodycamel;

#define MAX_SESSIONS 1024
#define BUFFER_SIZE 8192
#define TIMEOUT_MS 300000
#define CLEANUP_INTERVAL_MS 60000

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { SESS_IDLE = 1, SESS_ALIVE = 2, SESS_USING = 3 } session_state_t;

typedef struct {
  uint8_t *data;
  uint32_t capacity;
  uint32_t size;
  uint32_t read_pos;
} buffer_t;

typedef struct {
  SID session_id;
  UID user_id;
  uv_tcp_t tcp_handle;
  buffer_t recv_buffer;
  session_state_t state;
  uint64_t last_activity;
  uint32_t pool_index;
  HOOK *current_hook;
} session_t;

typedef struct {
  session_t *session;
  CommandNumber command_id;
  char *obj; //对象 null表示不变
  void *k1;
  void *k2;
  void *k3;
} command_t;

typedef struct {
  session_t *session;
  uint32_t response_len;
  uint32_t sent_len;
  union {
    uint8_t *data;
    uint8_t inline_data[48];
  };
} response_t;

typedef struct {
  uv_loop_t *loop;
  uv_tcp_t server;
  uv_timer_t cleanup_timer;

  session_t *sessions;
  uint32_t *idle_sessions;
  uint32_t max_sessions;
  uint32_t idle_count;

  uv_mutex_t pool_mutex;
} netplug_t;

// 全局变量
extern netplug_t *g_netplug;
extern BlockingConcurrentQueue<command_t *> command_queue;

// API函数
int netplug_init(uint16_t port);
int netplug_start();
void netplug_shutdown();
int auth_session(SID session_id, UID uid);

#ifdef __cplusplus
}
#endif

#endif