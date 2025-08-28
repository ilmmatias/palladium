/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
   SPDX-License-Identifier: GPL-3.0-or-later */

#include <config.h>
#include <console.h>
#include <ctype.h>
#include <file.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function skips contiguous whitespaces in the current line, prepping the line pointer
 *     for reading more data.
 *
 * PARAMETERS:
 *     Line - Pointer to the current position/line of the text.
 *     LineNumber - Pointer to the current line number.
 *
 * RETURN VALUE:
 *     The number of whitespace characters skipped.
 *-----------------------------------------------------------------------------------------------*/
static inline size_t SkipWhitespace(char *Line, size_t *LineNumber) {
    size_t Size = 0;

    while (Line[Size] && isspace(Line[Size])) {
        if (Line[Size] == '\n') {
            (*LineNumber)++;
        }

        Size++;
    }

    return Size;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function reads in a valid parameter name from the current line.
 *
 * PARAMETERS:
 *     Line - Pointer to the current position/line of the text.
 *     Name - Pointer to where we should store the parameter name.
 *
 * RETURN VALUE:
 *     Either the size of what we read, or 0 if we ran out of memory while allocating space for
 *     the name.
 *-----------------------------------------------------------------------------------------------*/
static inline size_t ReadName(char *Line, char **Name) {
    size_t Size = 0;
    while (Line[Size] && Line[Size] != '\n' && Line[Size] != '=' && !isspace(Line[Size])) {
        Size++;
    }

    EFI_STATUS Status = gBS->AllocatePool(EfiLoaderData, Size + 1, (VOID **)Name);
    if (Status != EFI_SUCCESS) {
        OslPrint("Failed to load the configuration file.\r\n");
        OslPrint("The system ran out of memory.\r\n");
        OslPrint("The boot process cannot continue.\r\n");
        return 0;
    }

    memcpy(*Name, Line, Size);
    (*Name)[Size] = 0;
    return Size;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function reads in a valid parameter value from the current line.
 *
 * PARAMETERS:
 *     Line - Pointer to the current position/line of the text.
 *     Value - Pointer to where we should store the parameter value.
 *
 * RETURN VALUE:
 *     Either the size of what we read, or 0 if we ran out of memory while allocating space for
 *     the value.
 *-----------------------------------------------------------------------------------------------*/
static inline size_t ReadValue(char *Line, char **Value) {
    size_t Size = 0;

    while (Line[Size] && Line[Size] != '\n' && !isspace(Line[Size])) {
        Size++;
    }

    EFI_STATUS Status = gBS->AllocatePool(EfiLoaderData, Size + 1, (VOID **)Value);
    if (Status != EFI_SUCCESS) {
        OslPrint("Failed to load the configuration file.\r\n");
        OslPrint("The system ran out of memory.\r\n");
        OslPrint("The boot process cannot continue.\r\n");
        return 0;
    }

    memcpy(*Value, Line, Size);
    (*Value)[Size] = 0;
    return Size;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function attempts to expand the capacity of the boot driver list.
 *
 * PARAMETERS:
 *     Config - Pointer to the boot configuration data.
 *
 * RETURN VALUE:
 *     Either true if we got more memory to expand it, or false if we failed to do so.
 *-----------------------------------------------------------------------------------------------*/
static bool ExpandBootDriverCapacity(OslConfig *Config) {
    char **BootDrivers = Config->BootDrivers;
    Config->BootDriverCapacity *= 2;
    EFI_STATUS Status = gBS->AllocatePool(
        EfiLoaderData, Config->BootDriverCapacity * sizeof(char *), (VOID **)&Config->BootDrivers);
    if (Status != EFI_SUCCESS) {
        OslPrint("Failed to load the configuration file.\r\n");
        OslPrint("The system ran out of memory.\r\n");
        OslPrint("The boot process cannot continue.\r\n");
        return false;
    }

    memcpy(Config->BootDrivers, BootDrivers, Config->BootDriverCount * sizeof(char *));
    return true;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function checks if the given boot driver is already in the list.
 *
 * PARAMETERS:
 *     Config - Pointer to the boot configuration data.
 *     Name - Which boot driver to check for.
 *
 * RETURN VALUE:
 *     true/false depending on if the driver is already in the list or not.
 *-----------------------------------------------------------------------------------------------*/
static bool CheckBootDrivers(OslConfig *Config, const char *Name) {
    /* This should be okay as long as we're right that the boot driver list is going to be always
     * small; because if we're wrong, this is going to be O(N^2)... */

    for (size_t i = 0; i < Config->BootDriverCount; i++) {
        if (!strcmp(Config->BootDrivers[i], Name)) {
            return true;
        }
    }

    return false;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function loads in the boot configuration file, filling in the data required to
 *     continue the initialization of the system.
 *
 * PARAMETERS:
 *     Path - Full path to the configuration file (inside the boot volume).
 *     Config - Pointer to where we should store the boot configuration data.
 *
 * RETURN VALUE:
 *     true if we loaded the configuration file and filled in at least the minimum required
 *     fields (in specific, if we at least filled in the boot driver list), false otherwise.
 *-----------------------------------------------------------------------------------------------*/
bool OslLoadConfigFile(const char *Path, OslConfig *Config) {
    /* Pre-initialize with some sane data (but very basic and probably insufficient, except for
     * Config->Kernel, which I guess should more than likely stay as KERNEL.EXE for most setups) */
    Config->Kernel = "KERNEL.EXE";
    Config->DebugEnabled = false;
    Config->DebugEchoEnabled = false;
    Config->DebugAddress[0] = 10;
    Config->DebugAddress[1] = 0;
    Config->DebugAddress[2] = 2;
    Config->DebugAddress[3] = 15;
    Config->DebugPort = 50005;
    Config->BootDriverCount = 0;
    Config->BootDrivers = NULL;

    /* Start with space for 16 driver slots, which should be more than enough for most setups
     * (remember that boot drivers are only used to get the system partition loaded, from where
     * we'll load most of the actual drivers). */
    Config->BootDriverCapacity = 16;
    EFI_STATUS Status = gBS->AllocatePool(
        EfiLoaderData, Config->BootDriverCapacity * sizeof(char *), (VOID **)&Config->BootDrivers);
    if (Status != EFI_SUCCESS) {
        OslPrint("Failed to load the configuration file.\r\n");
        OslPrint("The system ran out of memory.\r\n");
        OslPrint("The boot process cannot continue.\r\n");
        return false;
    }

    uint64_t FileSize = 0;
    char *FileContents = OslReadFile(Path, &FileSize);
    if (!FileContents) {
        OslPrint("Failed to load the configuration file.\r\n");
        OslPrint("Couldn't find %s on the boot/root volume.\r\n", Path);
        OslPrint("The boot process cannot continue.\r\n");
        return false;
    }

    /* At most this will do nothing (if we already read in a NULL terminator), but let's make sure
     * our loops won't enter invalid heap memory. */
    FileContents[FileSize - 1] = 0;

    /* Now, we just need a small loop to read each of the parts; Errors on the syntax of the file
     * will not be considered fatal (we'll just print a warning to the screen), though memory
     * allocation failures will (as we probably don't have enough memory to load in even just the
     * kernel if that happens). */
    size_t LineNumber = 1;
    char *Line = FileContents;
    while (*Line) {
        /* Extract out the preceding whitespace + <COMMAND NAME>. */
        Line += SkipWhitespace(Line, &LineNumber);
        if (!*Line) {
            break;
        }

        char *Name = NULL;
        size_t NameSize = ReadName(Line, &Name);
        Line += NameSize;
        if (!NameSize) {
            return false;
        }

        /* Followed by more whitespace + an equal sign. */
        Line += SkipWhitespace(Line, &LineNumber);
        char *EqualSign = Line++;
        if (!*EqualSign) {
            OslPrint("Unterminated command before end of the file %s.\r\n", Path);
            break;
        } else if (*EqualSign != '=') {
            OslPrint("Invalid command syntax at line %zu in the file %s.\r\n", LineNumber, Path);

            /* For this case, we do need to actually skip the remainder of the line. */
            while (*Line && *Line != '\n') {
                Line++;
            }

            gBS->FreePool(Name);
            continue;
        }

        /* All that's left is the last set of whitespaces + the <COMMAND VALUE>. */
        Line += SkipWhitespace(Line, &LineNumber);
        if (!*Line) {
            OslPrint("Unterminated command before end of the file %s.\r\n", Path);
            gBS->FreePool(Name);
            break;
        }

        char *Value = NULL;
        size_t ValueSize = ReadValue(Line, &Value);
        Line += ValueSize;
        if (!ValueSize) {
            return false;
        }

        /* We expect uppercase everywhere, so let's handle that now. */
        for (size_t i = 0; i < NameSize; i++) {
            Name[i] = toupper(Name[i]);
        }

        for (size_t i = 0; i < ValueSize; i++) {
            Value[i] = toupper(Value[i]);
        }

        /* Now it should be as simple as a few strcmp()s, as we shouldn't have many command
         * anyways. */
        if (!strcmp(Name, "KERNEL")) {
            Config->Kernel = Value;
        } else if (!strcmp(Name, "DEBUGENABLED")) {
            Config->DebugEnabled = !strcmp(Value, "TRUE");
        } else if (!strcmp(Name, "DEBUGECHOENABLED")) {
            Config->DebugEchoEnabled = !strcmp(Value, "TRUE");
        } else if (!strcmp(Name, "DEBUGADDRESS")) {
            if (sscanf(
                    Value,
                    "%hhu.%hhu.%hhu.%hhu",
                    &Config->DebugAddress[0],
                    &Config->DebugAddress[1],
                    &Config->DebugAddress[2],
                    &Config->DebugAddress[3]) != 4) {
                OslPrint("Invalid debug address at line %zu in the file %s.\r\n", LineNumber, Path);
            }
        } else if (!strcmp(Name, "DEBUGPORT")) {
            if (sscanf(Value, "%hu", &Config->DebugPort) != 1) {
                OslPrint("Invalid debug port at line %zu in the file %s.\r\n", LineNumber, Path);
            }
        } else if (!strcmp(Name, "BOOTDRIVER")) {
            if (Config->BootDriverCount >= Config->BootDriverCapacity &&
                !ExpandBootDriverCapacity(Config)) {
                return false;
            } else if (CheckBootDrivers(Config, Value)) {
                OslPrint(
                    "Ignoring duplicate boot driver '%s' at line %zu in the file %s.\r\n",
                    Value,
                    LineNumber,
                    Path);
                gBS->FreePool(Value);
            } else {
                Config->BootDrivers[Config->BootDriverCount++] = Value;
            }
        } else {
            OslPrint(
                "Unknown command '%s' at line %zu in the file %s.\r\n", Name, LineNumber, Path);
            gBS->FreePool(Value);
        }

        gBS->FreePool(Name);
    }

    gBS->FreePool(FileContents);
    return true;
}
