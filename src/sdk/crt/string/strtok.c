#include <stddef.h>
#include <string.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function implements generic code to find the next set of characters after a delimeter
 *     from the set.
 *
 * PARAMETERS:
 *     str - Either a string to analyze, or NULL to use the previous context.
 *     delim - Delimiter set.
 *
 * RETURN VALUE:
 *     Position of the next token, or NULL if there are no more tokens.
 *-----------------------------------------------------------------------------------------------*/
char *strtok(char *str, const char *delim) {
    static char *Context = NULL;

    if (!Context) {
        Context = str;
    }

    if (!Context) {
        return NULL;
    }

    Context += strspn(Context, delim);
    if (!*Context) {
        return NULL;
    }

    char *Token = Context;
    Context += strcspn(Context, delim);

    if (*Context) {
        *(Context++) = 0;
    } else {
        Context = NULL;
    }

    return Token;
}
