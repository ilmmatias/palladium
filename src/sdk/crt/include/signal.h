/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef SIGNAL_H
#define SIGNAL_H

#include <crt_impl/os_signal.h>

#define SIG_DFL 0
#define SIG_ERR 1
#define SIG_IGN 2

typedef int sig_atomic_t;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Specify signal handling. */
void (*signal(int sig, void (*func)(int)))(int);

/* Send signal. */
int raise(int sig);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* SIGNAL_H */
