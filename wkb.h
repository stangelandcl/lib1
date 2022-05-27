#ifndef WKB_H
#define WKB_H

/* well-known binary to well-known text converter */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WKB_PRECISION 100000000
#define WKB_ADD_LITERAL(sp, x) wkb_addn(sp, x, sizeof x - 1)

/* zero initialize */
typedef struct WKB {
    char *p;
    size_t n, cap;
} WKB;

typedef struct WkbReader {
    const uint8_t *p;
    const uint8_t *end;
    WKB *s;
    char endian;
} WkbReader;

static int
wkb_reserve(WKB *s, size_t n) {
	++n; /* for zero terminator */
	if(s->n + n > s->cap) {
		size_t cap = s->cap * 2;
		char *p;
		if(cap < 4096) cap = 4096;
		if(cap - s->n < n) cap = s->n + n;
		p = (char*)realloc(s->cap ? s->p : 0, cap);
		if(!p) return -1;
		s->cap = cap;
		s->p = p;
	}
	return 0;
}

static void
wkb_addn(WKB *s, const char *text, size_t n) {
	if(wkb_reserve(s, n)) return;
	memcpy(s->p + s->n, text, n);
	s->n += n;
	s->p[s->n] = 0;
}

static void
wkb_addi(WKB *s, int i) {
    char offset[16];
    int j= 0;
    do {
        offset[j++] = '0' + i % 10;
        i /= 10;
    } while(i && j < (int)sizeof offset);
    /* reverse */
    for(int k=0;k<j/2;k++) {
        char tmp = offset[k];
        int rev = j - k - 1;
        offset[k] = offset[rev];
        offset[rev] = tmp;
    }
    wkb_addn(s, offset, j);
}

static void
wkb_addd(WKB *s, double d) {
    d *= WKB_PRECISION; /* precision */
    int64_t x = (int64_t)d;
    if(x < 0) {
        wkb_addn(s, "-", 1);
        x = -x;
    }

    /* manual calculation is faster than snprintf */
    wkb_addi(s, (int)(x / WKB_PRECISION));
    int n = (int)(x % WKB_PRECISION);
    /* strip trailing zeros */
    while(n && n % 10 == 0)
        n /= 10;
    if(n) {
        wkb_addn(s, ".", 1);
        wkb_addi(s, n);
    }
}

static void
wkb_clear(WKB *s) {
	s->n = 0;
	if(s->cap) s->p[0] = 0;
	else s->p = (char*)"";
}

static void
wkb_free(WKB *s) {
	if(s->cap) free(s->p);
}

/* check bounds before calling */
static double
wkb_double(WkbReader *r) {
    union { double x; uint64_t y; }tmp;
    if(r->endian == 'b') {
        tmp.y =
            ((uint64_t)r->p[0] << 56) +
            ((uint64_t)r->p[1] << 48) +
            ((uint64_t)r->p[2] << 40) +
            ((uint64_t)r->p[3] << 32) +
            ((uint64_t)r->p[4] << 24) +
            ((uint64_t)r->p[5] << 16) +
            ((uint64_t)r->p[6] << 8) +
            ((uint64_t)r->p[7] << 0);
    } else {
        static union {
            uint16_t w;
            uint8_t b;
        } a = { 0x00FFU};
        if(a.b == 0xFF) {
            memcpy(&tmp.x, r->p, 8);
        } else {
            /* little endian */
            tmp.y =
                ((uint64_t)r->p[0] << 0) +
                ((uint64_t)r->p[1] << 8) +
                ((uint64_t)r->p[2] << 16) +
                ((uint64_t)r->p[3] << 24) +
                ((uint64_t)r->p[4] << 32) +
                ((uint64_t)r->p[5] << 40) +
                ((uint64_t)r->p[6] << 48) +
                ((uint64_t)r->p[7] << 56);
        }
    }
    r->p += 8;
    return tmp.x;
}

/* check bounds before calling */
static uint32_t
wkb_uint(WkbReader *r) {
    uint32_t x;
    if(r->endian == 'b') {
        x =
            ((uint64_t)r->p[0] << 24) +
            ((uint64_t)r->p[1] << 16) +
            ((uint64_t)r->p[2] << 8) +
            ((uint64_t)r->p[3] << 0);
    } else {
        static union {
            uint16_t w;
            uint8_t b;
        } a = { 0x00FFU};
        if(a.b == 0xFF) {
            memcpy(&x, r->p, 4);
        } else {
            /* little endian */
            x =
                ((uint64_t)r->p[0] << 0) +
                ((uint64_t)r->p[1] << 8) +
                ((uint64_t)r->p[2] << 16) +
                ((uint64_t)r->p[3] << 24);
        }
    }
    r->p += 4;
    return x;
}

static int
wkb2point(WkbReader *r) {
    if(r->p + 16 > r->end) return -1;
    double x = wkb_double(r);
    double y = wkb_double(r);
    wkb_addn(r->s, "(", 1);
    wkb_addd(r->s, x);
    wkb_addn(r->s, " ", 1);
    wkb_addd(r->s, y);
    wkb_addn(r->s, ")", 1);
    return 0;
}

static int
wkb2linestring(WkbReader *r) {
    if(r->p + 4 > r->end) return -1;
    uint32_t count = wkb_uint(r);
    if(r->p + 8 * count * 2 > r->end) return -2;
    wkb_addn(r->s, "(", 1);
    for(uint32_t i=0;i<count;i++) {
        if(i) wkb_addn(r->s, ", ", 1);
        double x = wkb_double(r);
        double y = wkb_double(r);
        wkb_addd(r->s, x);
        wkb_addn(r->s, " ", 1);
        wkb_addd(r->s, y);
    }

    wkb_addn(r->s, ")", 1);
    return 0;
}

static int
wkb_ring(WkbReader* r) {
    if(r->p + 4 > r->end) return -1;
    uint32_t count = wkb_uint(r);
    if(r->p + count * 8 * 2 > r->end) return -2;
    wkb_addn(r->s, "(", 1);
    for(uint32_t i=0;i<count;i++) {
        if(i) wkb_addn(r->s, ", ", 2);
        double x = wkb_double(r);
        double y = wkb_double(r);
        wkb_addd(r->s, x);
        wkb_addn(r->s, " ", 1);
        wkb_addd(r->s, y);
    }
    wkb_addn(r->s, ")", 1);
    return 0;
}

static int
wkb2polygon(WkbReader *r) {
    if(r->p + 4 > r->end) return -1;
    uint32_t nrings = wkb_uint(r);
    wkb_addn(r->s, "(", 1);
    if(r->p + 4 * nrings > r->end) return -2;
    for(uint32_t i=0;i<nrings;i++) {
        if(i) wkb_addn(r->s, ", ", 2);
        if(wkb_ring(r)) return -3;
    }
    wkb_addn(r->s, ")", 1);
    return 0;
}

static int
wkb2multipoint(WkbReader *r) {
    if(r->p + 4 > r->end) return -1;
    uint32_t count = wkb_uint(r);
    wkb_addn(r->s, "(", 1);
    if(r->p + 8 * 2 * count > r->end) return -2;
    for(uint32_t i=0;i<count;i++) {
        if(i) wkb_addn(r->s, ", ", 2);
        double x = wkb_double(r);
        double y = wkb_double(r);
        wkb_addd(r->s, x);
        wkb_addn(r->s, " ", 1);
        wkb_addd(r->s, y);
    }
    wkb_addn(r->s, ")", 1);
    return 0;
}


static int
wkb2multilinestring(WkbReader *r) {
    if(r->p + 4 > r->end) return -1;
    uint32_t count = wkb_uint(r);
    wkb_addn(r->s, "(", 1);
    for(uint32_t i=0;i<count;i++) {
        if(i) wkb_addn(r->s, ", ", 2);
        wkb2linestring(r);
    }
    wkb_addn(r->s, ")", 1);
    return 0;
}

static int
wkb2multipolygon(WkbReader *r) {
    if(r->p + 4 > r->end) return -1;
    uint32_t count = wkb_uint(r);
    wkb_addn(r->s, "(", 1);
    for(uint32_t i=0;i<count;i++) {
        if(i) wkb_addn(r->s, ", ", 2);
        wkb2polygon(r);
    }
    wkb_addn(r->s, ")", 1);
    return 0;
}

static int
wkb2geometry(WkbReader *r) {
    if(r->p == r->end) return -1;
    switch(*r->p++) {
    case 0: r->endian = 'b'; break;
    case 1: r->endian = 'l'; break;
    default: return -2;
    }
    if(r->p + 4 > r->end) return -3;
    uint32_t type = wkb_uint(r);

    switch(type) {
    case 1: /* point */
        WKB_ADD_LITERAL(r->s, "POINT");
        if(wkb2point(r)) return -5;
        break;
    case 2: /* line string */
        WKB_ADD_LITERAL(r->s, "LINESTRING");
        if(wkb2linestring(r)) return -6;
        break;
    case 3: /* polygon */
        WKB_ADD_LITERAL(r->s, "POLYGON");
        if(wkb2polygon(r)) return -7;
        break;
    case 4: /* multi-point */
        WKB_ADD_LITERAL(r->s, "MULTIPOINT");
        if(wkb2multipoint(r)) return -8;
        break;
    case 5: /* multi-line-string */
        WKB_ADD_LITERAL(r->s, "MULTILINESTRING");
        if(wkb2multilinestring(r)) return -9;
        break;
    case 6: /* multi-polygon */
        WKB_ADD_LITERAL(r->s, "MULTIPOLYGON");
        if(wkb2multipolygon(r)) return -10;
        break;
    case 7: {/* geometry collection */
        WKB_ADD_LITERAL(r->s, "GEOMETRYCOLLECTION(");
        if(r->p + 4 > r->end) return -11;
        uint32_t count = wkb_uint(r);
        for(uint32_t i=0;i<count;i++) {
            if(i) wkb_addn(r->s, ",", 1);
            if(wkb2geometry(r)) return -11;
        }
        wkb_addn(r->s, ")", 1);
    } break;
    default: return -4;
    }
    return 0;
}

static int
wkb2wkt(WKB *s, const void *src, size_t nsrc) {
    WkbReader r;
    r.s = s;
    r.p = (uint8_t*)src;
    r.end = r.p + nsrc;
    wkb_reserve(s, nsrc * 4);
    return wkb2geometry(&r);
}


#ifdef __cplusplus
}
#endif

#ifdef WKB_EXAMPLE
#include <stdio.h>
int main(int argc, char **argv) {
	WKB sb = {0};
	//wkb_add(&sb, "%s %d\n", "testing", 1);
	//printf("%s", sb.p);
	wkb_free(&sb);
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
#endif

