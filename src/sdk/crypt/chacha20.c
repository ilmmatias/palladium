/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <crypt/chacha20.h>
#include <crypt/compiler.h>
#include <string.h>

/* ChaCha is a bit nicer to modern architectures, as it's LE instead of BE. */
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define BSWAP(A) (A)
#else
#define BSWAP(A) BSWAP32((A))
#endif

/* Quarter round, macro because passing pointers arguments is kinda annoying (and C doesn't have
 * references). */
#define QR(A, B, C, D) \
    A += B;            \
    D ^= A;            \
    D = ROTL32(D, 16); \
    C += D;            \
    B ^= C;            \
    B = ROTL32(B, 12); \
    A += B;            \
    D ^= A;            \
    D = ROTL32(D, 8);  \
    C += D;            \
    B ^= C;            \
    B = ROTL32(B, 7);

static const uint32_t InitialState[4] = {
    0x61707865,
    0x3320646E,
    0x79622D32,
    0x6B206574,
};

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function generates the current block to be used as input for the cipher operation.
 *
 * PARAMETERS:
 *     Context - The context to generate the block for.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void GenerateBlock(CryptChaCha20Context *Context) {
    uint32_t *Buffer = Context->Buffer;

    /* Copy in the current state to the output buffer. */
    for (size_t i = 0; i < 16; i++) {
        Buffer[i] = Context->State[i];
    }

    /* Do the 20 rounds (10 odd on the columns, 10 even on the diagonals). */
    for (int i = 0; i < 10; i++) {
        QR(Buffer[0], Buffer[4], Buffer[8], Buffer[12]);
        QR(Buffer[1], Buffer[5], Buffer[9], Buffer[13]);
        QR(Buffer[2], Buffer[6], Buffer[10], Buffer[14]);
        QR(Buffer[3], Buffer[7], Buffer[11], Buffer[15]);
        QR(Buffer[0], Buffer[5], Buffer[10], Buffer[15]);
        QR(Buffer[1], Buffer[6], Buffer[11], Buffer[12]);
        QR(Buffer[2], Buffer[7], Buffer[8], Buffer[13]);
        QR(Buffer[3], Buffer[4], Buffer[9], Buffer[14]);
    }

    /* ADd the current state back in. */
    for (size_t i = 0; i < 16; i++) {
        Buffer[i] += Context->State[i];
    }

    /* And swap the bytes back to little endian if required. */
    for (size_t i = 0; i < 16; i++) {
        Buffer[i] = BSWAP(Buffer[i]);
    }

    Context->State[12]++;
    Context->Offset = 0;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function initializes a given ChaCha20 private data structure for further use.
 *
 * PARAMETERS:
 *     Context - Private data structure we use for the cipher operations.
 *     Key - 256-bit key for all operations.
 *     Nonce - 96-bit nonce for all operations.
 *     InitialCounter - Initial byte offset.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void CryptInitializeChaCha20Cipher(
    CryptChaCha20Context *Context,
    const uint32_t Key[8],
    const uint32_t Nonce[3],
    uint32_t InitialCounter) {
    /* We start the state with "expand 32-byte k". */
    MEMCPY_INLINE(Context->State, InitialState, sizeof(InitialState));

    /* Then comes in the key. */
    for (int i = 0; i < 8; i++) {
        Context->State[4 + i] = BSWAP(Key[i]);
    }

    /* And the nonce. */
    for (int i = 0; i < 3; i++) {
        Context->State[13 + i] = BSWAP(Nonce[i]);
    }

    /* Finally the starting byte offset, and we trigger a refresh by forcing an overflow. */
    Context->State[12] = InitialCounter;
    Context->Offset = sizeof(Context->Buffer);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function safely clears the given private ChaCha20 context.
 *
 * PARAMETERS:
 *     Context - Private data structure we use for the cipher operations.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void CryptClearChaCha20Cipher(CryptChaCha20Context *Context) {
    MEMSET_INLINE(Context, 0, sizeof(CryptChaCha20Context));
    COMPILER_BARRIER(Context);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function extracts the current XOR block from the ChaCha20 cipher.
 *
 * PARAMETERS:
 *     Context - Private data structure we use for the cipher operations.
 *     Buffer - Output data, can be NULL as long as BufferSize==0.
 *     BufferSize - How many output bytes we want.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void CryptExtractChaCha20Cipher(CryptChaCha20Context *Context, void *Buffer, size_t BufferSize) {
    uint8_t *Data = Buffer;

    while (BufferSize) {
        /* Grab a new XOR block if we ran out of bytes (or if this is the first call). */
        if (Context->Offset == sizeof(Context->Buffer)) {
            GenerateBlock(Context);
        }

        /* And just copy the XOR data as is. */
        size_t RemainingSize = sizeof(Context->Buffer) - Context->Offset;
        size_t DataSize = BufferSize < RemainingSize ? BufferSize : RemainingSize;
        memcpy(Data, Context->RawBuffer + Context->Offset, DataSize);

        Data += DataSize;
        BufferSize -= DataSize;
        Context->Offset += DataSize;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function either encrypts or decrypts a buffer using the ChaCha20 cipher.
 *
 * PARAMETERS:
 *     Context - Private data structure we use for the cipher operations.
 *     Buffer - Input data, can be NULL as long as BufferSize==0.
 *     BufferSize - How many input bytes we have.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void CryptEncryptChaCha20Cipher(CryptChaCha20Context *Context, void *Buffer, size_t BufferSize) {
    uint8_t *Data = Buffer;

    while (BufferSize) {
        /* Grab a new XOR block if we ran out of bytes (or if this is the first call). */
        if (Context->Offset == sizeof(Context->Buffer)) {
            GenerateBlock(Context);
        }

        /* Then we just XOR everything together (this is the cipher part of this). */
        size_t RemainingSize = sizeof(Context->Buffer) - Context->Offset;
        size_t XorSize = BufferSize < RemainingSize ? BufferSize : RemainingSize;
        for (size_t i = 0; i < XorSize; i++) {
            Data[i] ^= Context->RawBuffer[Context->Offset + i];
        }

        Data += XorSize;
        BufferSize -= XorSize;
        Context->Offset += XorSize;
    }
}
