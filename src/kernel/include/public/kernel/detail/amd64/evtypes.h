/* SPDX-FileCopyrightText: (C) 2026 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

/* IWYU pragma: private, include <kernel/ev.h> */

#ifndef _KERNEL_DETAIL_AMD64_EVTYPES_H_
#define _KERNEL_DETAIL_AMD64_EVTYPES_H_

#define EV_AMD64_MUTEX_SIZE 80
#define EV_AMD64_MUTEX_OWNER_LIST_OFFSET 40
#define EV_AMD64_MUTEX_RECURSION_OFFSET 56
#define EV_AMD64_MUTEX_CONTENTION_OFFSET 64
#define EV_AMD64_MUTEX_OWNER_OFFSET 72

#endif /* _KERNEL_DETAIL_AMD64_EVTYPES_H_ */
