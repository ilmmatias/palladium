/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function checks if a character is alphanumeric.
 *
 * PARAMETERS:
 *     ch - Character to check.
 *
 * RETURN VALUE:
 *     Result of the check.
 *-----------------------------------------------------------------------------------------------*/
int isalnum(int ch) {
    return (unsigned int)ch - 0x30 < 0x0A || (unsigned int)ch - 0x41 < 0x1A ||
           (unsigned int)ch - 0x61 < 0x1A;
}
