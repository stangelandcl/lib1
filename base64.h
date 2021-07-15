#ifndef BASE64_H
#define BASE64_H

/*
 * #define BASE64_IMPLEMENTATION in one file before including or
 * #define BASE64_STATIC before each include
 * example at bottom of file
 * public domain license at end of file
 */

#if defined(BASE64_STATIC) || defined(BASE64_EXAMPLE)
#define BASE64_API static
#define BASE64_IMPLEMENTATION
#else
#define BASE64_API extern
#endif

#ifdef __cplusplus
extern "C" {
#endif

BASE64_API int base64_encode(char *dst, const int ndst, const void *src, int nsrc);
BASE64_API int base64_encode_len(int n);
BASE64_API int base64_decode_len(int n);
/* returns bytes written to dst on success or negative number on failure */
BASE64_API int base64_decode(void* dst, int ndst, const char *src, int nsrc);

#ifdef __cplusplus
}
#endif

#endif

#ifdef BASE64_IMPLEMENTATION

#include <stddef.h>
#include <stdint.h>

BASE64_API int base64_encode_len(int n) { return (n * 4 + 2) / 3; }
BASE64_API int base64_decode_len(int n) { return n * 3 / 4; }

BASE64_API int base64_encode(char *dst, const int ndst, const void *src, int nsrc) {
	static const char d[] =
	    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

        int i,j, final, c, max;
        uint8_t x;
        const uint8_t *p = (const uint8_t*)src;

	final = ((nsrc + 2) / 3) * 4;
        if(ndst < final) return -1;

        j = i = 0;
        max = nsrc / 3 * 3;
        for(i=0;i<max;i+=3,j+=4) {
                x = p[i] >> 2; /* take top 6 bits */
                dst[j] = d[x];
                x = ((p[i] & 3) << 4) /* move remaining 2 bits high */
                    | (p[i+1] >> 4); /* add top 4 bits */
                dst[j+1] = d[x];
                x = ((p[i+1] & 0xF) << 2) | /* move remaining 4 bits high */
                    (p[i+2] >> 6); /*  take top 2 bits */
                dst[j+2] = d[x];
                x = (p[i+2] & 63); /* bottom 6 bits are all that remains */
                dst[j+3] = d[x];
        }

        c = nsrc - max;
        if(c > 0) {
                dst[j] = d[p[i] >> 2]; /* top 6 bits */
                if(c == 1) {
                        dst[j+1] = d[(p[i] & 3)<<4]; /* bottom 2 bits high */
                        dst[j+2] = '=';
                } else {
                         /* next 4 bits */
                        dst[j+1] = d[((p[i] & 3) << 4) | (p[i+1]>>4)];
                        dst[j+2] = d[(p[i+1] & 0xF) << 2]; /* last 4 bits */
                }
        }
        dst[j+3] = '=';
        return final;
}

/* returns decoded byte size if >=0 else error */
BASE64_API int base64_decode(void* dst, int ndst, const char *src, int nsrc) {
	/* reverses both base64 and base64url */
	static const uint8_t r[] = {
		255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255, /* 0-15 */
		255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255, /* 16-31 */
		255,255,255,255,255,255,255,255,255,255,255, 62,255,62,255, 63, /* 32-47 */
		 52, 53, 54, 55, 56, 57, 58, 59, 60, 61,255,255,255,254,255,255, /* 48-63 */
		255,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, /* 64-79 */
		 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,255,255,255,255,63, /* 80-95 */
		255, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, /* 96-111 */
		 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51,255,255,255,255,255, /* 112-127 */
		255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255, /* 128-143 */
		255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255, /* 144-159 */
		255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255, /* 160-175 */
		255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255, /* 176-192 */
		255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255, /* 192-207 */
		255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255, /* 208-223 */
		255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255, /* 224-239 */
		255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255, /* 239-255 */
	};
        int i, j;
        uint8_t *p = (uint8_t*)dst;
	const uint8_t *s = (const uint8_t*)src;
        uint8_t x0, x1, x2, x3;

	i = j = 0;
	if(i >= nsrc) return -1;
        while(i < nsrc) {
		x0 = r[s[i++]];
		if(x0 & 0x80) return -2;
		if(i == nsrc) return -3;
		x1 = r[s[i++]];
		if(x1 & 0x80) return -4;
		if(j == ndst) return -5;
		p[j++] = (x0 << 2) | (x1 >> 4);
		if(i == nsrc) break;
		x2 = r[s[i++]];
		if(x2 & 0x80) {
			if(x2 == 254) break;
			return -5;
		}
		if(j == ndst) return -6;
		p[j++] = (x1 << 4) | (x2 >> 2);
		if(i == nsrc) break;
		x3 = r[s[i++]];
		if(x3 & 0x80) {
			if(x3 == 254) break;
			return -7;
		}
		if(j == ndst) return -8;
		p[j++] = (x2 << 6) | x3;
        }
        return j;
}
#endif

#ifdef BASE64_EXAMPLE
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(int argc, char **argv) {
	char token[] = "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiIxMjM0NTY3ODkwIiwibmFtZSI6IkpvaG4gRG9lIiwiYWRtaW4iOnRydWV9.EkN-DOsnsuRjRO6BxXemmJDm3HbxrbRzXglbN2S4sOkopdU4IsDxTI8jO19W_A4K8ZPJijNLis4EZsHeY559a4DFOd50_OqgHGuERTqYZyuhtF39yxJPAjUESwxk2J5k_4zM3O-vtd1Ghyo4IbqKKSy6J9mTniYJPenn5-HIirE";
	char dst[4096];
	char *e, *t;

	e = strstr(token, ".");
	int n = base64_decode(dst, sizeof dst, token, e - token);
	assert(n > 0);
	t = e + 1;
	e = strstr(t, ".");
	n = base64_decode(dst, sizeof dst, t, e - t);
	assert(n > 0);
	t = e + 1;
	n = base64_decode(dst, sizeof dst, t, strlen(t));
	assert(n > 0);

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
