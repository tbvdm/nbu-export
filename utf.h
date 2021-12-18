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

#ifndef UTF_H
#define UTF_H

#define UTF_HIGH_SURROGATE_MIN	0xd800U
#define UTF_HIGH_SURROGATE_MAX	0xdbffU
#define UTF_LOW_SURROGATE_MIN	0xdc00U
#define UTF_LOW_SURROGATE_MAX	0xdfffU
#define UTF_BOM			0xfeffU
#define UTF_REPLACEMENT_CHAR	0xfffdU
#define UTF_CODEPOINT_MAX	0x10ffffU

size_t	utf8_encode(uint8_t *, uint32_t);

int	utf16_is_surrogate(uint16_t);
int	utf16_is_high_surrogate(uint16_t);
int	utf16_is_low_surrogate(uint16_t);

uint32_t utf16_decode_surrogate_pair(uint16_t, uint16_t);
size_t	utf16_decode(uint32_t *, uint16_t, uint16_t);
size_t	utf16_convert_string_to_utf8(uint8_t *, size_t, const uint16_t *);

#endif
