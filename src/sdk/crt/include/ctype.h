/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef CTYPE_H
#define CTYPE_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Character classification functions. */
int isalnum(int c);
int isalpha(int c);
int isblank(int c);
int iscntrl(int c);
int isdigit(int c);
int isgraph(int c);
int islower(int c);
int isprint(int c);
int ispunct(int c);
int isspace(int c);
int isupper(int c);
int isxdigit(int c);

/* Character case mapping functions. */
int tolower(int c);
int toupper(int c);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CTYPE_H */
