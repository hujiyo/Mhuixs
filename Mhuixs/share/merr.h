/*
#版权所有 (c) HuJi 2025.6
#保留所有权利
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
Email:hj18914255909@outlook.com
*/
#ifndef MERR_H
#define MERR_H
/*
merr.h
功能
1.提供模块错误码
2.提供错误码上报日志的接口（report函数）
*/

#define open_log // 是否开启日志

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

//Mhuixs return codes
enum mrc {
    success = 0,//成功
    
    merr = -1,// 通用错误码！
    merr_open_file,// 文件打开错误！
    init_failed,//启动失败！
    register_failed,//注册失败！
    hook_already_registered,//hook已经注册,禁止重复注册！
    null_hook,//hook为空！

    permission_denied,//权限不足！

    
    //...
    //...
};
enum where{
    pkg_module,
    hook_module,
    bitmap_module,
    table_module,
    kvalot_module,
    list_module,
    log_module,
    mshare_module,
    key_module,
    usergroup_module,
    session_module,
    register_module,
    lexer_module,
};

void report(enum mrc code, enum where wh, char *special_message);//报告错误码


//下面函数用来开启report函数的日志写入功能
int logger_init(const char *path);//在path下建立日志文件
void logger_close(void);//关闭report函数的日志功能

#ifdef __cplusplus
}
#endif

#endif