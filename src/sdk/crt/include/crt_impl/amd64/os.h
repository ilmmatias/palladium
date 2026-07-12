/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

/* IWYU pragma: private, include <crt_impl/os.h> */

#ifndef CRT_IMPL_AMD64_OS_H
#define CRT_IMPL_AMD64_OS_H

#define __PAGE_SHIFT 12
#define __ABORT __asm__ volatile("hlt")

#endif /* CRT_IMPL_AMD64_OS_H */
