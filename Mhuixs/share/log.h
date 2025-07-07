#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define open_log // 是否开启日志
int logger_init(const char *path);
void logger_close(void);

void log(char* message);


#ifdef __cplusplus
}
#endif

#endif // LOG_H