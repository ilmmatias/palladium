/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <acpi.h>

int AcpipReadFieldList(AcpipState *State, uint8_t *FieldFlags) {
    if (!AcpipReadByte(State, FieldFlags)) {
        return 0;
    }

    return 0;
}
