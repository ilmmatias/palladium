/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef TIME_H
#define TIME_H

#include <crt_impl/common.h>
#include <stddef.h>
#include <stdint.h>

#define __STDC_VERSION_TIME_H__ 202311L

/* Should this be OS-specific? */
#define CLOCKS_PER_SEC ((clock_t)1000000)

#define TIME_UTC 0
#define TIME_MONOTONIC 1
#define TIME_ACTIVE 2
#define TIME_THREAD_ACTIVE 3

typedef uint64_t clock_t;
typedef uint64_t time_t;

struct timespec {
    time_t tv_secs;
    time_t tv_nsec;
};

struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
};

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Time manipulation functions. */
clock_t clock(void);
double difftime(time_t time1, time_t time0);
time_t mktime(struct tm *timeptr);
time_t timegm(struct tm *timeptr);
time_t time(time_t *timer);
int timespec_get(struct timespec *ts, int base);
int timespec_getres(struct timespec *ts, int base);

/* Time conversion functions. */
char *asctime(const struct tm *timeptr);
char *ctime(const time_t *timer);
struct tm *gmtime(const time_t *timer);
struct tm *gmtime_r(const time_t *timer, struct tm *buf);
struct tm *localtime(const time_t *timer);
struct tm *localtime_r(const time_t *timer, struct tm *buf);
size_t strftime(
    char *CRT_RESTRICT s,
    size_t maxsize,
    const char *CRT_RESTRICT format,
    const struct tm *CRT_RESTRICT timeptr);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* TIME_H */
