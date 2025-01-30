/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _KE_H_
#define _KE_H_

#include <generic/irql.h>
#include <generic/panic.h>
#include <generic/processor.h>

#define KE_EVENT_TYPE_NONE 0
#define KE_EVENT_TYPE_FREEZE 1

#if defined(ARCH_amd64)
#define KE_STACK_SIZE 0x2000
#else
#error "Undefined ARCH for the kernel module!"
#endif /* ARCH */

typedef struct {
    RtDList ListHeader;
    void *ImageBase;
    void *EntryPoint;
    uint32_t SizeOfImage;
    const char *ImageName;
} KeModule;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void *KiFindAcpiTable(const char Signature[4], int Index);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _KE_H_ */
