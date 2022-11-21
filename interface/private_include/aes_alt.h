/**
 * \file aes.h
 *
 * \brief   This file contains AES definitions and functions.
 *
 *          The Advanced Encryption Standard (AES) specifies a FIPS-approved
 *          cryptographic algorithm that can be used to protect electronic
 *          data.
 *
 *          The AES algorithm is a symmetric block cipher that can
 *          encrypt and decrypt information. For more information, see
 *          <em>FIPS Publication 197: Advanced Encryption Standard</em> and
 *          <em>ISO/IEC 18033-2:2006: Information technology -- Security
 *          techniques -- Encryption algorithms -- Part 2: Asymmetric
 *          ciphers</em>.
 *
 *          The AES-XTS block mode is standardized by NIST SP 800-38E
 *          <https://nvlpubs.nist.gov/nistpubs/legacy/sp/nistspecialpublication800-38e.pdf>
 *          and described in detail by IEEE P1619
 *          <https://ieeexplore.ieee.org/servlet/opac?punumber=4375278>.
 */

/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may
 *  not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#ifndef MBEDTLS_AES_ALT_H
#define MBEDTLS_AES_ALT_H

#include <stddef.h>
#include <stdint.h>

#include "mbedtls/aes.h"
#include "tls.h"

#ifdef __cplusplus
extern "C" {
#endif

// AES硬件加速仅支持CBC/ECB/CTR

#if defined(MBEDTLS_AES_ALT)
// #if 1
// Regular implementation
//

/**
 * \brief The AES context-type definition.
 */
// #define mbedtls_aes_context aesInfo_t
typedef struct aes_alt
{
    aesInfo_t info;
    char buf[32];
}mbedtls_aes_context;


/**
 * \brief          This function initializes the specified AES context.
 *
 *                 It must be the first API called before using
 *                 the context.
 *
 * \param ctx      The AES context to initialize. This must not be \c NULL.
 */
void mbedtls_aes_init( mbedtls_aes_context *ctx );

/**
 * \brief          This function releases and clears the specified AES context.
 *
 * \param ctx      The AES context to clear.
 *                 If this is \c NULL, this function does nothing.
 *                 Otherwise, the context must have been at least initialized.
 */
void mbedtls_aes_free( mbedtls_aes_context *ctx );

int mbedtls_aes_setkey_enc( mbedtls_aes_context *ctx, const unsigned char *key,
                    unsigned int keybits );

int mbedtls_aes_setkey_dec( mbedtls_aes_context *ctx, const unsigned char *key,
                    unsigned int keybits );

int mbedtls_aes_crypt_ecb( mbedtls_aes_context *ctx,
                    int mode,
                    const unsigned char input[16],
                    unsigned char output[16] );

#if defined(MBEDTLS_CIPHER_MODE_CBC)
/**
 * \brief  This function performs an AES-CBC encryption or decryption operation
 *         on full blocks.
 *
 *         It performs the operation defined in the \p mode
 *         parameter (encrypt/decrypt), on the input data buffer defined in
 *         the \p input parameter.
 *
 *         It can be called as many times as needed, until all the input
 *         data is processed. mbedtls_aes_init(), and either
 *         mbedtls_aes_setkey_enc() or mbedtls_aes_setkey_dec() must be called
 *         before the first call to this API with the same context.
 *
 * \note   This function operates on full blocks, that is, the input size
 *         must be a multiple of the AES block size of \c 16 Bytes.
 *
 * \note   Upon exit, the content of the IV is updated so that you can
 *         call the same function again on the next
 *         block(s) of data and get the same result as if it was
 *         encrypted in one call. This allows a "streaming" usage.
 *         If you need to retain the contents of the IV, you should
 *         either save it manually or use the cipher module instead.
 *
 *
 * \param ctx      The AES context to use for encryption or decryption.
 *                 It must be initialized and bound to a key.
 * \param mode     The AES operation: #MBEDTLS_AES_ENCRYPT or
 *                 #MBEDTLS_AES_DECRYPT.
 * \param length   The length of the input data in Bytes. This must be a
 *                 multiple of the block size (\c 16 Bytes).
 * \param iv       Initialization vector (updated after use).
 *                 It must be a readable and writeable buffer of \c 16 Bytes.
 * \param input    The buffer holding the input data.
 *                 It must be readable and of size \p length Bytes.
 * \param output   The buffer holding the output data.
 *                 It must be writeable and of size \p length Bytes.
 *
 * \return         \c 0 on success.
 * \return         #MBEDTLS_ERR_AES_INVALID_INPUT_LENGTH
 *                 on failure.
 */
int mbedtls_aes_crypt_cbc( mbedtls_aes_context *ctx,
                    int mode,
                    size_t length,
                    unsigned char iv[16],
                    const unsigned char *input,
                    unsigned char *output );
#endif /* MBEDTLS_CIPHER_MODE_CBC */


#if defined(MBEDTLS_CIPHER_MODE_CTR)
/**
 * \brief      This function performs an AES-CTR encryption or decryption
 *             operation.
 *
 *             This function performs the operation defined in the \p mode
 *             parameter (encrypt/decrypt), on the input data buffer
 *             defined in the \p input parameter.
 *
 *             Due to the nature of CTR, you must use the same key schedule
 *             for both encryption and decryption operations. Therefore, you
 *             must use the context initialized with mbedtls_aes_setkey_enc()
 *             for both #MBEDTLS_AES_ENCRYPT and #MBEDTLS_AES_DECRYPT.
 *
 * \warning    You must never reuse a nonce value with the same key. Doing so
 *             would void the encryption for the two messages encrypted with
 *             the same nonce and key.
 *
 *             There are two common strategies for managing nonces with CTR:
 *
 *             1. You can handle everything as a single message processed over
 *             successive calls to this function. In that case, you want to
 *             set \p nonce_counter and \p nc_off to 0 for the first call, and
 *             then preserve the values of \p nonce_counter, \p nc_off and \p
 *             stream_block across calls to this function as they will be
 *             updated by this function.
 *
 *             With this strategy, you must not encrypt more than 2**128
 *             blocks of data with the same key.
 *
 *             2. You can encrypt separate messages by dividing the \p
 *             nonce_counter buffer in two areas: the first one used for a
 *             per-message nonce, handled by yourself, and the second one
 *             updated by this function internally.
 *
 *             For example, you might reserve the first 12 bytes for the
 *             per-message nonce, and the last 4 bytes for internal use. In that
 *             case, before calling this function on a new message you need to
 *             set the first 12 bytes of \p nonce_counter to your chosen nonce
 *             value, the last 4 to 0, and \p nc_off to 0 (which will cause \p
 *             stream_block to be ignored). That way, you can encrypt at most
 *             2**96 messages of up to 2**32 blocks each with the same key.
 *
 *             The per-message nonce (or information sufficient to reconstruct
 *             it) needs to be communicated with the ciphertext and must be unique.
 *             The recommended way to ensure uniqueness is to use a message
 *             counter. An alternative is to generate random nonces, but this
 *             limits the number of messages that can be securely encrypted:
 *             for example, with 96-bit random nonces, you should not encrypt
 *             more than 2**32 messages with the same key.
 *
 *             Note that for both stategies, sizes are measured in blocks and
 *             that an AES block is 16 bytes.
 *
 * \warning    Upon return, \p stream_block contains sensitive data. Its
 *             content must not be written to insecure storage and should be
 *             securely discarded as soon as it's no longer needed.
 *
 * \param ctx              The AES context to use for encryption or decryption.
 *                         It must be initialized and bound to a key.
 * \param length           The length of the input data.
 * \param nc_off           The offset in the current \p stream_block, for
 *                         resuming within the current cipher stream. The
 *                         offset pointer should be 0 at the start of a stream.
 *                         It must point to a valid \c size_t.
 * \param nonce_counter    The 128-bit nonce and counter.
 *                         It must be a readable-writeable buffer of \c 16 Bytes.
 * \param stream_block     The saved stream block for resuming. This is
 *                         overwritten by the function.
 *                         It must be a readable-writeable buffer of \c 16 Bytes.
 * \param input            The buffer holding the input data.
 *                         It must be readable and of size \p length Bytes.
 * \param output           The buffer holding the output data.
 *                         It must be writeable and of size \p length Bytes.
 *
 * \return                 \c 0 on success.
 */
int mbedtls_aes_crypt_ctr( mbedtls_aes_context *ctx,
                       size_t length,
                       size_t *nc_off,
                       unsigned char nonce_counter[16],
                       unsigned char stream_block[16],
                       const unsigned char *input,
                       unsigned char *output );
#endif /* MBEDTLS_CIPHER_MODE_CTR */

#endif

#ifdef __cplusplus
}
#endif

#endif /* aes.h */
