/*
#版权所有 (c) HuJi 2025.1
#保留所有权利
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
Email:hj18914255909@outlook.com
*/

#ifndef LEXER_H
#define LEXER_H

#include "stdstr.h"

// 主要lexer函数
// 将NAQL语句转换为HUJI协议格式的数据流
// 参数: string - 输入的NAQL语句字符串
//       len    - 字符串长度
// 返回: 包含HUJI协议数据的str结构，调用者需要释放
str lexer(char* string, int len);

// 本地变量管理函数
// 设置本地变量
// 参数: name  - 变量名
//       value - 变量值
// 返回: 0成功，-1失败
int set_local_variable(const char* name, const char* value);

// 获取本地变量值
// 参数: name - 变量名
// 返回: 变量值字符串，如果不存在返回NULL
const char* get_local_variable(const char* name);

// 删除本地变量
// 参数: name - 变量名
// 返回: 0成功，-1失败（变量不存在）
int delete_local_variable(const char* name);

// 清理本地资源
// 释放所有本地变量和控制状态
void cleanup_local_resources();

#endif // LEXER_H


