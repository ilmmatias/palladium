/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _AMD64_HALP_H_
#define _AMD64_HALP_H_

#include <halp.h>

void HalpInitializeIdt(void);
void HalpInitializeIoapic(void);
void HalpInitializeApic(void);
void HalpSendEoi(void);

#endif /* _AMD64_HALP_H_ */
