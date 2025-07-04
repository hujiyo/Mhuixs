#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
版权所有 (c) Mhuixs-team 2024
许可证协议:
任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
Email:hj18914255909@outlook.com

Mhuixs 数据库客户端 - Python版本
"""

import sys
import os
import ssl
import socket
import struct
import time
import zlib
import json
import argparse
import readline
import signal
import threading
from typing import Optional, Dict, Any, List, Tuple
from dataclasses import dataclass
from enum import IntEnum


class PacketType(IntEnum):
    """包类型枚举"""
    QUERY = 0x0001
    RESPONSE = 0x0002
    ERROR = 0x0003
    HEARTBEAT = 0x0004
    AUTH = 0x0005
    AUTH_RESP = 0x0006
    DISCONNECT = 0x0007
    KEEPALIVE = 0x0008


class PacketStatus(IntEnum):
    """包状态枚举"""
    OK = 0x0000
    ERROR = 0x0001
    PENDING = 0x0002
    TIMEOUT = 0x0003
    AUTH_FAILED = 0x0004
    PERMISSION_DENIED = 0x0005
    INVALID_QUERY = 0x0006
    SERVER_ERROR = 0x0007


@dataclass
class PacketHeader:
    """包头结构"""
    magic: int = 0x4D485558  # "MHUX"
    version: int = 0x0100    # 版本 1.0
    type: int = PacketType.QUERY
    status: int = PacketStatus.PENDING
    sequence: int = 0
    data_length: int = 0


@dataclass
class Packet:
    """数据包结构"""
    header: PacketHeader
    data: bytes = b''
    checksum: int = 0


class MhuixsClient:
    """Mhuixs客户端类"""
    
    def __init__(self, host: str = "127.0.0.1", port: int = 18482):
        self.host = host
        self.port = port
        self.socket = None
        self.ssl_context = None
        self.ssl_socket = None
        self.connected = False
        self.verbose = False
        self.sequence = 0
        self.running = True
        
        # 初始化SSL上下文
        self._init_ssl()
        
        # 设置信号处理
        signal.signal(signal.SIGINT, self._signal_handler)
        signal.signal(signal.SIGTERM, self._signal_handler)
    
    def _init_ssl(self):
        """初始化SSL上下文"""
        self.ssl_context = ssl.create_default_context()
        # 开发环境不验证证书
        self.ssl_context.check_hostname = False
        self.ssl_context.verify_mode = ssl.CERT_NONE
    
    def _signal_handler(self, signum, frame):
        """信号处理函数"""
        print("\n\n接收到中断信号，正在清理资源...")
        self.running = False
        self.disconnect()
        sys.exit(0)
    
    def _get_next_sequence(self) -> int:
        """获取下一个序列号"""
        self.sequence += 1
        return self.sequence
    
    def _calculate_checksum(self, data: bytes) -> int:
        """计算校验和"""
        return zlib.crc32(data) & 0xFFFFFFFF
    
    def _create_packet(self, packet_type: PacketType, status: PacketStatus, 
                      data: bytes = b'') -> Packet:
        """创建数据包"""
        header = PacketHeader(
            magic=0x4D485558,
            version=0x0100,
            type=packet_type,
            status=status,
            sequence=self._get_next_sequence(),
            data_length=len(data)
        )
        
        packet = Packet(header=header, data=data)
        
        # 计算校验和
        header_bytes = self._serialize_header(header)
        packet.checksum = self._calculate_checksum(header_bytes + data)
        
        return packet
    
    def _serialize_header(self, header: PacketHeader) -> bytes:
        """序列化包头"""
        return struct.pack('>IHHHI', 
                          header.magic, 
                          header.version, 
                          header.type, 
                          header.status, 
                          header.sequence) + struct.pack('>I', header.data_length)
    
    def _deserialize_header(self, data: bytes) -> PacketHeader:
        """反序列化包头"""
        if len(data) < 16:
            raise ValueError("包头数据不足")
        
        magic, version, pkt_type, status, sequence, data_length = struct.unpack('>IHHHI', data[:16])
        
        return PacketHeader(
            magic=magic,
            version=version,
            type=pkt_type,
            status=status,
            sequence=sequence,
            data_length=data_length
        )
    
    def _serialize_packet(self, packet: Packet) -> bytes:
        """序列化数据包"""
        header_bytes = self._serialize_header(packet.header)
        checksum_bytes = struct.pack('>I', packet.checksum)
        return header_bytes + packet.data + checksum_bytes
    
    def _deserialize_packet(self, data: bytes) -> Packet:
        """反序列化数据包"""
        if len(data) < 20:  # 最小包大小
            raise ValueError("数据包过小")
        
        # 解析包头
        header = self._deserialize_header(data[:16])
        
        # 验证魔数和版本
        if header.magic != 0x4D485558:
            raise ValueError(f"无效的魔数: 0x{header.magic:08X}")
        
        if header.version != 0x0100:
            raise ValueError(f"不支持的版本: 0x{header.version:04X}")
        
        # 解析数据和校验和
        packet_data = data[16:16+header.data_length]
        checksum = struct.unpack('>I', data[16+header.data_length:16+header.data_length+4])[0]
        
        # 验证校验和
        calculated_checksum = self._calculate_checksum(data[:16+header.data_length])
        if checksum != calculated_checksum:
            raise ValueError(f"校验和不匹配: 期望 0x{checksum:08X}, 实际 0x{calculated_checksum:08X}")
        
        return Packet(header=header, data=packet_data, checksum=checksum)
    
    def connect(self) -> bool:
        """连接到服务器"""
        if self.connected:
            print("已经连接到服务器")
            return True
        
        try:
            print(f"正在连接到 {self.host}:{self.port}...")
            
            # 创建TCP套接字
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.connect((self.host, self.port))
            
            # 创建SSL连接
            self.ssl_socket = self.ssl_context.wrap_socket(self.socket, server_hostname=self.host)
            
            self.connected = True
            print(f"✓ 已成功连接到 Mhuixs 服务器")
            return True
            
        except Exception as e:
            print(f"连接失败: {e}")
            self._cleanup_connection()
            return False
    
    def disconnect(self):
        """断开连接"""
        if not self.connected:
            return
        
        print("正在断开连接...")
        self.connected = False
        self._cleanup_connection()
        print("✓ 已断开连接")
    
    def _cleanup_connection(self):
        """清理连接资源"""
        if self.ssl_socket:
            try:
                self.ssl_socket.close()
            except:
                pass
            self.ssl_socket = None
        
        if self.socket:
            try:
                self.socket.close()
            except:
                pass
            self.socket = None
    
    def send_query(self, query: str) -> bool:
        """发送查询"""
        if not self.connected or not self.ssl_socket:
            print("错误: 未连接到服务器")
            return False
        
        try:
            if self.verbose:
                print(f"发送查询: {query}")
            
            # 创建查询包
            packet = self._create_packet(PacketType.QUERY, PacketStatus.PENDING, query.encode('utf-8'))
            
            # 序列化并发送
            packet_bytes = self._serialize_packet(packet)
            self.ssl_socket.send(packet_bytes)
            
            return True
            
        except Exception as e:
            print(f"发送查询失败: {e}")
            return False
    
    def receive_response(self) -> Optional[str]:
        """接收响应"""
        if not self.connected or not self.ssl_socket:
            print("错误: 未连接到服务器")
            return None
        
        try:
            # 接收包头
            header_data = self._receive_exactly(16)
            if not header_data:
                return None
            
            header = self._deserialize_header(header_data)
            
            # 接收数据和校验和
            remaining_data = self._receive_exactly(header.data_length + 4)
            if not remaining_data:
                return None
            
            # 反序列化完整数据包
            full_packet_data = header_data + remaining_data
            packet = self._deserialize_packet(full_packet_data)
            
            if self.verbose:
                print(f"接收响应: {packet.data.decode('utf-8', errors='ignore')}")
            
            return packet.data.decode('utf-8', errors='ignore')
            
        except Exception as e:
            print(f"接收响应失败: {e}")
            return None
    
    def _receive_exactly(self, size: int) -> bytes:
        """精确接收指定字节数"""
        data = b''
        while len(data) < size:
            chunk = self.ssl_socket.recv(size - len(data))
            if not chunk:
                break
            data += chunk
        return data
    
    def print_logo(self):
        """打印Logo"""
        logo = """
        ███╗   ███╗██╗  ██╗██╗   ██╗██╗██╗  ██╗███████╗
        ████╗ ████║██║  ██║██║   ██║██║╚██╗██╔╝██╔════╝
        ██╔████╔██║███████║██║   ██║██║ ╚███╔╝ ███████╗
        ██║╚██╔╝██║██╔══██║██║   ██║██║ ██╔██╗ ╚════██║
        ██║ ╚═╝ ██║██║  ██║╚██████╔╝██║██╔╝ ██╗███████║
        ╚═╝     ╚═╝╚═╝  ╚═╝ ╚═════╝ ╚═╝╚═╝  ╚═╝╚══════╝
        """
        print(logo)
        print("        Mhuixs 数据库客户端 - Python版本")
        print("        基于内存的高性能数据库")
        print()
    
    def print_help(self):
        """打印帮助信息"""
        print("Mhuixs 客户端帮助:")
        print("  \\q, \\quit              退出客户端")
        print("  \\h, \\help              显示帮助信息")
        print("  \\c, \\connect           连接到服务器")
        print("  \\d, \\disconnect        断开连接")
        print("  \\s, \\status            显示连接状态")
        print("  \\v, \\verbose           切换详细模式")
        print("  \\clear                 清屏")
        print()
        print("NAQL 查询示例:")
        print("  HOOK TABLE users;           # 创建表")
        print("  FIELD ADD name str;         # 添加字段")
        print("  ADD '张三' 25;             # 添加数据")
        print("  GET;                        # 查询所有数据")
        print("  WHERE name == '张三';       # 条件查询")
        print()
    
    def interactive_mode(self):
        """交互模式"""
        self.print_logo()
        print(f"服务器地址: {self.host}:{self.port}")
        print("输入 \\h 获取帮助信息，输入 \\q 退出")
        print("----------------------------------------")
        
        # 自动连接
        if not self.connect():
            print("警告: 无法连接到服务器，请使用 \\c 命令手动连接")
        
        while self.running:
            try:
                # 设置提示符
                if self.connected:
                    prompt = "mhuixs> "
                else:
                    prompt = "mhuixs(断开)> "
                
                user_input = input(prompt).strip()
                
                if not user_input:
                    continue
                
                # 处理内置命令
                if user_input.startswith('\\'):
                    self._handle_builtin_command(user_input)
                else:
                    # 处理NAQL查询
                    if self.connected:
                        if self.send_query(user_input):
                            response = self.receive_response()
                            if response:
                                print(response)
                    else:
                        print("错误: 未连接到服务器，请使用 \\c 命令连接")
                        
            except KeyboardInterrupt:
                print("\n使用 \\q 退出客户端")
                continue
            except EOFError:
                print("\n再见!")
                break
        
        self.disconnect()
    
    def _handle_builtin_command(self, command: str):
        """处理内置命令"""
        cmd = command.lower()
        
        if cmd in ['\\q', '\\quit']:
            print("再见!")
            self.running = False
        elif cmd in ['\\h', '\\help']:
            self.print_help()
        elif cmd in ['\\c', '\\connect']:
            self.connect()
        elif cmd in ['\\d', '\\disconnect']:
            self.disconnect()
        elif cmd in ['\\s', '\\status']:
            print(f"连接状态: {'已连接' if self.connected else '未连接'}")
            if self.connected:
                print(f"服务器: {self.host}:{self.port}")
        elif cmd in ['\\v', '\\verbose']:
            self.verbose = not self.verbose
            print(f"详细模式: {'开启' if self.verbose else '关闭'}")
        elif cmd == '\\clear':
            os.system('clear' if os.name == 'posix' else 'cls')
        else:
            print(f"未知命令: {command}")
    
    def batch_mode(self, filename: str):
        """批处理模式"""
        try:
            with open(filename, 'r', encoding='utf-8') as f:
                lines = f.readlines()
        except FileNotFoundError:
            print(f"错误: 文件 {filename} 不存在")
            return
        except Exception as e:
            print(f"错误: 读取文件失败 - {e}")
            return
        
        print(f"正在执行批处理文件: {filename}")
        
        if not self.connect():
            print("错误: 无法连接到服务器")
            return
        
        for line_num, line in enumerate(lines, 1):
            line = line.strip()
            
            # 跳过空行和注释
            if not line or line.startswith('#'):
                continue
            
            print(f"执行第 {line_num} 行: {line}")
            
            if self.send_query(line):
                response = self.receive_response()
                if response:
                    print(f"结果: {response}")
            else:
                print("查询发送失败")
        
        print("批处理完成")


def main():
    """主函数"""
    parser = argparse.ArgumentParser(description='Mhuixs 数据库客户端 - Python版本')
    parser.add_argument('-s', '--server', default='127.0.0.1', help='服务器IP地址')
    parser.add_argument('-p', '--port', type=int, default=18482, help='服务器端口')
    parser.add_argument('-f', '--file', help='批处理文件')
    parser.add_argument('-v', '--verbose', action='store_true', help='详细模式')
    parser.add_argument('--version', action='version', version='Mhuixs客户端 v1.0.0')
    
    args = parser.parse_args()
    
    # 创建客户端
    client = MhuixsClient(args.server, args.port)
    client.verbose = args.verbose
    
    if args.file:
        # 批处理模式
        client.batch_mode(args.file)
    else:
        # 交互模式
        client.interactive_mode()


if __name__ == "__main__":
    main()
