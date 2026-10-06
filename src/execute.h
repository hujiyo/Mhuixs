#ifndef EXECUTE_H
#define EXECUTE_H

#include <stddef.h>

/*
execute.h —— Mhuixs 命令执行层接口

一条命令 = 一个字符串。有两个入口，语义完全相同，只有输出版式不同：

    mhx_execute()       文本（人类可读，也是交互模式的默认版式）
    mhx_execute_json()  JSON（程序可读）

两者都返回 0 表示成功、非 0 表示失败，并都往调用方的缓冲区里写结果。

--------------------------------------------------------------------------
JSON 契约（调用方只需依赖这几个字段；文案随时可能改，不要解析 text）

    {"ok":true,"code":"ok","kind":"int","value":4,"text":"OK 4"}

    ok      bool    与返回码一致。判断成功失败只看它。
    code    string  稳定 token，见下表。失败时据此分支。
    kind    string  成功且命令有返回值时出现：
                    "int" | "num" | "str" | "bool" | "null" | "list"
    value   任意    随 kind：num 是 JSON 数字字面量（保留任意精度），
                    list 是字符串数组（超过 32 项时附 "truncated":true）
    text    string  同一命令在文本模式下的输出（仅去掉末尾一个换行）。
                    它是所有命令都有的兜底：即使某个命令还没提供结构化
                    返回值，信息也不会丢，LLM 调用方可以直接读 text。

    code 取值：
      ok                成功
      bad_usage         参数个数/格式不对
      no_hook           钩子不存在
      no_object         钩子没有挂对象
      type_mismatch     钩子类型与命令不符
      out_of_range      下标/偏移/区间越界
      no_key            键不存在
      no_field          列不存在
      empty             容器为空（如对空 LIST 做 pop）
      name_taken        钩子名已被占用
      unknown_command   命令不认识
      failed            其他失败（底层返回非 0 / 内存不足）
      internal          调用方参数不合法

参数：
    line    一条命令，不能为 NULL
    out     输出缓冲区，不能为 NULL，至少 2 字节。
            **JSON 版建议 >= 128KB** —— 它包含 text，且转义后可能膨胀。
    outlen  缓冲区大小

返回：
    0 成功（此时 JSON 的 ok 为 true）；非 0 失败。
*/

int mhx_execute(const char *line, char *out, size_t outlen);
int mhx_execute_json(const char *line, char *out, size_t outlen);

/*
 * 命令列表与用法说明（静态字符串，不要 free）。
 * 命令清单的唯一出处在这里，交互层的 :help 直接打印它。
 */
const char *mhx_help_text(void);

#endif /* EXECUTE_H */
