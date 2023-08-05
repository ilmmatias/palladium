/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef CRT_IMPL_H
#define CRT_IMPL_H

#include <stdarg.h>
#include <stddef.h>

struct FILE {
    void *handle;
    size_t file_pos;
    char *buffer;
    int user_buffer;
    int buffer_type;
    size_t buffer_size;
    size_t buffer_pos;
    int flags;
};

#ifndef __CRT_STDIO_H
#define __STDIO_FLAGS_READ 0x01
#define __STDIO_FLAGS_WRITE 0x02
#define __STDIO_FLAGS_APPEND 0x04
#define __STDIO_FLAGS_UPDATE 0x08
#define __STDIO_FLAGS_NO_OVERWRITE 0x10
#define __STDIO_FLAGS_ERROR 0x20
#define __STDIO_FLAGS_EOF 0x40
#define __STDIO_FLAGS_READING 0x80
#define __STDIO_FLAGS_WRITING 0x100
#define __STDIO_FLAGS_HAS_BUFFER 0x200

int __parse_fopen_mode(const char *mode);
void *__fopen(const char *filename, int mode);
void __fclose(void *handle);
int __fread(void *handle, size_t pos, void *buffer, size_t size);
int __fwrite(void *handle, size_t pos, void *buffer, size_t size);

void __put_stdout(const void *buffer, int size, void *context);
int __vprintf(
    const char *format,
    va_list vlist,
    void *context,
    void (*put_buf)(const void *buffer, int size, void *context));

double __strtod_hex(const char *str, double sign);
double __strtod_dec(const char *str, double sign);
#endif /* __CRT_STDIO_H */

#endif /* CRT_IMPL_H */
