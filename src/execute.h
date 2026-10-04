#ifndef EXECUTE_H
#define EXECUTE_H

#include <stddef.h>

/*
execute.h —— Mhuixs 命令执行层接口

一条命令 = 一个字符串，执行结果以文本形式写入 out。
成功返回 0；失败返回 0 以外的值，此时 out 中是以 "ERR" 开头的说明。
*/

int mhx_execute(const char *line, char *out, size_t outlen);

#endif /* EXECUTE_H */
