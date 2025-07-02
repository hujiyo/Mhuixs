#include "log.hpp"

FILE* logFile;

mrc logger_init(){
    string tmp = Env.MhuixsHomePath+(string)"log.inf";
    logFile = fopen(tmp.c_str(),"a");//以追加模式打开日志文件
    if (!logFile) {
       return merr_open_file;
    }
    return success;
}

void log(char* message){
    if (logFile) {
        time_t now = time(NULL);
        struct tm* t = localtime(&now);
        fprintf(logFile, "[LOG] [%04d-%02d-%02d %02d:%02d:%02d]: %s\n",
                t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                t->tm_hour, t->tm_min, t->tm_sec, message);
    }
}