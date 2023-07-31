/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function checks if a character is a hexadecimal digit (0-9, aA-fF).
 *
 * PARAMETERS:
 *     ch - Character to check.
 *
 * RETURN VALUE:
 *     Result of the check.
 *-----------------------------------------------------------------------------------------------*/
int isxdigit(int ch) {
    return (unsigned int)ch - 0x30 < 0x0A || (unsigned int)ch - 0x41 < 0x06 ||
           (unsigned char)ch - 0x61 < 0x06;
}
