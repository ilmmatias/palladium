/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function checks if a character has a graphical representation.
 *
 * PARAMETERS:
 *     ch - Character to check.
 *
 * RETURN VALUE:
 *     Result of the check.
 *-----------------------------------------------------------------------------------------------*/
int isgraph(int ch) {
    return (unsigned int)ch - 0x21 < 0x5E;
}
