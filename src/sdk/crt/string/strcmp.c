#include <stddef.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function implements generic code to compare two C strings.
 *
 * PARAMETERS:
 *     lhs - Left-hand side of the expression.
 *     rhs - Right-hand side of the expression.
 *
 * RETURN VALUE:
 *     0 for equality, *lhs - *rhs if an inequality is found (where `lhs` and `rhs` would be
 *     pointers to where the inequality happened).
 *-----------------------------------------------------------------------------------------------*/
int strcmp(const char *lhs, const char *rhs) {
    while (*lhs && *rhs && *lhs == *rhs) {
        lhs++;
        rhs++;
    }

    return *lhs - *rhs;
}
