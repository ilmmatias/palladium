/* SPDX-FileCopyrightText: (C) 2025-2026 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _CRYPT_SHA2_H_
#define _CRYPT_SHA2_H_

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t State[8];
    union {
        uint32_t Buffer[16];
        uint8_t RawBuffer[64];
    };
    uint64_t BufferSize;
    uint64_t TotalSize;
} CryptSha224Context, CryptSha256Context;

typedef struct {
    uint64_t State[8];
    union {
        uint64_t Buffer[16];
        uint8_t RawBuffer[128];
    };
    uint64_t BufferSize;
    uint64_t TotalSize;
} CryptSha384Context, CryptSha512Context;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void CryptGetSha224Hash(const void *Buffer, size_t BufferSize, uint8_t Result[28]);
void CryptInitializeSha224Hash(CryptSha224Context *Context);
void CryptClearSha224Hash(CryptSha224Context *Context);
void CryptUpdateSha224Hash(CryptSha224Context *Context, const void *Buffer, size_t BufferSize);
void CryptFinishSha224Hash(CryptSha224Context *Context, uint8_t Result[28]);

void CryptGetSha256Hash(const void *Buffer, size_t BufferSize, uint8_t Result[32]);
void CryptInitializeSha256Hash(CryptSha256Context *Context);
void CryptClearSha256Hash(CryptSha256Context *Context);
void CryptUpdateSha256Hash(CryptSha256Context *Context, const void *Buffer, size_t BufferSize);
void CryptFinishSha256Hash(CryptSha256Context *Context, uint8_t Result[32]);

void CryptGetSha384Hash(const void *Buffer, size_t BufferSize, uint8_t Result[56]);
void CryptInitializeSha384Hash(CryptSha384Context *Context);
void CryptClearSha384Hash(CryptSha384Context *Context);
void CryptUpdateSha384Hash(CryptSha384Context *Context, const void *Buffer, size_t BufferSize);
void CryptFinishSha384Hash(CryptSha384Context *Context, uint8_t Result[56]);

void CryptGetSha512Hash(const void *Buffer, size_t BufferSize, uint8_t Result[64]);
void CryptInitializeSha512Hash(CryptSha512Context *Context);
void CryptClearSha512Hash(CryptSha512Context *Context);
void CryptUpdateSha512Hash(CryptSha512Context *Context, const void *Buffer, size_t BufferSize);
void CryptFinishSha512Hash(CryptSha512Context *Context, uint8_t Result[64]);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _CRYPT_SHA2_H_ */
