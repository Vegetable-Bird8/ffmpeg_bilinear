#include "config.h"

#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_IO_H
#include <io.h>
#endif
#include <stdarg.h>
#include <stdlib.h>
#include "avutil.h"
// #include "bprint.h"
#include "common.h"
// #include "internal.h"
#include "log.h"
// #include "thread.h"

// static AVMutex mutex = AV_MUTEX_INITIALIZER;
/*
 * Copyright (c) 2005-2012 Michael Niedermayer <michaelni@gmx.at>
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 * @file
 * miscellaneous math routines and tables
 */

#include <stdint.h>
#include <limits.h>

#include "mathematics.h"
// #include "libavutil/intmath.h"
#include "common.h"
#include "avassert.h"
#include "version.h"

#define ff_ctzll(v) __builtin_ctzll(v)

/* Stein's binary GCD algorithm:
 * https://en.wikipedia.org/wiki/Binary_GCD_algorithm */
int64_t av_gcd(int64_t a, int64_t b) {
    int za, zb, k;
    int64_t u, v;
    if (a == 0)
        return b;
    if (b == 0)
        return a;
    za = ff_ctzll(a);
    zb = ff_ctzll(b);
    k  = FFMIN(za, zb);
    u = llabs(a >> za);
    v = llabs(b >> zb);
    while (u != v) {
        if (u > v)
            FFSWAP(int64_t, v, u);
        v -= u;
        v >>= ff_ctzll(v);
    }
    return (uint64_t)u << k;
}

int64_t av_rescale_rnd(int64_t a, int64_t b, int64_t c, enum AVRounding rnd)
{
    int64_t r = 0;
    av_assert2(c > 0);
    av_assert2(b >=0);
    av_assert2((unsigned)(rnd&~AV_ROUND_PASS_MINMAX)<=5 && (rnd&~AV_ROUND_PASS_MINMAX)!=4);

    if (c <= 0 || b < 0 || !((unsigned)(rnd&~AV_ROUND_PASS_MINMAX)<=5 && (rnd&~AV_ROUND_PASS_MINMAX)!=4))
        return INT64_MIN;

    if (rnd & AV_ROUND_PASS_MINMAX) {
        if (a == INT64_MIN || a == INT64_MAX)
            return a;
        rnd -= AV_ROUND_PASS_MINMAX;
    }

    if (a < 0)
        return -(uint64_t)av_rescale_rnd(-FFMAX(a, -INT64_MAX), b, c, rnd ^ ((rnd >> 1) & 1));

    if (rnd == AV_ROUND_NEAR_INF)
        r = c / 2;
    else if (rnd & 1)
        r = c - 1;

    if (b <= INT_MAX && c <= INT_MAX) {
        if (a <= INT_MAX)
            return (a * b + r) / c;
        else {
            int64_t ad = a / c;
            int64_t a2 = (a % c * b + r) / c;
            if (ad >= INT32_MAX && b && ad > (INT64_MAX - a2) / b)
                return INT64_MIN;
            return ad * b + a2;
        }
    } else {
#if 1
        uint64_t a0  = a & 0xFFFFFFFF;
        uint64_t a1  = a >> 32;
        uint64_t b0  = b & 0xFFFFFFFF;
        uint64_t b1  = b >> 32;
        uint64_t t1  = a0 * b1 + a1 * b0;
        uint64_t t1a = t1 << 32;
        int i;

        a0  = a0 * b0 + t1a;
        a1  = a1 * b1 + (t1 >> 32) + (a0 < t1a);
        a0 += r;
        a1 += a0 < r;

        for (i = 63; i >= 0; i--) {
            a1 += a1 + ((a0 >> i) & 1);
            t1 += t1;
            if (c <= a1) {
                a1 -= c;
                t1++;
            }
        }
        if (t1 > INT64_MAX)
            return INT64_MIN;
        return t1;
#else
        /* reference code doing (a*b + r) / c, requires libavutil/integer.h */
        AVInteger ai;
        ai = av_mul_i(av_int2i(a), av_int2i(b));
        ai = av_add_i(ai, av_int2i(r));

        return av_i2int(av_div_i(ai, av_int2i(c)));
#endif
    }
}

int64_t av_rescale(int64_t a, int64_t b, int64_t c)
{
    return av_rescale_rnd(a, b, c, AV_ROUND_NEAR_INF);
}

int64_t av_rescale_q_rnd(int64_t a, AVRational bq, AVRational cq,
                         enum AVRounding rnd)
{
    int64_t b = bq.num * (int64_t)cq.den;
    int64_t c = cq.num * (int64_t)bq.den;
    return av_rescale_rnd(a, b, c, rnd);
}

int64_t av_rescale_q(int64_t a, AVRational bq, AVRational cq)
{
    return av_rescale_q_rnd(a, bq, cq, AV_ROUND_NEAR_INF);
}

int av_compare_ts(int64_t ts_a, AVRational tb_a, int64_t ts_b, AVRational tb_b)
{
    int64_t a = tb_a.num * (int64_t)tb_b.den;
    int64_t b = tb_b.num * (int64_t)tb_a.den;
    if ((FFABS(ts_a)|a|FFABS(ts_b)|b) <= INT_MAX)
        return (ts_a*a > ts_b*b) - (ts_a*a < ts_b*b);
    if (av_rescale_rnd(ts_a, a, b, AV_ROUND_DOWN) < ts_b)
        return -1;
    if (av_rescale_rnd(ts_b, b, a, AV_ROUND_DOWN) < ts_a)
        return 1;
    return 0;
}

int64_t av_compare_mod(uint64_t a, uint64_t b, uint64_t mod)
{
    int64_t c = (a - b) & (mod - 1);
    if (c > (mod >> 1))
        c -= mod;
    return c;
}

int64_t av_rescale_delta(AVRational in_tb, int64_t in_ts,  AVRational fs_tb, int duration, int64_t *last, AVRational out_tb){
    int64_t a, b, this;

    av_assert0(in_ts != AV_NOPTS_VALUE);
    av_assert0(duration >= 0);

    if (*last == AV_NOPTS_VALUE || !duration || in_tb.num*(int64_t)out_tb.den <= out_tb.num*(int64_t)in_tb.den) {
simple_round:
        *last = av_rescale_q(in_ts, in_tb, fs_tb) + duration;
        return av_rescale_q(in_ts, in_tb, out_tb);
    }

    a =  av_rescale_q_rnd(2*in_ts-1, in_tb, fs_tb, AV_ROUND_DOWN)   >>1;
    b = (av_rescale_q_rnd(2*in_ts+1, in_tb, fs_tb, AV_ROUND_UP  )+1)>>1;
    if (*last < 2*a - b || *last > 2*b - a)
        goto simple_round;

    this = av_clip64(*last, a, b);
    *last = this + duration;

    return av_rescale_q(this, fs_tb, out_tb);
}

int64_t av_add_stable(AVRational ts_tb, int64_t ts, AVRational inc_tb, int64_t inc)
{
    int64_t m, d;

    if (inc != 1)
        inc_tb = av_mul_q(inc_tb, (AVRational) {inc, 1});

    m = inc_tb.num * (int64_t)ts_tb.den;
    d = inc_tb.den * (int64_t)ts_tb.num;

    if (m % d == 0)
        return ts + m / d;
    if (m < d)
        return ts;

    {
        int64_t old = av_rescale_q(ts, ts_tb, inc_tb);
        int64_t old_ts = av_rescale_q(old, inc_tb, ts_tb);
        return av_rescale_q(old + 1, inc_tb, ts_tb) + (ts - old_ts);
    }
}

#define LINE_SZ 1024

#if HAVE_VALGRIND_VALGRIND_H
#include <valgrind/valgrind.h>
/* this is the log level at which valgrind will output a full backtrace */
#define BACKTRACE_LOGLEVEL AV_LOG_ERROR
#endif

static int av_log_level = AV_LOG_INFO;
// static void (*av_log_callback)(void*, int, const char*, va_list) =
//     av_log_default_callback;

void av_log(void* avcl, int level, const char *fmt, ...)
{
    AVClass* avc = avcl ? *(AVClass **) avcl : NULL;
    va_list vl;
    va_start(vl, fmt);
    if (avc && avc->version >= (50 << 16 | 15 << 8 | 2) &&
        avc->log_level_offset_offset && level >= AV_LOG_FATAL)
        level += *(int *) (((uint8_t *) avcl) + avc->log_level_offset_offset);
    // av_vlog(avcl, level, fmt, vl);
    va_end(vl);
}

// void av_vlog(void* avcl, int level, const char *fmt, va_list vl)
// {
//     void (*log_callback)(void*, int, const char*, va_list) = av_log_callback;
//     if (log_callback)
//         log_callback(avcl, level, fmt, vl);
// }

static inline av_const int av_tolower(int c)
{
    if (c >= 'A' && c <= 'Z')
        c ^= 0x20;
    return c;
}
int av_strncasecmp(const char *a, const char *b, size_t n)
{
    const char *end = a + n;
    uint8_t c1, c2;
    do {
        c1 = av_tolower(*a++);
        c2 = av_tolower(*b++);
    } while (a < end && c1 && c1 == c2);
    return c1 - c2;
}
int av_match_name(const char *name, const char *names)
{
    const char *p;
    int len, namelen;

    if (!name || !names)
        return 0;

    namelen = strlen(name);
    while (*names) {
        int negate = '-' == *names;
        p = strchr(names, ',');
        if (!p)
            p = names + strlen(names);
        names += negate;
        len = FFMAX(p - names, namelen);
        if (!av_strncasecmp(name, names, len) || !strncmp("ALL", names, FFMAX(3, p - names)))
            return !negate;
        names = p + (*p == ',');
    }
    return 0;
}