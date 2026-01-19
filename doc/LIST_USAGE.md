# Logex List 类型使用指南

## 概述

Logex 现在支持 List 类型，基于 `lib/list.h` 库实现。List 类型提供了动态数组功能，支持在两端高效地添加和删除元素。

## 类型标识

- **类型常量**: `BIGNUM_TYPE_LIST` (值为 3)
- **类型检查**: 使用 `bignum_is_list(num)` 函数

## 基础操作函数

### 1. 类型转换包 (libtype.so)

```javascript
import type

// 创建空列表
let empty_list = lst()

// 将单个元素转换为列表
let single_item_list = lst(42)
let string_list = lst("hello")
```

### 2. 列表操作包 (liblist.so)

```javascript
import list

// 创建空列表
let my_list = list()

// 左端添加元素 (类似栈的 push)
my_list = lpush(my_list, 1)
my_list = lpush(my_list, 2)
my_list = lpush(my_list, 3)
// 现在 my_list 包含: [3, 2, 1]

// 右端添加元素 (类似队列的 enqueue)
my_list = rpush(my_list, 4)
my_list = rpush(my_list, 5)
// 现在 my_list 包含: [3, 2, 1, 4, 5]

// 左端弹出元素 (类似栈的 pop)
let left_item = lpop(my_list)  // 返回 3
// 现在 my_list 包含: [2, 1, 4, 5]

// 右端弹出元素 (类似队列的 dequeue)
let right_item = rpop(my_list)  // 返回 5
// 现在 my_list 包含: [2, 1, 4]

// 获取指定位置的元素 (索引从 0 开始)
let first = lget(my_list, 0)   // 返回 2
let second = lget(my_list, 1)  // 返回 1

// 获取列表大小
let size = lsize(my_list)      // 返回 3
```

## 可用函数列表

### type 包函数
- `lst()` - 创建空列表
- `lst(element)` - 创建包含单个元素的列表

### list 包函数
- `list()` - 创建空列表
- `lpush(list, element)` - 在列表左端添加元素
- `rpush(list, element)` - 在列表右端添加元素
- `lpop(list)` - 从列表左端弹出元素
- `rpop(list)` - 从列表右端弹出元素
- `lget(list, index)` - 获取指定位置的元素
- `lsize(list)` - 获取列表大小

## 底层实现

List 类型基于 `lib/list.h` 中的 `LIST` 结构实现：

- **数据结构**: 分块双端队列 (Block-based Deque)
- **块大小**: 4096 个元素
- **内存管理**: 动态分配，自动扩展和收缩
- **性能**: 两端操作 O(1)，随机访问 O(n)

## C API

如果需要在 C 代码中直接操作 List 类型：

```c
#include "bignum.h"

// 创建空列表
BigNum *list = bignum_create_list();

// 类型检查
if (bignum_is_list(list)) {
    // 获取底层 LIST 指针
    struct LIST *list_ptr = bignum_get_list(list);
    
    // 使用 lib/list.h 中的函数操作
    // list_lpush(list_ptr, element);
    // list_rpush(list_ptr, element);
    // 等等...
}

// 释放内存
bignum_destroy(list);
```

## 注意事项

1. **内存管理**: List 中的元素是 BigNum 指针，会自动管理内存
2. **类型安全**: 所有函数都会检查参数类型
3. **索引范围**: `lget()` 函数会检查索引边界
4. **空列表操作**: 对空列表执行 `lpop()` 或 `rpop()` 会返回错误

## 编译

确保在编译时包含必要的库：

```bash
# 编译包
cd package
make libtype.so liblist.so

# 在项目中使用
gcc -DLOGEX_BUILD -I. -Ishare -Ilib your_code.c bignum.c lib/list.c -o your_program
```

## 示例程序

参见 `test_list.c` 获取完整的使用示例。
