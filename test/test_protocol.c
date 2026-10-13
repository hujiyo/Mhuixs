/* 回归测试：MHP/1 编解码。
 *
 * 这一层没有 socket 依赖，所以半包、粘包、二进制安全这些最容易写错的
 * 路径可以离线逐条钉死 —— 等接上网络再调就太贵了。
 *
 * 三条不变量：
 *   1. 任何**合法命令的真前缀**都必须是 INCOMPLETE，绝不能误判成完整命令
 *      （这就是"一条命令跨多次 recv"）
 *   2. 消费掉的字节数必须精确，剩下来的能接着解析（这就是"一次 recv 含多条"）
 *   3. $ 数据体内的一切字符都原样保留 —— 含 CRLF、空格、引号、井号
 */
#include <stdio.h>
#include <string.h>

#include "proto.h"

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("  FAIL: %s\n", msg); fails++; } } while (0)

int main(void)
{
    proto_cmd c;

    printf("单条完整命令\n");
    {
        const char *req = "*3\r\n$5\r\nrpush\r\n$4\r\ntodo\r\n$4\r\nmilk\r\n";
        proto_result r = proto_parse(req, strlen(req), &c);

        CHECK(r == PROTO_OK, "应当解析成功");
        CHECK(c.argc == 3, "三个参数");
        CHECK(c.consumed == strlen(req), "consumed 等于命令总长");
        CHECK(c.args[0].len == 5 && memcmp(c.args[0].p, "rpush", 5) == 0, "argv[0] = rpush");
        CHECK(c.args[1].len == 4 && memcmp(c.args[1].p, "todo", 4) == 0,  "argv[1] = todo");
        CHECK(c.args[2].len == 4 && memcmp(c.args[2].p, "milk", 4) == 0,  "argv[2] = milk");
        CHECK(c.args[0].quoted == 0, "$ 元素是自动判定（不加引号）");
    }

    printf("半包：任何真前缀都必须 INCOMPLETE\n");
    {
        const char *req = "*3\r\n$5\r\nrpush\r\n$4\r\ntodo\r\n$4\r\nmilk\r\n";
        size_t full = strlen(req);
        int bad = 0;

        /* 逐字节喂前缀：少一个字节都不能被当成完整命令 */
        for (size_t n = 0; n < full; n++) {
            proto_result r = proto_parse(req, n, &c);
            if (r != PROTO_INCOMPLETE) {
                printf("  FAIL: 前 %zu 字节返回了 %d，应当是 INCOMPLETE\n", n, (int)r);
                fails++; bad = 1; break;
            }
        }
        if (!bad) printf("  （0..%zu 全部正确判为 INCOMPLETE）\n", full - 1);
    }

    printf("粘包：consumed 精确，剩下的能接着解析\n");
    {
        const char *first  = "*2\r\n$4\r\nllen\r\n$4\r\ntodo\r\n";
        const char *second = "*1\r\n$5\r\nhooks\r\n";
        char both[128];
        snprintf(both, sizeof(both), "%s%s", first, second);
        size_t total = strlen(both);

        proto_result r = proto_parse(both, total, &c);
        CHECK(r == PROTO_OK && c.argc == 2, "第一条解析成功");
        CHECK(c.consumed == strlen(first), "consumed 只覆盖第一条");

        r = proto_parse(both + c.consumed, total - c.consumed, &c);
        CHECK(r == PROTO_OK && c.argc == 1, "剩余部分接着解析出第二条");
        CHECK(c.consumed == strlen(second), "第二条 consumed 正确");
    }

    printf("二进制安全：值里的任何字节都原样保留\n");
    {
        const char *body = "a\r\nb c \"d\" #e";   /* CRLF、空格、引号、井号 */
        char req[256];
        snprintf(req, sizeof(req), "*2\r\n$4\r\nllen\r\n$%zu\r\n%s\r\n", strlen(body), body);

        proto_result r = proto_parse(req, strlen(req), &c);
        CHECK(r == PROTO_OK, "含 CRLF 的值应当解析成功");
        CHECK(c.argc == 2, "两个参数");
        CHECK(c.args[1].len == strlen(body), "长度按字节算");
        CHECK(memcmp(c.args[1].p, body, strlen(body)) == 0, "内容逐字节相等");
    }

    printf("+ 元素承载引号语义\n");
    {
        const char *req = "*2\r\n$4\r\nllen\r\n+123\r\n";
        proto_result r = proto_parse(req, strlen(req), &c);

        CHECK(r == PROTO_OK && c.argc == 2, "解析成功");
        CHECK(c.args[0].quoted == 0, "$ 元素不强制字符串");
        CHECK(c.args[1].quoted == 1, "+ 元素强制字符串");
        CHECK(c.args[1].len == 3 && memcmp(c.args[1].p, "123", 3) == 0, "+ 元素的内容是 123");
    }

    printf("非法输入一律 PROTO_ERR\n");
    {
        struct { const char *req; const char *why; } bad[] = {
            { "*0\r\n",                      "参数个数为 0" },
            { "*-1\r\n",                     "参数个数为负" },
            { "*49\r\n",                     "参数个数超过 48" },
            { "*99999999999999999999\r\n",   "参数个数溢出" },
            { "x3\r\n",                      "首字节不是 *" },
            { "*3x\r\n",                     "数组长度后面不是 CRLF" },
            { "*1\r\n*1\r\n",                "不支持嵌套数组" },
            { "*1\r\nx\r\n",                 "元素类型不认识" },
            { "*1\r\n$256\r\n",              "参数长度超过上限" },
            { "*1\r\n$99999999999999999999\r\n", "参数长度溢出" },
        };
        for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
            proto_result r = proto_parse(bad[i].req, strlen(bad[i].req), &c);
            if (r != PROTO_ERR) {
                printf("  FAIL: [%s] 应当 PROTO_ERR，实际 %d\n", bad[i].why, (int)r);
                fails++;
            } else if (!c.err) {
                printf("  FAIL: [%s] 报了错却没给原因\n", bad[i].why);
                fails++;
            }
        }
    }

    printf("编码：回包\n");
    {
        char buf[256];

        CHECK(proto_encode_ok(buf, sizeof(buf)) == 5 && strcmp(buf, "+OK\r\n") == 0,
              "+OK 编码");

        CHECK(proto_encode_err(buf, sizeof(buf), "PROTO", "bad") > 0 &&
              strcmp(buf, "-PROTO bad\r\n") == 0, "错误行编码");

        /* 消息里的 CR/LF 必须被替换 —— 否则一条错误消息就能把分帧搞乱 */
        proto_encode_err(buf, sizeof(buf), "PROTO", "a\r\nb");
        CHECK(strcmp(buf, "-PROTO a  b\r\n") == 0, "错误消息里的 CR/LF 被换成空格");

        /* 载荷自己含转义与换行：{"a":"x\ny"} 加结尾换行 = 13 字节 */
        const char *json = "{\"a\":\"x\\ny\"}\n";
        size_t jlen = strlen(json);
        size_t n = proto_encode_bulk(buf, sizeof(buf), json, jlen);
        CHECK(n == proto_bulk_size(jlen), "bulk 实际长度与预估一致");
        CHECK(n > 0 && strcmp(buf + n - 2, "\r\n") == 0, "bulk 以 CRLF 收尾");
        CHECK(memcmp(buf + 1, "13\r\n", 4) == 0, "长度头是 13（按字节算）");

        /* 缓冲区不够时整条不写，返回 0 */
        CHECK(proto_encode_bulk(buf, 4, json, jlen) == 0, "缓冲区不够时 bulk 不写");
        CHECK(proto_encode_ok(buf, 3) == 0, "缓冲区不够时 +OK 不写");
    }

    if (fails) { printf("\n协议编解码测试：%d 项失败\n", fails); return 1; }
    printf("\n协议编解码测试：全部通过\n");
    return 0;
}
