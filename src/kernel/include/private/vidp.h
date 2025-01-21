/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _VIDP_H_
#define _VIDP_H_

#include <ki.h>
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

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void VidpInitialize(KiLoaderBlock *LoaderBlock);
void VidpAcquireOwnership(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _VIDP_H_ */
