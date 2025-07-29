/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/hal.h>
#include <kernel/intrin.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function fill in the specified buffer using data from the PCI configuration space.
 *
 * PARAMETERS:
 *     Bus - Bus number.
 *     Slot - Device number.
 *     Function - Function number.
 *     Offset - Register offset inside the device config space.
 *     Buffer - Buffer to use as target.
 *     Size - Size of the buffer in bytes.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void HalReadPciConfigurationSpace(
    uint32_t Bus,
    uint32_t Slot,
    uint32_t Function,
    uint32_t Offset,
    void *Buffer,
    size_t Size) {
    /* Pre-calculate the "base" address/dword for accesing the target config space. */
    uint8_t *ByteBuffer = Buffer;
    uint32_t Address =
        0x80000000 | ((Function & 0x07) << 8) | ((Slot & 0x1F) << 11) | ((Bus & 0xFF) << 16);

    /* Align the offset so that we can loop over 4-byte blocks next up. */
    while (Size && (Offset & 3)) {
        WritePortDWord(0xCF8, Address | (Offset & 0xFC));
        *ByteBuffer++ = ReadPortByte(0xCFC + (Offset & 3));
        Size--;
        Offset++;
    }

    /* Now just read as many dwords as we can. */
    while (Size >= 4) {
        WritePortDWord(0xCF8, Address | Offset);
        uint32_t Data = ReadPortDWord(0xCFC);
        *(uint32_t *)ByteBuffer = Data;
        ByteBuffer += 4;
        Size -= 4;
        Offset += 4;
    }

    /* And wrap up by reading individual bytes again. */
    while (Size) {
        WritePortDWord(0xCF8, Address | Offset);
        *ByteBuffer++ = ReadPortByte(0xCFC + (Offset & 3));
        Size--;
        Offset++;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function writes data into the PCI configuration space using the specified buffer.
 *
 * PARAMETERS:
 *     Bus - Bus number.
 *     Slot - Device number.
 *     Function - Function number.
 *     Offset - Register offset inside the device config space.
 *     Buffer - Buffer to use as source.
 *     Size - Size of the buffer in bytes.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void HalWritePciConfigurationSpace(
    uint32_t Bus,
    uint32_t Slot,
    uint32_t Function,
    uint32_t Offset,
    const void *Buffer,
    size_t Size) {
    /* Pre-calculate the "base" address/dword for accesing the target config space. */
    const uint8_t *ByteBuffer = Buffer;
    uint32_t Address =
        0x80000000 | ((Function & 0x07) << 8) | ((Slot & 0x1F) << 11) | ((Bus & 0xFF) << 16);

    /* Align the offset so that we can loop over 4-byte blocks next up. */
    while (Size && (Offset & 3)) {
        WritePortDWord(0xCF8, Address | (Offset & 0xFC));
        WritePortByte(0xCFC + (Offset & 3), *ByteBuffer++);
        Size--;
        Offset++;
    }

    /* Now just write as many dwords as we can. */
    while (Size >= 4) {
        WritePortDWord(0xCF8, Address | Offset);
        WritePortDWord(0xCFC, *(uint32_t *)ByteBuffer);
        ByteBuffer += 4;
        Size -= 4;
        Offset += 4;
    }

    /* And wrap up by writing individual bytes again. */
    while (Size) {
        WritePortDWord(0xCF8, Address | Offset);
        WritePortByte(0xCFC + (Offset & 3), *ByteBuffer++);
        Size--;
        Offset++;
    }
}
