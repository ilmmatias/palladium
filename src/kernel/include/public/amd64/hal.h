/* SPDX-FileCopyrightText: (C) 2023-2024 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _AMD64_HAL_H_
#define _AMD64_HAL_H_

#include <amd64/regs.h>
#include <hal.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

int HalInstallInterruptHandlerAt(
    uint8_t Vector,
    void (*Handler)(HalRegisterState *),
    KeIrql TargetIrql);

uint8_t HalInstallInterruptHandler(void (*Handler)(HalRegisterState *), KeIrql TargetIrql);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _AMD64_HAL_H_ */
