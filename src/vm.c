/**
 * Logex 虚拟机实现
 */

#include "vm.h"
#include "builtin.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* 创建虚拟机 */
VM* vm_create(void) {
    VM *vm = (VM*)calloc(1, sizeof(VM));
    if (!vm) return NULL;
    
    vm->program = NULL;
    vm->pc = 0;
    vm->sp = 0;
    vm->status = VM_OK;
    
    /* 初始化栈 */
    for (int i = 0; i < VM_STACK_SIZE; i++) {
        bignum_init(&vm->stack[i]);
    }
    
    /* 创建运行时环境 */
    vm->context = (Context*)malloc(sizeof(Context));
    context_init(vm->context);
    
    vm->func_registry = (FunctionRegistry*)malloc(sizeof(FunctionRegistry));
    function_registry_init(vm->func_registry);
    
    vm->pkg_manager = (PackageManager*)malloc(sizeof(PackageManager));
    package_manager_init(vm->pkg_manager, "./package");
    
    vm->error_msg[0] = '\0';
    
    return vm;
}

/* 销毁虚拟机 */
void vm_destroy(VM *vm) {
    if (!vm) return;
    
    /* 清理栈 */
    for (int i = 0; i < VM_STACK_SIZE; i++) {
        bignum_free(&vm->stack[i]);
    }
    
    /* 清理运行时环境 */
    if (vm->context) {
        context_clear(vm->context);
        free(vm->context);
    }
    
    if (vm->pkg_manager) {
        package_manager_cleanup(vm->pkg_manager);
        free(vm->pkg_manager);
    }
    
    free(vm->func_registry);
    
    free(vm);
}

/* 设置错误 */
static void vm_error(VM *vm, const char *msg) {
    vm->status = VM_ERROR;
    snprintf(vm->error_msg, sizeof(vm->error_msg), "%s", msg);
}

/* 栈操作 */
static void vm_push(VM *vm, BHS *value) {
    if (vm->sp >= VM_STACK_SIZE) {
        vm_error(vm, "Stack overflow");
        return;
    }
    bignum_copy(value, &vm->stack[vm->sp++]);
}

static BHS* vm_pop(VM *vm) {
    if (vm->sp <= 0) {
        vm_error(vm, "Stack underflow");
        return NULL;
    }
    return &vm->stack[--vm->sp];
}

static BHS* vm_peek(VM *vm) {
    if (vm->sp <= 0) {
        vm_error(vm, "Stack empty");
        return NULL;
    }
    return &vm->stack[vm->sp - 1];
}

/* 加载字节码程序 */
int vm_load(VM *vm, BytecodeProgram *program) {
    if (!vm || !program) return -1;
    
    vm->program = program;
    vm->pc = program->header.entry_point;
    vm->sp = 0;
    vm->status = VM_OK;
    
    return 0;
}

/* 从文件加载字节码 */
int vm_load_file(VM *vm, const char *filename) {
    if (!vm || !filename) return -1;
    
    BytecodeProgram *prog = bytecode_load(filename);
    if (!prog) {
        vm_error(vm, "Failed to load bytecode file");
        return -1;
    }
    
    return vm_load(vm, prog);
}

/* 执行单步 */
int vm_step(VM *vm) {
    if (!vm || !vm->program || vm->status != VM_OK) {
        return -1;
    }
    
    if (vm->pc >= vm->program->header.code_size) {
        vm->status = VM_HALT;
        return 0;
    }
    
    Instruction *inst = &vm->program->code[vm->pc++];
    
    switch (inst->opcode) {
        case OP_NOP:
            break;
            
        case OP_PUSH_NUM: {
            /* 从常量池加载数字 */
            Constant *c = &vm->program->const_pool[inst->operand.u32];
            BHS num;
            bignum_init(&num);
            BHS *parsed = bignum_from_string(c->value.num.digits);
            if (parsed) {
                bignum_copy(parsed, &num);
                bignum_destroy(parsed);
            }
            vm_push(vm, &num);
            bignum_free(&num);
            break;
        }
        
        case OP_PUSH_STR: {
            /* 从常量池加载字符串 */
            Constant *c = &vm->program->const_pool[inst->operand.u32];
            BHS str;
            bignum_init(&str);
            BHS *parsed = bignum_from_raw_string(c->value.str);
            if (parsed) {
                bignum_copy(parsed, &str);
                bignum_destroy(parsed);
            }
            vm_push(vm, &str);
            bignum_free(&str);
            break;
        }
        
        case OP_POP: {
            vm_pop(vm);
            break;
        }
        
        case OP_ADD: {
            BHS *b = vm_pop(vm);
            BHS *a = vm_pop(vm);
            if (a && b) {
                BHS result;
                bignum_init(&result);
                bignum_add(a, b, &result, 10);
                vm_push(vm, &result);
                bignum_free(&result);
            }
            break;
        }
        
        case OP_SUB: {
            BHS *b = vm_pop(vm);
            BHS *a = vm_pop(vm);
            if (a && b) {
                BHS result;
                bignum_init(&result);
                bignum_subtract(a, b, &result, 10);
                vm_push(vm, &result);
                bignum_free(&result);
            }
            break;
        }
        
        case OP_MUL: {
            BHS *b = vm_pop(vm);
            BHS *a = vm_pop(vm);
            if (a && b) {
                BHS result;
                bignum_init(&result);
                bignum_multiply(a, b, &result, 10);
                vm_push(vm, &result);
                bignum_free(&result);
            }
            break;
        }
        
        case OP_DIV: {
            BHS *b = vm_pop(vm);
            BHS *a = vm_pop(vm);
            if (a && b) {
                BHS result;
                bignum_init(&result);
                if (bignum_divide(a, b, &result, 10) != 0) {
                    vm_error(vm, "Division by zero");
                }
                vm_push(vm, &result);
                bignum_free(&result);
            }
            break;
        }
        
        case OP_STORE_VAR: {
            /* 存储变量 */
            Constant *c = &vm->program->const_pool[inst->operand.u32];
            BHS *value = vm_peek(vm);
            if (value) {
                context_set(vm->context, c->value.str, value);
            }
            break;
        }
        
        case OP_LOAD_VAR: {
            /* 加载变量 */
            Constant *c = &vm->program->const_pool[inst->operand.u32];
            BHS *value = context_get(vm->context, c->value.str);
            if (value) {
                vm_push(vm, value);
            } else {
                vm_error(vm, "Undefined variable");
            }
            break;
        }
        
        case OP_JMP: {
            vm->pc = inst->operand.u32;
            break;
        }
        
        case OP_JMP_IF_FALSE: {
            BHS *cond = vm_pop(vm);
            if (cond && !bignum_is_true(cond)) {
                vm->pc = inst->operand.u32;
            }
            break;
        }
        
        case OP_CALL_LIST: {
            BHS result;
            bignum_init(&result);
            const BuiltinFunctionInfo *func = builtin_lookup("list");
            if (func) {
                builtin_call(func, NULL, 0, &result, 10);
                vm_push(vm, &result);
            }
            bignum_free(&result);
            break;
        }
        
        case OP_CALL_LPUSH: {
            BHS *value = vm_pop(vm);
            BHS *list = vm_pop(vm);
            if (list && value) {
                BHS args[2];
                bignum_init(&args[0]);
                bignum_init(&args[1]);
                bignum_copy(list, &args[0]);
                bignum_copy(value, &args[1]);
                
                BHS result;
                bignum_init(&result);
                const BuiltinFunctionInfo *func = builtin_lookup("lpush");
                if (func) {
                    builtin_call(func, args, 2, &result, 10);
                    vm_push(vm, &result);
                }
                bignum_free(&result);
                bignum_free(&args[0]);
                bignum_free(&args[1]);
            }
            break;
        }
        
        case OP_CALL_RPUSH: {
            BHS *value = vm_pop(vm);
            BHS *list = vm_pop(vm);
            if (list && value) {
                BHS args[2];
                bignum_init(&args[0]);
                bignum_init(&args[1]);
                bignum_copy(list, &args[0]);
                bignum_copy(value, &args[1]);
                
                BHS result;
                bignum_init(&result);
                const BuiltinFunctionInfo *func = builtin_lookup("rpush");
                if (func) {
                    builtin_call(func, args, 2, &result, 10);
                    vm_push(vm, &result);
                }
                bignum_free(&result);
                bignum_free(&args[0]);
                bignum_free(&args[1]);
            }
            break;
        }
        
        case OP_CALL_LLEN: {
            BHS *list = vm_pop(vm);
            if (list) {
                BHS result;
                bignum_init(&result);
                const BuiltinFunctionInfo *func = builtin_lookup("llen");
                if (func) {
                    builtin_call(func, list, 1, &result, 10);
                    vm_push(vm, &result);
                }
                bignum_free(&result);
            }
            break;
        }
        
        case OP_CALL_NUM: {
            BHS *arg = vm_pop(vm);
            if (arg) {
                BHS result;
                bignum_init(&result);
                const BuiltinFunctionInfo *func = builtin_lookup("num");
                if (func) {
                    builtin_call(func, arg, 1, &result, 10);
                    vm_push(vm, &result);
                }
                bignum_free(&result);
            }
            break;
        }
        
        case OP_CALL_STR: {
            BHS *arg = vm_pop(vm);
            if (arg) {
                BHS result;
                bignum_init(&result);
                const BuiltinFunctionInfo *func = builtin_lookup("str");
                if (func) {
                    builtin_call(func, arg, 1, &result, 10);
                    vm_push(vm, &result);
                }
                bignum_free(&result);
            }
            break;
        }
        
        case OP_HALT:
            vm->status = VM_HALT;
            break;
            
        default:
            vm_error(vm, "Unknown opcode");
            return -1;
    }
    
    return 0;
}

/* 执行程序 */
int vm_run(VM *vm) {
    if (!vm) return -1;
    
    while (vm->status == VM_OK) {
        if (vm_step(vm) != 0) {
            return -1;
        }
    }
    
    return (vm->status == VM_HALT) ? 0 : -1;
}

/* 获取结果 */
BHS* vm_get_result(VM *vm) {
    if (!vm || vm->sp <= 0) return NULL;
    return &vm->stack[vm->sp - 1];
}

/* 获取错误信息 */
const char* vm_get_error(VM *vm) {
    return vm ? vm->error_msg : "Unknown error";
}
