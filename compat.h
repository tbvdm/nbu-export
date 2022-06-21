/*
 * Copyright (c) 2018 Tim van der Molen <tim@kariliq.nl>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef COMPAT_H
#define COMPAT_H

#if !defined(__GNUC__) && !defined(__clang__)
#define __attribute__(a)
#endif

/* macOS defines __dead away */
#ifdef __APPLE__
#undef __dead
#endif

#ifndef __dead
#define __dead		__attribute__((__noreturn__))
#endif

#ifndef __unused
#define __unused	__attribute__((__unused__))
#endif

/* For size_t */
#include <stddef.h>

#ifndef HAVE_GETPROGNAME
const char *getprogname(void);
void setprogname(const char *);
#endif

#ifdef HAVE_LE16TOH
#  ifdef HAVE_ENDIAN_H
#    include <endian.h>
#  elif defined(HAVE_SYS_ENDIAN_H)
#    include <sys/endian.h>
#  endif
#else
#  ifdef __APPLE__
#    include <libkern/OSByteOrder.h>
#    define le16toh(x) OSSwapLittleToHostInt16(x)
#    define le32toh(x) OSSwapLittleToHostInt32(x)
#    define le64toh(x) OSSwapLittleToHostInt64(x)
#  elif defined(__sun)
#    include <sys/byteorder.h>
#    define le16toh(x) LE_16(x)
#    define le32toh(x) LE_32(x)
#    define le64toh(x) LE_64(x)
#  endif
#endif

#ifndef HAVE_PLEDGE
#define pledge(x, y) 0
#endif

#ifndef HAVE_REALLOCARRAY
void *reallocarray(void *, size_t, size_t);
#endif

#ifdef HAVE_SYS_QUEUE_H
#include <sys/queue.h>
#else
#include "compat/queue.h"
#endif

#ifndef HAVE_UNVEIL
#define unveil(x, y) 0
#endif

#endif
