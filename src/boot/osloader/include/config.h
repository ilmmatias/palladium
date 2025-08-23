/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
   SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <stddef.h>
#include <stdint.h>

typedef struct {
    char *Kernel;
    bool DebugEnabled;
    uint8_t DebugAddress[4];
    uint16_t DebugPort;
    size_t BootDriverCapacity;
    size_t BootDriverCount;
    char **BootDrivers;
} OslConfig;

bool OslLoadConfigFile(const char *Path, OslConfig *Config);

#endif /* _CONFIG_H_ */
