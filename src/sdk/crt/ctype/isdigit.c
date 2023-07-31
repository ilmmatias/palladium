/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function checks if a character is a digit.
 *
 * PARAMETERS:
 *     ch - Character to check.
 *
 * RETURN VALUE:
 *     Result of the check.
 *-----------------------------------------------------------------------------------------------*/
int isdigit(int ch) {
    return (unsigned int)ch - 0x30 < 0x0A;
}
