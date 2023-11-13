/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _BITMAP_H_
#define _BITMAP_H_

#include <stdint.h>

typedef struct __attribute__((packed)) {
    uint16_t Signature;
    uint32_t FileSize;
    uint16_t Reserved0;
    uint16_t Reserved1;
    uint32_t DataOffset;
    uint32_t HeaderSize;
    uint32_t Width;
    uint32_t Height;
    uint16_t ColorPlanes;
    uint16_t Bpp;
    uint32_t Compression;
    uint32_t ImageSize;
    uint32_t HorResolution;
    uint32_t VerResolution;
    uint32_t Colors;
    uint32_t ImportantColors;
    uint32_t ColorMasks[4];
} BitmapHeader;

#endif /* _BITMAP_H_ */
