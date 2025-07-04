/*
#版权所有 (c) Mhuixs-team 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2025.1
Email:hj18914255909@outlook.com
*/

#ifndef AES_H
#define AES_H
#include <stdint.h>

#define err -1

typedef struct SKEY {
    int8_t flag; // SKEY标志位，用于指示data联合体中使用的是哪个字段
    uint8_t info[32]; //密钥
} SKEY;

#define AES128 1
#define AES192 2
#define AES256 3

int init_SKEY(SKEY* skey,int8_t flag);
/*
    初始化SKEY,生成全新的密钥
    
    成功返回0，失败返回-1
    skey:AES密钥描述符
    flag:SKEY标志位
*/
uint8_t* aes_encrypt(const uint8_t *text, uint32_t len, const SKEY* skey,uint8_t* vi_out,uint32_t* len_out);
/*
    根据SKEY对data进行加密
    成功返回加密后的数据，失败返回NULL
    text:明文
    len:明文长度
    skey:AES密钥描述符
    vi_out:输出随机辅助向量（ 确保可用字节数大于等于16字节 ）
    len_out:密文的数据长度

    注意：
    密文内存由调用者负责释放
*/
uint8_t* aes_decrypt(const uint8_t *ciphertext,const uint32_t len, const SKEY* skey,const uint8_t* vi_in,uint32_t* len_out);
/*
    根据SKEY对data进行解密
    成功返回解密后的数据，失败返回NULL
    ciphertext:密文
    len:密文长度
    skey:AES密钥描述符
    vi:辅助向量
    len_out:输出解密后的数据长度
*/

#endif