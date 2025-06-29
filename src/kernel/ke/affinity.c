/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/halp.h>
#include <kernel/ke.h>
#include <string.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function initializes an affinity mask, with all processors setup as usable.
 *
 * PARAMETERS:
 *     Mask - Which affinity mask struct to initialize.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void KeInitializeAffinity(KeAffinity *Mask) {
    Mask->Size = HalpOnlineProcessorCount;

    /* Handle head+middle bits. */
    memset((void *)Mask->Bits, 0xFF, Mask->Size >> 3);

    /* And set all remaining bits by hand (as to not overshoot). */
    for (uint64_t Bit = (Mask->Size >> 3) << 3; Bit < Mask->Size; Bit++) {
        Mask->Bits[Bit >> 6] |= 1ull << (Bit & 0x3F);
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function queries about the specified processor in the affinity mask.
 *
 * PARAMETERS:
 *     Mask - Affinity mask struct pointer.
 *     Number - Which processor to query about.
 *
 * RETURN VALUE:
 *     true if the processor is usable, false otherwise.
 *-----------------------------------------------------------------------------------------------*/
bool KeGetAffinityBit(KeAffinity *Mask, uint32_t Number) {
    if (Number >= Mask->Size) {
        return false;
    }

    uint64_t BitMask = 1ull << (Number & 0x3F);
    return (__atomic_load_n(&Mask->Bits[Number >> 6], __ATOMIC_RELAXED) & BitMask) == BitMask;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function sets the specified processor as usable in the affinity mask.
 *
 * PARAMETERS:
 *     Mask - Affinity mask struct pointer.
 *     Number - Which processor to set as usable.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void KeSetAffinityBit(KeAffinity *Mask, uint32_t Number) {
    if (Number < Mask->Size) {
        __atomic_fetch_or(&Mask->Bits[Number >> 6], 1ull << (Number & 0x3F), __ATOMIC_SEQ_CST);
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function sets the specified processor as unusable in the affinity mask.
 *
 * PARAMETERS:
 *     Mask - Affinity mask struct pointer.
 *     Number - Which processor to set as unusable.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void KeClearAffinityBit(KeAffinity *Mask, uint32_t Number) {
    if (Number < Mask->Size) {
        __atomic_fetch_and(&Mask->Bits[Number >> 6], ~(1ull << (Number & 0x3F)), __ATOMIC_SEQ_CST);
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function finds the first processor marked as usable in the affinity mask.
 *
 * PARAMETERS:
 *     Mask - Affinity mask struct pointer.
 *
 * RETURN VALUE:
 *     Either the number of the processor, or -1 if no processor was marked as usable.
 *-----------------------------------------------------------------------------------------------*/
uint32_t KeGetFirstAffinitySetBit(KeAffinity *Mask) {
    for (uint32_t Number = 0; Number < (Mask->Size + 63) >> 6; Number++) {
        uint64_t Word = __atomic_load_n(&Mask->Bits[Number], __ATOMIC_SEQ_CST);
        if (Word) {
            uint32_t Result = (Number << 6) + __builtin_ctzll(Word);
            return Result >= Mask->Size ? (uint32_t)-1 : Result;
        }
    }

    return (uint32_t)-1;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function finds the first processor marked as unusable in the affinity mask.
 *
 * PARAMETERS:
 *     Mask - Affinity mask struct pointer.
 *
 * RETURN VALUE:
 *     Either the number of the processor, or -1 if no processor was marked as unusable.
 *-----------------------------------------------------------------------------------------------*/
uint32_t KeGetFirstAffinityClearBit(KeAffinity *Mask) {
    for (uint32_t Number = 0; Number < (Mask->Size + 63) >> 6; Number++) {
        uint64_t Word = __atomic_load_n(&Mask->Bits[Number], __ATOMIC_SEQ_CST);
        if (Word != UINT64_MAX) {
            uint32_t Result = (Number << 6) + __builtin_ctzll(~Word);
            return Result >= Mask->Size ? (uint32_t)-1 : Result;
        }
    }

    return (uint32_t)-1;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function counts how many processors are marked as usable in the affinity mask.
 *
 * PARAMETERS:
 *     Mask - Affinity mask struct pointer.
 *
 * RETURN VALUE:
 *     Number of the processor marked as usable.
 *-----------------------------------------------------------------------------------------------*/
uint64_t KeCountAffinitySetBits(KeAffinity *Mask) {
    uint64_t Result = 0;
    uint32_t WordCount = (Mask->Size + 63) >> 6;

    for (uint32_t Number = 0; Number < WordCount - 1; Number++) {
        uint64_t Word = __atomic_load_n(&Mask->Bits[Number], __ATOMIC_SEQ_CST);
        Result += __builtin_popcountll(Word);
    }

    /* Separate handling for the last word; We need to unset the higher bits (so that popcount
     * ignores them) if the size isn't a multiple of 64. */
    uint64_t Word = __atomic_load_n(&Mask->Bits[WordCount - 1], __ATOMIC_SEQ_CST);
    uint32_t Remainder = Mask->Size & 63;
    if (Remainder) {
        Result += __builtin_popcountll(Word & ((1ull << Remainder) - 1));
    } else {
        Result += __builtin_popcountll(Word);
    }

    return Result;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function counts how many processors are marked as unusable in the affinity mask.
 *
 * PARAMETERS:
 *     Mask - Affinity mask struct pointer.
 *
 * RETURN VALUE:
 *     Number of the processor marked as unusable.
 *-----------------------------------------------------------------------------------------------*/
uint64_t KeCountAffinityClearBits(KeAffinity *Mask) {
    uint64_t Result = 0;
    uint32_t WordCount = (Mask->Size + 63) >> 6;

    for (uint32_t Number = 0; Number < WordCount - 1; Number++) {
        uint64_t Word = __atomic_load_n(&Mask->Bits[Number], __ATOMIC_SEQ_CST);
        Result += __builtin_popcountll(~Word);
    }

    /* Separate handling for the last word; Same logic as CountSetBits, but we first invert the word
     * (as we want to count clear instead of set). */
    uint64_t Word = __atomic_load_n(&Mask->Bits[WordCount - 1], __ATOMIC_SEQ_CST);
    uint32_t Remainder = Mask->Size & 63;
    if (Remainder) {
        Result += __builtin_popcountll(~Word & ((1ull << Remainder) - 1));
    } else {
        Result += __builtin_popcountll(~Word);
    }

    return Result;
}
