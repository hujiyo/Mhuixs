/*
#版权所有 (c) Mhuixs-team 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2025.1
Email:hj18914255909@outlook.com
*/

#ifndef MRSA_H
#define MRSA_H

#include <stdint.h>

// 定义公私钥对结构体
typedef struct KEYPAIR {
    uint32_t key_length;  // 公私钥长度
    uint8_t* public_key;  // 公钥存储的位置
    uint8_t* private_key; // 私钥的存储位置
}KEYPAIR;

typedef struct public_key{
    uint32_t key_length;
    uint8_t* public_key;
}PUBLIC_KEY;

typedef struct private_key{
    uint32_t key_length;
    uint8_t* private_key;
}PRIVATE_KEY;

// 生成指定位数的公私钥对
KEYPAIR* generate_keypair(uint32_t key_length);

// 释放公私钥对内存
void free_keypair(KEYPAIR* keypair);


#endif
