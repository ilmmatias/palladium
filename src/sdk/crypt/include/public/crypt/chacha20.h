/* SPDX-FileCopyrightText: (C) 2025-2026 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _CRYPT_CHACHA20_H_
#define _CRYPT_CHACHA20_H_

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t State[16];
    union {
        uint32_t Buffer[16];
        uint8_t RawBuffer[64];
    };
    size_t Offset;
} CryptChaCha20Context;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void CryptInitializeChaCha20Cipher(
    CryptChaCha20Context *Context,
    const uint32_t Key[8],
    const uint32_t Nonce[3],
    uint32_t InitialCounter);
void CryptClearChaCha20Cipher(CryptChaCha20Context *Context);
void CryptExtractChaCha20Cipher(CryptChaCha20Context *Context, void *Buffer, size_t BufferSize);
void CryptEncryptChaCha20Cipher(CryptChaCha20Context *Context, void *Buffer, size_t BufferSize);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _CRYPT_CHACHA20_H_ */
