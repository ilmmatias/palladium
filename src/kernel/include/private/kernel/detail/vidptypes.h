/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _KERNEL_DETAIL_VIDPTYPES_H_
#define _KERNEL_DETAIL_VIDPTYPES_H_

#include <kernel/detail/vidtypes.h>

/* clang-format off */
#if __has_include(ARCH_MAKE_INCLUDE_PATH(kernel/detail, vidptypes.h))
#include ARCH_MAKE_INCLUDE_PATH(kernel/detail, vidptypes.h)
#endif /* __has__include */
/* clang-format on */

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

#endif /* _KERNEL_DETAIL_VIDPTYPES_H_ */
