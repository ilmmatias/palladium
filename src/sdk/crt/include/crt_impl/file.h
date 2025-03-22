/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef CRT_IMPL_FILE_H
#define CRT_IMPL_FILE_H

#include <stddef.h>
#include <stdint.h>

typedef uint64_t fpos_t;

typedef struct {
    void *handle;
    void *lock_handle;
    bool user_lock;
    char *buffer;
    bool user_buffer;
    int buffer_type;
    size_t buffer_size;
    size_t buffer_read;
    size_t buffer_pos;
    char unget_buffer[16];
    size_t unget_size;
    int flags;
} FILE;

#endif /* CRT_IMPL_FILE_H */
