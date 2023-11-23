/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <rt/bitmap.h>
#include <string.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function initializes a bitmap. We do not clear (or modify) the bitmap, you'll probably
 *     want to use RtClearAllBits or RtSetAllBits following RtInitializeBitmap.
 *
 * PARAMETERS:
 *     Header - Bitmap header struct.
 *     Buffer - Pointer to the bitmap array. The size of the buffer needs to be a multiple of
 *              sizeof(uint64_t).
 *     NumberOfBits - How many bits we have; Make sure this fits in the buffer.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void RtInitializeBitmap(RtBitmap *Header, uint64_t *Buffer, uint64_t NumberOfBits) {
    Header->Buffer = Buffer;
    Header->NumberOfBits = NumberOfBits;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function zeroes out the specified bit in the given bitmap.
 *
 * PARAMETERS:
 *     Header - Bitmap header struct.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void RtClearBit(RtBitmap *Header, uint64_t Bit) {
    Header->Buffer[Bit >> 6] &= ~(1ull << (Bit & 0x3F));
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function zeroes out a range in the given bitmap.
 *
 * PARAMETERS:
 *     Header - Bitmap header struct.
 *     Start - First bit in the range.
 *     NumberOfBits - Size of the range.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void RtClearBits(RtBitmap *Header, uint64_t Start, uint64_t NumberOfBits) {
    uint64_t *Buffer = Header->Buffer + (Start >> 6);
    uint64_t LeadingBits = Start & 0x3F;
    uint64_t TrailingBits = 64 - LeadingBits;

    /* Try to align the starting bit and the buffer, so that we can use memset() on multiple
       uint64_ts. */
    if (LeadingBits) {
        uint64_t Mask = UINT64_MAX << LeadingBits;

        if (NumberOfBits < TrailingBits) {
            TrailingBits -= NumberOfBits;
            *Buffer &= ~(Mask << TrailingBits >> TrailingBits);
            return;
        }

        *(Buffer++) &= ~Mask;
        NumberOfBits -= TrailingBits;
    }

    memset(Buffer, 0, (NumberOfBits >> 6) << 3);
    Buffer += NumberOfBits >> 6;
    TrailingBits = NumberOfBits & 0x3F;

    /* Clear out anything remaining. */
    if (TrailingBits) {
        *Buffer &= ~(UINT64_MAX >> (64 - TrailingBits));
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function zeroes out the entire bitmap buffer.
 *
 * PARAMETERS:
 *     Header - Bitmap header struct.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void RtClearAllBits(RtBitmap *Header) {
    memset(Header->Buffer, 0, Header->NumberOfBits >> 3);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function sets the specified bit in the given bitmap to 1.
 *
 * PARAMETERS:
 *     Header - Bitmap header struct.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void RtSetBit(RtBitmap *Header, uint64_t Bit) {
    Header->Buffer[Bit >> 6] |= 1ull << (Bit & 0x3F);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function sets out a range in the given bitmap to 1.
 *
 * PARAMETERS:
 *     Header - Bitmap header struct.
 *     Start - First bit in the range.
 *     NumberOfBits - Size of the range.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void RtSetBits(RtBitmap *Header, uint64_t Start, uint64_t NumberOfBits) {
    uint64_t *Buffer = Header->Buffer + (Start >> 6);
    uint64_t LeadingBits = Start & 0x3F;
    uint64_t TrailingBits = 64 - LeadingBits;

    /* Try to align the starting bit and the buffer, so that we can use memset() on multiple
       uint64_ts. */
    if (LeadingBits) {
        uint64_t Mask = UINT64_MAX << LeadingBits;

        if (NumberOfBits < TrailingBits) {
            TrailingBits -= NumberOfBits;
            *Buffer |= Mask << TrailingBits >> TrailingBits;
            return;
        }

        *(Buffer++) |= Mask;
        NumberOfBits -= TrailingBits;
    }

    memset(Buffer, 0xFF, (NumberOfBits >> 6) << 3);
    Buffer += NumberOfBits >> 6;
    TrailingBits = NumberOfBits & 0x3F;

    /* Clear out anything remaining. */
    if (TrailingBits) {
        *Buffer |= UINT64_MAX >> (64 - TrailingBits);
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function sets all bits of the bitmap buffer to 1.
 *
 * PARAMETERS:
 *     Header - Bitmap header struct.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void RtSetAllBits(RtBitmap *Header) {
    memset(Header->Buffer, 0xFF, Header->NumberOfBits >> 3);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function counts how many equal bits in a row we have.
 *
 * PARAMETERS:
 *     Header - Bitmap header struct.
 *     Offset - Where in the bitmap we should start.
 *     NumberOfBits - The max amount of bits we should count.
 *     Inverse - 1 for set (1) bits, otherwise, we'll count clear (0) bits.
 *
 * RETURN VALUE:
 *     Amount of clear bits in a row.
 *-----------------------------------------------------------------------------------------------*/
static uint64_t CountBitRow(RtBitmap *Header, uint64_t Offset, uint64_t NumberOfBits, int Inverse) {
    uint64_t *Buffer = Header->Buffer + (Offset >> 6);
    uint64_t *End = Buffer + ((NumberOfBits + 63) >> 6);

    /* Do not consider anything before the given offset */
    uint64_t LeadingBits = Offset & 0x3F;
    uint64_t Value = (Inverse ? ~*Buffer : *Buffer) >> LeadingBits << LeadingBits;

    /* Read up all zeroes we can; We only need to ctz() if we find something that's not empty. */
    while (!Value && Buffer + 1 < End) {
        Value = Inverse ? ~*(++Buffer) : *(++Buffer);
    }

    if (!Value) {
        return NumberOfBits;
    }

    uint64_t Size = ((uint64_t)(Buffer - Header->Buffer) << 6) - Offset + __builtin_ctzll(Value);
    return Size > NumberOfBits ? NumberOfBits : Size;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries finding the first range of equal bits of the given size in the bitmap.
 *
 * PARAMETERS:
 *     Header - Bitmap header struct.
 *     Hint - Where to start looking for the range.
 *     NumberOfBits - How many bits we need to find.
 *     Inverse - 1 for set (1) bits, otherwise, we'll count clear (0) bits.
 *
 * RETURN VALUE:
 *     The number/index of the first bit in the range, or -1 on failure.
 *-----------------------------------------------------------------------------------------------*/
static uint64_t FindBitRow(RtBitmap *Header, uint64_t Hint, uint64_t NumberOfBits, int Inverse) {
    if (Hint >= Header->NumberOfBits) {
        Hint = 0;
    }

    if (NumberOfBits > Header->NumberOfBits) {
        return -1;
    } else if (!NumberOfBits) {
        return Hint;
    }

    /* Two attempts, one from the hint to the end, and one from the start to the hint. */
    int Retry = (Hint != 0) + 1;
    uint64_t Offset = Hint;
    uint64_t End = Header->NumberOfBits;

    do {
        while (Offset + NumberOfBits < End) {
            Offset += CountBitRow(Header, Offset, Header->NumberOfBits - Offset, !Inverse);
            if (Offset + NumberOfBits > End) {
                break;
            }

            uint64_t Size = CountBitRow(Header, Offset, NumberOfBits, Inverse);
            if (Size >= NumberOfBits) {
                return Offset;
            }

            Offset += Size;
        }

        Retry--;
        if (Retry) {
            Offset = 0;
            End = Hint;
        }
    } while (Retry);

    return -1;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries finding the first range of clear bits of the given size in the
 *     bitmap.
 *
 * PARAMETERS:
 *     Header - Bitmap header struct.
 *     Hint - Where to start looking for the range.
 *     NumberOfBits - How many bits we need to find.
 *
 * RETURN VALUE:
 *     The number/index of the first bit in the range, or -1 on failure.
 *-----------------------------------------------------------------------------------------------*/
uint64_t RtFindClearBits(RtBitmap *Header, uint64_t Hint, uint64_t NumberOfBits) {
    return FindBitRow(Header, Hint, NumberOfBits, 0);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries finding the first range of clear bits of the given size in the
 *     bitmap, and sets all bits in the range to 1 before returning.
 *
 * PARAMETERS:
 *     Header - Bitmap header struct.
 *     Hint - Where to start looking for the range.
 *     NumberOfBits - How many bits we need to find.
 *
 * RETURN VALUE:
 *     The number/index of the first bit in the range, or -1 on failure.
 *-----------------------------------------------------------------------------------------------*/
uint64_t RtFindClearBitsAndSet(RtBitmap *Header, uint64_t Hint, uint64_t NumberOfBits) {
    uint64_t Start = FindBitRow(Header, Hint, NumberOfBits, 0);

    if (Start != (uint64_t)-1) {
        RtSetBits(Header, Start, NumberOfBits);
    }

    return Start;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries finding the first range of set (1) bits of the given size in the bitmap.
 *
 * PARAMETERS:
 *     Header - Bitmap header struct.
 *     Hint - Where to start looking for the range.
 *     NumberOfBits - How many bits we need to find.
 *
 * RETURN VALUE:
 *     The number/index of the first bit in the range, or -1 on failure.
 *-----------------------------------------------------------------------------------------------*/
uint64_t RtFindSetBits(RtBitmap *Header, uint64_t Hint, uint64_t NumberOfBits) {
    return FindBitRow(Header, Hint, NumberOfBits, 1);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries finding the first range of set (1) bits of the given size in the bitmap,
 *     and clears all bits in the range before returning.
 *
 * PARAMETERS:
 *     Header - Bitmap header struct.
 *     Hint - Where to start looking for the range.
 *     NumberOfBits - How many bits we need to find.
 *
 * RETURN VALUE:
 *     The number/index of the first bit in the range, or -1 on failure.
 *-----------------------------------------------------------------------------------------------*/
uint64_t RtFindSetBitsAndClear(RtBitmap *Header, uint64_t Hint, uint64_t NumberOfBits) {
    uint64_t Start = FindBitRow(Header, Hint, NumberOfBits, 1);

    if (Start != (uint64_t)-1) {
        RtClearBits(Header, Start, NumberOfBits);
    }

    return Start;
}
