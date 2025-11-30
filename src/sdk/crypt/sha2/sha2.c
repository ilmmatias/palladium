/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <string.h>

/* Ch and Maj are independent from the actual variant of the SHA2 algorithm. */
#define CH(A, B, C) (((A) & (B)) ^ (~(A) & (C)))
#define MAJ(A, B, C) (((A) & (B)) ^ ((A) & (C)) ^ ((B) & (C)))

/* And some other things can be derived from the data we already have. */
#define BUFFER_SIZE (16 * sizeof(WORD))
#define DIGEST_SIZE (DIGEST_WORDS * sizeof(WORD))
#define LENGTH_OFF (BUFFER_SIZE - 2 * sizeof(WORD))

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function runs a single round of the SHA2 compression algorithm.
 *
 * PARAMETERS:
 *     State - Current working values.
 *     Constant - Round constant + matching message scheduler value for the current round.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static inline void ProcessRound(WORD State[8], size_t Index, WORD Constant) {
    /* Calculate the 8 indexes needed to access the rotating state. */
    WORD A = Index & 0x07;
    WORD B = (Index + 1) & 0x07;
    WORD C = (Index + 2) & 0x07;
    WORD D = (Index + 3) & 0x07;
    WORD E = (Index + 4) & 0x07;
    WORD F = (Index + 5) & 0x07;
    WORD G = (Index + 6) & 0x07;
    WORD H = (Index + 7) & 0x07;

    /* Then pre-calculate the terms needed for the non-rotating values. */
    WORD Term1 = State[H] + SUM1(State[E]) + CH(State[E], State[F], State[G]) + Constant;
    WORD Term2 = SUM0(State[A]) + MAJ(State[A], State[B], State[C]);

    /* And write those new values. */
    State[D] += Term1;
    State[H] = Term1 + Term2;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function runs the full SHA2 compression algorithm for the current block.
 *
 * PARAMETERS:
 *     Context - Private data structure we use for the hash calculation.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void ProcessBuffer(CONTEXT *Context) {
    /* Start up by loading the message schedule with the buffer values (in big endian). */
    WORD Schedule[16];
    for (int i = 0; i < 16; i++) {
        Schedule[i] = BSWAP(Context->Buffer[i]);
    }

    /* Then copy in the current state into the working variables (array instead of separate
     * A/B/C/D/E/F/G/H, as Clang can properly expand this into registers as long as we use
     * MEMCPY_INLINE). */
    WORD State[8];
    MEMCPY_INLINE(State, Context->State, sizeof(State));

    /* Do the initial 16 rounds (they don't need any schedule setup other than what we already
     * loaded). */

#pragma unroll
    for (int i = 0; i < 16; i++) {
        ProcessRound(State, (8 - i) & 0x07, Schedule[i] + RoundConstants[i]);
    }

    /* And then for anything beyond the already loaded initial 16 words, we need to combine the
     * pre-existing schedule data into the new entry to be used. */
    for (int i = 16; i < ROUNDS; i += 16) {
#pragma unroll
        for (int t = 0; t < 16; t++) {
            WORD Sigma0 = SIG0(Schedule[(t + 1) & 0x0F]);
            WORD Sigma1 = SIG1(Schedule[(t + 14) & 0x0F]);
            Schedule[t] += Sigma0 + Schedule[(t + 9) & 0x0F] + Sigma1;
            ProcessRound(State, (8 - t) & 0x07, Schedule[t] + RoundConstants[i + t]);
        }
    }

    /* Finally, cascade the working variables back into the hash state. */
    for (int i = 0; i < 8; i++) {
        Context->State[i] += State[i];
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function gets the digest/hash for a buffer using the SHA2 algorithm.
 *
 * PARAMETERS:
 *     Buffer - Input data, can be NULL as long as BufferSize==0.
 *     BufferSize - How many input bytes we have.
 *     Result - Where to store the digest.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void GET_HASH(const void *Buffer, size_t BufferSize, uint8_t Result[DIGEST_SIZE]) {
    CONTEXT Context;
    INIT_HASH(&Context);
    UPDATE_HASH(&Context, Buffer, BufferSize);
    FINISH_HASH(&Context, Result);
    CLEAR_HASH(&Context);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function initializes the given private SHA2 context.
 *
 * PARAMETERS:
 *     Context - Private data structure we use for the hash calculation.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void INIT_HASH(CONTEXT *Context) {
    MEMSET_INLINE(Context, 0, sizeof(CONTEXT));
    MEMCPY_INLINE(Context->State, InitialState, sizeof(Context->State));
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function safely clears the given private SHA2 context.
 *
 * PARAMETERS:
 *     Context - Private data structure we use for the hash calculation.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void CLEAR_HASH(CONTEXT *Context) {
    MEMSET_INLINE(Context, 0, sizeof(CONTEXT));
    COMPILER_BARRIER(Context);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function appends some extra buffer data to be used in the SHA2 hash calculation.
 *
 * PARAMETERS:
 *     Context - Private data structure we use for the hash calculation.
 *     Buffer - Input data, can be NULL as long as BufferSize==0.
 *     BufferSize - How many input bytes we have.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void UPDATE_HASH(CONTEXT *Context, const void *Buffer, size_t BufferSize) {
    const uint8_t *Input = Buffer;

    while (BufferSize) {
        size_t CopySize = BufferSize;

        if (Context->BufferSize + CopySize >= BUFFER_SIZE) {
            /* If we can fill up the internal buffer we're free to process whatever we previously
             * appended (plus the current data). */
            CopySize = BUFFER_SIZE - Context->BufferSize;
            memcpy(Context->RawBuffer + Context->BufferSize, Input, CopySize);
            ProcessBuffer(Context);
            Context->BufferSize = 0;
        } else {
            /* Otherwise, just update our pointers and leave the processing to the next time we're
             * called (or when finish is called). */
            memcpy(Context->RawBuffer + Context->BufferSize, Input, CopySize);
            Context->BufferSize += CopySize;
        }

        Context->TotalSize += CopySize;
        BufferSize -= CopySize;
        Input += CopySize;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function generates the SHA2 digest out of all previously appended data.
 *     After calling this function, you should reset the hash state before calling update again.
 *
 * PARAMETERS:
 *     Context - Private data structure we use for the hash calculation.
 *     Result - Where to store the digest.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void FINISH_HASH(CONTEXT *Context, uint8_t Result[DIGEST_SIZE]) {
    /* SHA2 mandates a one bit followed by all zeroes, so append the 1 bit + the first 7 zeroes. */
    Context->RawBuffer[Context->BufferSize++] = 0x80;

    /* After the padding we need to put the length, if we don't have space for that let's just
     * process the current block so we can zero its size. */
    if (Context->BufferSize > LENGTH_OFF) {
        memset(Context->RawBuffer + Context->BufferSize, 0, BUFFER_SIZE - Context->BufferSize);
        ProcessBuffer(Context);
        Context->BufferSize = 0;
    }

    /* Then clean up all remaining bytes. */
    memset(Context->RawBuffer + Context->BufferSize, 0, LENGTH_OFF - Context->BufferSize);

    /* Store the big endian length at the very end of the buffer. */
    Context->Buffer[14] = HIGH_BITS(Context->TotalSize);
    Context->Buffer[15] = LOW_BITS(Context->TotalSize);

    /* And now we have a filled buffer, so we process this current/last block. */
    ProcessBuffer(Context);

    /* And copy out the digest (the stored value is host-endian, but we need to swap it to big
     * endian before anything else). */
    for (int i = 0; i < DIGEST_WORDS; i++) {
        WORD Word = BSWAP(Context->State[i]);
        MEMCPY_INLINE(Result + i * sizeof(WORD), &Word, sizeof(WORD));
    }
}
