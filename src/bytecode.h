/**
 * Logex 字节码定义
 * .lgx 源文件编译为 .ls 字节码文件
 */

#ifndef BYTECODE_H
#define BYTECODE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 字节码文件魔数 */
#define LOGEX_MAGIC 0x4C534758  /* "LSGX" */
#define LOGEX_VERSION 1

/* 字节码操作码 */
typedef enum {
    /* 栈操作 (0-19) */
    OP_NOP = 0,           /* 空操作 */
    OP_PUSH_NUM,          /* 压入数字常量 */
    OP_PUSH_STR,          /* 压入字符串常量 */
    OP_PUSH_VAR,          /* 压入变量值 */
    OP_POP,               /* 弹出栈顶 */
    OP_DUP,               /* 复制栈顶 */
    OP_SWAP,              /* 交换栈顶两个元素 */
    
    /* 算术运算 (20-39) */
    OP_ADD,               /* 加法 */
    OP_SUB,               /* 减法 */
    OP_MUL,               /* 乘法 */
    OP_DIV,               /* 除法 */
    OP_MOD,               /* 取模 */
    OP_POW,               /* 幂运算 */
    OP_NEG,               /* 取负 */
    
    /* 比较运算 (40-49) */
    OP_EQ,                /* == */
    OP_NE,                /* != */
    OP_LT,                /* < */
    OP_LE,                /* <= */
    OP_GT,                /* > */
    OP_GE,                /* >= */
    
    /* 逻辑运算 (50-59) */
    OP_AND,               /* 逻辑与 ^ */
    OP_OR,                /* 逻辑或 v */
    OP_NOT,               /* 逻辑非 ! */
    OP_XOR,               /* 异或 ⊽ */
    OP_IMPLIES,           /* 蕴含 → */
    OP_IFF,               /* 等价 ↔ */
    
    /* 变量操作 (60-69) */
    OP_STORE_VAR,         /* 存储变量 */
    OP_LOAD_VAR,          /* 加载变量 */
    OP_DEL_VAR,           /* 删除变量 */
    
    /* 控制流 (70-89) */
    OP_JMP,               /* 无条件跳转 */
    OP_JMP_IF_FALSE,      /* 条件跳转（假） */
    OP_JMP_IF_TRUE,       /* 条件跳转（真） */
    OP_CALL,              /* 函数调用 */
    OP_RETURN,            /* 返回 */
    OP_LOOP_START,        /* 循环开始标记 */
    OP_LOOP_END,          /* 循环结束标记 */
    OP_BREAK,             /* 跳出循环 */
    OP_CONTINUE,          /* 继续循环 */
    
    /* 内置函数调用 (90-109) */
    OP_CALL_BUILTIN,      /* 调用内置函数 */
    OP_CALL_LIST,         /* list() */
    OP_CALL_LPUSH,        /* lpush() */
    OP_CALL_RPUSH,        /* rpush() */
    OP_CALL_LPOP,         /* lpop() */
    OP_CALL_RPOP,         /* rpop() */
    OP_CALL_LGET,         /* lget() */
    OP_CALL_LLEN,         /* llen() */
    OP_CALL_NUM,          /* num() */
    OP_CALL_STR,          /* str() */
    OP_CALL_BMP,          /* bmp() */
    OP_CALL_BSET,         /* bset() */
    OP_CALL_BGET,         /* bget() */
    OP_CALL_BCOUNT,       /* bcount() */
    
    /* 外部包函数 (110-119) */
    OP_IMPORT,            /* 导入包 */
    OP_CALL_PACKAGE,      /* 调用包函数 */
    
    /* 数据库操作 (120-199) - 对应 funseq.h */
    OP_DB_HOOK = 120,     /* HOOK 操作 */
    OP_DB_TABLE,          /* TABLE 操作 */
    OP_DB_KVALOT,         /* KVALOT 操作 */
    OP_DB_LIST,           /* LIST 操作 */
    OP_DB_BITMAP,         /* BITMAP 操作 */
    OP_DB_STREAM,         /* STREAM 操作 */
    
    /* 持久化变量操作 (125-129) */
    OP_STORE_STATIC = 125,/* 存储持久化变量（注册到 Mhuixs） */
    OP_LOAD_STATIC,       /* 加载持久化变量 */
    
    /* 调试和元数据 (200-209) */
    OP_LINE = 200,        /* 行号信息（调试用） */
    OP_HALT,              /* 停止执行 */
    
} OpCode;

/* NAQL 子操作码 - 用于 OP_DB_* 指令的第二字节 */
typedef enum {
    /* HOOK 子操作 */
    DB_HOOK_CREATE = 0,   /* 创建 HOOK */
    DB_HOOK_SWITCH,       /* 切换 HOOK */
    DB_HOOK_DEL,          /* 删除 HOOK */
    DB_HOOK_CLEAR,        /* 清空 HOOK */
    
    /* TABLE 子操作 */
    DB_TABLE_ADD = 10,    /* 添加记录 */
    DB_TABLE_GET,         /* 获取记录 */
    DB_TABLE_SET,         /* 设置记录 */
    DB_TABLE_DEL,         /* 删除记录 */
    DB_TABLE_WHERE,       /* 条件查询 */
    
    /* FIELD 子操作 */
    DB_FIELD_ADD = 20,    /* 添加字段 */
    DB_FIELD_DEL,         /* 删除字段 */
    DB_FIELD_SWAP,        /* 交换字段 */
    
    /* KVALOT 子操作 */
    DB_KVALOT_SET = 30,   /* 设置键值 */
    DB_KVALOT_GET,        /* 获取键值 */
    DB_KVALOT_DEL,        /* 删除键值 */
    DB_KVALOT_EXISTS,     /* 检查键是否存在 */
    
    /* LIST 子操作 */
    DB_LIST_LPUSH = 40,   /* 左侧压入 */
    DB_LIST_RPUSH,        /* 右侧压入 */
    DB_LIST_LPOP,         /* 左侧弹出 */
    DB_LIST_RPOP,         /* 右侧弹出 */
    DB_LIST_GET,          /* 获取元素 */
    
    /* BITMAP 子操作 */
    DB_BITMAP_SET = 50,   /* 设置位 */
    DB_BITMAP_GET,        /* 获取位 */
    DB_BITMAP_COUNT,      /* 统计位数 */
    DB_BITMAP_FLIP,       /* 翻转位 */
    
} DBSubOpCode;

/* 字节码指令结构 */
typedef struct {
    OpCode opcode;        /* 操作码 */
    union {
        int64_t i64;      /* 整数操作数 */
        double f64;       /* 浮点操作数 */
        uint32_t u32;     /* 无符号整数 */
        struct {
            uint32_t idx; /* 常量池索引 */
            uint32_t len; /* 长度 */
        } ref;
    } operand;
} Instruction;

/* 常量池条目类型 */
typedef enum {
    CONST_NUMBER,         /* 数字常量 */
    CONST_STRING,         /* 字符串常量 */
    CONST_IDENTIFIER,     /* 标识符 */
} ConstType;

/* 常量池条目 */
typedef struct {
    ConstType type;
    union {
        char *str;        /* 字符串/标识符 */
        struct {
            char *digits; /* 数字字符串 */
            int decimal_pos;
            int is_negative;
        } num;
    } value;
} Constant;

/* 字节码文件头 */
typedef struct {
    uint32_t magic;           /* 魔数 LSGX */
    uint32_t version;         /* 版本号 */
    uint32_t const_count;     /* 常量池大小 */
    uint32_t code_size;       /* 代码段大小 */
    uint32_t entry_point;     /* 入口点 */
    uint32_t flags;           /* 标志位 */
    char source_file[256];    /* 源文件名 */
} BytecodeHeader;

/* 字节码程序 */
typedef struct {
    BytecodeHeader header;    /* 文件头 */
    Constant *const_pool;     /* 常量池 */
    Instruction *code;        /* 代码段 */
} BytecodeProgram;

/* 字节码程序操作 */
BytecodeProgram* bytecode_create(void);
void bytecode_destroy(BytecodeProgram *prog);

/* 添加常量到常量池 */
uint32_t bytecode_add_const_number(BytecodeProgram *prog, const char *digits, int decimal_pos, int is_negative);
uint32_t bytecode_add_const_string(BytecodeProgram *prog, const char *str);
uint32_t bytecode_add_const_identifier(BytecodeProgram *prog, const char *id);

/* 添加指令 */
void bytecode_emit(BytecodeProgram *prog, OpCode opcode);
void bytecode_emit_i64(BytecodeProgram *prog, OpCode opcode, int64_t operand);
void bytecode_emit_u32(BytecodeProgram *prog, OpCode opcode, uint32_t operand);
void bytecode_emit_ref(BytecodeProgram *prog, OpCode opcode, uint32_t idx, uint32_t len);

/* 跳转指令辅助 */
uint32_t bytecode_current_pos(BytecodeProgram *prog);
void bytecode_patch_jump(BytecodeProgram *prog, uint32_t pos, uint32_t target);

/* 序列化/反序列化 */
int bytecode_save(BytecodeProgram *prog, const char *filename);
BytecodeProgram* bytecode_load(const char *filename);

/* 反汇编（调试用） */
void bytecode_disassemble(BytecodeProgram *prog);

#ifdef __cplusplus
}
#endif

#endif /* BYTECODE_H */
