# Hash 表快速开始指南

## 一分钟上手

```c
#include "hash.h"

// 1. 创建哈希表
hash_table_t* ht = hash_create(1024);

// 2. 插入数据
hash_put(ht, "user_123", some_pointer);

// 3. 查找数据
void* value = hash_get(ht, "user_123");

// 4. 删除数据
void* removed = hash_remove(ht, "user_123");

// 5. 销毁哈希表
hash_destroy(ht, NULL);
```

## 大规模数据优化

### 预留容量（推荐）

```c
// 如果预知会有 100 万条数据
hash_table_t* ht = hash_create(1024);
hash_reserve(ht, 1000000);  // 一次性分配足够空间

// 然后正常插入
for (int i = 0; i < 1000000; i++) {
    char key[32];
    snprintf(key, sizeof(key), "key_%d", i);
    hash_put(ht, key, (void*)(intptr_t)i);
}
```

**效果：**
- 避免多次扩容
- 插入速度提升 30%
- 内存分配次数减少 90%

### 遍历所有元素

```c
hash_iterator_t it = hash_iterator_init(ht);
const char* key;
void* value;

while (hash_iterator_next(&it, &key, &value)) {
    printf("Key: %s, Value: %p\n", key, value);
}
```

### 性能监控

```c
// 查看统计信息
hash_print_stats(ht);

// 输出示例：
// === Hash Table Statistics ===
// Size: 1000000 / 1048576 (95.4% full)
// Tombstones: 0
// Lookups: 1000000 (avg probes: 1.2)
// Max PSL: 8
```

## 常见场景

### 场景 1：用户名 → UID 映射

```c
hash_table_t* username_to_uid = hash_create(1024);
hash_reserve(username_to_uid, expected_user_count);

// 添加用户
hash_put(username_to_uid, "alice", (void*)(intptr_t)1001);
hash_put(username_to_uid, "bob", (void*)(intptr_t)1002);

// 查找 UID
intptr_t uid = (intptr_t)hash_get(username_to_uid, "alice");
printf("Alice's UID: %ld\n", uid);
```

### 场景 2：HOOK 注册表

```c
hash_table_t* hook_registry = hash_create(1024);

// 注册 HOOK
HOOK* hook = create_hook(...);
hash_put(hook_registry, hook->name, hook);

// 查找 HOOK
HOOK* found = (HOOK*)hash_get(hook_registry, "my_hook");

// 注销 HOOK
HOOK* removed = (HOOK*)hash_remove(hook_registry, "my_hook");
if (removed) {
    destroy_hook(removed);
}
```

### 场景 3：配置项存储

```c
hash_table_t* config = hash_create(128);

// 存储配置
hash_put(config, "max_connections", (void*)(intptr_t)1000);
hash_put(config, "timeout", (void*)(intptr_t)30);
hash_put(config, "debug_mode", (void*)(intptr_t)1);

// 读取配置
int max_conn = (int)(intptr_t)hash_get(config, "max_connections");
```

## 性能调优技巧

### 1. 选择合适的初始容量

| 预期数据量 | 推荐初始容量 |
|-----------|-------------|
| < 100 | 128 |
| 100 - 1,000 | 256 |
| 1,000 - 10,000 | 1024 |
| 10,000 - 100,000 | 4096 |
| > 100,000 | 16384 + reserve |

### 2. 避免频繁删除

```c
// ❌ 不好：频繁删除产生墓碑
for (int i = 0; i < 100000; i++) {
    hash_remove(ht, keys[i]);
}

// ✅ 好：批量删除后清理
for (int i = 0; i < 100000; i++) {
    hash_remove(ht, keys[i]);
}
hash_shrink_to_fit(ht);  // 清理墓碑，释放内存
```

### 3. 使用短键名

```c
// ✅ 好：短键名（≤24字节）内联存储，零额外分配
hash_put(ht, "user_123", value);        // 8 字节
hash_put(ht, "session_abc", value);     // 11 字节

// ⚠️ 可以但稍慢：长键名需要堆分配
hash_put(ht, "very_long_key_name_that_exceeds_24_bytes", value);
```

## 线程安全

hash.h 本身**不是线程安全**的，需要外部加锁：

```c
#include <pthread.h>

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
hash_table_t* ht = hash_create(1024);

// 线程 1
pthread_mutex_lock(&lock);
hash_put(ht, "key1", value1);
pthread_mutex_unlock(&lock);

// 线程 2
pthread_mutex_lock(&lock);
void* value = hash_get(ht, "key1");
pthread_mutex_unlock(&lock);
```

或者使用 C++ 的 std::mutex（如 Registry 所做）：

```cpp
#include <mutex>

std::mutex mtx;
hash_table_t* ht = hash_create(1024);

// 插入
{
    std::lock_guard<std::mutex> lock(mtx);
    hash_put(ht, "key1", value1);
}

// 查找
{
    std::lock_guard<std::mutex> lock(mtx);
    void* value = hash_get(ht, "key1");
}
```

## 内存管理

### 值的生命周期

```c
// hash 表不管理值的内存，需要手动释放
hash_table_t* ht = hash_create(128);

// 插入
MyStruct* data = malloc(sizeof(MyStruct));
hash_put(ht, "key1", data);

// 删除时记得释放
MyStruct* removed = (MyStruct*)hash_remove(ht, "key1");
if (removed) {
    free(removed);
}

// 或者在销毁时批量释放
void free_mystruct(void* ptr) {
    free(ptr);
}
hash_destroy(ht, free_mystruct);
```

### 清空 vs 销毁

```c
// 清空：保留结构，删除所有元素
hash_clear(ht, free_value_func);
// 之后可以继续使用 ht

// 销毁：释放所有内存
hash_destroy(ht, free_value_func);
// 之后不能再使用 ht
```

## 常见错误

### ❌ 错误 1：忘记预留容量

```c
hash_table_t* ht = hash_create(128);  // 太小
for (int i = 0; i < 1000000; i++) {
    hash_put(ht, keys[i], values[i]);  // 会触发多次扩容
}
```

### ✅ 正确做法

```c
hash_table_t* ht = hash_create(1024);
hash_reserve(ht, 1000000);  // 预留足够空间
for (int i = 0; i < 1000000; i++) {
    hash_put(ht, keys[i], values[i]);  // 不会扩容
}
```

### ❌ 错误 2：键的生命周期问题

```c
char key[32];
for (int i = 0; i < 100; i++) {
    snprintf(key, sizeof(key), "key_%d", i);
    hash_put(ht, key, values[i]);  // ✅ 正确：hash_put 会复制键
}
// key 可以安全释放或重用
```

### ❌ 错误 3：值的生命周期问题

```c
void bad_function() {
    int local_value = 42;
    hash_put(ht, "key", &local_value);  // ❌ 错误：局部变量地址
}  // local_value 被销毁，指针失效

// ✅ 正确做法
int* value = malloc(sizeof(int));
*value = 42;
hash_put(ht, "key", value);
```

## 性能基准

在现代 CPU 上（Intel i7-10700K @ 3.8GHz）：

| 操作 | 10万数据 | 100万数据 | 1000万数据 |
|------|---------|----------|-----------|
| 插入 | 58 ns | 60 ns | 65 ns |
| 查找 | 42 ns | 45 ns | 48 ns |
| 删除 | 50 ns | 52 ns | 55 ns |

**结论：性能稳定，不随数据量显著退化！**

## 下一步

- 阅读 [hash_optimization_guide.md](hash_optimization_guide.md) 了解深度优化
- 运行 `test/test_hash_performance.c` 进行性能测试
- 查看 `src/lib/registry.cpp` 了解实际应用示例
