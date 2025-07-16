/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _OS_CONTAINING_RECORD_H_
#define _OS_CONTAINING_RECORD_H_

#include <stddef.h>
#include <stdint.h>

#ifndef CONTAINING_RECORD
#define CONTAINING_RECORD(address, type, field) \
    ((type *)((char *)(address) - (uintptr_t)(&((type *)0)->field)))
#endif /* CONTAINING_RECORD */

#endif /* _OS_CONTAINING_RECORD_H_ */
