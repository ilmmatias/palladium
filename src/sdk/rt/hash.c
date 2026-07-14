/* SPDX-FileCopyrightText: (C) 2023-2026 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <stddef.h>
#include <stdint.h>

#define PRIME32_1 UINT32_C(0x9E3779B1)
#define PRIME32_2 UINT32_C(0x85EBCA77)
#define PRIME32_3 UINT32_C(0xC2B2AE3D)
#define PRIME32_4 UINT32_C(0x27D4EB2F)
#define PRIME32_5 UINT32_C(0x165667B1)

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function reads one little-endian 32-bit value without requiring an aligned buffer.
 *
 * PARAMETERS:
 *     Buffer - Data to be read.
 *
 * RETURN VALUE:
 *     The decoded 32-bit value.
 *-----------------------------------------------------------------------------------------------*/
static uint32_t ReadUInt32(const uint8_t *Buffer) {
    return Buffer[0] | ((uint32_t)Buffer[1] << 8) | ((uint32_t)Buffer[2] << 16) |
           ((uint32_t)Buffer[3] << 24);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function gives a 32-bits hash of some data, using a fast (though not cryptographically
 *     secure) algorithm.
 *
 * PARAMETERS:
 *     Buffer - Data to be hashed.
 *     Length - Size of the data.
 *
 * RETURN VALUE:
 *     32-bits hash of the data.
 *-----------------------------------------------------------------------------------------------*/
uint32_t RtGetHash(const void *Buffer, size_t Length) {
    /* We follow the 32-bits version of xxHash, as described in
     * https://github.com/Cyan4973/xxHash/blob/dev/doc/xxhash_spec.md.
     * The Seed is assumed to be zero, other then that, this algorithm is a 1-to-1 match with
     * what's described in the reference. */

    const uint8_t *Data = Buffer;
    uint32_t Accum1 = PRIME32_1 + PRIME32_2;
    uint32_t Accum2 = PRIME32_2;
    uint32_t Accum3 = 0;
    uint32_t Accum4 = -PRIME32_1;
    uint32_t Result = Length;

    if (Length < 16) {
        Result += PRIME32_5;
    } else {
        while (Length >= 16) {
            Accum1 += ReadUInt32(Data) * PRIME32_2;
            Accum1 = (Accum1 << 13) | (Accum1 >> 19);
            Accum1 *= PRIME32_1;

            Accum2 += ReadUInt32(Data + 4) * PRIME32_2;
            Accum2 = (Accum2 << 13) | (Accum2 >> 19);
            Accum2 *= PRIME32_1;

            Accum3 += ReadUInt32(Data + 8) * PRIME32_2;
            Accum3 = (Accum3 << 13) | (Accum3 >> 19);
            Accum3 *= PRIME32_1;

            Accum4 += ReadUInt32(Data + 12) * PRIME32_2;
            Accum4 = (Accum4 << 13) | (Accum4 >> 19);
            Accum4 *= PRIME32_1;

            Data += 16;
            Length -= 16;
        }

        Result += ((Accum1 << 1) | (Accum1 >> 31)) + ((Accum2 << 7) | (Accum2 >> 25)) +
                  ((Accum3 << 12) | (Accum3 >> 20)) + ((Accum4 << 18) | (Accum4 >> 14));
    }

    while (Length >= 4) {
        Result += ReadUInt32(Data) * PRIME32_3;
        Result = ((Result << 17) | (Result >> 15)) * PRIME32_4;
        Data += 4;
        Length -= 4;
    }

    while (Length-- >= 1) {
        Result += *(Data++) * PRIME32_5;
        Result = ((Result << 11) | (Result >> 21)) * PRIME32_1;
    }

    Result ^= Result >> 15;
    Result *= PRIME32_2;
    Result ^= Result >> 13;
    Result *= PRIME32_3;
    Result ^= Result >> 16;

    return Result;
}
