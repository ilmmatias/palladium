/* SPDX-FileCopyrightText: (C) 2025-2026 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/ev.h>
#include <kernel/ob.h>
#include <stddef.h>

ObType ObpMutexType = {
    .Name = "Mutex",
    .Size = sizeof(EvMutex),
    .Delete = NULL,
};
