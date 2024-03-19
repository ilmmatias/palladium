/* SPDX-FileCopyrightText: (C) 2024 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _GRAPHICS_H_
#define _GRAPHICS_H_

#include <efi/spec.h>
#include <stdint.h>

EFI_STATUS OslpInitializeGraphics(
    void **Framebuffer,
    uint32_t *FramebufferWidth,
    uint32_t *FramebufferHeight,
    uint32_t *FramebufferPitch);

#endif /* _GRAPHICS_H_ */
