/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _KERNEL_DETAIL_OBFUNCS_H_
#define _KERNEL_DETAIL_OBFUNCS_H_

#include <kernel/detail/obtypes.h>

/* clang-format off */
#if __has_include(ARCH_MAKE_INCLUDE_PATH(kernel/detail, obfuncs.h))
#include ARCH_MAKE_INCLUDE_PATH(kernel/detail, obfuncs.h)
#endif /* __has__include */
/* clang-format on */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

extern ObType ObpDirectoryType;
extern ObType ObpThreadType;

extern ObDirectory ObRootDirectory;

void *ObCreateObject(ObType *Type, char Tag[4]);
void ObReferenceObject(void *Body);
void ObDereferenceObject(void *Body);

ObDirectory *ObCreateDirectory(void);
bool ObInsertIntoDirectory(ObDirectory *Directory, const char *Name, void *Object);
void ObRemoveFromDirectory(void *Object);
void *ObLookupDirectoryEntryByName(ObDirectory *Directory, const char *Name);
void *ObLookupDirectoryEntryByIndex(ObDirectory *Directory, size_t Index, char **Name);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _KERNEL_DETAIL_OBFUNCS_H_ */
