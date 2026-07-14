/* SPDX-FileCopyrightText: (C) 2025-2026 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <crypt/compiler.h>
#include <crypt/hash.h>
#include <crypt/hmac.h>
#include <stdint.h>
#include <string.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function gets the auth code/hash (HMAC) for the given buffer.
 *
 * PARAMETERS:
 *     Handle - The hash algorithm handle.
 *     Key - Input key, can be NULL as long as KeySize==0.
 *     KeySize - How many bytes our key has.
 *     Buffer - Input data, can be NULL as long as BufferSize==0.
 *     BufferSize - How many bytes our input has.
 *     Result - Where to store the digest.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void CryptGetHmac(
    const CryptHashHandle *Handle,
    const void *Key,
    size_t KeySize,
    const void *Buffer,
    size_t BufferSize,
    uint8_t *Result) {
    CryptHmacContext Context;
    CryptInitializeHmac(&Context, Handle, Key, KeySize);
    CryptUpdateHmac(&Context, Buffer, BufferSize);
    CryptFinishHmac(&Context, Result);
    CryptClearHmac(&Context);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     Initializes the private HMAC context, using the specified algorithm as the backing hash.
 *
 * PARAMETERS:
 *     Context - Private data structure we use for the MAC calculation.
 *     Handle - The hash algorithm handle.
 *     Key - Input key, can be NULL as long as KeySize==0.
 *     KeySize - How many bytes our key has.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void CryptInitializeHmac(
    CryptHmacContext *Context,
    const CryptHashHandle *Handle,
    const void *Key,
    size_t KeySize) {
    Context->HashHandle = Handle;
    Context->HashBufferSize = CryptGetHashBufferSize(Handle);
    Context->HashDigestSize = CryptGetHashDigestSize(Handle);

    /* If the key is too big, we can compress it using the hash function itself. */
    uint8_t KeyData[CRYPT_HASH_MAX_BUFFER_SIZE] = {0};
    if (KeySize > Context->HashBufferSize) {
        CryptGetHash(Handle, Key, KeySize, KeyData);
        KeySize = Context->HashDigestSize;
    } else {
        memcpy(KeyData, Key, KeySize);
    }

    /* Then we do some XOR trickery that HMAC requires. */
    uint8_t Inner[CRYPT_HASH_MAX_BUFFER_SIZE];
    for (size_t i = 0; i < Context->HashBufferSize; i++) {
        Inner[i] = KeyData[i] ^ 0x36;
        Context->Outer[i] = KeyData[i] ^ 0x5C;
    }

    /* And startup the hash calculation. */
    CryptInitializeHash(&Context->HashContext, Handle);
    CryptUpdateHash(&Context->HashContext, Inner, Context->HashBufferSize);

    /* Just don't forget to clear sensitive stack data. */
    MEMSET_INLINE(KeyData, 0, sizeof(KeyData));
    MEMSET_INLINE(Inner, 0, sizeof(Inner));
    COMPILER_BARRIER(KeyData);
    COMPILER_BARRIER(Inner);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function safely clears the given private HMAC context.
 *
 * PARAMETERS:
 *     Context - Private data structure we use for the MAC calculation.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void CryptClearHmac(CryptHmacContext *Context) {
    CryptClearHash(&Context->HashContext);
    MEMSET_INLINE(Context, 0, sizeof(CryptHmacContext));
    COMPILER_BARRIER(Context);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function appends some extra buffer data to be used in the MAC calculation.
 *
 * PARAMETERS:
 *     Context - Private data structure we use for the MAC calculation.
 *     Buffer - Input data, can be NULL as long as BufferSize==0.
 *     BufferSize - How many input bytes we have.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void CryptUpdateHmac(CryptHmacContext *Context, const void *Buffer, size_t BufferSize) {
    CryptUpdateHash(&Context->HashContext, Buffer, BufferSize);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function generates the MAC out of all previously appended data.
 *     After calling this function, you should reset the state before calling update again.
 *
 * PARAMETERS:
 *     Context - Private data structure we use for the MAC calculation.
 *     Result - Where to store the digest.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void CryptFinishHmac(CryptHmacContext *Context, uint8_t *Result) {
    CryptFinishHash(&Context->HashContext, Result);
    CryptInitializeHash(&Context->HashContext, Context->HashHandle);
    CryptUpdateHash(&Context->HashContext, Context->Outer, Context->HashBufferSize);
    CryptUpdateHash(&Context->HashContext, Result, Context->HashDigestSize);
    CryptFinishHash(&Context->HashContext, Result);
}
