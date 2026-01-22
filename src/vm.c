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
        
        case OP_STORE_STATIC: {
            /* 存储持久化变量（注册到 Mhuixs） */
            Constant *c = &vm->program->const_pool[inst->operand.u32];
            BHS *value = vm_peek(vm);
            if (value) {
                /* TODO: 调用 C 接口注册到 Mhuixs */
                /* hook_set_bhs(current_hook, caller_uid, value); */
                /* 暂时也存储到本地上下文 */
                context_set(vm->context, c->value.str, value);
            }
            break;
        }
        
        case OP_LOAD_STATIC: {
            /* 加载持久化变量 */
            Constant *c = &vm->program->const_pool[inst->operand.u32];
            /* TODO: 调用 C 接口从 Mhuixs 获取 */
            /* BHS *value = hook_get_bhs(current_hook, caller_uid); */
            BHS *value = context_get(vm->context, c->value.str);
            if (value) {
                vm_push(vm, value);
            } else {
                vm_error(vm, "Undefined static variable");
            }
            break;
        }
        
        case OP_DB_HOOK: {
            /* HOOK 操作 */
            uint32_t subop = inst->operand.ref.idx;
            uint32_t arg_count = inst->operand.ref.len;
            
            /* 弹出参数 */
            BHS *obj_name = vm_pop(vm);
            BHS *obj_type = (arg_count > 1) ? vm_pop(vm) : NULL;
            
            /* TODO: 调用 C 接口执行 HOOK 操作 */
            /* 根据 subop 执行不同操作 */
            switch (subop) {
                case DB_HOOK_CREATE:
                    /* reg_register(caller_uid, name, &hook); */
                    break;
                case DB_HOOK_SWITCH:
                    /* 切换当前 HOOK */
                    break;
                case DB_HOOK_DEL:
                    /* reg_unregister(name); */
                    break;
                case DB_HOOK_CLEAR:
                    /* 清空 HOOK 内容 */
                    break;
            }
            break;
        }
        
        case OP_DB_TABLE: {
            /* TABLE 操作 */
            uint32_t subop = inst->operand.ref.idx;
            uint32_t arg_count = inst->operand.ref.len;
            
            switch (subop) {
                case DB_TABLE_ADD: {
                    /* 弹出所有值字符串 */
                    /* TODO: 解析字符串为 BHS，调用 add_record */
                    for (uint32_t i = 0; i < arg_count; i++) {
                        vm_pop(vm);
                    }
                    break;
                }
                case DB_TABLE_GET: {
                    BHS *index = vm_pop(vm);
                    /* TODO: 调用 get_record */
                    (void)index;
                    break;
                }
                case DB_TABLE_SET: {
                    BHS *value = vm_pop(vm);
                    BHS *col = vm_pop(vm);
                    BHS *row = vm_pop(vm);
                    /* TODO: 调用 set_record */
                    (void)value; (void)col; (void)row;
                    break;
                }
                case DB_TABLE_DEL: {
                    BHS *index = vm_pop(vm);
                    /* TODO: 调用 del_record */
                    (void)index;
                    break;
                }
                case DB_TABLE_WHERE: {
                    BHS *condition = vm_pop(vm);
                    /* TODO: 解析条件，执行查询 */
                    (void)condition;
                    break;
                }
                case DB_FIELD_ADD: {
                    /* 弹出字段信息 */
                    for (uint32_t i = 0; i < arg_count; i++) {
                        vm_pop(vm);
                    }
                    /* TODO: 调用 add_field */
                    break;
                }
                case DB_FIELD_DEL: {
                    BHS *index = vm_pop(vm);
                    /* TODO: 调用 del_field */
                    (void)index;
                    break;
                }
                case DB_FIELD_SWAP: {
                    BHS *idx2 = vm_pop(vm);
                    BHS *idx1 = vm_pop(vm);
                    /* TODO: 调用 swap_field */
                    (void)idx1; (void)idx2;
                    break;
                }
            }
            break;
        }
        
        case OP_DB_KVALOT: {
            /* KVALOT 操作 */
            uint32_t subop = inst->operand.ref.idx;
            
            switch (subop) {
                case DB_KVALOT_SET: {
                    BHS *value = vm_pop(vm);
                    BHS *key = vm_pop(vm);
                    /* TODO: 调用 kvalot_set */
                    (void)key; (void)value;
                    break;
                }
                case DB_KVALOT_GET: {
                    BHS *key = vm_pop(vm);
                    /* TODO: 调用 kvalot_get，结果压栈 */
                    (void)key;
                    break;
                }
                case DB_KVALOT_DEL: {
                    BHS *key = vm_pop(vm);
                    /* TODO: 调用 kvalot_del */
                    (void)key;
                    break;
                }
                case DB_KVALOT_EXISTS: {
                    BHS *key = vm_pop(vm);
                    /* TODO: 调用 kvalot_exists，结果压栈 */
                    (void)key;
                    break;
                }
            }
            break;
        }
        
        case OP_DB_LIST: {
            /* LIST 操作 */
            uint32_t subop = inst->operand.ref.idx;
            
            switch (subop) {
                case DB_LIST_LPUSH: {
                    BHS *value = vm_pop(vm);
                    /* TODO: 调用 list_lpush */
                    (void)value;
                    break;
                }
                case DB_LIST_RPUSH: {
                    BHS *value = vm_pop(vm);
                    /* TODO: 调用 list_rpush */
                    (void)value;
                    break;
                }
                case DB_LIST_LPOP: {
                    /* TODO: 调用 list_lpop，结果压栈 */
                    break;
                }
                case DB_LIST_RPOP: {
                    /* TODO: 调用 list_rpop，结果压栈 */
                    break;
                }
                case DB_LIST_GET: {
                    BHS *index = vm_pop(vm);
                    /* TODO: 调用 list_get，结果压栈 */
                    (void)index;
                    break;
                }
            }
            break;
        }
        
        case OP_DB_BITMAP: {
            /* BITMAP 操作 */
            uint32_t subop = inst->operand.ref.idx;
            
            switch (subop) {
                case DB_BITMAP_SET: {
                    BHS *value = vm_pop(vm);
                    BHS *offset = vm_pop(vm);
                    /* TODO: 调用 bitmap_set */
                    (void)offset; (void)value;
                    break;
                }
                case DB_BITMAP_GET: {
                    BHS *offset = vm_pop(vm);
                    /* TODO: 调用 bitmap_get，结果压栈 */
                    (void)offset;
                    break;
                }
                case DB_BITMAP_COUNT: {
                    /* TODO: 调用 bitmap_count，结果压栈 */
                    break;
                }
                case DB_BITMAP_FLIP: {
                    BHS *offset = vm_pop(vm);
                    /* TODO: 调用 bitmap_flip */
                    (void)offset;
                    break;
                }
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
