/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef CRT_IMPL_H
#define CRT_IMPL_H

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

struct FILE {
    void *handle;
    size_t file_size;
    size_t file_pos;
    char *buffer;
    int user_buffer;
    int buffer_type;
    size_t buffer_size;
    size_t buffer_read;
    size_t buffer_pos;
    size_t buffer_file_pos;
    char unget_buffer[16];
    size_t unget_size;
    int flags;
};

struct fpos_t {
    size_t file_pos;
};

#ifndef __CRT_STDIO_H

#if defined(ARCH_x86) || defined(ARCH_amd64)
#define __PAGE_SHIFT 12
#else
#error "Undefined ARCH for the CRT module!"
#endif /* ARCH */

#define __PAGE_SIZE (1ull << (__PAGE_SHIFT))

#define __STDIO_FLAGS_READ 0x01
#define __STDIO_FLAGS_WRITE 0x02
#define __STDIO_FLAGS_APPEND 0x04
#define __STDIO_FLAGS_UPDATE 0x08
#define __STDIO_FLAGS_NO_OVERWRITE 0x10
#define __STDIO_FLAGS_ERROR 0x20
#define __STDIO_FLAGS_EOF 0x40
#define __STDIO_FLAGS_READING 0x80
#define __STDIO_FLAGS_WRITING 0x100

int __parse_fopen_mode(const char *mode);
void *__fopen(const char *filename, int mode, size_t *length);
void __fclose(void *handle);
int __fread(void *handle, size_t pos, void *buffer, size_t size, size_t *read);
int __fwrite(void *handle, size_t pos, const void *buffer, size_t size, size_t *wrote);

int __vprintf(
    const char *format,
    va_list vlist,
    void *context,
    void (*put_buf)(const void *buffer, int size, void *context));

int __vscanf(
    const char *format,
    va_list vlist,
    void *context,
    int (*read_ch)(void *context),
    void (*unread_ch)(void *context, int ch));

void *__allocate_pages(size_t pages);

double __strtod_hex(const char *str, double sign);
double __strtod_dec(const char *str, double sign);

uint64_t __rand64(void);
void __srand64(uint64_t seed);

#endif /* __CRT_STDIO_H */

#endif /* CRT_IMPL_H */
