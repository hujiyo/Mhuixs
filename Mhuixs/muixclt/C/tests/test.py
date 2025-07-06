#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Mhuixs客户端自动测试程序
版权所有 (c) HuJi 2024
"""

import os
import sys
import subprocess
import glob
import time
import re
from pathlib import Path

class MuixcltTestRunner:
    def __init__(self):
        self.script_dir = Path(__file__).parent.parent  # 上级目录（C文件夹）
        self.tests_dir = Path(__file__).parent  # 当前目录（tests文件夹）
        self.muixclt_source = self.script_dir / "muixclt.c"
        self.muixclt_binary = self.script_dir / "muixclt"
        self.makefile = self.script_dir / "Makefile"
        
    def enable_debug_mode(self):
        """启用muixclt.c中的DEBUG_MODE"""
        print("🔧 启用DEBUG模式...")
        
        if not self.muixclt_source.exists():
            print(f"❌ 错误: 找不到源文件 {self.muixclt_source}")
            return False
            
        # 读取源文件
        with open(self.muixclt_source, 'r', encoding='utf-8') as f:
            content = f.read()
        
        # 检查是否已经启用DEBUG模式
        if '#define DEBUG_MODE 1' in content:
            print("✅ DEBUG模式已启用")
            return True
        
        # 启用DEBUG模式
        content = re.sub(r'#define DEBUG_MODE 0', '#define DEBUG_MODE 1', content)
        
        # 写回文件
        with open(self.muixclt_source, 'w', encoding='utf-8') as f:
            f.write(content)
        
        print("✅ DEBUG模式已启用")
        return True
    
    def compile_muixclt(self):
        """编译muixclt"""
        print("🔨 编译muixclt...")
        
        try:
            # 先清理
            result = subprocess.run(['make', 'clean'], 
                                  cwd=self.script_dir, 
                                  capture_output=True, 
                                  text=True)
            
            # 编译
            result = subprocess.run(['make'], 
                                  cwd=self.script_dir, 
                                  capture_output=True, 
                                  text=True)
            
            if result.returncode != 0:
                print(f"❌ 编译失败:")
                print(result.stderr)
                return False
            
            if not self.muixclt_binary.exists():
                print(f"❌ 编译后未找到可执行文件 {self.muixclt_binary}")
                return False
            
            print("✅ 编译成功")
            return True
            
        except Exception as e:
            print(f"❌ 编译过程中发生错误: {e}")
            return False
    
    def get_test_files(self):
        """获取所有测试文件"""
        if not self.tests_dir.exists():
            print(f"❌ 错误: 测试目录不存在 {self.tests_dir}")
            return []
        
        test_files = list(self.tests_dir.glob("*.naql"))
        test_files.sort()  # 按文件名排序
        
        print(f"📁 找到 {len(test_files)} 个测试文件:")
        for f in test_files:
            print(f"   - {f.name}")
        
        return test_files
    
    def run_test_file(self, test_file):
        """运行单个测试文件"""
        print(f"\n🧪 运行测试: {test_file.name}")
        print("=" * 50)
        
        try:
            # 运行muixclt，传入测试文件
            result = subprocess.run([str(self.muixclt_binary), '-f', str(test_file)], 
                                  cwd=self.script_dir,
                                  capture_output=True, 
                                  text=True,
                                  timeout=30)  # 30秒超时
            
            # 显示输出
            if result.stdout:
                print("📤 标准输出:")
                print(result.stdout)
            
            if result.stderr:
                print("📤 错误输出:")
                print(result.stderr)
            
            if result.returncode == 0:
                print(f"✅ 测试 {test_file.name} 通过")
                return True
            else:
                print(f"❌ 测试 {test_file.name} 失败 (返回码: {result.returncode})")
                return False
                
        except subprocess.TimeoutExpired:
            print(f"⏰ 测试 {test_file.name} 超时")
            return False
        except Exception as e:
            print(f"❌ 运行测试时发生错误: {e}")
            return False
    
    def run_all_tests(self):
        """运行所有测试"""
        print("🚀 开始自动测试...")
        print("=" * 60)
        
        # 1. 启用DEBUG模式
        if not self.enable_debug_mode():
            return False
        
        # 2. 编译
        if not self.compile_muixclt():
            return False
        
        # 3. 获取测试文件
        test_files = self.get_test_files()
        if not test_files:
            print("❌ 没有找到测试文件")
            return False
        
        # 4. 运行所有测试
        passed = 0
        failed = 0
        
        for test_file in test_files:
            if self.run_test_file(test_file):
                passed += 1
            else:
                failed += 1
            
            # 测试之间稍微等待
            time.sleep(0.5)
        
        # 5. 显示总结
        print("\n" + "=" * 60)
        print("📊 测试总结:")
        print(f"   总计: {len(test_files)} 个测试")
        print(f"   通过: {passed} 个")
        print(f"   失败: {failed} 个")
        
        if failed == 0:
            print("🎉 所有测试都通过了!")
            return True
        else:
            print("❌ 部分测试失败")
            return False
    
    def interactive_test(self):
        """交互式测试模式"""
        print("🎮 交互式测试模式")
        print("=" * 40)
        
        # 启用DEBUG模式并编译
        if not self.enable_debug_mode() or not self.compile_muixclt():
            return
        
        test_files = self.get_test_files()
        if not test_files:
            print("❌ 没有找到测试文件")
            return
        
        while True:
            print("\n选择测试文件:")
            for i, f in enumerate(test_files, 1):
                print(f"  {i}. {f.name}")
            print("  0. 退出")
            print("  a. 运行所有测试")
            
            try:
                choice = input("\n请选择 (0-{}/a): ".format(len(test_files))).strip().lower()
                
                if choice == '0':
                    break
                elif choice == 'a':
                    self.run_all_tests()
                    break
                else:
                    idx = int(choice) - 1
                    if 0 <= idx < len(test_files):
                        self.run_test_file(test_files[idx])
                    else:
                        print("❌ 无效选择")
                        
            except (ValueError, KeyboardInterrupt):
                print("\n👋 再见!")
                break

def main():
    runner = MuixcltTestRunner()
    
    if len(sys.argv) > 1:
        if sys.argv[1] == '--interactive' or sys.argv[1] == '-i':
            runner.interactive_test()
        elif sys.argv[1] == '--help' or sys.argv[1] == '-h':
            print("Mhuixs客户端自动测试程序")
            print("用法:")
            print("  python3 test_runner.py           # 运行所有测试")
            print("  python3 test_runner.py -i        # 交互式模式")
            print("  python3 test_runner.py -h        # 显示帮助")
        else:
            print("❌ 未知参数，使用 -h 查看帮助")
    else:
        runner.run_all_tests()

if __name__ == "__main__":
    main() 