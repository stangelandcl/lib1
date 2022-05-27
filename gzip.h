/* SPDX-License-Identifier: Unlicense */
#ifndef GZIP_H
#define GZIP_H

/* Not a single file library since it uses zlib, just a nice wrapper around zlib
   for compressing a buffer in gzip, deflate or zlib formats */
#include <zlib.h>
#include <assert.h>
#include <stdlib.h>

#if defined(GZIP_STATIC) || defined(GZIP_EXAMPLE)
#define GZIP_API static
#define GZIP_IMPLEMENTATION
#else
#define GZIP_API extern
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define GZIP_GZIP 0
#define GZIP_DEFLATE 1
#define GZIP_ZLIB 2

/* returns max compressed size or < 0 for error */
GZIP_API ptrdiff_t gzip_bound(int format, size_t uncompressed_length);
/* returns compressed size or < 0 for error */
GZIP_API ptrdiff_t gzip_compress(int format, void *dst, size_t ndst, const void *src, size_t nsrc);
/* returns malloc'd ptr or null for error */
GZIP_API void* gzip_uncompress(int format, const void *src, size_t nsrc, size_t *nresult);

#ifdef __cplusplus
}
#endif

#endif
#ifdef GZIP_IMPLEMENTATION

#define GZIP_LEVEL 3 /* 0 fast but bigger to 9 slow but smaller */
/* 31 = gzip
   15 = zlib
   -15 = raw deflate */
#define GZIP_GZIP_WINDOW 31
#define GZIP_ZLIB_WINDOW 15
#define GZIP_DEFLATE_WINDOW -15

static int
gzip_window(int format) {
    switch(format) {
    default:
    case GZIP_GZIP: return GZIP_GZIP_WINDOW;
    case GZIP_ZLIB: return GZIP_ZLIB_WINDOW;
    case GZIP_DEFLATE: return GZIP_DEFLATE_WINDOW;
    }
}

/* compression bound */
GZIP_API ptrdiff_t
gzip_bound(int format, size_t nsrc) {
    z_stream s;
    s.next_in = 0;
    s.avail_in = 0;
    s.next_out = 0;
    s.avail_out = 0;
    s.zalloc = Z_NULL;
    s.zfree = Z_NULL;
    s.opaque = Z_NULL;

    int window = gzip_window(format);
    deflateInit2(&s, GZIP_LEVEL, Z_DEFLATED, window, 9, Z_DEFAULT_STRATEGY);
    ptrdiff_t bound = (ptrdiff_t)deflateBound(&s, (uLong)nsrc);
    deflateEnd(&s);
    return bound;
}

GZIP_API ptrdiff_t
gzip_compress(int format, void *dst, size_t ndst, const void *src, size_t nsrc) {
    z_stream s = {0};
    int e;

    s.next_in = (unsigned char*)src;
    s.avail_in = (unsigned)nsrc;
    s.next_out = (unsigned char*)dst;
    s.avail_out = (unsigned)ndst;

    int window = gzip_window(format);
    deflateInit2(&s, GZIP_LEVEL, Z_DEFLATED, window, 9, Z_DEFAULT_STRATEGY);

    e = deflate(&s, Z_FINISH);
    if(e != Z_STREAM_END) {
            deflateEnd(&s);
            return -1;
    }

    ndst = s.total_out;
    deflateEnd(&s);
    return (ptrdiff_t)ndst;
}

GZIP_API void*
gzip_uncompress(int format, const void *src, size_t nsrc, size_t *nresult) {
    size_t ndst = nsrc * 10 + 64;
    char *dst = (char*)malloc(ndst);
    assert(dst);
    z_stream s = {0};
    s.next_in = (Bytef*)src;
    s.avail_in = (unsigned)nsrc;
    s.next_out = (Bytef*)dst;
    s.avail_out = (unsigned)ndst;
    size_t output = 0;

    int window = gzip_window(format);

    inflateInit2(&s, window);
    for(;;) {
        int rc = inflate(&s, Z_NO_FLUSH);
        output = ndst - s.avail_out;
        switch(rc) {
        case Z_BUF_ERROR:
        case Z_OK:
            if(!s.avail_out) {
                ndst *= 2;
                dst = (char*)realloc(dst, ndst);
                s.avail_out = (unsigned)(ndst - output);
                s.next_out = (Bytef*)(dst + output);
            }
            break;
        case Z_STREAM_END: goto done;
        default: goto error;
        }
    }
done:
    inflateEnd(&s);
    *nresult = output;
    return realloc(dst, *nresult);
error:
    inflateEnd(&s);
    *nresult = 0;
    free(dst);
    return 0;
}
#endif

#ifdef GZIP_EXAMPLE
#include <string.h>
int main(int argc, char **argv) {
    const char test[] = "test";
    int format = GZIP_DEFLATE;
    size_t nbuf = gzip_bound(format, sizeof test);
    char *buf = (char*)malloc(nbuf);
    assert(buf);
    ptrdiff_t n = gzip_compress(format, buf, nbuf, test, sizeof test);
    assert(n >= 0);
    size_t ndecomp;
    char *decomp = (char*)gzip_uncompress(format, buf, n, &ndecomp);
    assert(decomp);
    assert(ndecomp == sizeof test);
    assert(!strcmp(decomp, test));
    free(buf);
    free(decomp);
    return 0;
}
#endif
/* Public Domain (www.unlicense.org)
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

