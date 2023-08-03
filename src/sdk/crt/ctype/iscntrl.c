/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function checks if a character is a control character.
 *
 * PARAMETERS:
 *     ch - Character to check.
 *
 * RETURN VALUE:
 *     Result of the check.
 *-----------------------------------------------------------------------------------------------*/
int iscntrl(int ch) {
    return (unsigned int)ch < 0x20 || ch == 0x7F;
}