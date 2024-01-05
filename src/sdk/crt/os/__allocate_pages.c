/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <stddef.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function requests the OS for more memory.
 *
 * PARAMETERS:
 *     pages - How many pages (multiple of __PAGE_SIZE) we need.
 *
 * RETURN VALUE:
 *     Pointer to the first page on success, NULL otherwise.
 *-----------------------------------------------------------------------------------------------*/
void *__allocate_pages(size_t pages) {
    (void)pages;
    return NULL;
}
