#ifndef TOKEN_H
#define TOKEN_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
这个文件定义了NAQL语法的token类型
用于在词法分析器中识别和处理NAQL语句
*/  
typedef enum {
    //对象管理
    TOKEN_HOOK,         // 钩子
    TOKEN_KEY,          // 键

    //对象类型
    TOKEN_TABLE,        // 表类型
    TOKEN_KVALOT,       // 键库类型
    TOKEN_LIST,         // 列表类型
    TOKEN_BITMAP,       // 位图类型
    TOKEN_STREAM,       // 流类型

    //操作关键字
    TOKEN_DEL,          // 删除
    TOKEN_TYPE,         // 类型
    TOKEN_RANK,         // 权限等级
    TOKEN_CLEAR,        // 清空
    TOKEN_DESC,         // 描述
    TOKEN_GET,          // 获取
    TOKEN_WHERE,        // 条件
    TOKEN_TEMP,         // 临时
    TOKEN_SET,          // 设置
    TOKEN_EXISTS,       // 存在
    TOKEN_SELECT,       // 选择
    
    TOKEN_INSERT,       // 插入
    TOKEN_FIELD,        // 字段
    TOKEN_ADD,          // 添加
    TOKEN_SWAP,         // 交换
    TOKEN_ATTRIBUTE,    // 属性
    TOKEN_POS,          // 位置
    TOKEN_APPEND,       // 追加
    TOKEN_LEN,          // 长度
    TOKEN_LPUSH,        // 左压入
    TOKEN_RPUSH,        // 右压入
    TOKEN_LPOP,         // 左弹出
    TOKEN_RPOP,         // 右弹出
    TOKEN_COUNT,        // 计数
    TOKEN_COPY,         // 复制
    TOKEN_MOVE,         // 移动
    TOKEN_MERGE,        // 合并
    TOKEN_SPLIT,        // 分割
    TOKEN_EXPORT,       // 导出
    TOKEN_IMPORT,       // 导入
    TOKEN_SUBSCRIBE,    // 订阅
    TOKEN_UNSUBSCRIBE,  // 取消订阅
    TOKEN_NOTIFY,       // 通知
    TOKEN_LOCK,         // 锁定
    TOKEN_UNLOCK,       // 解锁
    TOKEN_COMMIT,       // 提交
    TOKEN_ROLLBACK,     // 回滚
    TOKEN_ASYNC,        // 异步
    TOKEN_SYNC,         // 同步
    TOKEN_WAIT,         // 等待
    TOKEN_RENAME,       // 重命名
    TOKEN_CHMOD,        // 权限
    TOKEN_MULTI,        // 开始事务
    TOKEN_EXEC,         // 执行事务
    TOKEN_EXPIRE,       // 过期
    TOKEN_PERSIST,      // 持久化
    TOKEN_TTL,          // 生存时间
    TOKEN_INCR,         // 递增
    TOKEN_DECR,         // 递减
    TOKEN_KEYS,         // 键集合
    TOKEN_FLIP,         // 翻转
    TOKEN_FILL,         // 填充
    TOKEN_FIND,         // 查找
    TOKEN_AND,          // 与操作
    TOKEN_OR,           // 或操作
    TOKEN_XOR,          // 异或操作
    TOKEN_NOT,          // 非操作
    TOKEN_SHIFT,        // 位移
    TOKEN_RESIZE,       // 调整大小
    TOKEN_JOIN,         // 连接
    TOKEN_REVERSE,      // 反转
    TOKEN_SORT,         // 排序
    TOKEN_UNIQUE,       // 去重
    TOKEN_SYSTEM,       // 系统
    TOKEN_INFO,         // 信息
    TOKEN_STATUS,       // 状态
    TOKEN_REGISTER,     // 注册表
    TOKEN_CLEANUP,      // 清理
    TOKEN_BACKUP,       // 备份
    TOKEN_RESTORE,      // 恢复
    TOKEN_LOG,          // 日志
    TOKEN_ZS,           // 压缩

    //类型关键字
    TOKEN_i1,           // int8_t
    TOKEN_i2,           // int16_t
    TOKEN_i4,           // int32_t/int
    TOKEN_i8,           // int64_t
    TOKEN_ui1,          // uint8_t
    TOKEN_ui2,          // uint16_t
    TOKEN_ui4,          // uint32_t
    TOKEN_ui8,          // uint64_t
    TOKEN_f4,           // float
    TOKEN_f8,           // double
    TOKEN_str,          // string
    TOKEN_date,         // 日期
    TOKEN_time,         // 时间
    TOKEN_datetime,     // 日期时间
    TOKEN_bool,         // 布尔
    TOKEN_blob,         // 二进制大对象
    TOKEN_json,         // JSON类型

    //键关键字
    TOKEN_PKEY,         // 主键
    TOKEN_FKEY,         // 外键
    TOKEN_NOTNULL,      // 非空
    TOKEN_DEFAULT,      // 默认值
    TOKEN_INDEX,        // 索引
    TOKEN_AUTO_INCREMENT, // 自动递增

    //条件关键字
    TOKEN_IF,           // 如果
    TOKEN_ELSE,         // 否则
    TOKEN_ELIF,         // 否则如果
    TOKEN_ENDIF,        // 结束如果
    TOKEN_WHILE,        // 循环
    TOKEN_ENDWHILE,     // 结束循环
    TOKEN_FOR,          // for循环
    TOKEN_ENDFOR,       // 结束for循环
    TOKEN_END,          // 结束（通用）
    TOKEN_BREAK,        // 跳出
    TOKEN_CONTINUE,     // 继续

    //逻辑关键字
    TOKEN_AND_LOGIC,    // 逻辑与
    TOKEN_OR_LOGIC,     // 逻辑或
    TOKEN_NOT_LOGIC,    // 逻辑非
    TOKEN_IN,           // 在...中
    TOKEN_BETWEEN,      // 在...之间
    TOKEN_LIKE,         // 模糊匹配
    TOKEN_IS,           // 是
    TOKEN_NULL,         // 空值

    //其他关键字
    TOKEN_ASC,          // 升序
    TOKEN_DESC_ORDER,   // 降序
    TOKEN_LEFT,         // 左
    TOKEN_RIGHT,        // 右
    TOKEN_MATHS,        // 数学运算
    TOKEN_SIZE,         // 大小
    TOKEN_CREATE,       // 创建
    TOKEN_WARN,         // 警告
    TOKEN_ERROR,        // 错误
    TOKEN_JSON_FORMAT,  // JSON格式
    TOKEN_CSV,          // CSV格式
    TOKEN_BINARY,       // 二进制格式
    TOKEN_PATTERN,      // 模式
    TOKEN_MICROSECONDS, // 微秒
    TOKEN_DIRECTION,    // 方向
    TOKEN_FORMAT,       // 格式
    TOKEN_DATA,         // 数据
    TOKEN_PATH,         // 路径
    TOKEN_LEVEL,        // 级别
    TOKEN_TIMEOUT,      // 超时
    TOKEN_MODE,         // 模式
    TOKEN_STEP,         // 步长
    TOKEN_START,        // 开始
    TOKEN_OFFSET,       // 偏移
    TOKEN_VALUE,        // 值
    TOKEN_CONDITION,    // 条件
    TOKEN_VAR,          // 变量

    //宏替换
    TOKEN_MACRO,        // 宏 ($)

    // 基本类型
    TOKEN_NAME,         // 名称
    TOKEN_VALUES,       // 数据值
    TOKEN_NUM,          // 数字

    // 特殊token
    TOKEN_END_STMT,     // 语句结束符 (;)
    TOKEN_EEROR,        // token错误
    TOKEN_UNKNOWN,      // 未识别

    // 符号
    TOKEN_SYMBOL,       // 符号
    TOKEN_DY,           // 大于号 >
    TOKEN_XY,           // 小于号 <
    TOKEN_DYDY,         // 大于等于号 >=
    TOKEN_XYDY,         // 小于等于号 <=
    TOKEN_DDY,          // 等于号 ==
    TOKEN_BDY,          // 不等于号 !=
    TOKEN_YDY,          // 模糊匹配 ~=
    TOKEN_BJ,           // 并集 <>
    TOKEN_JJ,           // 交集 ><
} TokType,toktype;

typedef struct {
    const char *keyword;//关键字字面值
    toktype type;//关键字类型
} keyword,kw;

// 定义关键字数组大小
#define KEYWORD_MAP_SIZE 172

extern const keyword keyword_map[];

#ifdef __cplusplus
}
#endif

#endif