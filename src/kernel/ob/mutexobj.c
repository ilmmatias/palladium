/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/ev.h>
#include <kernel/ob.h>

ObType ObpMutexType = {
    .Name = "Mutex",
    .Size = sizeof(EvMutex),
    .Delete = NULL,
};
