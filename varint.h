/* SPDX-License-Identifier: Unlicense */
#ifndef VARINT_H
#define VARINT_H

#include <stddef.h>
#include <stdint.h>

/*
 * #define VARINT_IMPLEMENTATION in one file before including or
 * #define VARINT_STATIC before each include
 * example at bottom of file
 */

#if defined(VARINT_STATIC) || defined(VARINT_EXAMPLE)
#define VARINT_API static
#define VARINT_IMPLEMENTATION
#else
#define VARINT_API extern
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* max required buffer size */
#define VARINT_MAX 10

VARINT_API void varint_write(uint8_t **pp, uint64_t x);
VARINT_API void zigzag_write(uint8_t **pp, int64_t x);
VARINT_API uint64_t varint_read(const uint8_t **pp, size_t n);
VARINT_API int64_t zigzag_read(const uint8_t **pp, size_t n);

#ifdef __cplusplus
}
#endif

#endif

#ifdef VARINT_IMPLEMENTATION

VARINT_API void
varint_write(uint8_t **pp, uint64_t x) {
	uint8_t *p = *pp;
	do {
		*p = x & 127;
		x >>= 7;
		if(x) *p |= 128;
		++p;
	} while(x);
	*pp = p;
}
VARINT_API void
zigzag_write(uint8_t **p, int64_t x) {
	uint64_t y = (uint64_t)x;
	varint_write(p, (y >> 63) | (y << 1));
}
VARINT_API uint64_t
varint_read(const uint8_t **pp, size_t n) {
	const uint8_t *p = *pp;
	uint64_t x = 0;
	size_t count = 0;
	while(n--) {
		uint8_t b = *p++;
		x |= ((uint64_t)(b & 127)) << count;
		if(!(b & 128)) break;
		count += 7;
	}
	*pp = p;
	return x;
}
VARINT_API int64_t
zigzag_read(const uint8_t **pp, size_t n) {
	uint64_t x = varint_read(pp, n);
	x = ((x >> 1) | (x << 63));
	return (int64_t)x;
}


#endif

#ifdef VARINT_EXAMPLE
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
int main(int argc, char **argv) {
	uint8_t *p, buf[VARINT_MAX];
	const uint8_t *cp;
	int64_t x = INT64_MAX;
	uint64_t y = UINT64_MAX;

	p = buf;
	varint_write(&p, y);
	printf("len=%d\n", (int)(p - buf));

	cp = buf;
	uint64_t y2 = varint_read(&cp, sizeof buf);
	printf("y=%" PRIu64 " y2=%" PRIu64 "\n", y, y2);
	assert(y == y2);

	p = buf;
	zigzag_write(&p, x);
	printf("len=%d\n", (int)(p - buf));

	cp = buf;
	int64_t x2 = zigzag_read(&cp, sizeof buf);
	printf("x=%" PRId64 " x2=%" PRId64 "\n", x, x2);
	assert(x == x2);


	return 0;
}
#endif
/*
Public Domain (www.unlicense.org)
This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.
In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
