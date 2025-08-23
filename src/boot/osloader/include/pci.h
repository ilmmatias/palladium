/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _PCI_H_
#define _PCI_H_

#include <efi/spec.h>
#include <stdint.h>

UINTN OslFindPciDevicesByClass(uint16_t ClassId, EFI_PCI_IO_PROTOCOL ***Devices);

#endif /* _PCI_H_ */
