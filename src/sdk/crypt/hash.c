/* SPDX-FileCopyrightText: (C) 2025-2026 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <crypt/hash.h>
#include <crypt/sha2.h>
#include <stddef.h>
#include <stdint.h>

static const CryptHashHandle Handles[CRYPT_HASH_COUNT] = {
    {.BufferSize = 64,
     .DigestSize = 24,
     .GetHash = (void *)CryptGetSha224Hash,
     .InitHash = (void *)CryptInitializeSha224Hash,
     .ClearHash = (void *)CryptClearSha224Hash,
     .UpdateHash = (void *)CryptUpdateSha224Hash,
     .FinishHash = (void *)CryptFinishSha224Hash},
    {.BufferSize = 64,
     .DigestSize = 32,
     .GetHash = (void *)CryptGetSha256Hash,
     .InitHash = (void *)CryptInitializeSha256Hash,
     .ClearHash = (void *)CryptClearSha256Hash,
     .UpdateHash = (void *)CryptUpdateSha256Hash,
     .FinishHash = (void *)CryptFinishSha256Hash},
    {.BufferSize = 128,
     .DigestSize = 56,
     .GetHash = (void *)CryptGetSha384Hash,
     .InitHash = (void *)CryptInitializeSha384Hash,
     .ClearHash = (void *)CryptClearSha384Hash,
     .UpdateHash = (void *)CryptUpdateSha384Hash,
     .FinishHash = (void *)CryptFinishSha384Hash},
    {.BufferSize = 128,
     .DigestSize = 64,
     .GetHash = (void *)CryptGetSha512Hash,
     .InitHash = (void *)CryptInitializeSha512Hash,
     .ClearHash = (void *)CryptClearSha512Hash,
     .UpdateHash = (void *)CryptUpdateSha512Hash,
     .FinishHash = (void *)CryptFinishSha512Hash},
};

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     Returns an opaque handle to the specified hash algorithm.
 *
 * PARAMETERS:
 *     Type - The hash algorithm to get.
 *
 * RETURN VALUE:
 *     A pointer to the handle, or NULL if the specified type is invalid.
 *-----------------------------------------------------------------------------------------------*/
const CryptHashHandle *CryptGetHashHandle(int Type) {
    if (Type < 0 || Type >= CRYPT_HASH_COUNT) {
        return NULL;
    }

    return &Handles[Type];
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     Returns the internal buffer size of a hash algorithm; you probably don't want to use this,
 *     unless you're implementing HMAC.
 *
 * PARAMETERS:
 *     Handle - The hash algorithm handle.
 *
 * RETURN VALUE:
 *     The internal buffer size of the specified hash algorithm.
 *-----------------------------------------------------------------------------------------------*/
size_t CryptGetHashBufferSize(const CryptHashHandle *Handle) {
    return Handle->BufferSize;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     Returns the digest (result) size for the specified hash algorithm.
 *
 * PARAMETERS:
 *     Handle - The hash algorithm handle.
 *
 * RETURN VALUE:
 *     The digest size for the specified hash algorithm.
 *-----------------------------------------------------------------------------------------------*/
size_t CryptGetHashDigestSize(const CryptHashHandle *Handle) {
    return Handle->DigestSize;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function gets the digest/hash for a buffer using the specified algorithm.
 *
 * PARAMETERS:
 *     Handle - The hash algorithm handle.
 *     Buffer - Input data, can be NULL as long as BufferSize==0.
 *     BufferSize - How many input bytes we have.
 *     Result - Where to store the digest.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void CryptGetHash(
    const CryptHashHandle *Handle,
    const void *Buffer,
    size_t BufferSize,
    uint8_t *Result) {
    Handle->GetHash(Buffer, BufferSize, Result);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     Initializes the private hash context for the specified algorithm.
 *
 * PARAMETERS:
 *     Context - Private data structure we use for the hash calculation.
 *     Handle - The hash algorithm handle.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void CryptInitializeHash(CryptHashContext *Context, const CryptHashHandle *Handle) {
    Context->Handle = Handle;
    Handle->InitHash(&Context->Context);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function safely clears the given private hash context.
 *
 * PARAMETERS:
 *     Context - Private data structure we use for the hash calculation.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void CryptClearHash(CryptHashContext *Context) {
    Context->Handle->ClearHash(&Context->Context);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function appends some extra buffer data to be used in the hash calculation.
 *
 * PARAMETERS:
 *     Context - Private data structure we use for the hash calculation.
 *     Buffer - Input data, can be NULL as long as BufferSize==0.
 *     BufferSize - How many input bytes we have.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void CryptUpdateHash(CryptHashContext *Context, const void *Buffer, size_t BufferSize) {
    Context->Handle->UpdateHash(&Context->Context, Buffer, BufferSize);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function generates the hash digest out of all previously appended data.
 *     After calling this function, you should reset the hash state before calling update again.
 *
 * PARAMETERS:
 *     Context - Private data structure we use for the hash calculation.
 *     Result - Where to store the digest.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void CryptFinishHash(CryptHashContext *Context, uint8_t *Result) {
    Context->Handle->FinishHash(&Context->Context, Result);
}
