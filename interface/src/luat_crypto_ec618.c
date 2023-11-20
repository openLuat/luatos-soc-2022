/*
 * Copyright (c) 2022 OpenLuat & AirM2M
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */


#include "common_api.h"
#include "FreeRTOS.h"
#include "task.h"

#include "luat_base.h"
#include "rng.h"
#include "luat_crypto.h"
#include "mbedtls/md5.h"
#include "mbedtls/sha1.h"
#include "mbedtls/sha256.h"
#include "mbedtls/base64.h"
#include "mbedtls/des.h"
#include "mbedtls/aes.h"
#include "mbedtls/rsa.h"


void luat_crypto_md5( unsigned char *input, int ilen, unsigned char output[16] )
{
    mbedtls_md5_ret(input, ilen, output);
}

#ifndef __LUATOS__
/**
 * @brief BASE64加密
 * @param dst buffer
 * @param dlen buffer长度
 * @param olen 写入的字节数
 * @param src 加密密钥
 * @param slen 加密密钥长度
 * @return 0成功
 */
int luat_crypto_base64_encode( unsigned char *dst, size_t dlen, size_t *olen, const unsigned char *src, size_t slen )
{
    mbedtls_base64_encode(dst, dlen, olen, src, slen);
}

/**
 * @brief BASE64解密
 * @param dst buffer
 * @param dlen buffer长度
 * @param olen 写入的字节数
 * @param src 密钥
 * @param slen 密钥长度
 * @return 0成功
 */
int luat_crypto_base64_decode( unsigned char *dst, size_t dlen, size_t *olen, const unsigned char *src, size_t slen )
{
    mbedtls_base64_decode(dst, dlen, olen, src, slen);
}

#endif

/**
 * @brief 进行SHA1校验
 * @param input    输入的数据
 * @param ilen     输入的数据长度
 * @param output   输出的SHA1检验值
 */
void luat_crypto_sha1(const unsigned char *input, size_t ilen, unsigned char output[20])
{
    mbedtls_sha1_ret(input, ilen, output);
}


/**
 * @brief 进行SHA256校验
 * @param input    输入的数据
 * @param ilen     输入的数据长度
 * @param output   输出的SHA1检验值
 * @param is_224   是否是224校验
 */
void luat_crypto_sha256(const unsigned char *input, size_t ilen, unsigned char output[20], int is_224)
{
    mbedtls_sha256_ret(input, ilen, output, is_224);
}

#ifdef __LUATOS__
#define LUAT_LOG_TAG "crypto"
#include "luat_log.h"
#include "luat_mcu.h"
#endif

static uint8_t trng_wait;
static uint8_t trng_pool[24];

int luat_crypto_trng(char* buff, size_t len) {
    char* dst = buff;
    while (len > 0) {
        // 池内没有剩余的随机值? 生成一次
        if (trng_wait == 0) {
            // LLOGD("生成一次随机数 24字节,放入池中");
            rngGenRandom(trng_pool);
            trng_wait = 24;
        }
        // 剩余随机值够用, 直接拷贝
        if (len <= trng_wait) {
            memcpy(dst, trng_pool + (24 - trng_wait), len);
            trng_wait -= len;
            return 0;
        }
        // 不够用, 先把现有的用完, 然后下一个循环
        memcpy(dst, trng_pool + (24 - trng_wait), trng_wait);
        dst += trng_wait;
        len -= trng_wait;
        trng_wait = 0;
    }
    return 0;
}