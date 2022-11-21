
#include "common_api.h"
#include "luat_base.h"
#include "mbedtls/aes.h"

#if defined(MBEDTLS_AES_ALT)

#include "tls.h"

static int tls_init = 0;

void mbedtls_aes_init( mbedtls_aes_context *ctx ) {
    ctx = malloc(sizeof(mbedtls_aes_context));
    if (ctx == NULL)
        return;
    if (tls_init == 0) {
        sctInit();
        tls_init = 1;
    }
    memset(ctx, 0, sizeof(mbedtls_aes_context));
    ctx->info.aesCkAddr = (uint32_t)(ctx->buf);
}

void mbedtls_aes_free( mbedtls_aes_context *ctx ) {
    if (ctx != NULL)
        free(ctx);
}

int mbedtls_aes_setkey_enc( mbedtls_aes_context *ctx, const unsigned char *key,
                    unsigned int keybits ) {
    if (ctx == NULL)
        return MBEDTLS_ERR_AES_FEATURE_UNAVAILABLE;
    memcpy(ctx->buf, key, keybits / 8);
    switch (keybits)
    {
    case 128:
        ctx->info.aesCtrl.ckLen = 0;
        break;
    case 192:
        ctx->info.aesCtrl.ckLen = 1;
        break;
    case 256:
        ctx->info.aesCtrl.ckLen = 2;
        break;
    
    default:
        return -1;
    }
    ctx->info.aesCtrl.dir = 0;
    return 0;
}

int mbedtls_aes_setkey_dec( mbedtls_aes_context *ctx, const unsigned char *key,
                    unsigned int keybits ) {
    if (ctx == NULL)
        return MBEDTLS_ERR_AES_FEATURE_UNAVAILABLE;
    memcpy(ctx->buf, key, keybits / 8);
    switch (keybits)
    {
    case 128:
        ctx->info.aesCtrl.ckLen = 0;
        break;
    case 192:
        ctx->info.aesCtrl.ckLen = 1;
        break;
    case 256:
        ctx->info.aesCtrl.ckLen = 2;
        break;
    
    default:
        return -1;
    }
    ctx->info.aesCtrl.dir = 1;
    return 0;
}

int mbedtls_aes_crypt_ecb( mbedtls_aes_context *ctx,
                    int mode,
                    const unsigned char input[16],
                    unsigned char output[16] ) {
    if (ctx == NULL)
        return MBEDTLS_ERR_AES_FEATURE_UNAVAILABLE;
    aesInfo_t* info = (aesInfo_t*)&ctx->info;
    info->ivAddr = 0;
    info->srcAddr = (uint32_t)input;
    info->dstAddr = (uint32_t)output;
    info->length = 16;

    info->aesCtrl.aesMode = 0;

    int ret = aesUpdate(info);
    if (ret == 0)
        return 0;

    return MBEDTLS_ERR_AES_FEATURE_UNAVAILABLE;
}

int mbedtls_aes_crypt_cbc( mbedtls_aes_context *ctx,
                    int mode,
                    size_t length,
                    unsigned char iv[16],
                    const unsigned char *input,
                    unsigned char *output ) {
    if (ctx == NULL)
        return MBEDTLS_ERR_AES_FEATURE_UNAVAILABLE;
    aesInfo_t* info = (aesInfo_t*)&ctx->info;
    if (ctx == NULL)
        return -1;
    info->ivAddr = 0;
    info->srcAddr = (uint32_t)input;
    info->dstAddr = (uint32_t)output;
    info->length = 16;

    info->aesCtrl.aesMode = 1;

    int ret = aesUpdate(info);
    if (ret == 0)
        return 0;

    return MBEDTLS_ERR_AES_FEATURE_UNAVAILABLE;
}

int mbedtls_aes_crypt_ctr( mbedtls_aes_context *ctx,
                       size_t length,
                       size_t *nc_off,
                       unsigned char nonce_counter[16],
                       unsigned char stream_block[16],
                       const unsigned char *input,
                       unsigned char *output ) {
    if (ctx == NULL)
        return MBEDTLS_ERR_AES_FEATURE_UNAVAILABLE;
    aesInfo_t* info = (aesInfo_t*)&ctx->info;
    if (ctx == NULL)
        return -1;
    info->ivAddr = 0;
    info->srcAddr = (uint32_t)input;
    info->dstAddr = (uint32_t)output;
    info->length = 16;

    info->aesCtrl.aesMode = 2;

    int ret = aesUpdate(info);
    if (ret == 0)
        return 0;

    return MBEDTLS_ERR_AES_FEATURE_UNAVAILABLE;
}

#endif

