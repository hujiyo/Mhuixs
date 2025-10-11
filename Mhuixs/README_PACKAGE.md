# 包的使用说明书

## 简单来说

将包放入 `package/` 目录，执行import `包名`即可

## 快速开始

### 1. 查看可用的包

```bash
expr > packages
可用的包：
  math
  example
```

### 2. 导入包并使用包中的函数

```bash
expr > import math
已导入 math 包 (22 个函数)

expr > import example
已导入 example 包 (3 个函数)

# 使用 math 包
expr > sin(π/2)
= 1

expr > log(100)
= 2

# 使用 example 包
expr > double(5)
= 10
```

### 3. 查看已注册的函数

```bash
expr > funcs
可用函数列表：
  sin - 正弦函数 sin(x)
  cos - 余弦函数 cos(x)
  sqrt - 平方根 √x
  double - 将数值翻倍 double(x)
  ...
```

## 标准包介绍

### Math 包 (`libmath.so`)

**提供 22 个数学函数和 3 个常量**：

- `sin(x)`, `cos(x)`, `tan(x)`
- `asin(x)`, `acos(x)`, `atan(x)`, `atan2(y,x)`
- `exp(x)` - e^x
- `ln(x)` - 自然对数
- `log(x)` 或 `log(x, base)` - 对数
- `log10(x)`, `log2(x)`
- `sqrt(x)` - 平方根
- `cbrt(x)` - 立方根
- `floor(x)`, `ceil(x)`, `round(x)`, `trunc(x)`
- `abs(x)` - 绝对值
- `sign(x)` - 符号函数
- `max(a,b,...)` - 最大值（可变参数）
- `min(a,b,...)` - 最小值（可变参数）
- `pi` (π) = 3.14159...
- `e` = 2.71828...
- `phi` (φ) = 1.61803...

### Example 包 (`libexample.so`)

**提供 3 个示例函数**：

- `double(x)` - 返回 x * 2
- `triple(x)` - 返回 x * 3
- `square(x)` - 返回 x * x

## 包的开发文档

完整的包开发指南请参考：[PACKAGE_GUIDE.md](./PACKAGE_GUIDE.md)

包括：
- 函数签名规范
- BigNum API 参考
- 高级特性（可变参数、常量注册等）
- 完整示例（统计包）
- 调试技巧

