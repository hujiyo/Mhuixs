#ifndef LOGO_H
#define LOGO_H

#include <stdio.h>

/*
logo.h —— 启动横幅

写哪个流由调用方决定：交互模式走 stdout，服务端走 stderr
（服务进程的 stdout 常被重定向到文件，横幅不该混进去）。
脚本模式不打 —— 它的 stdout 要留给命令输出。
*/
void print_logo(FILE *out);

#endif /* LOGO_H */
