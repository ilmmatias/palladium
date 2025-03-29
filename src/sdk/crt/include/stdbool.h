/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef STDBOOL_H
#define STDBOOL_H

#if defined(__STDC_VERSION__) && __STDC_VERSION__ < 202311L
#define bool _Bool
#define true 1
#define false 0
#endif /* __STDC_VERSION__ */

#define __bool_true_false_are_defined 1

#endif /* STDBOOL_H */
