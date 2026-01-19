# lib/ 接口更新总结

## 🎯 更新目标

将 lib/ 中的数据结构对外接口从 `Obj` 改为 `BHS*`，明确表示这些数据结构存储的是 BHS 类型的指针。

---

## ✅ 完成的工作

### 1. **lib/list.h - LIST 数据结构**

#### 对外接口更新
```c
// 之前（使用 Obj）
int list_lpush(LIST* lst, Obj value);
int list_rpush(LIST* lst, Obj value);
Obj list_lpop(LIST* lst);
Obj list_rpop(LIST* lst);
int list_insert(LIST* lst, size_t pos, Obj value);
Obj list_get_index(const LIST* lst, size_t pos);
int list_set_index(LIST* lst, size_t pos, Obj value);

// 之后（使用 BHS*）
int list_lpush(LIST* lst, BHS* value);
int list_rpush(LIST* lst, BHS* value);
BHS* list_lpop(LIST* lst);
BHS* list_rpop(LIST* lst);
int list_insert(LIST* lst, size_t pos, BHS* value);
BHS* list_get_index(const LIST* lst, size_t pos);
int list_set_index(LIST* lst, size_t pos, BHS* value);
```

#### 内部实现
```c
/* 内部使用 Obj 作为 void* 别名，减少代码改动 */
typedef void* Obj;

typedef struct Block {
    Obj data[UINTDEQUE_BLOCK_SIZE];  // 内部仍使用 Obj
    struct Block *prev, *next;
    uint32_t size;
    uint32_t start;
} Block;
```

**说明**：
- 对外接口明确使用 `BHS*`
- 内部实现保留 `Obj` 作为 `void*` 别名，避免大量修改 `.c` 文件
- 由于 `BHS*` 本质上也是指针，与 `void*` 兼容

---

### 2. **lib/tblh.h - TABLE 数据结构**

#### 对外接口更新
```c
// 之前（使用 Obj）
int add_record(TABLE* table, Obj* values, size_t num);
Obj get_value(TABLE* table, size_t idx_x, size_t idx_y);
int set_value(TABLE* table, size_t idx_x, size_t idx_y, Obj content);
int update_record(TABLE* table, size_t logic_index, Obj* values, size_t num);
Obj* get_record(TABLE* table, size_t logic_index);

// 之后（使用 BHS*）
int add_record(TABLE* table, BHS** values, size_t num);
BHS* get_value(TABLE* table, size_t idx_x, size_t idx_y);
int set_value(TABLE* table, size_t idx_x, size_t idx_y, BHS* content);
int update_record(TABLE* table, size_t logic_index, BHS** values, size_t num);
BHS** get_record(TABLE* table, size_t logic_index);
```

#### 内部实现
```c
#include "../bignum.h"  /* 提供 BHS 类型定义 */

/* 内部使用 Obj 作为 void* 别名，减少代码改动 */
typedef void* Obj;

typedef struct FIELD {
    size_t column_index;
    Obj* data;  // 内部仍使用 Obj
    mstring name;
    int type;
} FIELD;
```

**说明**：
- 对外接口明确使用 `BHS*` 或 `BHS**`
- 内部实现保留 `Obj` 作为 `void*` 别名
- 添加了 `#include "../bignum.h"` 以获取 BHS 类型定义

---

### 3. **bignum.h - 核心类型定义**

#### 删除 Obj 定义
```c
// 之前
/* Obj - 通用对象指针，用于 lib/ 中的数据结构（LIST, TABLE） */
typedef void* Obj;

/* BHS 相关常量定义 */
#define BIGNUM_SMALL_SIZE 32

// 之后
/* BHS 相关常量定义 */
#define BIGNUM_SMALL_SIZE 32
```

**说明**：
- `Obj` 不再在 `bignum.h` 中定义
- 各个 lib/ 文件内部自行定义 `Obj` 作为 `void*` 别名
- `bignum.h` 专注于 BHS 类型定义

---

## 🏗️ 架构设计

### **对外接口 vs 内部实现**

```
┌─────────────────────────────────────────────┐
│  对外接口（头文件 .h）                        │
│  ┌───────────────────────────────────┐     │
│  │ int list_lpush(LIST*, BHS*);      │     │
│  │ BHS* list_lpop(LIST*);            │     │
│  │ BHS* get_value(TABLE*, ...);      │     │
│  │ int add_record(TABLE*, BHS**, ...);│     │
│  └───────────────────────────────────┘     │
└─────────────────────────────────────────────┘
                    ↓
        明确表示存储 BHS* 类型
                    ↓
┌─────────────────────────────────────────────┐
│  内部实现（.c 文件）                          │
│  ┌───────────────────────────────────┐     │
│  │ typedef void* Obj;                │     │
│  │                                   │     │
│  │ Obj data[SIZE];                   │     │
│  │ Obj* field_data;                  │     │
│  └───────────────────────────────────┘     │
└─────────────────────────────────────────────┘
        使用 Obj 作为 void* 别名
        减少代码改动，保持灵活性
```

---

## 📊 类型兼容性

### **为什么可以这样做？**

```c
// BHS* 本质上是指针
BHS* ptr1 = ...;

// void* 是通用指针
void* ptr2 = ptr1;  // ✅ 合法

// Obj 是 void* 的别名
typedef void* Obj;
Obj ptr3 = ptr1;    // ✅ 合法

// 因此 BHS* 和 Obj 可以互相转换
BHS* value = (BHS*)list_lpop(lst);  // ✅ 对外接口
// 内部实现：
// Obj internal_value = (Obj)value;  // ✅ 内部存储
```

**关键点**：
- `BHS*` 是具体类型的指针
- `Obj` (即 `void*`) 是通用指针
- 两者在 C 语言中可以隐式转换
- 对外使用 `BHS*` 提供类型安全
- 内部使用 `Obj` 保持实现灵活性

---

## 🎯 设计优势

### 1. **类型安全**
```c
// 对外接口明确类型
BHS* value = bignum_from_string("123");
list_lpush(mylist, value);  // ✅ 类型明确

// 而不是
Obj value = ...;  // ❌ 不清楚存储的是什么类型
```

### 2. **语义清晰**
```c
// 清楚地表明 LIST 存储的是 BHS* 类型
int list_lpush(LIST* lst, BHS* value);

// 而不是模糊的 Obj
int list_lpush(LIST* lst, Obj value);  // ❌ Obj 是什么？
```

### 3. **减少改动**
```c
// 内部实现不需要大量修改
typedef void* Obj;  // 保持原有代码逻辑

// .c 文件中的代码几乎不需要改动
Obj data[SIZE];  // 仍然使用 Obj
```

### 4. **向后兼容**
```c
// 旧代码可以逐步迁移
Obj old_value = list_lpop(lst);  // 仍然可以编译（有警告）
BHS* new_value = list_lpop(lst); // ✅ 推荐的新写法
```

---

## 📝 使用示例

### **LIST 操作**
```c
// 创建 BHS 值
BHS* num1 = bignum_from_string("100");
BHS* num2 = bignum_from_string("200");

// 创建列表
LIST* mylist = list_create();

// 插入元素（使用 BHS*）
list_rpush(mylist, num1);
list_rpush(mylist, num2);

// 获取元素（返回 BHS*）
BHS* value = list_get_index(mylist, 0);

// 弹出元素（返回 BHS*）
BHS* popped = list_lpop(mylist);
```

### **TABLE 操作**
```c
// 创建表
TABLE* table = create_table(...);

// 准备一行数据（BHS* 数组）
BHS* row[3];
row[0] = bignum_from_string("1");
row[1] = bignum_from_raw_string("Alice");
row[2] = bignum_from_string("25");

// 添加记录（使用 BHS**）
add_record(table, row, 3);

// 获取单元格值（返回 BHS*）
BHS* value = get_value(table, 0, 1);  // 第0行第1列

// 设置单元格值（使用 BHS*）
BHS* new_value = bignum_from_raw_string("Bob");
set_value(table, 0, 1, new_value);
```

---

## 🔧 迁移指南

### **如果你在使用 lib/ 的代码**

#### 1. 更新函数调用
```c
// 旧代码
Obj value = list_lpop(mylist);

// 新代码
BHS* value = list_lpop(mylist);
```

#### 2. 更新变量声明
```c
// 旧代码
Obj values[10];

// 新代码
BHS* values[10];
```

#### 3. 更新函数参数
```c
// 旧代码
void my_function(Obj value) {
    list_lpush(mylist, value);
}

// 新代码
void my_function(BHS* value) {
    list_lpush(mylist, value);
}
```

---

## ✅ 总结

### **改动内容**
1. ✅ `lib/list.h` - 对外接口改为 `BHS*`
2. ✅ `lib/tblh.h` - 对外接口改为 `BHS*`
3. ✅ `bignum.h` - 删除 `Obj` 定义
4. ✅ 各 lib/ 文件内部定义 `typedef void* Obj;`

### **设计原则**
- **对外明确**：接口使用 `BHS*`，类型安全
- **内部灵活**：实现使用 `Obj`，减少改动
- **语义清晰**：明确表示存储的是 BHS 类型
- **向后兼容**：旧代码可以逐步迁移

### **架构优势**
- 🎯 类型安全 - 编译时检查
- 📖 语义清晰 - 一眼看出存储类型
- 🔧 易于维护 - 减少代码改动
- 🚀 性能无损 - 指针转换无开销

---

**现在 lib/ 的接口更加清晰，明确表示存储的是 BHS* 类型！**
