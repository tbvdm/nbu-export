/*
 * macOS
 */

#ifdef __APPLE__

#define HAVE_GETPROGNAME

#endif

/*
 * Cygwin
 */

#ifdef __CYGWIN__

#define _GNU_SOURCE

#define HAVE_ENDIAN_H
#define HAVE_GETPROGNAME
#define HAVE_LE16TOH
#define HAVE_REALLOCARRAY

#endif

/*
 * DragonFly BSD
 */

#ifdef __DragonFly__

#define HAVE_GETPROGNAME
#define HAVE_LE16TOH
#define HAVE_REALLOCARRAY
#define HAVE_SYS_ENDIAN_H

#endif

/*
 * FreeBSD
 */

#ifdef __FreeBSD__

#define HAVE_GETPROGNAME
#define HAVE_LE16TOH
#define HAVE_REALLOCARRAY
#define HAVE_SYS_ENDIAN_H

#endif

/*
 * NetBSD
 */

#ifdef __NetBSD__

#define _OPENBSD_SOURCE

#define HAVE_GETPROGNAME
#define HAVE_LE16TOH
#define HAVE_REALLOCARRAY
#define HAVE_SYS_ENDIAN_H

#endif

/*
 * OpenBSD
 */

#ifdef __OpenBSD__

#define HAVE_ENDIAN_H
#define HAVE_GETPROGNAME
#define HAVE_LE16TOH
#define HAVE_PLEDGE
#define HAVE_REALLOCARRAY
#define HAVE_SYS_QUEUE_H
#define HAVE_UNVEIL

#endif

/*
 * Linux
 */

#ifdef __linux__

#define _GNU_SOURCE

#include <features.h>

/* All modern versions of glibc, musl and bionic have these */
#define HAVE_ENDIAN_H
#define HAVE_LE16TOH

/* glibc */

#ifdef __GLIBC_PREREQ
#  if __GLIBC_PREREQ(2, 26)
#    define HAVE_REALLOCARRAY
#  endif
#endif

/* bionic */

#ifdef __ANDROID_API__
#  if __ANDROID_API__ >= 21
#    define HAVE_GETPROGNAME
#  endif
#  if __ANDROID_API__ >= 29
#    define HAVE_REALLOCARRAY
#  endif
#endif

/* musl */

/* Define if you have musl >= 1.2.2 */
/* #define HAVE_REALLOCARRAY */

#endif

/*
 * Solaris
 */

#ifdef __sun

#define HAVE_GETPROGNAME

#endif
