#include "merr.h"

static FILE* logFile = NULL;//日志模块是否开启的标志

void logger(char* message){
    #ifdef open_log
    if (logFile && message) { //自动检查log是否开启
        time_t now = time(NULL);
        struct tm* t = localtime(&now);
        fprintf(logFile, "[LOG] [%04d-%02d-%02d %02d:%02d:%02d]: %s\n",
                t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                t->tm_hour, t->tm_min, t->tm_sec, message);
    }
    #endif
}

void report(enum mrc code, enum where wh, char *special_message){
    //先打印问题位置
    switch(wh){
        case pkg_module:printf("[pkg]:");break;
        case hook_module:printf("[hook]:");break;
        case bitmap_module:printf("[bitmap]:");break;
        case table_module:printf("[table]:");break;
        case kvalot_module:printf("[kvalot]:");break;
        case list_module:printf("[list]:");break;
        case log_module:printf("[log]:");break;
        case mshare_module:printf("[mshare]:");break;
        case key_module:printf("[key]:");break;
        case usergroup_module:printf("[usergroup]:");break;
        case session_module:printf("[session]:");break;
        case register_module:printf("[register]:");break;
        case lexer_module:printf("[lexer]:");break;
        default:printf("[unknown]:");break;
    }
    //再打印标准错误码对应的报错
    char* message =NULL;
    int iflog = 0;//是否记录错误,[Error]需要写入日志,[Hint]警告无需写入日志

    switch (code){
        case merr:message = "[Error]:Random error",iflog=1;break;
        case merr_open_file:message="[Error]:Failed to open file",iflog=1;break;
        case register_failed:message="[Error]:Failed to register hook",iflog=1;break;
        case hook_already_registered:message="[Hint]:Hook already registered";break;
        case null_hook:message="[Error]:Hook is null\n",iflog=1;break;
        case permission_denied:message="[Error]:Permission denied";break;
        default:message="[Error]:Unknown error",iflog=1;break;
    }
    printf("%s",message);
    if(iflog) logger(message);

    if(special_message){
        logger(special_message);
        printf("|%s",special_message);
    }
    printf("\n");
}


int logger_init(const char *path){
    if (!path || strlen(path) >= 1024) {
        fprintf(stderr, "Invalid log path\n");
        return -1;
    }
    int len = strlen(path);
    
    char* log_path= (char*)calloc(1,len+strlen("log.inf"));
    if(!log_path)printf("[log][Error]:calloc err!\n");
    memcpy(log_path,path,len);
    memcpy(log_path+len,"log.inf",strlen("log.inf"));

    logFile = fopen(log_path,"a");//以追加模式打开日志文件
    if (!logFile) {
        printf("Failed to open log file\n");
        return -1;
    }
    free(log_path);
    return 0;
}


void logger_close(void) {
    if (logFile) { // 写入日志关闭信息
        time_t now = time(NULL);
        struct tm* t = localtime(&now);
        fprintf(logFile, "[INFO] Logger closed at %04d-%02d-%02d %02d:%02d:%02d\n",
                t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                t->tm_hour, t->tm_min, t->tm_sec);
                
        fclose(logFile);
        logFile = NULL;
    }
}