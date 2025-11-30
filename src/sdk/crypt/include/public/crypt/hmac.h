/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _CRYPT_HMAC_H_
#define _CRYPT_HMAC_H_

#include <crypt/hash.h>

typedef struct {
    const CryptHashHandle *HashHandle;
    size_t HashBufferSize;
    size_t HashDigestSize;
    CryptHashContext HashContext;
    uint8_t Outer[CRYPT_HASH_MAX_BUFFER_SIZE];
} CryptHmacContext;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void CryptGetHmac(
    const CryptHashHandle *Handle,
    const void *Key,
    size_t KeySize,
    const void *Buffer,
    size_t BufferSize,
    uint8_t *Result);
void CryptInitializeHmac(
    CryptHmacContext *Context,
    const CryptHashHandle *Handle,
    const void *Key,
    size_t KeySize);
void CryptClearHmac(CryptHmacContext *Context);
void CryptUpdateHmac(CryptHmacContext *Context, const void *Buffer, size_t BufferSize);
void CryptFinishHmac(CryptHmacContext *Context, uint8_t *Result);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _CRYPT_SHA2_H_ */
