#include "../src/lib/hash.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

/* 性能测试工具 */
typedef struct {
    clock_t start;
    clock_t end;
} timer_t;

static inline void timer_start(timer_t* t) {
    t->start = clock();
}

static inline double timer_end(timer_t* t) {
    t->end = clock();
    return (double)(t->end - t->start) / CLOCKS_PER_SEC;
}

/* 测试：插入性能 */
void test_insert_performance(int num_items) {
    printf("\n=== Insert Performance Test (%d items) ===\n", num_items);
    
    hash_table_t* ht = hash_create(1024);
    hash_reserve(ht, num_items);
    
    timer_t timer;
    timer_start(&timer);
    
    for (int i = 0; i < num_items; i++) {
        char key[32];
        snprintf(key, sizeof(key), "user_%d", i);
        hash_put(ht, key, (void*)(intptr_t)i);
    }
    
    double elapsed = timer_end(&timer);
    
    printf("Time: %.3f s\n", elapsed);
    printf("Throughput: %.0f ops/s\n", num_items / elapsed);
    printf("Avg time per insert: %.0f ns\n", elapsed * 1e9 / num_items);
    
    hash_print_stats(ht);
    hash_destroy(ht, NULL);
}

/* 测试：查找性能 */
void test_lookup_performance(int num_items) {
    printf("\n=== Lookup Performance Test (%d items) ===\n", num_items);
    
    hash_table_t* ht = hash_create(1024);
    hash_reserve(ht, num_items);
    
    /* 先插入数据 */
    for (int i = 0; i < num_items; i++) {
        char key[32];
        snprintf(key, sizeof(key), "user_%d", i);
        hash_put(ht, key, (void*)(intptr_t)i);
    }
    
    /* 测试查找 */
    timer_t timer;
    timer_start(&timer);
    
    int found = 0;
    for (int i = 0; i < num_items; i++) {
        char key[32];
        snprintf(key, sizeof(key), "user_%d", i);
        void* value = hash_get(ht, key);
        if (value) found++;
    }
    
    double elapsed = timer_end(&timer);
    
    printf("Time: %.3f s\n", elapsed);
    printf("Throughput: %.0f ops/s\n", num_items / elapsed);
    printf("Avg time per lookup: %.0f ns\n", elapsed * 1e9 / num_items);
    printf("Found: %d / %d\n", found, num_items);
    
    hash_print_stats(ht);
    hash_destroy(ht, NULL);
}

/* 测试：删除性能 */
void test_delete_performance(int num_items) {
    printf("\n=== Delete Performance Test (%d items) ===\n", num_items);
    
    hash_table_t* ht = hash_create(1024);
    hash_reserve(ht, num_items);
    
    /* 先插入数据 */
    for (int i = 0; i < num_items; i++) {
        char key[32];
        snprintf(key, sizeof(key), "user_%d", i);
        hash_put(ht, key, (void*)(intptr_t)i);
    }
    
    /* 测试删除 */
    timer_t timer;
    timer_start(&timer);
    
    int deleted = 0;
    for (int i = 0; i < num_items; i += 2) {  /* 删除一半 */
        char key[32];
        snprintf(key, sizeof(key), "user_%d", i);
        void* value = hash_remove(ht, key);
        if (value) deleted++;
    }
    
    double elapsed = timer_end(&timer);
    
    printf("Time: %.3f s\n", elapsed);
    printf("Throughput: %.0f ops/s\n", deleted / elapsed);
    printf("Avg time per delete: %.0f ns\n", elapsed * 1e9 / deleted);
    printf("Deleted: %d\n", deleted);
    
    hash_print_stats(ht);
    hash_destroy(ht, NULL);
}

/* 测试：混合操作 */
void test_mixed_operations(int num_items) {
    printf("\n=== Mixed Operations Test (%d items) ===\n", num_items);
    
    hash_table_t* ht = hash_create(1024);
    
    timer_t timer;
    timer_start(&timer);
    
    /* 50% 插入，30% 查找，20% 删除 */
    for (int i = 0; i < num_items; i++) {
        int op = rand() % 100;
        char key[32];
        snprintf(key, sizeof(key), "key_%d", rand() % (num_items / 2));
        
        if (op < 50) {
            /* 插入 */
            hash_put(ht, key, (void*)(intptr_t)i);
        } else if (op < 80) {
            /* 查找 */
            hash_get(ht, key);
        } else {
            /* 删除 */
            hash_remove(ht, key);
        }
    }
    
    double elapsed = timer_end(&timer);
    
    printf("Time: %.3f s\n", elapsed);
    printf("Throughput: %.0f ops/s\n", num_items / elapsed);
    printf("Avg time per op: %.0f ns\n", elapsed * 1e9 / num_items);
    
    hash_print_stats(ht);
    hash_destroy(ht, NULL);
}

/* 测试：内存占用 */
void test_memory_usage() {
    printf("\n=== Memory Usage Test ===\n");
    
    int sizes[] = {1000, 10000, 100000, 1000000};
    
    for (int i = 0; i < 4; i++) {
        int num_items = sizes[i];
        hash_table_t* ht = hash_create(1024);
        hash_reserve(ht, num_items);
        
        /* 插入数据 */
        for (int j = 0; j < num_items; j++) {
            char key[32];
            snprintf(key, sizeof(key), "user_%d", j);
            hash_put(ht, key, (void*)(intptr_t)j);
        }
        
        /* 估算内存占用 */
        size_t entry_size = sizeof(hash_entry_t);
        size_t total_memory = hash_capacity(ht) * entry_size;
        size_t used_memory = hash_size(ht) * entry_size;
        
        printf("\n%d items:\n", num_items);
        printf("  Capacity: %u\n", hash_capacity(ht));
        printf("  Load factor: %.2f\n", hash_load_factor(ht));
        printf("  Total memory: %.2f MB\n", total_memory / 1024.0 / 1024.0);
        printf("  Used memory: %.2f MB\n", used_memory / 1024.0 / 1024.0);
        printf("  Bytes per item: %.1f\n", (double)used_memory / num_items);
        
        hash_destroy(ht, NULL);
    }
}

/* 测试：不同键长度的性能 */
void test_key_length_performance() {
    printf("\n=== Key Length Performance Test ===\n");
    
    int num_items = 100000;
    int key_lengths[] = {8, 16, 24, 32, 64, 128};
    
    for (int i = 0; i < 6; i++) {
        int key_len = key_lengths[i];
        hash_table_t* ht = hash_create(1024);
        hash_reserve(ht, num_items);
        
        /* 生成指定长度的键 */
        char* key = (char*)malloc(key_len + 1);
        memset(key, 'x', key_len);
        key[key_len] = '\0';
        
        timer_t timer;
        timer_start(&timer);
        
        for (int j = 0; j < num_items; j++) {
            /* 修改键的最后几个字符 */
            snprintf(key + key_len - 8, 9, "%08d", j);
            hash_put(ht, key, (void*)(intptr_t)j);
        }
        
        double elapsed = timer_end(&timer);
        
        printf("\nKey length %d:\n", key_len);
        printf("  Insert time: %.0f ns/op\n", elapsed * 1e9 / num_items);
        printf("  Inline storage: %s\n", 
               key_len <= HASH_INLINE_KEY_SIZE ? "YES" : "NO");
        
        free(key);
        hash_destroy(ht, NULL);
    }
}

int main() {
    printf("===========================================\n");
    printf("  Hash Table Performance Benchmark\n");
    printf("===========================================\n");
    
    srand(time(NULL));
    
    /* 基础性能测试 */
    test_insert_performance(10000);
    test_insert_performance(100000);
    test_insert_performance(1000000);
    
    test_lookup_performance(10000);
    test_lookup_performance(100000);
    test_lookup_performance(1000000);
    
    test_delete_performance(10000);
    test_delete_performance(100000);
    
    /* 混合操作测试 */
    test_mixed_operations(100000);
    
    /* 内存占用测试 */
    test_memory_usage();
    
    /* 键长度性能测试 */
    test_key_length_performance();
    
    printf("\n===========================================\n");
    printf("  All tests completed!\n");
    printf("===========================================\n");
    
    return 0;
}
