#include "log.h"

static FILE* logFile = NULL;
static char log_path[1024] = {0};

int logger_init(const char *path){
    if (!path || strlen(path) >= 1024) {
        fprintf(stderr, "Invalid log path\n");
        return -1;
    }
    int len = strlen(path);
    
    char* log_path= (char*)calloc(1,len+strlen("log.inf"));
    memcpy(log_path,path,len);
    memcpy(log_path+len,"log.inf",strlen("log.inf"))

    logFile = fopen(log_path,"a");//以追加模式打开日志文件
    if (!logFile) {
        printf("Failed to open log file\n");
        return -1;
    }
    free(log_path);
    return 0;
}

void log(char* message){
    #ifdef open_log
    if (logFile && message) {
        time_t now = time(NULL);
        struct tm* t = localtime(&now);
        fprintf(logFile, "[LOG] [%04d-%02d-%02d %02d:%02d:%02d]: %s\n",
                t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                t->tm_hour, t->tm_min, t->tm_sec, message);
    }
    #endif
}


void logger_close(void) {
    if (logFile) {
        // 写入日志关闭信息
        time_t now = time(NULL);
        struct tm* t = localtime(&now);
        fprintf(logFile, "[INFO] Logger closed at %04d-%02d-%02d %02d:%02d:%02d\n",
                t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                t->tm_hour, t->tm_min, t->tm_sec);
                
        fclose(logFile);
        logFile = NULL;
    }
}