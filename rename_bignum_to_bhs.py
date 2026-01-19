#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
BigNum → BHS 重命名脚本

功能：
1. 将所有 .c 和 .h 文件中的 BigNum 类型名替换为 BHS
2. 保持函数名 bignum_* 和宏定义 BIGNUM_* 不变
3. 生成详细的替换报告
"""

import os
import re
from pathlib import Path

# 配置
SRC_DIR = Path(__file__).parent / "src"
EXCLUDE_FILES = {
    "share/obj.h",      # 已手动修改
    "bignum.h",         # 已手动修改
}

# 统计
stats = {
    "files_processed": 0,
    "files_modified": 0,
    "total_replacements": 0,
    "files_detail": []
}

def should_process_file(file_path: Path) -> bool:
    """判断文件是否需要处理"""
    # 只处理 .c 和 .h 文件
    if file_path.suffix not in ['.c', '.h']:
        return False
    
    # 排除已处理的文件
    rel_path = file_path.relative_to(SRC_DIR)
    if str(rel_path).replace('\\', '/') in EXCLUDE_FILES:
        return False
    
    return True

def replace_bignum_in_content(content: str) -> tuple[str, int]:
    """
    替换内容中的 BigNum 为 BHS
    
    规则：
    1. 只替换类型名 BigNum（单词边界）
    2. 不替换函数名 bignum_*
    3. 不替换宏定义 BIGNUM_*
    4. 不替换字符串字面量中的内容
    
    返回：(新内容, 替换次数)
    """
    # 使用正则表达式替换
    # \b 表示单词边界，确保只匹配完整的 BigNum
    pattern = r'\bBigNum\b'
    
    new_content, count = re.subn(pattern, 'BHS', content)
    
    return new_content, count

def process_file(file_path: Path) -> int:
    """
    处理单个文件
    
    返回：替换次数
    """
    try:
        # 读取文件内容
        with open(file_path, 'r', encoding='utf-8') as f:
            original_content = f.read()
        
        # 执行替换
        new_content, count = replace_bignum_in_content(original_content)
        
        # 如果有修改，写回文件
        if count > 0:
            with open(file_path, 'w', encoding='utf-8') as f:
                f.write(new_content)
            
            stats["files_modified"] += 1
            stats["total_replacements"] += count
            
            # 记录详情
            rel_path = file_path.relative_to(SRC_DIR)
            stats["files_detail"].append({
                "file": str(rel_path),
                "replacements": count
            })
            
            print(f"✓ {rel_path}: {count} 处替换")
        
        return count
        
    except Exception as e:
        print(f"✗ 处理文件 {file_path} 时出错: {e}")
        return 0

def main():
    """主函数"""
    print("=" * 60)
    print("BigNum → BHS 重命名脚本")
    print("=" * 60)
    print()
    
    # 检查目录
    if not SRC_DIR.exists():
        print(f"错误: 源代码目录不存在: {SRC_DIR}")
        return
    
    print(f"源代码目录: {SRC_DIR}")
    print(f"排除文件: {', '.join(EXCLUDE_FILES)}")
    print()
    print("开始处理...")
    print("-" * 60)
    
    # 遍历所有文件
    for file_path in SRC_DIR.rglob("*"):
        if file_path.is_file() and should_process_file(file_path):
            stats["files_processed"] += 1
            process_file(file_path)
    
    # 输出统计报告
    print("-" * 60)
    print()
    print("=" * 60)
    print("替换完成！")
    print("=" * 60)
    print()
    print(f"📊 统计信息:")
    print(f"  - 处理文件数: {stats['files_processed']}")
    print(f"  - 修改文件数: {stats['files_modified']}")
    print(f"  - 总替换次数: {stats['total_replacements']}")
    print()
    
    if stats["files_detail"]:
        print("📝 详细信息:")
        # 按替换次数排序
        sorted_details = sorted(stats["files_detail"], 
                               key=lambda x: x["replacements"], 
                               reverse=True)
        for detail in sorted_details:
            print(f"  - {detail['file']}: {detail['replacements']} 处")
        print()
    
    print("✅ 所有替换已完成！")
    print()
    print("下一步:")
    print("  1. 编译测试: cd src && make clean && make")
    print("  2. 检查错误: 如有编译错误，请检查替换结果")
    print("  3. 运行测试: make test && ./logex_repl")

if __name__ == "__main__":
    main()
