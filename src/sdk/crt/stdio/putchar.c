#include <stddef.h>

void __put_stdout(const void *buffer, int size, void *context);

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function outputs a single character to the screen.
 *
 * PARAMETERS:
 *     ch - Which character to write.
 *
 * RETURN VALUE:
 *     Whatever stored in `ch` on success, EOF otherwise.
 *-----------------------------------------------------------------------------------------------*/
int putchar(int ch) {
    __put_stdout(&ch, 1, NULL);
    return ch;
}
