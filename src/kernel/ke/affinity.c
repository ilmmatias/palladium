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
    memset((void *)Mask->Bits, 0xFF, (Mask->Size + 7) >> 3);
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
    __atomic_fetch_or(&Mask->Bits[Number >> 6], 1ull << (Number & 0x3F), __ATOMIC_SEQ_CST);
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
    __atomic_fetch_and(&Mask->Bits[Number >> 6], ~(1ull << (Number & 0x3F)), __ATOMIC_SEQ_CST);
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
    for (uint32_t Number = 0; Number < (Mask->Size + 7) >> 3; Number++) {
        uint64_t Word = __atomic_load_n(&Mask->Bits[Number], __ATOMIC_SEQ_CST);
        if (Word) {
            uint32_t Result = (Number << 6) + __builtin_ctzll(Word);
            return Result > Mask->Size ? (uint32_t)-1 : Result;
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
    for (uint32_t Number = 0; Number < (Mask->Size + 7) >> 3; Number++) {
        uint64_t Word = __atomic_load_n(&Mask->Bits[Number], __ATOMIC_SEQ_CST);
        if (Word != UINT64_MAX) {
            uint32_t Result = (Number << 6) + __builtin_ctzll(~Word);
            return Result > Mask->Size ? (uint32_t)-1 : Result;
        }
    }

    return (uint32_t)-1;
}
