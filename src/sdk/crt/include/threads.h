/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef THREADS_H
#define THREADS_H

#include <time.h>

#define __STDC_VERSION_THREADS_H__ 202311L

#define ONCE_FLAG_INIT 0
#define TSS_DTOR_ITERATIONS 64

typedef void *cnd_t;
typedef void *thrd_t;
typedef void *tss_t;
typedef void *mtx_t;
typedef void (*tss_dtor_t)(void *);
typedef int (*thrd_start_t)(void *);
typedef volatile int once_flag;

enum { mtx_plain, mtx_recursive, mtx_timed };
enum { thrd_success, thrd_busy, thrd_error, thrd_nomem };

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Initialization functions. */
void call_once(once_flag *flag, void (*func)(void));

/* Condition variable functions. */
int cnd_broadcast(cnd_t *cond);
void cnd_destroy(cnd_t *cond);
int cnd_init(cnd_t *cond);
int cnd_signal(cnd_t *cond);
int cnd_timedwait(cnd_t *restrict cond, mtx_t *restrict mtx, const struct timespec *restrict ts);
int cnd_wait(cnd_t *cond, mtx_t *mtx);

/* Mutex functions. */
void mtx_destroy(mtx_t *mtx);
int mtx_init(mtx_t *mtx, int type);
int mtx_lock(mtx_t *mtx);
int mtx_timedlock(mtx_t *restrict mtx, const struct timespec *restrict ts);
int mtx_trylock(mtx_t *mtx);
int mtx_unlock(mtx_t *mtx);

/* Thread functions. */
int thrd_create(thrd_t *thr, thrd_start_t func, void *arg);
thrd_t thrd_current(void);
int thrd_detach(thrd_t thr);
int thrd_equal(thrd_t thr0, thrd_t thr1);
[[noreturn]] void thrd_exit(int res);
int thrd_join(thrd_t thr, int *res);
int thrd_sleep(const struct timespec *duration, struct timespec *remaining);
void thrd_yield(void);

/* Thread-specific storage functions. */
int tss_create(tss_t *key, tss_dtor_t dtor);
void tss_delete(tss_t key);
void *tss_get(tss_t key);
int tss_set(tss_t key, void *val);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* THREADS_H */
