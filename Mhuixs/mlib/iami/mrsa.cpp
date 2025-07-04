#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/core_names.h>
#include <openssl/err.h>
#include <openssl/evp.h>

// 定义公私钥对结构体
struct KEYPAIR {
    uint32_t key_length;  // 公私钥长度
    uint8_t* public_key;  // 公钥存储的位置
    uint8_t* private_key; // 私钥的存储位置
};

// 生成指定位数的公私钥对
struct KEYPAIR* generate_keypair(uint32_t key_length) {
    EVP_PKEY_CTX* ctx;
    EVP_PKEY* pkey = NULL;
    BIO *pub_bio, *priv_bio;
    struct KEYPAIR* keypair = NULL;
    size_t pub_len, priv_len;

    // 初始化 OpenSSL
    OpenSSL_add_all_algorithms();
    ERR_load_crypto_strings();

    // 创建 EVP_PKEY_CTX 上下文
    ctx = EVP_PKEY_CTX_new_from_name(NULL, "RSA", NULL);
    if (!ctx) {
        fprintf(stderr, "Error creating EVP_PKEY_CTX: %s\n", ERR_error_string(ERR_get_error(), NULL));
        goto cleanup;
    }

    // 初始化上下文
    if (EVP_PKEY_keygen_init(ctx) <= 0) {
        fprintf(stderr, "Error initializing key generation: %s\n", ERR_error_string(ERR_get_error(), NULL));
        goto cleanup;
    }

    // 设置密钥长度
    if (EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, key_length) <= 0) {
        fprintf(stderr, "Error setting key length: %s\n", ERR_error_string(ERR_get_error(), NULL));
        goto cleanup;
    }

    // 生成密钥对
    if (EVP_PKEY_keygen(ctx, &pkey) <= 0) {
        fprintf(stderr, "Error generating key pair: %s\n", ERR_error_string(ERR_get_error(), NULL));
        goto cleanup;
    }

    // 创建 BIO 用于存储公钥和私钥
    pub_bio = BIO_new(BIO_s_mem());
    priv_bio = BIO_new(BIO_s_mem());
    if (!pub_bio || !priv_bio) {
        fprintf(stderr, "Error creating BIO: %s\n", ERR_error_string(ERR_get_error(), NULL));
        goto cleanup;
    }

    // 将公钥写入 BIO
    if (PEM_write_bio_PUBKEY(pub_bio, pkey) <= 0) {
        fprintf(stderr, "Error writing public key: %s\n", ERR_error_string(ERR_get_error(), NULL));
        goto cleanup;
    }

    // 将私钥写入 BIO
    if (PEM_write_bio_PrivateKey(priv_bio, pkey, NULL, NULL, 0, NULL, NULL) <= 0) {
        fprintf(stderr, "Error writing private key: %s\n", ERR_error_string(ERR_get_error(), NULL));
        goto cleanup;
    }

    // 获取公钥和私钥的长度
    pub_len = BIO_pending(pub_bio);
    priv_len = BIO_pending(priv_bio);

    // 分配内存存储公钥和私钥
    keypair = (struct KEYPAIR*)malloc(sizeof(struct KEYPAIR));
    if (!keypair) {
        fprintf(stderr, "Memory allocation error\n");
        goto cleanup;
    }
    keypair->key_length = key_length;
    keypair->public_key = (uint8_t*)malloc(pub_len + 1);
    keypair->private_key = (uint8_t*)malloc(priv_len + 1);
    if (!keypair->public_key || !keypair->private_key) {
        fprintf(stderr, "Memory allocation error\n");
        free(keypair);
        keypair = NULL;
        goto cleanup;
    }

    // 将公钥和私钥从 BIO 复制到内存中
    BIO_read(pub_bio, keypair->public_key, pub_len);
    keypair->public_key[pub_len] = '\0';
    BIO_read(priv_bio, keypair->private_key, priv_len);
    keypair->private_key[priv_len] = '\0';

cleanup:
    if (ctx) EVP_PKEY_CTX_free(ctx);
    if (pkey) EVP_PKEY_free(pkey);
    if (pub_bio) BIO_free(pub_bio);
    if (priv_bio) BIO_free(priv_bio);

    return keypair;
}

// 释放公私钥对内存
void free_keypair(struct KEYPAIR* keypair) {
    if (keypair) {
        if (keypair->public_key) free(keypair->public_key);
        if (keypair->private_key) free(keypair->private_key);
        free(keypair);
    }
}

