/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _AMD64_HAL_H_
#define _AMD64_HAL_H_

#include <amd64/regs.h>
#include <hal.h>

uint8_t HalInstallInterruptHandler(void (*Handler)(RegisterState *));

#endif /* _AMD64_HAL_H_ */
