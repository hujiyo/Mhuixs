#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/evp.h>
#include <openssl/aes.h>
#include <openssl/rand.h>
#include "maes.h"

/*
#版权所有 (c) Mhuixs-team 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2025.1
Email:hj18914255909@outlook.com
*/

int init_SKEY(SKEY* skey,int8_t flag)
{
    /*
        初始化SKEY,生成全新的密钥
        
        成功返回0，失败返回-1
        skey:AES密钥描述符
        flag:SKEY标志位
    */
    if(flag != AES128 && flag != AES192 && flag != AES256){
        return err;
    }
    skey->flag = flag;
    //使用随机数生成密钥
    if(!RAND_bytes(skey->info,flag/8)){
        return err;
    }
    return 0;
}

uint8_t* aes_encrypt(const uint8_t *text, uint32_t len, const SKEY* skey,uint8_t* vi_out,uint32_t* len_out) 
{
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
    
    //判断flag
    int key_len_bits;
    switch (skey->flag) {
        case AES128:
            key_len_bits = 128;
            break;
        case AES192:
            key_len_bits = 192;
            break;
        case AES256:
            key_len_bits = 256;
            break;
        default:
            return NULL;
    }

    // 使用随机数生成初始化向量
    if (!RAND_bytes(vi_out, AES_BLOCK_SIZE)) {
        return NULL;
    }

    // PKCS#7填充
    int padding_len = AES_BLOCK_SIZE - (len % AES_BLOCK_SIZE);
    len += padding_len;
    uint8_t *ciphertext = (uint8_t *)malloc(len);
    if (!ciphertext) {
        return NULL;
    }
    memcpy(ciphertext, text, len - padding_len);
    for (int i = len - padding_len; i < len; i++) {
        ciphertext[i] = padding_len;
    }

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        free(ciphertext);
        return NULL;
    }

    const EVP_CIPHER *cipher;
    switch (key_len_bits) {
        case 128:
            cipher = EVP_aes_128_cbc();
            break;
        case 192:
            cipher = EVP_aes_192_cbc();
            break;
        case 256:
            cipher = EVP_aes_256_cbc();
            break;
        default:
            EVP_CIPHER_CTX_free(ctx);
            free(ciphertext);
            return NULL;
    }

    if (EVP_EncryptInit_ex(ctx, cipher, NULL, skey->info, vi_out) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        free(ciphertext);
        return NULL;
    }

    int ciphertext_len;
    if (EVP_EncryptUpdate(ctx, ciphertext, &ciphertext_len, text, len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        free(ciphertext);
        return NULL;
    }

    int final_len;
    if (EVP_EncryptFinal_ex(ctx, ciphertext + ciphertext_len, &final_len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        free(ciphertext);
        return NULL;
    }

    ciphertext_len += final_len;
    *len_out = ciphertext_len;

    EVP_CIPHER_CTX_free(ctx);
    return ciphertext;
}

uint8_t* aes_decrypt(const uint8_t *ciphertext,const uint32_t len, const SKEY* skey,const uint8_t* vi_in,uint32_t* len_out)
{
    /*
        根据SKEY对data进行解密
        成功返回解密后的数据，失败返回NULL
        ciphertext:密文
        len:密文长度
        skey:AES密钥描述符
        vi:辅助向量
        len_out:输出解密后的数据长度
    */

    //判断flag
    int key_len_bits;
    switch (skey->flag) {
        case AES128:
            key_len_bits = 128;
            break;
        case AES192:
            key_len_bits = 192;
            break;
        case AES256:
            key_len_bits = 256;
            break;
        default:
            return NULL;
    }

    // 判断密文长度是否合法
    if(len % AES_BLOCK_SIZE != 0){
        return NULL;
    }

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        return NULL;
    }

    const EVP_CIPHER *cipher;
    switch (key_len_bits) {
        case 128:
            cipher = EVP_aes_128_cbc();
            break;
        case 192:
            cipher = EVP_aes_192_cbc();
            break;
        case 256:
            cipher = EVP_aes_256_cbc();
            break;
        default:
            EVP_CIPHER_CTX_free(ctx);
            return NULL;
    }

    if (EVP_DecryptInit_ex(ctx, cipher, NULL, skey->info, vi_in) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return NULL;
    }

    uint8_t *plaintext = (uint8_t *)malloc(len);
    if (!plaintext) {
        EVP_CIPHER_CTX_free(ctx);
        return NULL;
    }

    int plaintext_len;
    if (EVP_DecryptUpdate(ctx, plaintext, &plaintext_len, ciphertext, len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        free(plaintext);
        return NULL;
    }

    int final_len;
    if (EVP_DecryptFinal_ex(ctx, plaintext + plaintext_len, &final_len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        free(plaintext);
        return NULL;
    }

    plaintext_len += final_len;
    *len_out = plaintext_len;

    // 删除PKCS#7填充
    int padding_len = plaintext[plaintext_len - 1];
    if (padding_len > AES_BLOCK_SIZE || padding_len == 0) {
        EVP_CIPHER_CTX_free(ctx);
        free(plaintext);
        return NULL;
    }
    for (int i = 1; i <= padding_len; i++) {
        if (plaintext[plaintext_len - i] != padding_len) {
            EVP_CIPHER_CTX_free(ctx);
            free(plaintext);
            return NULL;
        }
    }

    uint8_t *new_plaintext = (uint8_t *)realloc(plaintext, plaintext_len - padding_len);
    if (!new_plaintext) {
        EVP_CIPHER_CTX_free(ctx);
        free(plaintext);
        return NULL;
    }

    plaintext = new_plaintext;
    *len_out -= padding_len;

    EVP_CIPHER_CTX_free(ctx);
    return plaintext;
}