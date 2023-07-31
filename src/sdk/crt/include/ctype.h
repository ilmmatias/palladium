/* SPDX-FileCopyrightText: (C) 2023 yuuma03
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef CTYPE_H
#define CTYPE_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

int isalnum(int ch);
int isalpha(int ch);
int islower(int ch);
int isupper(int ch);
int isdigit(int ch);
int isxdigit(int ch);
int iscntrl(int ch);
int isgraph(int ch);
int isspace(int ch);
int isblank(int ch);
int isprint(int ch);
int ispunct(int ch);

int tolower(int ch);
int toupper(int ch);

#ifdef __cplusplus
}
#endif

#endif /* CTYPE_H */
