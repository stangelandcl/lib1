#ifndef BASE64_H
#define BASE64_H

#if defined(BASE64_STATIC)
#define BASE64_API static
#define BASE64_IMPLEMENTATION
#else
#define BASE64_API extern
#endif

BASE64_API int base64_encode(char *dst, const int ndst, const void *src, int nsrc);
BASE64_API int base64_encode_len(int n);
BASE64_API int base64_decode_len(int n);
BASE64_API int base64_decode(void* dst, int ndst, const char *src, int nsrc);

#endif

#ifdef BASE64_IMPLEMENTATION

#include <stddef.h>
#include <stdint.h>

static const char base64_digit[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* reverses both base64 and base64url */
static const uint8_t base64_reverse[] = {
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
BASE64_API int base64_encode_len(int n) { return (n * 4 + 2) / 3; }
BASE64_API int base64_decode_len(int n) { return n * 3 / 4; }

BASE64_API int base64_encode(char *dst, const int ndst, const void *src, int nsrc) {
        int i,j, final, c, max;
        uint8_t x;
        const uint8_t *p = (const uint8_t*)src;

	final = ((nsrc + 2) / 3) * 4;
        if(ndst < final) return -1;

        j = i = 0;
        max = nsrc / 3 * 3;
        for(i=0;i<max;i+=3,j+=4) {
                x = p[i] >> 2; /* take top 6 bits */
                dst[j] = base64_digit[x];
                x = ((p[i] & 3) << 4) /* move remaining 2 bits high */
                    | (p[i+1] >> 4); /* add top 4 bits */
                dst[j+1] = base64_digit[x];
                x = ((p[i+1] & 0xF) << 2) | /* move remaining 4 bits high */
                    (p[i+2] >> 6); /*  take top 2 bits */
                dst[j+2] = base64_digit[x];
                x = (p[i+2] & 63); /* bottom 6 bits are all that remains */
                dst[j+3] = base64_digit[x];
        }

        c = nsrc - max;
        if(c > 0) {
                dst[j] = base64_digit[p[i] >> 2]; /* top 6 bits */
                if(c == 1) {
                        dst[j+1] = base64_digit[(p[i] & 3)<<4]; /* bottom 2 bits high */
                        dst[j+2] = '=';
                } else {
                         /* next 4 bits */
                        dst[j+1] = base64_digit[((p[i] & 3) << 4) | (p[i+1]>>4)];
                        dst[j+2] = base64_digit[(p[i+1] & 0xF) << 2]; /* last 4 bits */
                }
        }
        dst[j+3] = '=';
        return final;
}

/* returns decoded byte size if >=0 else error */
BASE64_API int base64_decode(void* dst, int ndst, const char *src, int nsrc) {
        int i, j, end;
        const uint32_t FORMAT_MASK = 0x80808080;
        uint8_t *p = (uint8_t*)dst;
	const uint8_t *s = (const uint8_t*)src;
        union { uint8_t x[4]; uint32_t n; } u;

	if(nsrc % 4) return -1;
        for(i=0,j=0;i<nsrc;i+=4,j+=3) {
                /*printf("i=%d j=%d nsrc=%d ndst=%d\n", i, j, nsrc, ndst);*/
		u.x[0] = base64_reverse[s[i]];
                u.x[1] = base64_reverse[s[i+1]];
                u.x[2] = base64_reverse[s[i+2]];
                u.x[3] = base64_reverse[s[i+3]];
                if(u.n & FORMAT_MASK) {
                        if((u.x[0] & 0x80) || (u.x[1] & 0x80)) {
				printf("u.x[0]=%d\n", u.x[0]);
				printf("u.x[1]=%d\n", u.x[1]);
				return -1;
			}
                        if(u.x[2] == 255 || u.x[3] == 255) {
				printf("u.x[2]=%d %d\n", u.x[2], (int)s[i+2]);
				printf("u.x[3]=%d %d\n", u.x[3], (int)s[i+3]);
				return -1;
			}
                        p[j++] = (u.x[0] << 2) | (u.x[1] >> 4);
                        if(u.x[2] == 254) break;
                        p[j++] = (u.x[1] << 4) | (u.x[2] >> 2);
                        break;
                }
                end = j + 3;
                /* bounds check plus wrap around check */
                if(end > ndst || end < j) {
			printf("bounds check\n");
			return -1;
		}
                p[j+0] = (u.x[0] << 2) | (u.x[1] >> 4);
                p[j+1] = (u.x[1] << 4) | (u.x[2] >> 2);
                p[j+2] = (u.x[2] << 6) | u.x[3];
        }
        return j;
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
