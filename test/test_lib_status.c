#include <stdio.h>
#include "list.h"
#include "bitmap.h"
#include "tblh.h"

// 测试 LIST 基础功能
int test_list() {
    printf("=== 测试 LIST ===\n");
    
    LIST* lst = list_create();
    if (!lst) {
        printf("❌ list_create 失败\n");
        return -1;
    }
    printf("✅ list_create 成功\n");
    
    // 测试 rpush
    int ret = list_rpush(lst, (Obj)(intptr_t)100);
    if (ret != 0) {
        printf("❌ list_rpush 失败\n");
        free_list(lst);
        return -1;
    }
    printf("✅ list_rpush 成功\n");
    
    // 测试 size
    size_t size = list_size(lst);
    if (size != 1) {
        printf("❌ list_size 错误: %zu (期望 1)\n", size);
        free_list(lst);
        return -1;
    }
    printf("✅ list_size 正确: %zu\n", size);
    
    // 测试 get
    Obj val = list_get_index(lst, 0);
    if ((intptr_t)val != 100) {
        printf("❌ list_get_index 错误: %ld (期望 100)\n", (intptr_t)val);
        free_list(lst);
        return -1;
    }
    printf("✅ list_get_index 正确: %ld\n", (intptr_t)val);
    
    free_list(lst);
    printf("✅ LIST 基础功能正常\n\n");
    return 0;
}

// 测试 BITMAP 基础功能
int test_bitmap() {
    printf("=== 测试 BITMAP ===\n");
    
    BigNum* bm = bitmap_create_with_size(100);
    if (!bm) {
        printf("❌ bitmap_create_with_size 失败\n");
        return -1;
    }
    printf("✅ bitmap_create_with_size 成功\n");
    
    // 测试类型检查
    if (!check_if_bitmap(bm)) {
        printf("❌ check_if_bitmap 失败\n");
        free_bitmap(bm);
        return -1;
    }
    printf("✅ check_if_bitmap 正确\n");
    
    // 测试 set
    int ret = bitmap_set(bm, 10, 1);
    if (ret != 0) {
        printf("❌ bitmap_set 失败\n");
        free_bitmap(bm);
        return -1;
    }
    printf("✅ bitmap_set 成功\n");
    
    // 测试 get
    int bit = bitmap_get(bm, 10);
    if (bit != 1) {
        printf("❌ bitmap_get 错误: %d (期望 1)\n", bit);
        free_bitmap(bm);
        return -1;
    }
    printf("✅ bitmap_get 正确: %d\n", bit);
    
    free_bitmap(bm);
    printf("✅ BITMAP 基础功能正常\n\n");
    return 0;
}

// 测试 TABLE 基础功能
int test_table() {
    printf("=== 测试 TABLE ===\n");
    
    // 准备字段类型和名称
    int types[] = {1, 2};  // 假设类型
    mstring field_names[2];
    field_names[0] = mstr("id");
    field_names[1] = mstr("name");
    mstring table_name = mstr("users");
    
    TABLE* table = create_table(types, field_names, 2, table_name);
    if (!table) {
        printf("❌ create_table 失败\n");
        return -1;
    }
    printf("✅ create_table 成功\n");
    
    // 测试添加记录
    Obj values[2];
    values[0] = (Obj)(intptr_t)1;
    values[1] = (Obj)(intptr_t)100;
    
    int ret = add_record(table, values, 2);
    if (ret != 0) {
        printf("❌ add_record 失败\n");
        free_table(table);
        return -1;
    }
    printf("✅ add_record 成功\n");
    
    // 测试记录数
    size_t count = get_record_count(table);
    if (count != 1) {
        printf("❌ get_record_count 错误: %zu (期望 1)\n", count);
        free_table(table);
        return -1;
    }
    printf("✅ get_record_count 正确: %zu\n", count);
    
    free_table(table);
    printf("✅ TABLE 基础功能正常\n\n");
    return 0;
}

int main() {
    printf("========================================\n");
    printf("Mhuixs lib/ 数据结构测试\n");
    printf("========================================\n\n");
    
    int failed = 0;
    
    if (test_list() != 0) failed++;
    if (test_bitmap() != 0) failed++;
    if (test_table() != 0) failed++;
    
    printf("========================================\n");
    if (failed == 0) {
        printf("✅ 所有测试通过！\n");
    } else {
        printf("❌ %d 个测试失败\n", failed);
    }
    printf("========================================\n");
    
    return failed;
}
