/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _INTELLISENSE_H_
#define _INTELLISENSE_H_

/* Just a fix for vscode not detecting true/false as valid keywords. */
#if !defined(__cplusplus) && defined(__INTELLISENSE__)
#define true 1
#define false 0
#endif

#endif /* _INTELLISENSE_H_ */
