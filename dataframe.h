#ifndef DATAFRAME_H
#define DATAFRAME_H

#include <stddef.h>
#include <stdint.h>

#if defined(DATAFRAME_STATIC) || defined(DATAFRAME_EXAMPLE)
#define DATAFRAME_API static
#define DATAFRAME_IMPLEMENTATION
#else
#define DATAFRAME_API extern
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define DATAFRAME_MAXCOL 64
#define DATAFRAME_MAXNAME 64

typedef uint8_t DataframeType;

enum {
    dataframe_i8,
    dataframe_i16,
    dataframe_i32,
    dataframe_i64,
    dataframe_u8,
    dataframe_u16,
    dataframe_u32,
    dataframe_u64,
    dataframe_str,
    dataframe_float,
    dataframe_double,
    /* int64_t nanoseconds since unix epoch */
    dataframe_time,
    dataframe_uuid
};

typedef uint8_t DataframeUuid[16];

typedef union DataframeVal {
    int8_t *i8;
    int16_t *i16;
    int32_t *i32;
    int64_t *i64;
    uint8_t *u8;
    uint16_t *u16;
    uint32_t *u32;
    uint64_t *u64;
    float *f;
    double *d;
    char **s;
    int64_t *time;
    void *v;
    DataframeUuid* uuid;
} DataframeVal;


typedef struct DataframeCol {
    char *name;
    DataframeVal vals;
    DataframeType type;
} DataframeCol;

typedef struct Dataframe {
    DataframeCol cols[DATAFRAME_MAXCOL];
    char *name;
    size_t ncols, nrows, row_capacity;
    unsigned deserialized : 1;
} Dataframe;

DATAFRAME_API void dataframe_init(Dataframe*, const char *name);
DATAFRAME_API DataframeCol* dataframe_addcol(Dataframe*, const char *name, DataframeType);
DATAFRAME_API void dataframe_reserve(Dataframe*, size_t n_new_rows);
DATAFRAME_API void dataframe_resize(Dataframe *df, size_t n);
DATAFRAME_API int dataframe_typesize(DataframeType);
DATAFRAME_API DataframeCol* dataframe_col(Dataframe* df, const char *name);
DATAFRAME_API void dataframe_serialize(Dataframe *df, uint8_t **data, size_t *size);
DATAFRAME_API char* dataframe_strdup(const char *str);
static size_t dataframe_idx(Dataframe *df) { return df->nrows; }
DATAFRAME_API size_t dataframe_add(Dataframe *df);
/* deserializes into read-only memory referencing data, size.
   Do not add-remove or delete from dataframe. do not call dataframe_free.
   do not free or modify data until done using dataframe */
DATAFRAME_API int dataframe_deserialize(Dataframe *df, const uint8_t *data, size_t size);
DATAFRAME_API void dataframe_free(Dataframe*);

#ifdef __cplusplus
}
#endif

#endif


#ifdef DATAFRAME_IMPLEMENTATION
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef struct DataframeHeader {
    uint64_t nrows;
    uint32_t ncols;
} DataframeHeader;

DATAFRAME_API void
dataframe_init(Dataframe *df, const char *name) {
    memset(df, 0, sizeof *df);
    df->name = dataframe_strdup(name);
}

DATAFRAME_API char*
dataframe_strdup(const char *str) {
    if(!str) str = "";
    size_t n = strlen(str) + 1;
    char *p = (char*)malloc(n);
    memcpy(p, str, n);
    return p;
}

DATAFRAME_API DataframeCol*
dataframe_col(Dataframe* df, const char *name) {
    for(size_t i=0;i<df->nrows;i++) {
        DataframeCol *c = &df->cols[i];
        if(!strcmp(c->name, name)) return c;
    }
    return 0;
}

DATAFRAME_API void
dataframe_resize(Dataframe *df, size_t n) {
    if(df->row_capacity == n) return;
    for(size_t i=0;i<df->ncols;i++) {
        DataframeCol *col = &df->cols[i];
        col->vals.v = realloc(col->vals.v, n * dataframe_typesize(col->type));
    }
    df->row_capacity = n;
}

DATAFRAME_API void
dataframe_reserve(Dataframe *df, size_t nnew_rows) {
    uint64_t req = df->nrows + nnew_rows;
    uint64_t cap;
    if(df->row_capacity >= req) return;
    cap = df->row_capacity * 2;
    if(cap < 1024) cap = 1024;
    if(cap < req) cap = req;

    dataframe_resize(df, cap);
}

DATAFRAME_API size_t
dataframe_add(Dataframe *df) {
    size_t n = df->nrows;
    dataframe_reserve(df, 1);
    df->nrows++;
    return n;
}

DATAFRAME_API int
dataframe_typesize(DataframeType type) {
    int sz;
    switch (type) {
    case dataframe_u8:
    case dataframe_i8: sz = 1; break;
    case dataframe_u16:
    case dataframe_i16: sz = 2; break;
    case dataframe_u32:
    case dataframe_i32:
    case dataframe_float: sz = 4; break;
    case dataframe_u64:
    case dataframe_i64:
    case dataframe_time:
    case dataframe_double: sz = 8; break;
    case dataframe_str: sz = sizeof(char*); break;
    case dataframe_uuid: sz = 16; break;
    default: sz = -1; break;
    }
    return sz;
}

DATAFRAME_API DataframeCol*
dataframe_addcol(Dataframe *df, const char *name, DataframeType type) {
    if(df->ncols >= DATAFRAME_MAXCOL) return 0;

    DataframeCol *col = &df->cols[df->ncols];
    memset(col, 0, sizeof *col);
    size_t n = strlen(name) + 1;
    col->name = (char*)malloc(n);
    memcpy(col->name, name, n);
    col->type = type;
    int sz = dataframe_typesize(type);
    if(sz < 0) return 0;
    col->vals.v = malloc(df->row_capacity * (uint32_t)sz);
    if(!col->vals.v) {
        free(col->vals.v);
        return 0;
    }
    return &df->cols[df->ncols++];
}

DATAFRAME_API void
dataframe_serialize(Dataframe *df, uint8_t **data, size_t *size) {
    DataframeHeader h = {0};
    h.ncols = df->ncols;
    h.nrows = df->nrows;
    size_t total = 0;
    total += sizeof h; /* header */
    char *name = df->name ? df->name : (char*)"";
    uint32_t n = strlen(name) + 1;
    total += sizeof(n) + n;

    for(size_t i=0;i<df->ncols;i++) {
        DataframeCol *col = &df->cols[i];
        total += strlen(col->name) + sizeof(uint32_t) + 1; /* name */
        total += sizeof col->type; /* type */
    }
    printf("total=%zu\n", total);

    for(size_t i=0;i<df->ncols;i++) {
        DataframeCol *col = &df->cols[i];
        size_t sz = dataframe_typesize(col->type);
        if(col->type == dataframe_str) {
            for(size_t j=0;j<df->nrows;j++) {
                char *s = col->vals.s[j];
                n = strlen(s) + 1;
                total += sizeof n + n;
                printf("row=%zu total=%zu\n", j, total);
            }
        } else total += sz * df->nrows;
        printf("col=%zu total=%zu\n", i, total);
    }
    printf("total2=%zu\n", total);

    uint8_t *p = (uint8_t*)malloc(total);
    *data = p;
    memcpy(p, &h, sizeof h); p += sizeof h;
    n = strlen(name) + 1;
    memcpy(p, &n, sizeof(n)); p += sizeof n;
    memcpy(p, name, n); p += n;
    for(size_t i=0;i<df->ncols;i++) {
        DataframeCol *col = &df->cols[i];
        n = strlen(col->name) + 1;
        memcpy(p, &n, sizeof n); p += sizeof n;
        memcpy(p, col->name, n); p += n;
        memcpy(p, &col->type, sizeof col->type); p += sizeof col->type;
    }
    printf("data=%zu\n", (size_t)(p - *data));
    for(size_t i=0;i<df->ncols;i++) {
        DataframeCol *col = &df->cols[i];
        size_t sz = dataframe_typesize(col->type);
        if(col->type == dataframe_str) {
            for(size_t j=0;j<df->nrows;j++) {
                char *s = col->vals.s[j];
                n = strlen(s) + 1;
                memcpy(p, &n, sizeof n); p += sizeof n;
                memcpy(p, s, n); p += n;
                printf("row=%zu total=%zu\n", j, (size_t)(p - *data));
            }
        } else {
            memcpy(p, col->vals.v, sz * df->nrows);
            p += sz * df->nrows;
        }
        printf("col=%zu total=%zu\n", i, (size_t)(p - *data));
    }
    printf("data2=%zu\n", (size_t)(p - *data));
    assert((ptrdiff_t)(p - *data) == (ptrdiff_t)total);
    *size = total;
}

DATAFRAME_API int
dataframe_deserialize(Dataframe *df, const uint8_t *data, size_t size) {
    memset(df, 0, sizeof *df);

    df->deserialized = 1;
    DataframeHeader h;
    if(size < sizeof h) return -1;
    memcpy(&h, data, sizeof h); data += sizeof h; size -= sizeof h;
    df->ncols = h.ncols;
    df->nrows = df->row_capacity = h.nrows;
    uint32_t n;
    if(size < sizeof n) return -1;
    memcpy(&n, data, sizeof n); data += sizeof n; size -= sizeof n;
    if(size < n) return -1;
    df->name = (char*)data; data += n; size -= n;

    for(size_t i=0;i<df->ncols;i++) {
        DataframeCol *col = &df->cols[i];

        if(size < sizeof n) return -1;
        memcpy(&n, data, sizeof n); data += sizeof n; size -= sizeof n;

        if(size < n) return -1;
        col->name = (char*)data; data += n; size -= n;

        if(size < sizeof col->type) return -1;
        memcpy(&col->type, data, sizeof col->type);
        data += sizeof col->type; size -= sizeof col->type;
        col->vals.s = 0;
    }
    for(size_t i=0;i<df->ncols;i++) {
        DataframeCol *col = &df->cols[i];
        size_t sz = dataframe_typesize(col->type);

        size_t x = sz * df->nrows;
        if(size < x) return -1;
        if(col->type == dataframe_str) {
            df->cols[i].vals.s = (char**)malloc(sizeof(char*) * df->nrows);
            for(size_t j=0;j<df->nrows;j++) {
                if(size < sizeof n) goto error;
                memcpy(&n, data, sizeof n); data += sizeof n; size -= sizeof n;
                if(size < n) goto error;
                col->vals.s[j] = (char*)data;
                data += n; size -= n;
            }
        } else df->cols[i].vals.v = (void*)data; data += x; size -= x;
    }
    return 0;
error:
    dataframe_free(df);
    return -1;
}

DATAFRAME_API void
dataframe_free(Dataframe *df) {
    if(!df->deserialized) free(df->name);
    for(size_t i=0;i<df->ncols;i++) {
        DataframeCol *col = &df->cols[i];
        if(df->deserialized) {
            if(col->type == dataframe_str)
                free(col->vals.s);
        } else {
            free(col->name);
            if(col->type == dataframe_str) {
                for(size_t j=0;j<df->nrows;j++)
                    free(col->vals.s[j]);
            }
            free(col->vals.v);
        }
    }
}

#endif

#ifdef DATAFRAME_EXAMPLE
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 199309L
#endif
#include <time.h>
static double now() {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return (double)ts.tv_sec + (double)ts.tv_nsec / 1000.0/1000.0/1000.0;
}
int main(int argc, char **argv) {
    Dataframe df;
    dataframe_init(&df, "values");
    DataframeCol *id = dataframe_addcol(&df, "id", dataframe_i32);
    DataframeCol *name = dataframe_addcol(&df, "name", dataframe_str);
    dataframe_reserve(&df, 10);
    for(int i=0;i<5;i++) {
        size_t x = dataframe_add(&df);
        id->vals.i32[x] = i;
        name->vals.s[x] = dataframe_strdup("hello");
    }
    uint8_t *ser;
    size_t nser;
    double t = now();
    dataframe_serialize(&df, &ser, &nser);
    printf("serialized %zu in %f sec\n", nser, now() - t);
    Dataframe d2;
    t = now();
    dataframe_deserialize(&d2, ser, nser);
    printf("deserialized %zu in %f sec\n", nser, now() - t);
    assert(d2.deserialized == 1);
    assert(d2.ncols == df.ncols);
    assert(d2.nrows == df.nrows);
    assert(!strcmp(d2.name, df.name));
    for(size_t i=0;i<df.ncols;i++) {
        DataframeCol *c, *c2;
        c = &df.cols[i];
        c2 = &d2.cols[i];
        assert(c->type == c2->type);
        assert(!strcmp(c->name, c2->name));
        if(c->type == dataframe_str) {
            for(size_t j=0;j<df.nrows;j++) {
                printf("j=%zu %s %s\n", j, c->vals.s[j], c2->vals.s[j]);
                assert(!strcmp(c->vals.s[j], c2->vals.s[j]));
            }
        } else
            assert(!memcmp(c->vals.v, c2->vals.v, dataframe_typesize(c2->type) * df.nrows));
    }

    free(ser);
    dataframe_free(&d2);
    dataframe_free(&df);
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