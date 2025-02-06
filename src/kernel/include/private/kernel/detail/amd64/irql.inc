/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _AMD64_IRQL_INC_
#define _AMD64_IRQL_INC_

/* Keep these in sync with their values on ke.h. */
#define KE_IRQL_PASSIVE 0
#define KE_IRQL_DISPATCH 2
#define KE_IRQL_DEVICE 3
#define KE_IRQL_TIMER 13
#define KE_IRQL_SYNCH 13
#define KE_IRQL_IPI 14
#define KE_IRQL_MAX 15

/* And these with the values in halp.h. */
#define HALP_INT_DISPATCH_IRQL 2
#define HALP_INT_TIMER_IRQL 13
#define HALP_INT_SPURIOUS_IRQL 15

#endif /* _AMD64_IRQL_INC_ */
