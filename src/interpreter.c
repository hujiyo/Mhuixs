#include "interpreter.h"
#include "lexer.h"
#include "evaluator.h"  // 临时使用旧的evaluator作为后端
#include "vm.h"
#include "compiler.h"
#include "builtin.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* 创建解释器实例 */
Interpreter* interpreter_create(void) {
    Interpreter *interp = malloc(sizeof(Interpreter));
    if (!interp) return NULL;
    
    /* 初始化错误处理 */
    error_init(&interp->error);
    
    /* 创建运行时上下文 */
    interp->context = malloc(sizeof(Context));
    if (!interp->context) {
        free(interp);
        return NULL;
    }
    context_init(interp->context);
    
    /* 创建函数注册表 */
    interp->funcs = malloc(sizeof(FunctionRegistry));
    if (!interp->funcs) {
        free(interp->context);
        free(interp);
        return NULL;
    }
    function_registry_init(interp->funcs);
    
    /* 创建包管理器 */
    interp->packages = malloc(sizeof(PackageManager));
    if (!interp->packages) {
        free(interp->funcs);
        free(interp->context);
        free(interp);
        return NULL;
    }
    package_manager_init(interp->packages, "./package");
    
    /* 设置默认精度 */
    interp->precision = -1;  // 使用默认精度
    
    /* 创建 VM 和编译器 */
    interp->vm = vm_create();
    if (!interp->vm) {
        package_manager_cleanup(interp->packages);
        free(interp->packages);
        free(interp->funcs);
        context_clear(interp->context);
        free(interp->context);
        free(interp);
        return NULL;
    }
    
    interp->compiler = compiler_create();
    if (!interp->compiler) {
        vm_destroy(interp->vm);
        package_manager_cleanup(interp->packages);
        free(interp->packages);
        free(interp->funcs);
        context_clear(interp->context);
        free(interp->context);
        free(interp);
        return NULL;
    }
    
    /* 默认使用 VM 模式 */
    interp->use_vm = 1;
    
    return interp;
}

/* 销毁解释器实例 */
void interpreter_destroy(Interpreter *interp) {
    if (!interp) return;
    
    if (interp->packages) {
        package_manager_cleanup(interp->packages);
        free(interp->packages);
    }
    
    if (interp->context) {
        context_clear(interp->context);
        free(interp->context);
    }
    
    if (interp->funcs) {
        free(interp->funcs);
    }
    
    if (interp->compiler) {
        compiler_destroy(interp->compiler);
    }
    
    if (interp->vm) {
        vm_destroy(interp->vm);
    }
    
    free(interp);
}

/* 将旧的错误码转换为新的错误类型 */
static void convert_legacy_error(Interpreter *interp, int legacy_result, const char *source, const char *filename) {
    if (legacy_result == 0) {
        error_clear(&interp->error);
        return;
    }
    
    ErrorType error_type;
    const char *message;
    
    switch (legacy_result) {
        case -1:  // EVAL_ERROR
            error_type = ERR_SYNTAX;
            message = "syntax error, undefined variable or function";
            break;
        case -2:  // EVAL_DIV_ZERO (假设)
            error_type = ERR_DIV_ZERO;
            message = "division by zero";
            break;
        default:
            error_type = ERR_RUNTIME;
            message = "runtime error";
            break;
    }
    
    error_set_simple(&interp->error, error_type, message);
    if (filename) {
        strncpy(interp->error.filename, filename, sizeof(interp->error.filename) - 1);
        interp->error.filename[sizeof(interp->error.filename) - 1] = '\0';
    }
}

/* 执行源代码 */
int interpreter_execute(Interpreter *interp, const char *source, const char *filename, InterpreterResult *result) {
    if (!interp || !source || !result) return -1;
    
    /* 清除之前的错误 */
    error_clear(&interp->error);
    
    /* 初始化结果 */
    result->type = RESULT_NONE;
    result->value[0] = '\0';
    result->message[0] = '\0';
    
    /* 根据模式选择执行方式 */
    if (interp->use_vm) {
        /* VM 模式：编译 -> 执行 */
        
        /* 1. 编译源代码为字节码 */
        Compiler *comp = compiler_create();
        if (!comp) {
            result->type = RESULT_ERROR;
            snprintf(result->value, sizeof(result->value), "Failed to create compiler");
            return -1;
        }
        
        if (compiler_compile_string(comp, source, filename ? filename : "<stdin>") != 0) {
            result->type = RESULT_ERROR;
            snprintf(result->value, sizeof(result->value), "Compile error: %s", compiler_get_error(comp));
            compiler_destroy(comp);
            return -1;
        }
        
        /* 2. 加载字节码到 VM */
        VM *vm = vm_create();
        if (!vm) {
            result->type = RESULT_ERROR;
            snprintf(result->value, sizeof(result->value), "Failed to create VM");
            compiler_destroy(comp);
            return -1;
        }
        
        /* 共享上下文和包管理器 */
        if (vm->context) {
            context_clear(vm->context);
            free(vm->context);
        }
        vm->context = interp->context;
        
        if (vm->pkg_manager) {
            package_manager_cleanup(vm->pkg_manager);
            free(vm->pkg_manager);
        }
        vm->pkg_manager = interp->packages;
        
        if (vm->func_registry) {
            free(vm->func_registry);
        }
        vm->func_registry = interp->funcs;
        
        if (vm_load(vm, comp->program) != 0) {
            result->type = RESULT_ERROR;
            snprintf(result->value, sizeof(result->value), "Failed to load bytecode");
            /* 恢复指针避免 double free */
            vm->context = NULL;
            vm->pkg_manager = NULL;
            vm->func_registry = NULL;
            vm_destroy(vm);
            compiler_destroy(comp);
            return -1;
        }
        
        /* 3. 执行字节码 */
        if (vm_run(vm) != 0) {
            result->type = RESULT_ERROR;
            snprintf(result->value, sizeof(result->value), "Runtime error: %s", vm_get_error(vm));
            /* 恢复指针避免 double free */
            vm->context = NULL;
            vm->pkg_manager = NULL;
            vm->func_registry = NULL;
            vm_destroy(vm);
            compiler_destroy(comp);
            return -1;
        }
        
        /* 4. 获取结果 */
        BigNum *vm_result = vm_get_result(vm);
        if (vm_result) {
            result->type = RESULT_VALUE;
            bignum_to_string(vm_result, result->value, sizeof(result->value), interp->precision);
        } else {
            result->type = RESULT_NONE;
        }
        
        /* 清理（不释放共享的资源） */
        vm->context = NULL;
        vm->pkg_manager = NULL;
        vm->func_registry = NULL;
        vm_destroy(vm);
        compiler_destroy(comp);
        
    } else {
        /* 直接解释模式（旧方式） */
        
        /* 使用增强词法分析器进行初步检查 */
        Lexer lexer;
        lexer_init(&lexer, source, filename, &interp->error);
        
        /* 检查词法错误 */
        TokenType first_token = lexer_next(&lexer);
        if (first_token == TOK_ERROR) {
            result->type = RESULT_ERROR;
            error_format(&interp->error, result->value, sizeof(result->value));
            return -1;
        }
        
        /* 临时使用旧的evaluator执行代码 */
        char temp_result[512];
        int legacy_result = eval_statement(source, temp_result, sizeof(temp_result), 
                                           interp->context, interp->funcs, interp->packages, 
                                           interp->precision);
        
        /* 转换结果 */
        if (legacy_result < 0) {
            convert_legacy_error(interp, legacy_result, source, filename);
            result->type = RESULT_ERROR;
            error_format(&interp->error, result->value, sizeof(result->value));
            return -1;
        }
        
        /* 根据返回值确定结果类型 */
        switch (legacy_result) {
            case 0:  // 表达式求值
                result->type = RESULT_VALUE;
                strncpy(result->value, temp_result, sizeof(result->value) - 1);
                result->value[sizeof(result->value) - 1] = '\0';
                break;
                
            case 1:  // 赋值语句
                result->type = RESULT_ASSIGNMENT;
                strncpy(result->value, temp_result, sizeof(result->value) - 1);
                result->value[sizeof(result->value) - 1] = '\0';
                break;
                
            case 2:  // 导入语句
                result->type = RESULT_IMPORT;
                strncpy(result->message, temp_result, sizeof(result->message) - 1);
                result->message[sizeof(result->message) - 1] = '\0';
                break;
                
            default:
                result->type = RESULT_NONE;
                break;
        }
    }
    
    return 0;
}

/* 执行文件 */
int interpreter_execute_file(Interpreter *interp, const char *filename, InterpreterResult *result) {
    if (!interp || !filename || !result) return -1;
    
    FILE *file = fopen(filename, "r");
    if (!file) {
        error_set_simple(&interp->error, ERR_IMPORT, "cannot open file");
        result->type = RESULT_ERROR;
        error_format(&interp->error, result->value, sizeof(result->value));
        return -1;
    }
    
    /* 读取文件内容 */
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char *source = malloc(file_size + 1);
    if (!source) {
        fclose(file);
        error_set_simple(&interp->error, ERR_MEMORY, "out of memory");
        result->type = RESULT_ERROR;
        error_format(&interp->error, result->value, sizeof(result->value));
        return -1;
    }
    
    size_t read_size = fread(source, 1, file_size, file);
    source[read_size] = '\0';
    fclose(file);
    
    /* 执行源代码 */
    int ret = interpreter_execute(interp, source, filename, result);
    free(source);
    
    return ret;
}

/* 获取错误信息 */
const LogexError* interpreter_get_error(const Interpreter *interp) {
    return interp ? &interp->error : NULL;
}

/* 清除错误信息 */
void interpreter_clear_error(Interpreter *interp) {
    if (interp) {
        error_clear(&interp->error);
    }
}

/* 设置数值精度 */
void interpreter_set_precision(Interpreter *interp, int precision) {
    if (interp) {
        interp->precision = precision;
    }
}

/* 获取数值精度 */
int interpreter_get_precision(const Interpreter *interp) {
    return interp ? interp->precision : -1;
}

/* 导入包 */
int interpreter_import_package(Interpreter *interp, const char *package_name) {
    if (!interp || !package_name) return -1;
    
    int count = package_load(interp->packages, package_name, interp->funcs, interp->context);
    if (count < 0) {
        error_set_simple(&interp->error, ERR_IMPORT, "failed to load package");
        return -1;
    }
    
    return count;
}

/* 获取变量值 */
int interpreter_get_variable(const Interpreter *interp, const char *name, char *value, size_t max_len) {
    if (!interp || !name || !value || max_len == 0) return -1;
    
    BigNum var_value;
    bignum_init(&var_value);
    
    if (context_get(interp->context, name, &var_value) != 0) {
        bignum_free(&var_value);
        return -1;
    }
    
    int ret = bignum_to_string(&var_value, value, max_len, interp->precision);
    bignum_free(&var_value);
    
    return ret == BIGNUM_SUCCESS ? 0 : -1;
}

/* 设置变量值 */
int interpreter_set_variable(Interpreter *interp, const char *name, const char *value) {
    if (!interp || !name || !value) return -1;
    
    /* 解析值 */
    BigNum var_value;
    bignum_init(&var_value);
    
    BigNum *parsed = bignum_from_string(value);
    if (!parsed) {
        bignum_free(&var_value);
        return -1;
    }
    
    bignum_copy(parsed, &var_value);
    bignum_destroy(parsed);
    
    /* 设置变量 */
    int ret = context_set(interp->context, name, &var_value);
    bignum_free(&var_value);
    
    return ret;
}

/* 列出所有变量 */
int interpreter_list_variables(const Interpreter *interp, char *buffer, size_t max_len) {
    if (!interp || !buffer || max_len == 0) return -1;
    
    context_list(interp->context, buffer, max_len);
    return 0;
}

/* 列出所有函数 */
int interpreter_list_functions(const Interpreter *interp, char *buffer, size_t max_len) {
    if (!interp || !buffer || max_len == 0) return -1;
    
    function_list(interp->funcs, buffer, max_len);
    return 0;
}

/* 清空所有变量 */
void interpreter_clear_variables(Interpreter *interp) {
    if (interp) {
        context_clear(interp->context);
    }
}

/* 设置执行模式 */
void interpreter_set_vm_mode(Interpreter *interp, int use_vm) {
    if (interp) {
        interp->use_vm = use_vm;
    }
}

/* 兼容性接口实现 - 直接调用原有的evaluator函数 */
