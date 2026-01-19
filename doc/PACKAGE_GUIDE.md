# 包开发指南

## 概述

Logex 现在支持**包加载机制**。你只需将编译好的包（`.so` 动态库）放入 `package/` 目录，就可以通过 `import` 语句加载

## 核心特性

- ✅ **函数动态注册**：包中的函数动态注册到计算器
- ✅ **常量动态注册**：包可以注册数学常量（如 π, e）
- ✅ **标准接口**：统一的函数签名，易于开发

## 🔧 开发自己的包

### 包的基本结构

每个包必须：
1. 实现 `package_register()` 函数（必需）
2. 可选实现 `package_register_constants()` 函数
3. 编译为共享库（`.so` 文件）
4. 文件名格式：`lib<包名>.so`（例如：`libmath.so`）

### 包加载流程
```
┌─────────────────────────────────────┐
│         calculator.c                │
│  - 初始化 PackageManager            │
│  - 处理 packages 命令               │
└──────────────┬──────────────────────┘
               │
┌──────────────▼──────────────────────┐
│         evaluator.c                 │
│  - 解析 import 语句                 │
│  - 调用 package_load()              │
└──────────────┬──────────────────────┘
               │
┌──────────────▼──────────────────────┐
│         package.c                   │
│  - 扫描 package 目录                │
│  - 加载动态库 (dlopen)              │
│  - 查找符号 (dlsym)                 │
│  - 调用 package_register()          │
└──────────────┬──────────────────────┘
               │
┌──────────────▼──────────────────────┐
│    package/lib*.so (动态库)         │
│  - 实现 package_register()          │
│  - 注册函数到 FunctionRegistry      │
│  - 可选：注册常量到 Context         │
└─────────────────────────────────────┘
```
### 关键技术点

1. **动态库加载**
   - 使用 Linux 的 dlopen/dlsym/dlclose API
   - RTLD_LAZY 延迟绑定模式

2. **符号导出**
   - 使用 -rdynamic 编译标志
   - 让主程序的符号对动态库可见

3. **标准接口**
   - 统一的函数签名：`NativeFunction`
   - 统一的注册接口：`package_register()`
   - 可选的常量接口：`package_register_constants()`

4. **自动发现**
   - 使用 opendir/readdir/closedir 扫描目录
   - 识别 lib*.so 文件模式

5. **防重复加载**
   - 维护已加载包列表
   - 检查包是否已加载

### 资源限制
- 最大包数量：32（MAX_PACKAGES）
- 最大函数数量：256（MAX_FUNCTIONS）
- 可根据需要调整

### 最小示例

创建 `package/mypackage.c`：


> 函数签名必须符合 `NativeFunction` 类型
```c
#include "../function.h"
#include "../bignum.h"

/* 你的函数实现 */
static int my_func(const BHS *args, int arg_count, BHS *result, int precision) {
    if (arg_count != 1) return -1;
    
    // 实现你的逻辑
    BHS two;
    bhs_from_string("2", &two);
    return bhs_mul(&args[0], &two, result);
}

/* 包注册函数（必需） */
int package_register(FunctionRegistry *registry) {
    if (registry == NULL) return -1;
    
    int count = 0;
    count += (function_register(registry, "myfunc", my_func, 1, 1, 
                                "函数描述") == 0);
    return count;
}
```

### 编译并使用包

编译成.so并放到package目录下

- **文件名格式**：`lib<包名>.so`
- **导入时使用**：`import <包名>`

示例：
- `libmath.so` → `import math`
- `libstats.so` → `import stats`
- `libmypackage.so` → `import mypackage`
```bash
gcc -shared -fPIC -o package/libmypackage.so package/mypackage.c -I.
```

导入包：
```bash
expr > import mypackage
已导入 mypackage 包 (1 个函数)

expr > myfunc(10)
= 20
```


## 📋 函数签名规范

### 函数类型定义

```c
typedef int (*NativeFunction)(const BHS *args, int arg_count, BHS *result, int precision);
```

### 参数说明

- `args`: 参数数组（BHS 类型）
- `arg_count`: 参数数量
- `result`: 结果输出（BHS 类型）
- `precision`: 精度（小数位数）

### 返回值

- `0`: 成功
- `-1`: 失败（参数错误、定义域错误等）

### 示例实现

```c
/* 计算平方：square(x) = x * x */
static int my_square(const BHS *args, int arg_count, BHS *result, int precision) {
    /* 检查参数数量 */
    if (arg_count != 1) return -1;
    
    /* 计算 x * x */
    return bhs_mul(&args[0], &args[0], result);
}
```

## 🔬 高级特性

### 1. 可变参数函数

```c
/* max(a, b, c, ...) - 返回最大值 */
static int my_max(const BHS *args, int arg_count, BHS *result, int precision) {
    if (arg_count < 1) return -1;
    
    *result = args[0];
    
    for (int i = 1; i < arg_count; i++) {
        double current = bhs_to_double(result);
        double next = bhs_to_double(&args[i]);
        if (next > current) {
            *result = args[i];
        }
    }
    
    return 0;
}

/* 注册时设置 max_args 为 -1 表示无限制 */
function_register(registry, "max", my_max, 1, -1, "最大值 max(a,b,...)");
```

### 2. 注册常量

```c
/* 包常量注册函数（可选） */
int package_register_constants(void *ctx) {
    if (ctx == NULL) return -1;
    
    Context *context = (Context *)ctx;
    BHS value;
    
    /* 注册圆周率 π */
    double_to_bignum(3.141592653589793, &value, BHS_DEFAULT_PRECISION);
    context_set(context, "pi", &value);
    
    return 0;
}
```

### 3. 使用数学库函数

```c
#include <math.h>

/* 辅助函数：BHS 转 double */
static double bhs_to_double(const BHS *num) {
    if (num == NULL) return 0.0;
    
    double result = 0.0;
    double multiplier = 1.0;
    
    for (int i = 0; i < num->length; i++) {
        if (i == num->decimal_pos) {
            multiplier = 1.0;
        } else if (i > num->decimal_pos) {
            multiplier *= 10.0;
        }
        result += num->digits[i] * multiplier;
        if (i < num->decimal_pos) {
            multiplier /= 10.0;
        }
    }
    
    return num->is_negative ? -result : result;
}

/* 辅助函数：double 转 BHS */
static int double_to_bignum(double value, BHS *num, int precision) {
    if (num == NULL) return -1;
    
    char buffer[512];
    snprintf(buffer, sizeof(buffer), "%.*f", precision, value);
    return bhs_from_string(buffer, num);
}

/* 正弦函数 */
static int my_sin(const BHS *args, int arg_count, BHS *result, int precision) {
    if (arg_count != 1) return -1;
    double x = bhs_to_double(&args[0]);
    return double_to_bignum(sin(x), result, precision);
}
```

### 4. 错误处理

```c
/* 平方根函数（带定义域检查） */
static int my_sqrt(const BHS *args, int arg_count, BHS *result, int precision) {
    if (arg_count != 1) return -1;
    
    double x = bhs_to_double(&args[0]);
    
    /* 检查定义域 */
    if (x < 0.0) {
        return -1;  /* 不支持复数 */
    }
    
    return double_to_bignum(sqrt(x), result, precision);
}
```

## 🛠️ BHS API 参考

### 基本操作

```c
/* 初始化 */
void bhs_init(BHS *num);

/* 从字符串创建 */
int bhs_from_string(const char *str, BHS *num);

/* 转换为字符串 */
int bhs_to_string(const BHS *num, char *str, size_t max_len, int precision);
```

### 算术运算

```c
/* 加法：result = a + b */
int bhs_add(const BHS *a, const BHS *b, BHS *result);

/* 减法：result = a - b */
int bhs_sub(const BHS *a, const BHS *b, BHS *result);

/* 乘法：result = a * b */
int bhs_mul(const BHS *a, const BHS *b, BHS *result);

/* 除法：result = a / b */
int bhs_div(const BHS *a, const BHS *b, BHS *result, int precision);

/* 取模：result = a % b */
int bhs_mod(const BHS *a, const BHS *b, BHS *result);

/* 幂运算：result = base ^ exp */
int bhs_pow(const BHS *base, const BHS *exp, BHS *result, int precision);
```

## 🎓 完整示例：统计包

创建 `package/stats_package.c`：

```c
#include "../function.h"
#include "../bignum.h"
#include <math.h>

/* 辅助函数 */
static double bhs_to_double(const BHS *num) {
    /* ... 实现见上文 ... */
}

static int double_to_bignum(double value, BHS *num, int precision) {
    /* ... 实现见上文 ... */
}

/* 平均值：avg(a, b, c, ...) */
static int stats_avg(const BHS *args, int arg_count, BHS *result, int precision) {
    if (arg_count < 1) return -1;
    
    double sum = 0.0;
    for (int i = 0; i < arg_count; i++) {
        sum += bhs_to_double(&args[i]);
    }
    
    return double_to_bignum(sum / arg_count, result, precision);
}

/* 标准差：std(a, b, c, ...) */
static int stats_std(const BHS *args, int arg_count, BHS *result, int precision) {
    if (arg_count < 2) return -1;
    
    /* 计算平均值 */
    double sum = 0.0;
    for (int i = 0; i < arg_count; i++) {
        sum += bhs_to_double(&args[i]);
    }
    double mean = sum / arg_count;
    
    /* 计算方差 */
    double variance = 0.0;
    for (int i = 0; i < arg_count; i++) {
        double diff = bhs_to_double(&args[i]) - mean;
        variance += diff * diff;
    }
    variance /= arg_count;
    
    /* 返回标准差 */
    return double_to_bignum(sqrt(variance), result, precision);
}

/* 包注册函数 */
int package_register(FunctionRegistry *registry) {
    if (registry == NULL) return -1;
    
    int count = 0;
    count += (function_register(registry, "avg", stats_avg, 1, -1, 
                                "平均值 avg(a,b,...)") == 0);
    count += (function_register(registry, "std", stats_std, 2, -1, 
                                "标准差 std(a,b,...)") == 0);
    
    return count;
}
```

编译并使用：

```bash
# 编译
gcc -shared -fPIC -o package/libstats.so package/stats_package.c -I. -lm

# 使用
expr > import stats
已导入 stats 包 (2 个函数)

expr > avg(1, 2, 3, 4, 5)
= 3

expr > std(1, 2, 3, 4, 5)
= 1.414213562373095...
```

## 🔍 调试技巧

### 1. 检查包是否正确加载

```bash
expr > packages
可用的包：
  mypackage
```

### 2. 查看注册的函数

```bash
expr > import mypackage
已导入 mypackage 包 (1 个函数)

expr > funcs
可用函数列表：
  double - 将数值翻倍 double(x)
```

### 3. 编译时添加调试信息

```bash
gcc -shared -fPIC -g -o package/libmypackage.so package/mypackage.c -I.
```

### 4. 使用 ldd 检查依赖

```bash
ldd package/libmypackage.so
```

## ⚠️ 注意事项

1. **文件命名**：包文件必须以 `lib` 开头，`.so` 结尾
   - 正确：`libmath.so` → 导入时使用 `import math`
   - 错误：`math.so`, `mathlib.so`

2. **函数签名**：必须严格遵循 `NativeFunction` 类型定义

3. **内存管理**：BHS 结构体由计算器管理，包函数不需要释放

4. **线程安全**：当前版本不支持多线程，包函数无需考虑线程安全

5. **精度处理**：使用提供的 `precision` 参数控制输出精度

## 📚 参考资料

- [function.h](./function.h) - 函数注册机制接口
- [bignum.h](./bignum.h) - 大数运算接口
- [context.h](./context.h) - 变量上下文接口
- [package/math_package.c](./package/math_package.c) - Math 包源码
- [package/example_package.c](./package/example_package.c) - 示例包源码

## 🚀 快速开始模板

```c
#include "../function.h"
#include "../bignum.h"

/* 你的函数实现 */
static int my_func(const BHS *args, int arg_count, BHS *result, int precision) {
    // TODO: 实现你的函数
    return 0;
}

/* 包注册函数 */
int package_register(FunctionRegistry *registry) {
    if (registry == NULL) return -1;
    
    int count = 0;
    count += (function_register(registry, "myfunc", my_func, 1, 1, 
                                "函数描述") == 0);
    
    return count;
}
```

编译：
```bash
gcc -shared -fPIC -o package/libmypkg.so package/mypkg.c -I.
```

使用：
```bash
expr > import mypkg
expr > myfunc(123)
```
