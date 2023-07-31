/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function checks if a character is a blank character.
 *
 * PARAMETERS:
 *     ch - Character to check.
 *
 * RETURN VALUE:
 *     Result of the check.
 *-----------------------------------------------------------------------------------------------*/
int isblank(int ch) {
    return ch == 0x09 || ch == 0x20;
}
