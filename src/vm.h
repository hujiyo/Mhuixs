/**
 * Logex 虚拟机
 * 执行 .ls 字节码文件
 */

#ifndef VM_H
#define VM_H

#include "bytecode.h"
#include "bignum.h"
#include "context.h"
#include "function.h"
#include "package.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VM_STACK_SIZE 1024

/* 虚拟机状态 */
typedef enum {
    VM_OK,
    VM_ERROR,
    VM_HALT,
} VMStatus;

/* 虚拟机 */
typedef struct {
    BytecodeProgram *program;     /* 字节码程序 */
    
    /* 执行状态 */
    uint32_t pc;                  /* 程序计数器 */
    VMStatus status;              /* 虚拟机状态 */
    
    /* 栈 */
    BHS stack[VM_STACK_SIZE];  /* 操作数栈 */
    int sp;                       /* 栈指针 */
    
    /* 运行时环境 */
    Context *context;             /* 变量上下文 */
    FunctionRegistry *func_registry;  /* 函数注册表 */
    PackageManager *pkg_manager;  /* 包管理器 */
    
    /* 错误信息 */
    char error_msg[256];
    
} VM;

/* 虚拟机初始化和清理 */
VM* vm_create(void);
void vm_destroy(VM *vm);

/* 加载字节码程序 */
int vm_load(VM *vm, BytecodeProgram *program);
int vm_load_file(VM *vm, const char *filename);

/* 执行 */
int vm_run(VM *vm);
int vm_step(VM *vm);  /* 单步执行 */

/* 获取结果 */
BHS* vm_get_result(VM *vm);
const char* vm_get_error(VM *vm);

#ifdef __cplusplus
}
#endif

#endif /* VM_H */
