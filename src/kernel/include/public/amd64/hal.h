/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _AMD64_HAL_H_
#define _AMD64_HAL_H_

#include <amd64/regs.h>
#include <hal.h>

int HalInstallInterruptHandlerAt(uint8_t Vector, void (*Handler)(HalRegisterState *));
uint8_t HalInstallInterruptHandler(void (*Handler)(HalRegisterState *));

#endif /* _AMD64_HAL_H_ */
