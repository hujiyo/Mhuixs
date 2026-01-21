/* SPDX-License-Identifier: MIT */
/*
 * bitcpy.h - 位级别内存拷贝库头文件
 *
 * 本库提供高效的位级别内存拷贝功能,支持任意位偏移的拷贝操作。
 * 适用于需要精确位操作的场景,如压缩算法、网络协议等。
 *
 * Copyright (C) 2024-2025 hujiyo
 * Author: hujiyo <Mhuixs.db@outlook.com>
 *
 * Repository: https://github.com/hujiyo/bitcpy
 *
 * 主要功能:
 *   - 支持任意位偏移的内存拷贝 (0-7位)
 *   - 自动优化对齐拷贝,使用 memcpy 加速
 *   - 支持自拷贝 (源和目标可以是同一区域)
 *   - 高效的三阶段处理: 前导位对齐 + 64位块 + 尾部处理
 *
 * 性能特性:
 *   - 字节对齐时直接使用 memcpy
 *   - 64位块处理利用 CPU 64位加载
 *   - 预计算的位掩码表避免运行时计算
 */

#ifndef BITCPY_H
#define BITCPY_H

#include <stdint.h>
#include <string.h>

/**
 * @brief 位级别内存拷贝
 * @param dest      目标缓冲区（指向起始字节），必须非 NULL
 * @param dest_bit  目标起始位偏移（0-7），0 表示最低有效位
 * @param src       源缓冲区（指向起始字节），必须非 NULL
 * @param src_bit   源起始位偏移（0-7），0 表示最低有效位
 * @param len       要拷贝的位数，必须 >= 0
 *
 * === 参数要求（调用者保证） ===
 * - dest 必须是非 NULL 指针
 * - src 必须是非 NULL 指针
 * - dest_bit 必须在 [0, 7] 范围内
 * - src_bit 必须在 [0, 7] 范围内
 * - len 必须 >= 0
 * - dest 缓冲区至少需要 (dest_bit + len + 7) / 8 字节
 * - src 缓冲区至少需要 (src_bit + len + 7) / 8 字节
 * - 当 len > 0 时，dest 和 src 可以指向同一内存区域（支持自拷贝）
 *
 * === 未定义行为 ===
 * 如果违反以下任何条件，行为未定义：
 * 1. dest 为 NULL
 * 2. src 为 NULL
 * 3. dest_bit 不在 [0, 7] 范围内
 * 4. src_bit 不在 [0, 7] 范围内
 * 5. dest 缓冲区空间不足
 * 6. src 缓冲区空间不足
 * === 位序说明 ===
 * 位序为小端序（Little Endian）：
 * - 位 0 是字节的最低有效位（LSB）
 * - 位 7 是字节的最高有效位（MSB）
 * === 性能特点 ===
 * - 最优情况：字节对齐且长度为8的倍数，接近 memcpy 性能
 * - 次优情况：目标对齐后的大块数据（>=64位），充分利用内存带宽
 * - 一般情况：通过分阶段处理减少位操作次数
 * - 查找表优化：mask_low[] 和 mask_high[] 避免运行时计算
 */
void bitcpy(uint8_t* dest, uint8_t dest_bit,const uint8_t* src, uint8_t src_bit,uint64_t len);

#endif // BITCPY_H