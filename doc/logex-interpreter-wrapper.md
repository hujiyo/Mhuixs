# logex.c是对interpreter.h的对外封装

## 概述

`logex.c` 是 Logex 语言工具链中的应用层入口模块，它通过 **统一接口层** 实现对解释器功能的调用，是 interpreter 模块的对外封装。该模块遵循清晰的分层架构设计，确保系统可维护性和模块化。

## 架构位置

在 Logex 系统的分层架构中，`logex.c` 位于最上层：

```
应用层 (logex.c)
    ↓ 仅通过接口调用
接口层 (interpreter.h)
    ↓ 内部实现
执行层 (vm.h, evaluator.h)
```

## 核心设计原则

### 单一依赖原则
`logex.c` 仅包含 `interpreter.h` 头文件，完全不直接依赖底层的虚拟机（VM）或求值器实现。这种设计保证了：

1. **架构清晰**：应用层不关心底层实现细节
2. **低耦合**：VM 实现可替换而不影响应用层
3. **接口统一**：所有执行路径通过同一套接口调用

## 功能封装

`logex.c` 封装了三种主要执行模式，均通过 interpreter 接口实现：

### 1. REPL 交互模式
```c
// 从标准输入读取并执行代码
interpreter_execute(interp, source, "<stdin>", &result);
```

### 2. 源代码文件执行
```c
// 编译并执行 .lgx 源文件
interpreter_execute_file(interp, "script.lgx", &result);
```

### 3. 字节码文件执行
```c
// 直接执行预编译的字节码文件 (.ls)
interpreter_execute_bytecode(interp, "script.ls", &result);
```

## 关键接口方法

### 字节码执行接口
`logex.c` 通过以下接口间接执行字节码，这是封装的核心：

```c
// logex.c 中的调用方式
int execute_bytecode(const char *bytecode_file, int verbose) {
    Interpreter *interp = interpreter_create();
    InterpreterResult result;
    
    // 通过接口层调用，不直接操作 VM
    interpreter_execute_bytecode(interp, bytecode_file, &result);
    
    if (result.type == RESULT_VALUE) {
        printf("%s\n", result.value);
    }
    
    interpreter_destroy(interp);
    return 0;
}
```

### 接口层实现
实际的 VM 调用被封装在 `interpreter.c` 中：

```c
// interpreter.c 内部实现
int interpreter_execute_bytecode(Interpreter *interp, 
                                  const char *bytecode_file, 
                                  InterpreterResult *result) {
    // 创建并配置 VM（对 logex.c 透明）
    VM *vm = vm_create();
    vm->context = interp->context;
    vm->pkg_manager = interp->packages;
    vm->func_registry = interp->funcs;
    
    // 执行字节码
    vm_load_file(vm, bytecode_file);
    vm_run(vm);
    
    // 获取结果并转换
    BHS *vm_result = vm_get_result(vm);
    if (vm_result) {
        result->type = RESULT_VALUE;
        bignum_to_string(vm_result, result->value, ...);
    }
    
    // 清理资源
    vm->context = NULL;  // 避免双重释放
    vm_destroy(vm);
    
    return 0;
}
```

## 架构优势

### 1. 分层清晰
```
logex.c (应用层)
    ├── 命令行参数解析
    ├── 用户交互处理
    └── 通过 interpreter.h 调用执行功能
    
interpreter.c (接口层)
    ├── VM 生命周期管理
    ├── 资源共享协调
    └── 结果格式转换
    
vm.c (执行层)
    └── 实际的字节码执行逻辑
```

### 2. 可维护性
- **关注点分离**：`logex.c` 专注于用户交互，不涉及执行细节
- **单一修改点**：VM 调用逻辑集中在 `interpreter.c`
- **接口稳定**：底层实现变更不影响应用层

### 3. 扩展性
新增执行模式时：
1. 在 `interpreter.h` 中添加接口
2. 在 `interpreter.c` 中实现
3. `logex.c` 通过统一方式调用

## 设计演进

### 原始问题
早期版本中，`logex.c` 直接调用 VM 函数：
```c
// 违反分层架构
VM *vm = vm_create();      // 直接依赖 vm.h
vm_load_file(vm, ...);
vm_run(vm);
```

### 解决方案
通过引入 `interpreter_execute_bytecode()` 接口：
1. 将 VM 依赖移至接口层
2. 保持应用层简洁
3. 建立清晰的依赖边界

## 总结

`logex.c` 作为 Logex 工具链的入口点，通过精心设计的封装：
- 提供统一的用户界面
- 隐藏底层实现的复杂性
- 遵循分层架构原则
- 确保系统的可维护性和可扩展性

这种设计模式使得 Logex 系统能够灵活演进，同时保持上层应用的稳定性和简洁性。