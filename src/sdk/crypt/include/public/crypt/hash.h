/* SPDX-FileCopyrightText: (C) 2025-2026 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _CRYPT_HASH_H_
#define _CRYPT_HASH_H_

#include <crypt/sha2.h>

#define CRYPT_HASH_SHA224 0
#define CRYPT_HASH_SHA256 1
#define CRYPT_HASH_SHA384 2
#define CRYPT_HASH_SHA512 3

#define CRYPT_HASH_COUNT 4

#define CRYPT_HASH_MAX_BUFFER_SIZE 256
#define CRYPT_HASH_MAX_DIGEST_SIZE 256

struct CryptHashContext;

typedef struct {
    size_t BufferSize;
    size_t DigestSize;
    void (*GetHash)(const void *Buffer, size_t BufferSize, uint8_t *Result);
    void (*InitHash)(void *Context);
    void (*ClearHash)(void *Context);
    void (*UpdateHash)(void *Context, const void *Buffer, size_t BufferSize);
    void (*FinishHash)(void *Context, uint8_t *Result);
} CryptHashHandle;

typedef struct CryptHashHandle {
    const CryptHashHandle *Handle;
    union {
        CryptSha224Context Sha224Context;
        CryptSha256Context Sha256Context;
        CryptSha384Context Sha384Context;
        CryptSha512Context Sha512Context;
    } Context;
} CryptHashContext;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

const CryptHashHandle *CryptGetHashHandle(int Type);

size_t CryptGetHashBufferSize(const CryptHashHandle *Handle);
size_t CryptGetHashDigestSize(const CryptHashHandle *Handle);

void CryptGetHash(
    const CryptHashHandle *Handle,
    const void *Buffer,
    size_t BufferSize,
    uint8_t *Result);

void CryptInitializeHash(CryptHashContext *Context, const CryptHashHandle *Handle);
void CryptClearHash(CryptHashContext *Context);
void CryptUpdateHash(CryptHashContext *Context, const void *Buffer, size_t BufferSize);
void CryptFinishHash(CryptHashContext *Context, uint8_t *Result);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _CRYPT_HASH_H_ */
