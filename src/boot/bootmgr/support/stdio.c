/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <crt_impl.h>
#include <display.h>
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
        BmSetColor(0x4F);
        BmInitDisplay();

        BmPutString("An error occoured while trying to setup the boot manager environment.\n");
        BmPutString("Could not setup one or more of the Standard I/O files.\n");

        while (1)
            ;
    }

    setbuf(stdin, NULL);
    stdout->buffer_type = _IOLBF;
    stderr->buffer_type = _IOLBF;
}
