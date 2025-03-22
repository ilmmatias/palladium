/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef CRT_IMPL_OS_H
#define CRT_IMPL_OS_H

#include <stddef.h>
#include <stdint.h>

#if defined(ARCH_amd64)
#include <crt_impl/amd64/os.h>
#else
#error "Undefined ARCH for the CRT module!"
#endif /* ARCH */

#define __PAGE_SIZE (1ull << (__PAGE_SHIFT))

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

[[noreturn]] void __terminate_process(int res);

void *__create_lock(void);
void __delete_lock(void *handle);
void __acquire_lock(void *handle);
void __release_lock(void *handle);

void *__open_file(const char *filename, int mode);
void __close_file(void *handle);
int __read_file(void *handle, void *buffer, size_t size, size_t *read);
int __write_file(void *handle, const void *buffer, size_t size, size_t *wrote);
int __seek_file(void *handle, long offset, int origin);
int __tell_file(void *handle, long *offset);

void *__allocate_pages(size_t pages);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CRT_IMPL_OS_H */
