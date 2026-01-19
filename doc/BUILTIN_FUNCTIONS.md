# Logex 内置函数文档

Logex 是 Mhuixs 的原生操作语言。以下函数是内置的，**无需 import**，可直接使用。

> **底层类型**：所有数据使用 BHS (Basic Handle Struct) 统一封装，支持 NUMBER、STRING、BITMAP、LIST 等类型。

---

## 📋 LIST 操作函数

### `list()` - 创建空列表
创建一个新的空列表。

**语法**：
```javascript
let mylist = list()
```

**返回值**：LIST 类型

---

### `lpush(list, value)` - 左侧插入
在列表左侧插入元素。

**参数**：
- `list`: LIST 类型
- `value`: 任意类型

**返回值**：修改后的 LIST

**示例**：
```javascript
let mylist = list()
let mylist = lpush(mylist, 100)
let mylist = lpush(mylist, 200)
// mylist: [200, 100]
```

---

### `rpush(list, value)` - 右侧插入
在列表右侧插入元素。

**参数**：
- `list`: LIST 类型
- `value`: 任意类型

**返回值**：修改后的 LIST

**示例**：
```javascript
let mylist = list()
let mylist = rpush(mylist, 100)
let mylist = rpush(mylist, 200)
// mylist: [100, 200]
```

---

### `lpop(list)` - 左侧弹出
从列表左侧弹出并返回元素。

**参数**：
- `list`: LIST 类型

**返回值**：弹出的元素

**示例**：
```javascript
let mylist = rpush(list(), 100)
let mylist = rpush(mylist, 200)
let val = lpop(mylist)  // val = 100
```

---

### `rpop(list)` - 右侧弹出
从列表右侧弹出并返回元素。

**参数**：
- `list`: LIST 类型

**返回值**：弹出的元素

---

### `lget(list, index)` - 获取元素
获取列表指定位置的元素。

**参数**：
- `list`: LIST 类型
- `index`: 索引（从 0 开始）

**返回值**：指定位置的元素

**示例**：
```javascript
let mylist = rpush(list(), 100)
let mylist = rpush(mylist, 200)
let val = lget(mylist, 0)  // val = 100
let val2 = lget(mylist, 1) // val2 = 200
```

---

### `llen(list)` - 列表长度
获取列表的元素数量。

**参数**：
- `list`: LIST 类型

**返回值**：列表长度（数字）

**示例**：
```javascript
let mylist = rpush(list(), 100)
let mylist = rpush(mylist, 200)
let size = llen(mylist)  // size = 2
```

---

## 🔄 TYPE 转换函数

### `num(value)` - 转换为数字
将字符串或位图转换为数字。

**参数**：
- `value`: 字符串、位图或数字

**返回值**：数字类型

**示例**：
```javascript
let n1 = num("123.456")  // n1 = 123.456
let n2 = num("789")      // n2 = 789
let n3 = num(bmp(255))   // n3 = 255
```

---

### `str(value)` - 转换为字符串
将数字或位图转换为字符串。

**参数**：
- `value`: 数字、位图或字符串

**返回值**：字符串类型

**示例**：
```javascript
let s1 = str(123.456)  // s1 = "123.456"
let s2 = str(789)      // s2 = "789"
```

---

### `bmp(value)` - 转换为位图
将数字或字符串转换为位图。

**参数**：
- `value`: 数字、字符串或位图

**返回值**：BITMAP 类型

**示例**：
```javascript
let b1 = bmp(255)      // 创建值为 255 的位图
let b2 = bmp("100")    // 创建值为 100 的位图
let b3 = bmp(0)        // 创建空位图
```

---

## 🎯 BITMAP 操作函数

### `bset(bitmap, offset, value)` - 设置位
设置位图指定位置的位值。

**参数**：
- `bitmap`: BITMAP 类型
- `offset`: 位偏移量
- `value`: 位值（0 或 1）

**返回值**：修改后的 BITMAP

**示例**：
```javascript
let bm = bmp(0)
let bm = bset(bm, 10, 1)  // 设置第 10 位为 1
let bm = bset(bm, 20, 1)  // 设置第 20 位为 1
```

---

### `bget(bitmap, offset)` - 获取位
获取位图指定位置的位值。

**参数**：
- `bitmap`: BITMAP 类型
- `offset`: 位偏移量

**返回值**：位值（0 或 1）

**示例**：
```javascript
let bm = bmp(0)
let bm = bset(bm, 10, 1)
let bit = bget(bm, 10)  // bit = 1
let bit2 = bget(bm, 11) // bit2 = 0
```

---

### `bcount(bitmap, start, end)` - 统计位数
统计位图指定范围内值为 1 的位数。

**参数**：
- `bitmap`: BITMAP 类型
- `start`: 起始位置
- `end`: 结束位置

**返回值**：值为 1 的位数

**示例**：
```javascript
let bm = bmp(0)
let bm = bset(bm, 10, 1)
let bm = bset(bm, 20, 1)
let bm = bset(bm, 30, 1)
let count = bcount(bm, 0, 100)  // count = 3
```

---

## 💡 完整示例

### 示例 1：LIST 操作
```javascript
# 创建列表并操作
let mylist = list()
let mylist = rpush(mylist, 100)
let mylist = rpush(mylist, 200)
let mylist = rpush(mylist, 300)

# 获取列表信息
let size = llen(mylist)      # size = 3
let first = lget(mylist, 0)  # first = 100
let last = lget(mylist, 2)   # last = 300

# 弹出元素
let val = lpop(mylist)       # val = 100, mylist 现在是 [200, 300]
```

### 示例 2：类型转换
```javascript
# 字符串转数字
let n = num("123.456")
let result = n + 100  # result = 223.456

# 数字转字符串
let s = str(789)

# 位图转数字
let bm = bmp(255)
let n2 = num(bm)  # n2 = 255
```

### 示例 3：BITMAP 操作
```javascript
# 创建位图并设置位
let bm = bmp(0)
let bm = bset(bm, 0, 1)
let bm = bset(bm, 5, 1)
let bm = bset(bm, 10, 1)

# 检查位值
let bit0 = bget(bm, 0)   # bit0 = 1
let bit1 = bget(bm, 1)   # bit1 = 0
let bit5 = bget(bm, 5)   # bit5 = 1

# 统计位数
let count = bcount(bm, 0, 20)  # count = 3
```

---

## 🔑 关键特性

1. **无需 import**：所有内置函数直接可用
2. **类型安全**：函数会检查参数类型
3. **内存管理**：自动管理内存，无需手动释放
4. **性能优化**：内置函数直接调用，无额外开销

---

## 📚 与外部包的区别

**内置函数**（无需 import）：
- `list()`, `lpush()`, `rpush()`, `lpop()`, `rpop()`, `lget()`, `llen()`
- `num()`, `str()`, `bmp()`
- `bset()`, `bget()`, `bcount()`

**外部包**（需要 import）：
- `import math` - 数学函数（sin, cos, sqrt 等）
- 其他非数据库核心功能

**示例**：
```javascript
# 内置函数 - 无需 import
let mylist = list()
let n = num("123")

# 外部包 - 需要 import
import math
let result = math.sin(3.14)
```
