/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _VIDP_H_
#define _VIDP_H_

#include <vid.h>

typedef struct {
    int Offset;
    int Width;
    int Height;
    int Left;
    int Top;
    int Advance;
} VidpFontGlyph;

typedef struct {
    int Ascender;
    int Descender;
    int Height;
    const VidpFontGlyph *GlyphInfo;
    const uint8_t *GlyphData;
} VidpFontData;

void VidpInitialize(void *LoaderData);

#endif /* _VIDP_H_ */
