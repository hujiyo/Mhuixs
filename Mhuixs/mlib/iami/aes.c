#include <openssl/aes.h>
#include <openssl/rand.h>
#include "iami.h"
#define err -1

typedef struct SKEY {
    int8_t flag; // SKEY标志位，用于指示data联合体中使用的是哪个字段
    uint8_t info[32]; //密钥
} SKEY;

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

    AES_KEY aes_key;
    // 从SKEY中获得 AES 加密密钥
    if (AES_set_encrypt_key(skey->info, key_len_bits, &aes_key) < 0) {
        return NULL;
    }
    
    /*
    PKCS#7填充：
    在AES等分组加密算法中，如果待加密数据的长度不是分组长度的整数倍，
    就需要进行填充。PKCS#7填充是一种常用的填充方式。
    填充规则是：在数据的末尾添加若干个字节，每个字节的值等于需要填充的字节数。
    例如，如果数据块的长度比分组长度少3个字节，那么就在数据末尾添加3个字节，每个字节的值都是3。
    */
    int padding_len = AES_BLOCK_SIZE - (len % AES_BLOCK_SIZE);
    len += padding_len;
    uint8_t *ciphertext = (uint8_t *)malloc(len);
    if (!ciphertext) {
        free(ciphertext);
        return NULL;
    }    

    // 执行 AES-CBC 加密
    AES_cbc_encrypt(text, ciphertext, len, &aes_key, vi_out, AES_ENCRYPT);

    // 输出加密后的数据长度
    *len_out = len;

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
}