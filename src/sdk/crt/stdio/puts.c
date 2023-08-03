#include <string.h>

void __put_stdout(const void *buffer, int size, void *context);

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function outputs a string followed by a new-line character to the screen.
 *
 * PARAMETERS:
 *     ch - Which character to write.
 *
 * RETURN VALUE:
 *     How many characters we wrote on success, EOF on failure.
 *-----------------------------------------------------------------------------------------------*/
int puts(const char *str) {
    size_t len = strlen(str);
    char nl = '\n';
    __put_stdout(str, len, NULL);
    __put_stdout(&nl, 1, NULL);
    return len + nl;
}
