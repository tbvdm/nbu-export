/*
 * Copyright (c) 2021 Tim van der Molen <tim@kariliq.nl>
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

#include <stddef.h>
#include <stdint.h>

#include "utf.h"

size_t
utf8_encode(uint8_t *buf, uint32_t cp)
{
	if (cp <= 0x7f) {
		if (buf != NULL)
			buf[0] = cp;
		return 1;
	}
	if (cp <= 0x7ff) {
		if (buf != NULL) {
			buf[0] = 0xc0 | (cp >>  6 & 0x1f);
			buf[1] = 0x80 | (cp       & 0x3f);
		}
		return 2;
	}
	if (cp <= 0xffff) {
		if (buf != NULL) {
			buf[0] = 0xe0 | (cp >> 12 & 0x0f);
			buf[1] = 0x80 | (cp >>  6 & 0x3f);
			buf[2] = 0x80 | (cp       & 0x3f);
		}
		return 3;
	}
	if (cp <= 0x10ffff) {
		if (buf != NULL) {
			buf[0] = 0xf0 | (cp >> 18 & 0x07);
			buf[1] = 0x80 | (cp >> 12 & 0x3f);
			buf[2] = 0x80 | (cp >>  6 & 0x3f);
			buf[3] = 0x80 | (cp       & 0x3f);
		}
		return 4;
	}
	return 0;
}

int
utf16_is_surrogate(uint16_t u)
{
	return (u & 0xf800) == 0xd800;
}

int
utf16_is_high_surrogate(uint16_t u)
{
	return (u & 0xfc00) == 0xd800;
}

int
utf16_is_low_surrogate(uint16_t u)
{
	return (u & 0xfc00) == 0xdc00;
}

uint32_t
utf16_decode_surrogate_pair(uint16_t hi, uint16_t lo)
{
	return (((uint32_t)(hi & 0x3ff) << 10) | (lo & 0x3ff)) + 0x10000;
}

size_t
utf16_decode(uint32_t *cp, uint16_t u1, uint16_t u2)
{
	if (!utf16_is_surrogate(u1)) {
		*cp = u1;
		return 1;
	}

	if (utf16_is_high_surrogate(u1) && utf16_is_low_surrogate(u2)) {
		*cp = utf16_decode_surrogate_pair(u1, u2);
		return 2;
	}

	/* Invalid */
	*cp = UTF_REPLACEMENT_CHAR;
	return 0;
}

size_t
utf16_convert_string_to_utf8(uint8_t *buf, size_t bufsize,
    const uint16_t *utf16)
{
	size_t buflen, i, n, seqlen;
	uint32_t cp;

	if (buf != NULL && bufsize == 0)
		return 0;

	buflen = 0;

	for (i = 0; utf16[i] != 0; i += n) {
		if ((n = utf16_decode(&cp, utf16[i], utf16[i + 1])) == 0)
			n = 1;

		if (buf == NULL) {
			seqlen = utf8_encode(NULL, cp);
			if (seqlen > SIZE_MAX - buflen) {
				buflen = SIZE_MAX;
				break;
			}
			buflen += seqlen;
		} else {
			if (bufsize - buflen - 1 < 4) {
				seqlen = utf8_encode(NULL, cp);
				if (seqlen > bufsize - buflen - 1)
					break;
			}
			buflen += utf8_encode(buf + buflen, cp);
		}
	}

	if (buf != NULL)
		buf[buflen] = 0;

	return buflen;
}
