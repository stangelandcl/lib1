#ifndef CSTRING_H
#define CSTRING_H

/* C strings with short string optimization */

#include <ctype.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CSTRING_SHORT_MAX (sizeof(void*)*4-1)
typedef struct CStringShort {
    unsigned char s[CSTRING_SHORT_MAX + 1];
} CStringShort;

typedef struct CStringLong {
    size_t n;
    size_t capacity;
    char *s;
} CStringLong;

typedef union CString {
    CStringShort s;
    CStringLong l;
} CString;



static void
cstring_init(CString *s) {
    s->s.s[0] = 0;
    s->s.s[1] = 0;
}

static int
cstring_islong(CString *s) {
    return s->s.s[0] & 1;
}

static int
cstring_isshort(CString *s) {
    return !cstring_islong(s);
}

static void
cstring_destroy(CString *s) {
    if(cstring_islong(s)) free(s->l.s);
    cstring_init(s);
}

static size_t
cstring_size(CString *s) {
    if(cstring_islong(s)) return s->l.n >> 1;
    return s->s.s[0] >> 1;
}

static char*
cstring_str(CString *s) {
    if(cstring_islong(s)) return s->l.s ? s->l.s : (char*)"";
    return (char*)s->s.s + 1;
}

static void
cstring_lower(CString *s) {
    char *p = cstring_str(s);
    size_t i, n = cstring_size(s);
    for(i=0;i<n;i++) p[i] = tolower(p[i]);
}

static void
cstring_upper(CString *s) {
    char *p = cstring_str(s);
    size_t i, n = cstring_size(s);
    for(i=0;i<n;i++) p[i] = toupper(p[i]);
}

static int
cstring_casecmp(CString *x, CString *y) {
    char *xp = cstring_str(x);
    size_t xn = cstring_size(x);
    char *yp = cstring_str(y);
    size_t yn = cstring_size(y);
    size_t i, mn = xn < yn ? xn : yn;
    for(i=0;i<mn;i++) {
        int a = toupper(xp[i]);
        int b = toupper(yp[i]);
        int c = a - b;
        if(c) return c;
    }
    if(xn < yn) return -1;
    if(xn > yn) return 1;
    return 0;
}

static int
cstring_cmp(CString *x, CString *y) {
    char *xp = cstring_str(x);
    size_t xn = cstring_size(x);
    char *yp = cstring_str(y);
    size_t yn = cstring_size(y);
    size_t i, mn = xn < yn ? xn : yn;
    for(i=0;i<mn;i++) {
        int c = (int)xp[i] - (int)yp[i];
        if(c) return c;
    }
    if(xn < yn) return -1;
    if(xn > yn) return 1;
    return 0;
}

static void
cstring_clear(CString *s) {
    if(cstring_islong(s)) {
        s->l.n = 0;
        s->s.s[0] = 1;
    } else {
        s->s.s[0] = 0;
        s->s.s[1] = 0;
    }
}

static int
cstring_setn(CString *s, const char *str, size_t n) {
    if(!str) str = "";
    ++n; /* include null terminator */
    if(n <= CSTRING_SHORT_MAX) {
        if(cstring_islong(s)) {
            if(s->l.capacity >= n) goto addlong;
            cstring_destroy(s);
        }

        memcpy(s->s.s + 1, str, --n);
        s->s.s[n] = 0;
        s->s.s[0] = n << 1;
        return 0;
    }

    if(cstring_isshort(s)) {
        s->l.capacity = 0;
        s->l.n = 0;
        s->l.s = 0;
    }

    if(s->l.capacity < n) {
        size_t cap = s->l.capacity * 2;
        char *p;
        if(cap < n) cap = n;
        p = (char*)realloc(s->l.s, cap);
        if(!p) return -1;
        s->l.s = p;
        s->l.capacity = cap;
    }
addlong:
    memcpy(s->l.s, str, --n);
    s->l.s[n] = 0;
    s->l.n = (n << 1) | 1;
    return 0;
}

/* return -1 on memory allocation failure */
static int
cstring_set(CString *s, const char *str) {
    if(!str) str = "";
    return cstring_setn(s, str, strlen(str));
}



#ifdef __cplusplus
}
#endif

#endif
#ifdef CSTRING_EXAMPLE
#include <stdio.h>
int main(int argc, char **argv) {
    CString s = {0};

    cstring_set(&s, "hello world!");
    printf("cstring=%s size=%zu short=%d\n",
        cstring_str(&s), cstring_size(&s), cstring_isshort(&s));
    cstring_set(&s, "a lOOOOoooooooooooooooooooooong STRing!!!!!!!!!!!!!!!!!!!!");
    printf("cstring=%s size=%zu short=%d\n",
        cstring_str(&s), cstring_size(&s), cstring_isshort(&s));
    cstring_upper(&s);
    printf("cstring=%s size=%zu short=%d\n",
        cstring_str(&s), cstring_size(&s), cstring_isshort(&s));
    cstring_lower(&s);
    printf("cstring=%s size=%zu short=%d\n",
        cstring_str(&s), cstring_size(&s), cstring_isshort(&s));
    cstring_destroy(&s);
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