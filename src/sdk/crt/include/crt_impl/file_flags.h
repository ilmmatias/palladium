/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef CRT_IMPL_FILE_FLAGS_H
#define CRT_IMPL_FILE_FLAGS_H

#define __STDIO_FLAGS_READ 0x01
#define __STDIO_FLAGS_WRITE 0x02
#define __STDIO_FLAGS_APPEND 0x04
#define __STDIO_FLAGS_BINARY 0x08
#define __STDIO_FLAGS_EXCL 0x10
#define __STDIO_FLAGS_EXEC 0x20
#define __STDIO_FLAGS_ERROR 0x40
#define __STDIO_FLAGS_EOF 0x80
#define __STDIO_FLAGS_READING 0x100
#define __STDIO_FLAGS_WRITING 0x200

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

int __parse_open_mode(const char *mode);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CRT_IMPL_FILE_FLAGS_H */
