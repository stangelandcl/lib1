#ifndef WKT_H
#define WKT_H

/* Well-known text to Well-known binary geometry format converter */

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WKT_TAKE(r, literal) wkt_take(r, literal, sizeof literal - 1)

/* zero initialize */
typedef struct WKT {
    char *p;
    size_t n, cap;
} WKT;

typedef struct WktReader {
    const char *p;
    const char *end;
    WKT *s;
} WktReader;

static void
wkt_clear(WKT *s) {
    s->n = 0;
}

static void
wkt_reserve(WktReader *r, size_t n) {
	++n; /* for zero terminator */
	if(r->s->n + n > r->s->cap) {
		size_t cap = r->s->cap * 2;
		char *p;
		if(cap < 4096) cap = 4096;
		if(cap - r->s->n < n) cap = r->s->n + n;
		p = (char*)realloc(r->s->cap ? r->s->p : 0, cap);
        assert(p);
		if(!p) {
            printf("Error wkt_reserve: out of memory allocating %zu\n", cap);
            abort();
            return;
        }
		r->s->cap = cap;
		r->s->p = p;
	}
}

static void
wkt_putb(WktReader *r, uint8_t x) {
    wkt_reserve(r, 1);
    r->s->p[r->s->n++] = x;
}

static void
wkt_putu(WktReader *r, uint32_t x) {
    wkt_reserve(r, sizeof x);
    memcpy(r->s->p + r->s->n, &x, sizeof x);
    r->s->n += sizeof x;
}

static void
wkt_putd(WktReader *r, double x) {
    wkt_reserve(r, sizeof x);
    memcpy(r->s->p + r->s->n, &x, sizeof x);
    r->s->n += sizeof x;
}

static void
wkt_skip(WktReader *r) {
    while(r->p < r->end && (*r->p == '\t' || *r->p == ' ' || *r->p == '\r' || *r->p == '\n'))
        ++r->p;
}

static int
wkt_take(WktReader *r, const char *s, size_t n) {
    wkt_skip(r);
    if(r->p + n > r->end) return 0;
    int c = memcmp(r->p, s, n);
    if(!c) r->p += n;
    return !c;
}

static int
wkt_space(WktReader *r) {
    if(r->p == r->end) return 0;
    const char *start = r->p;
    while(r->p < r->end && *r->p == ' ')
        ++r->p;
    return r->p != start;
}

/* return 1 on found, 0 on not */
static int
wkt_double(WktReader *r, double *result) {
    wkt_skip(r);
    if(r->p == r->end) return 0;
	const char *p = r->p, *e = r->end;
	int64_t i = 0, j=0, jj=1;
	double f = nan(""), s = 1;
	if(p != e) {
        //printf("p[0]=%c ", *p);
		if(*p == '-') {++p; s = -1; } /* sign */
		for(;p != e && *p>='0' && *p<='9';++p) /* int */
			i = i * 10 + (*p - '0');
		f = (double)i;
		if(p != e && *p == '.') { /* fraction */
			for(++p;p != e && *p>='0' && *p<='9';++p) {
				j = j * 10 + (*p - '0');
				jj *= 10;
			}
			f += (double)j / (double)jj;
		}
		f *= s; /* must be done after adding in the fraction */
		if(p != e && (*p == 'e' || *p=='E')) { /* exponent */
            size_t k = 0, ks = 1;
			if(++p < e) {
				if(*p == '+') ++p; /* exponent sign */
				else if(*p == '-') {
					ks = -1;
					++p;
				}
				for(;p != e && *p>='0' && *p<='9';++p)
					k = k * 10 + (*p - '0');
				f *= pow(10, (double)(k * ks));
			}
		}
	}
    r->p = p;
	*result = f;
    //printf("d=%g sign=%f\n", f, s);
    return !isnan(f);
}

static int
wkt_point(WktReader *r) {
    wkt_skip(r);
    if(!WKT_TAKE(r, "(")) return -3;
    double x, y;
    if(!wkt_double(r, &x)) return -5;
    if(!wkt_space(r)) return -7;
    if(!wkt_double(r, &y)) return -6;
    if(!WKT_TAKE(r, ")")) return -4;

    wkt_putu(r, 1); /* point prefix */
    wkt_putd(r, x);
    wkt_putd(r, y);
    return 0;
}

static size_t
wkt_reserve_count(WktReader *r) {
    /* leave space for count */
    wkt_reserve(r, sizeof(uint32_t));
    size_t position = r->s->n;
    r->s->n += sizeof(uint32_t);
    return position;
}

static int
wkt_put_count(WktReader *r, size_t position, size_t count) {
    uint32_t c = count;
    if(c != count) return -10;
    memcpy(r->s->p + position, &c, sizeof c);
    return 0;
}

static int
wkt_ring(WktReader *r) {
    wkt_skip(r);
    if(!WKT_TAKE(r, "(")) return -3;

    size_t pos = wkt_reserve_count(r);
    size_t count = 0;

    do {
        double x, y;
        if(!wkt_double(r, &x)) return -5;
        if(!wkt_space(r)) return -7;
        if(!wkt_double(r, &y)) return -6;

        wkt_putd(r, x);
        wkt_putd(r, y);
        ++count;
    } while(WKT_TAKE(r, ","));
    if(!WKT_TAKE(r, ")")) return -4;

    return wkt_put_count(r, pos, count);
}

static int
wkt_linestring(WktReader *r) {
    wkt_putu(r, 2); /* line string prefix */
    return wkt_ring(r);
}

static int
wkt_polygon(WktReader *r) {
    wkt_skip(r);
    if(!WKT_TAKE(r, "(")) return -12;
    wkt_putu(r, 3); /* polygon prefix */
    size_t pos = wkt_reserve_count(r);
    size_t count = 0;
    do{
        int rc;
        if((rc = wkt_ring(r))) return rc;
        ++count;
    } while(WKT_TAKE(r, ","));
    if(!WKT_TAKE(r, ")")) return -11;
    return wkt_put_count(r, pos, count);
}

static int
wkt_multipoint(WktReader *r) {
    wkt_putu(r, 4); /* multi-point prefix */
    return wkt_ring(r);
}

static int
wkt_multilinestring(WktReader *r) {
    wkt_skip(r);
    if(!WKT_TAKE(r, "(")) return -12;
    wkt_putu(r, 5); /* multi-linestring prefix */
    size_t pos = wkt_reserve_count(r);
    size_t count = 0;
    do{
        int rc;
        if((rc = wkt_ring(r))) return rc;
        ++count;
    } while(WKT_TAKE(r, ","));
    if(!WKT_TAKE(r, ")")) return -11;
    return wkt_put_count(r, pos, count);
}

static int
wkt_multipolygon(WktReader *r) {
    int rc;
    wkt_skip(r);
    if(!WKT_TAKE(r, "(")) return -12;
    wkt_putu(r, 6); /* multi-polygon prefix */
    size_t pos = wkt_reserve_count(r);
    size_t count = 0;

    do {
        size_t pos1 = wkt_reserve_count(r);
        size_t count1 = 0;
        if(!WKT_TAKE(r, "(")) return -13;
        do{
            if((rc = wkt_ring(r))) return rc;
            ++count1;
        } while(WKT_TAKE(r, ","));
        if(!WKT_TAKE(r, ")")) return -14;
        if((rc = wkt_put_count(r, pos1, count1)))
            return rc;
        ++count;
    } while(WKT_TAKE(r, ","));
    if(!WKT_TAKE(r, ")")) return -11;
    return wkt_put_count(r, pos, count);
}

static void
wkt_free(WKT *s) {
	if(s->cap) free(s->p);
}

static int
wkt2geometry1(WktReader *r) {
    wkt_putb(r, 1); /* little endian */
    int rc;
    if(WKT_TAKE(r, "POINT")) {
        if((rc = wkt_point(r))) return rc;
    } else if(WKT_TAKE(r, "LINESTRING")) {
        if((rc = wkt_linestring(r))) return rc;
    } else if(WKT_TAKE(r, "POLYGON")) {
        if((rc = wkt_polygon(r))) return rc;
    } else if(WKT_TAKE(r, "MULTIPOINT")) {
        if((rc = wkt_multipoint(r))) return rc;
    } else if(WKT_TAKE(r, "MULTILINESTRING")) {
        if((rc = wkt_multilinestring(r))) return rc;
    } else if(WKT_TAKE(r, "MULTIPOLYGON")) {
        if((rc = wkt_multipolygon(r))) return rc;
    } else if(WKT_TAKE(r, "GEOMETRYCOLLECTION")) {
        wkt_skip(r);
        if(!WKT_TAKE(r, "(")) return -20;
        wkt_putu(r, 7); /* geometry prefix */
        size_t pos = wkt_reserve_count(r);
        size_t count = 0;
        do {
            if((rc = wkt2geometry1(r))) return rc;
            ++count;
        } while(WKT_TAKE(r, ","));
        if((rc = wkt_put_count(r, pos, count))) return -22;
        if(!WKT_TAKE(r, ")")) return -21;
    } else {
        return -2;
    }
    return 0;
}

static int
wkt2geometry(WktReader *r) {
    int rc = 0;
    wkt_skip(r);
    if(r->p == r->end) return -1;

    while(r->p < r->end) {
        if((rc = wkt2geometry1(r))) return rc;
        wkt_skip(r);
    }

    return 0;
}

static int
wkt2wkb(WKT *s, const char *src, size_t nsrc) {
    WktReader r;
    r.s = s;
    r.p = src;
    r.end = src + nsrc;
    wkt_reserve(&r, nsrc);
    return wkt2geometry(&r);
}

#ifdef WKT_EXAMPLE
#include "wkb.h"
#include <assert.h>

#define WKT2WKB(s, literal) wkt2wkb(s, literal, sizeof literal - 1)

int main(int argc, char **argv) {
	WKT s = {0};
    wkt_clear(&s);
    assert(!WKT2WKB(&s, "POINT(10 11)"));

    WKB b = {0};
    assert(!wkb2wkt(&b, s.p, s.n));
    printf("(%zu) '%.*s'\n", b.n, (int)b.n, b.p);

    wkt_clear(&s);
    wkb_clear(&b);
    assert(!WKT2WKB(&s, "POINT(10   -11.44)"));
    assert(!wkb2wkt(&b, s.p, s.n));
    printf("(%zu) '%.*s'\n", b.n, (int)b.n, b.p);

    wkt_clear(&s);
    wkb_clear(&b);
    assert(!WKT2WKB(&s, "POINT ( 10 12.34 )  "));
    assert(!wkb2wkt(&b, s.p, s.n));
    printf("(%zu) '%.*s'\n", b.n, (int)b.n, b.p);

    wkt_clear(&s);
    wkb_clear(&b);
    assert(!WKT2WKB(&s, " MULTIPOINT(10 12.34, 40 30, 20 20 )  "));
    assert(!wkb2wkt(&b, s.p, s.n));
    printf("(%zu) '%.*s'\n", b.n, (int)b.n, b.p);

    wkt_clear(&s);
    wkb_clear(&b);
    assert(!WKT2WKB(&s, " MULTIPOINT(10 12.34, 40 30, 20 20 )  "));
    assert(!wkb2wkt(&b, s.p, s.n));
    printf("(%zu) '%.*s'\n", b.n, (int)b.n, b.p);

    wkt_clear(&s);
    wkb_clear(&b);
    assert(!WKT2WKB(&s, " POLYGON ( (10 12.34, 40 30, 20 20) , (4 5, 6 7 ,8 9) )  "));
    assert(!wkb2wkt(&b, s.p, s.n));
    printf("(%zu) '%.*s'\n", b.n, (int)b.n, b.p);

    wkt_clear(&s);
    wkb_clear(&b);
    assert(!WKT2WKB(&s, " MULTIPOLYGON ( ((10 12.34, 40 30, 20 20) , (4 5, 6 7 ,8 9) ), ((100 200, 300 400, 500  -600), (-4 -5, -6 -7, -8.55 -9)))  "));
    assert(!wkb2wkt(&b, s.p, s.n));
    printf("(%zu) '%.*s'\n", b.n, (int)b.n, b.p);

    wkt_clear(&s);
    wkb_clear(&b);
    assert(!WKT2WKB(&s, " MULTILINESTRING ( (10 12.34, 40 30, 20 20, 4 5, 6 7 ,8 9) , (100 200, 300 400, 500  -600, -4 -5, -6 -7, -8.55 -9))  "));
    assert(!wkb2wkt(&b, s.p, s.n));
    printf("(%zu) '%.*s'\n", b.n, (int)b.n, b.p);

    wkt_clear(&s);
    wkb_clear(&b);
    assert(!WKT2WKB(&s, " LINESTRING ( 10 12.34, 40 30, 20 20, 4 5, 6 7 ,8 9 , 100 200, 300 400, 500  -600, -4 -5, -6 -7, -8.55 -9)  "));
    assert(!wkb2wkt(&b, s.p, s.n));
    printf("(%zu) '%.*s'\n", b.n, (int)b.n, b.p);


    wkt_clear(&s);
    wkb_clear(&b);
    assert(!WKT2WKB(&s, "GEOMETRYCOLLECTION( POINT(10   -11.44), LINESTRING(40 50, 60.1 70.234))"));
    assert(!wkb2wkt(&b, s.p, s.n));
    printf("(%zu) '%.*s'\n", b.n, (int)b.n, b.p);

    wkb_free(&b);
    wkt_free(&s);
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
