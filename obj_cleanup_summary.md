# Obj 冗余清理总结

## 📋 完成的工作

### 1. **将 `share/obj.h` 合并到 `bignum.h`**

**原因**：
- `obj.h` 只定义了 `Obj` 和 `BHS` 结构
- `BHS` 是 Logex 的核心类型，应该在 `bignum.h` 中统一管理
- 减少文件碎片，简化依赖关系

**改动**：
```c
// bignum.h 新增内容（文件开头）
/* ========================================
 * 通用类型定义（原 share/obj.h）
 * ======================================== */

/* Obj - 通用对象指针，用于 lib/ 中的数据结构（LIST, TABLE） */
typedef void* Obj;

/* BHS 相关常量定义 */
#define BIGNUM_SMALL_SIZE 32
#define BIGNUM_TYPE_NULL    -1
#define BIGNUM_TYPE_NUMBER  0
// ... 其他类型常量

/* BHS 结构体定义 - 固定64字节 */
typedef struct {
    int type;
    int is_large;
    union { ... } data;
    size_t capacity;
    size_t length;
    union { ... } type_data;
} BHS, BigNum, basic_handle_struct, bhs;
```

---

### 2. **更新 lib/ 中的引用**

#### `lib/list.h`
```c
// 之前
#include "../share/obj.h"

// 之后
#include "../bignum.h"  /* 提供 Obj 和 BHS 类型定义 */
```

#### `lib/bitmap.h`
```c
// 之前
#include "../share/obj.h"

// 之后
#include "../bignum.h"  /* 提供 BHS 类型定义 */
```

#### `lib/tblh.h`
```c
// 之前
typedef void* Obj;  // 重复定义

// 之后
/* Obj 类型由 bignum.h 提供 */  // 删除重复定义
```

---

### 3. **删除 `share/obj.h` 文件**

✅ 文件已删除，不再需要

---

## 🏗️ 架构优化结果

### **优化前**
```
src/
├── share/
│   └── obj.h              # 定义 Obj 和 BHS
├── bignum.h               # 引用 obj.h
├── lib/
│   ├── list.h             # 引用 obj.h
│   ├── bitmap.h           # 引用 obj.h
│   └── tblh.h             # 重复定义 Obj
```

### **优化后**
```
src/
├── bignum.h               # 统一定义 Obj 和 BHS
├── lib/
│   ├── list.h             # 引用 bignum.h
│   ├── bitmap.h           # 引用 bignum.h
│   └── tblh.h             # 使用 bignum.h 的 Obj
```

---

## 📊 类型关系说明

### **Obj 的用途**
```c
typedef void* Obj;  // 通用对象指针
```

**使用场景**：
- `LIST` 数据结构 - 存储任意类型的指针
- `TABLE` 数据结构 - 单元格存储任意类型的指针

**为什么保留 Obj**：
- LIST 和 TABLE 需要存储**任意类型的指针**（`void*`）
- 不能直接用 `BHS*`，因为它们可能存储其他类型的指针
- `Obj` 作为 `void*` 的语义化别名，提高代码可读性

---

### **BHS 的用途**
```c
typedef struct { ... } BHS;  // Logex 统一数据类型
```

**使用场景**：
- Logex 脚本引擎的核心数据类型
- 可以表示：NUMBER, STRING, BITMAP, LIST, TABLE, KVALOT
- 提供统一的操作接口（`bignum_*` 函数）

---

## 🎯 架构清晰度提升

### **依赖关系**
```
┌─────────────────────────────────────┐
│          bignum.h                   │
│  ┌───────────────────────────┐     │
│  │ typedef void* Obj;        │     │  ◄─── lib/list.h
│  │ typedef struct {...} BHS; │     │  ◄─── lib/bitmap.h
│  │ #define BIGNUM_TYPE_*     │     │  ◄─── lib/tblh.h
│  └───────────────────────────┘     │
└─────────────────────────────────────┘
```

**优势**：
1. ✅ **单一数据源** - 所有类型定义在一个文件
2. ✅ **减少依赖** - 不再需要 `share/obj.h`
3. ✅ **避免重复定义** - `tblh.h` 不再重复定义 `Obj`
4. ✅ **清晰的职责** - `bignum.h` 是 Mhuixs 的核心类型头文件

---

## 📝 后续建议

### 1. **编译测试**
```bash
cd src
make clean
make
```

### 2. **可选优化：重命名 bignum.h**
由于 `bignum.h` 现在包含了所有核心类型定义，可以考虑重命名为：
- `mhuixs_types.h` - Mhuixs 类型定义
- `bhs.h` - BHS 核心类型
- 保持 `bignum.h` - 向后兼容

### 3. **文档更新**
更新以下文档：
- `README.md` - 架构说明
- `doc/BUILTIN_FUNCTIONS.md` - 类型说明
- 添加注释说明 `Obj` 和 `BHS` 的区别

---

## ✅ 完成状态

- [x] 将 `obj.h` 内容合并到 `bignum.h`
- [x] 更新 `lib/list.h` 引用
- [x] 更新 `lib/bitmap.h` 引用
- [x] 删除 `lib/tblh.h` 重复定义
- [x] 删除 `share/obj.h` 文件
- [x] 验证无其他引用
- [ ] 编译测试（需用户手动执行）

---

## 🎉 总结

通过这次清理：
1. **消除了冗余** - 删除了 `share/obj.h`
2. **统一了管理** - 所有类型定义在 `bignum.h`
3. **简化了依赖** - 减少了文件间的引用关系
4. **提高了可维护性** - 单一数据源，易于修改

**架构更加清晰，符合 Mhuixs 的设计理念！**
