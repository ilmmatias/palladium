/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _FONT_H_
#define _FONT_H_

#include <stdint.h>

typedef struct {
    int Offset;
    int Width;
    int Height;
    int Left;
    int Top;
    int Advance;
} BiFontGlyph;

typedef struct {
    int Ascender;
    int Descender;
    int Height;
    const BiFontGlyph *GlyphInfo;
    const uint8_t *GlyphData;
} BiFontData;

extern const BiFontData BiFont;

#endif /* _FONT_H_ */
