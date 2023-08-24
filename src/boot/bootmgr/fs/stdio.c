/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <boot.h>
#include <crt_impl.h>
#include <stdio.h>

struct FILE *stdin = NULL;
struct FILE *stdout = NULL;
struct FILE *stderr = NULL;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function sets up the screen-I/O related standard files.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void BmInitStdio(void) {
    stdin = fopen("console()", "r");
    stdout = fopen("console()", "w");
    stderr = fopen("console()", "w");

    if (!stdin || !stdout || !stderr) {
        BmPanic("An error occoured while trying to setup the boot manager environment.\n"
                "Could not setup one or more of the Standard I/O files.\n");
    }

    setbuf(stdin, NULL);
    setbuf(stderr, NULL);
    stdout->buffer_type = _IOLBF;
}
