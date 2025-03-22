/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef ERRNO_H
#define ERRNO_H

#include <crt_impl/os_errno.h>

#define errno (*__errno())

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

int *__errno(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* ERRNO_H */
