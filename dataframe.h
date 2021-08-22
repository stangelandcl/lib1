#ifndef DATAFRAME_H
#define DATAFRAME_H

/* license (public domain) and example at bottom of file */

#if defined(DATAFRAME_STATIC) || defined(DATAFRAME_EXAMPLE)
#define DATAFRAME_API static
#define DATAFRAME_IMPLEMENTATION
#else
#define DATAFRAME_API extern
#endif

#include <stddef.h>
#include <stdint.h>


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

typedef union DataframeVals {
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
} DataframeVals;

typedef union DataframeVal {
    int8_t i8;
    int16_t i16;
    int32_t i32;
    int64_t i64;
    uint8_t u8;
    uint16_t u16;
    uint32_t u32;
    uint64_t u64;
    float f;
    double d;
    char *s;
    int64_t time;
    DataframeUuid uuid;
} DataframeVal;

typedef struct DataframeItem {
    DataframeVal *v;
    DataframeType type;
    size_t item_size;
} DataframeItem;

typedef struct DataframeCol {
    char *name;
    DataframeVals vals;
    DataframeType type;
    size_t item_size; /* in bytes */
    size_t i;
} DataframeCol;

typedef struct Dataframe {
    DataframeCol cols[DATAFRAME_MAXCOL];
    char *name;
    size_t ncols, nrows, row_capacity;
    unsigned deserialized : 1;
} Dataframe;

typedef struct DataframeBuf {
    uint8_t *p;
    size_t n, capacity;
} DataframeBuf;

DATAFRAME_API void dataframe_buf_reserve(DataframeBuf*, size_t n);
DATAFRAME_API void dataframe_init(Dataframe*, const char *name);
DATAFRAME_API DataframeCol* dataframe_addcol(Dataframe*, const char *name, DataframeType);
DATAFRAME_API DataframeItem dataframe_item(Dataframe*, size_t col, size_t row);
DATAFRAME_API size_t dataframe_itemsize(DataframeItem*);
DATAFRAME_API void* dataframe_itemptr(DataframeItem*);
DATAFRAME_API int dataframe_equal(DataframeItem *x, DataframeItem *y);
DATAFRAME_API size_t dataframe_add(Dataframe *df, size_t n);
DATAFRAME_API void dataframe_set(Dataframe *df, size_t col, size_t row, const void *data);
DATAFRAME_API size_t dataframe_typesize(DataframeType);
DATAFRAME_API DataframeCol* dataframe_col(Dataframe* df, const char *name);
DATAFRAME_API void dataframe_serialize(Dataframe *df, DataframeBuf *buf);
DATAFRAME_API char* dataframe_strdup(const char *str);
DATAFRAME_API void dataframe_print(Dataframe *df);
static size_t dataframe_idx(Dataframe *df) { return df->nrows; }
/* clear rows */
DATAFRAME_API void dataframe_clear(Dataframe *df);
/* deserializes into read-only memory referencing data, size.
   Do not add-remove or delete from dataframe. do not call dataframe_free.
   do not free or modify data until done using dataframe */
DATAFRAME_API int dataframe_deserialize(Dataframe *df, const uint8_t *data, size_t size);
DATAFRAME_API int dataframe_deserialize_copy(Dataframe *df, const uint8_t *data, size_t size);
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

DATAFRAME_API void
dataframe_print(Dataframe *df) {
    /* header */
    for(size_t k=0;k<df->ncols;k++) {
        DataframeCol *c = &df->cols[k];
        if(k) printf(",");
        printf("%s", c->name);
    }
    printf("\n");

    /* body */
    for(size_t j=0;j<df->nrows;j++) {
        for(size_t k=0;k<df->ncols;k++) {
            DataframeCol *c = &df->cols[k];
            if(k) printf(",");
            switch(c->type) {
            case dataframe_i8: printf("%d", c->vals.i8[j]); break;
            case dataframe_i16: printf("%d", c->vals.i16[j]); break;
            case dataframe_i32: printf("%d", c->vals.i32[j]); break;
            case dataframe_time:
            case dataframe_i64: printf("%ld", c->vals.i64[j]); break;
            case dataframe_u8: printf("%u", c->vals.u8[j]); break;
            case dataframe_u16: printf("%u", c->vals.u16[j]); break;
            case dataframe_u32: printf("%u", c->vals.u32[j]); break;
            case dataframe_u64: printf("%lu", c->vals.u64[j]); break;
            case dataframe_double: printf("%f", c->vals.d[j]); break;
            case dataframe_float: printf("%f", c->vals.f[j]); break;
            case dataframe_str: printf("%s", c->vals.s[j]); break;
            case dataframe_uuid:
                for(size_t x=0;x<sizeof(DataframeUuid);x++)
                    printf("%02x", c->vals.uuid[j][x]);
                break;
            default: printf("(type?)");
            }
        }
        printf("\n");
    }
}

DATAFRAME_API DataframeItem
dataframe_item(Dataframe *df, size_t column, size_t row) {
    DataframeItem v;
    DataframeCol *col = &df->cols[column];
    v.v = (void*)col->vals.u8 + col->item_size * row;
    v.type = col->type;
    v.item_size = col->item_size;
    return v;
}

DATAFRAME_API size_t
dataframe_itemsize(DataframeItem *item) {
    if(item->type == dataframe_str)
        return strlen(item->v->s) + 1;
    return item->item_size;
}

DATAFRAME_API void*
dataframe_itemptr(DataframeItem *item) {
    if(item->type == dataframe_str) return item->v->s;
    else return item->v;
}

DATAFRAME_API int
dataframe_equal(DataframeItem *x, DataframeItem *y) {
    if(x->type != y->type) return -1;
    if(x->type == dataframe_str)
        return !strcmp(x->v->s, y->v->s);
    return !memcmp(x->v, y->v, x->item_size);
}

DATAFRAME_API void
dataframe_set(Dataframe *df, size_t coli, size_t row, const void *data) {
    if(coli >= df->ncols || row >= df->nrows) return;
    DataframeCol *col = &df->cols[coli];
    if(col->type == dataframe_str)
        col->vals.s[row] = dataframe_strdup((const char*)data);
    else {
        uint8_t *p = col->vals.u8 + col->item_size * row;
        memcpy(p, data, col->item_size);
    }
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
    for(size_t i=0;i<df->ncols;i++) {
        DataframeCol *c = &df->cols[i];
        if(!strcmp(c->name, name)) return c;
    }
    return 0;
}

DATAFRAME_API size_t
dataframe_add(Dataframe *df, size_t n) {
    size_t req = df->nrows + n;
    size_t cap;
    size_t old = df->nrows;
    if(df->row_capacity < req) {
        cap = df->row_capacity * 2;
        if(cap < 1024) cap = 1024;
        if(cap < req) cap = req;
        for(size_t i=0;i<df->ncols;i++) {
            DataframeCol *col = &df->cols[i];
            col->vals.v = realloc(col->vals.v, cap * col->item_size);
            memset(col->vals.u8 + old * col->item_size, 0, (cap - old) * col->item_size);
        }
        df->row_capacity = cap;
    }
    df->nrows += n;
    assert(df->nrows <= df->row_capacity);
    return (size_t)old;
}

DATAFRAME_API void
dataframe_clear(Dataframe *df) {
    for(size_t i=0;i<df->ncols;i++) {
        DataframeCol *col = &df->cols[i];
        if(col->type == dataframe_str) {
            for(size_t j=0;j<df->nrows;j++)
                free(col->vals.s[j]);
        }
    }
    df->nrows = 0;
}

DATAFRAME_API size_t
dataframe_typesize(DataframeType type) {
    size_t sz;
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
    default: sz = 0; break;
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
    col->item_size = dataframe_typesize(type);
    if(col->item_size < 0) return 0;
    col->vals.v = malloc(df->row_capacity * (uint32_t)col->item_size);
    if(!col->vals.v) {
        free(col->vals.v);
        return 0;
    }
    col->i = df->ncols;
    return &df->cols[df->ncols++];
}

DATAFRAME_API void
dataframe_buf_reserve(DataframeBuf *buf, size_t n) {
    size_t req = n + buf->n;
    if(buf->capacity < req) {
        size_t cap = buf->capacity * 2;
        if(cap < 1024) cap = 1024;
        if(cap < req) cap = req;
        buf->p = realloc(buf->p, cap);
        buf->capacity = cap;
    }
}

DATAFRAME_API void
dataframe_serialize(Dataframe *df, DataframeBuf *buf) {
    DataframeHeader h = {0};
    h.ncols = df->ncols;
    h.nrows = df->nrows;
    size_t total = 0;
    total += sizeof h; /* header */
    char *name = df->name ? df->name : (char*)"";
    uint32_t n = strlen(name) + 1;
    total += sizeof(n) + n;

    printf("name=%zu\n", total);
    for(size_t i=0;i<df->ncols;i++) {
        DataframeCol *col = &df->cols[i];
        total += strlen(col->name) + sizeof(uint32_t) + 1; /* name */
        total += sizeof col->type; /* type */
    }
    printf("header=%zu\n", total);

    for(size_t i=0;i<df->ncols;i++) {
        DataframeCol *col = &df->cols[i];
        size_t sz = dataframe_typesize(col->type);
        if(col->type == dataframe_str) {
            for(size_t j=0;j<df->nrows;j++) {
                char *s = col->vals.s[j];
                if(!s) s = "";
                n = strlen(s) + 1;
                total += sizeof n + n;
            }
        } else total += sz * df->nrows;
    }

    dataframe_buf_reserve(buf, total);
    uint8_t *p = buf->p + buf->n;

    memcpy(p, &h, sizeof h); p += sizeof h;
    n = strlen(name) + 1;
    memcpy(p, &n, sizeof(n)); p += sizeof n;
    memcpy(p, name, n); p += n;
    printf("name=%zu\n", p - buf->p - buf->n);

    for(size_t i=0;i<df->ncols;i++) {
        DataframeCol *col = &df->cols[i];
        n = strlen(col->name) + 1;
        memcpy(p, &n, sizeof n); p += sizeof n;
        memcpy(p, col->name, n); p += n;
        memcpy(p, &col->type, sizeof col->type); p += sizeof col->type;
    }
    printf("header=%zu\n", p - buf->p - buf->n);

    for(size_t i=0;i<df->ncols;i++) {
        DataframeCol *col = &df->cols[i];
        size_t sz = dataframe_typesize(col->type);
        if(col->type == dataframe_str) {
            for(size_t j=0;j<df->nrows;j++) {
                char *s = col->vals.s[j];
                if(!s) s = "";
                n = strlen(s) + 1;
                memcpy(p, &n, sizeof n); p += sizeof n;
                memcpy(p, s, n); p += n;
            }
        } else {
            memcpy(p, col->vals.v, sz * df->nrows);
            p += sz * df->nrows;
        }
    }
    printf("old=%zu total=%zu\n", p - buf->p - buf->n, total);
    assert((ptrdiff_t)(p - buf->p - buf->n) == (ptrdiff_t)total);
    buf->n += total;
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
        col->item_size = (uint32_t)dataframe_typesize(col->type);
        col->i = i;
    }
    for(size_t i=0;i<df->ncols;i++) {
        DataframeCol *col = &df->cols[i];
        size_t sz = col->item_size;

        if(col->type == dataframe_str) {
            df->cols[i].vals.s = (char**)calloc(sizeof(char*), df->nrows);
            for(size_t j=0;j<df->nrows;j++) {
                if(size < sizeof n) goto error;
                memcpy(&n, data, sizeof n); data += sizeof n; size -= sizeof n;
                if(size < n) goto error;
                col->vals.s[j] = (char*)data;
                data += n; size -= n;
            }
        } else {
            size_t x = sz * df->nrows;
            if(size < x) return -1;
            df->cols[i].vals.v = (void*)data;
            data += x; size -= x;
        }
    }
    return 0;
error:
    dataframe_free(df);
    return -1;
}

DATAFRAME_API int
dataframe_deserialize_copy(Dataframe *df, const uint8_t *data, size_t size) {
    memset(df, 0, sizeof *df);

    DataframeHeader h;
    if(size < sizeof h) return -1;
    memcpy(&h, data, sizeof h); data += sizeof h; size -= sizeof h;
    df->ncols = h.ncols;
    df->nrows = df->row_capacity = h.nrows;
    uint32_t n;
    if(size < sizeof n) return -1;
    memcpy(&n, data, sizeof n); data += sizeof n; size -= sizeof n;
    if(size < n) return -1;
    df->name = malloc(n);
    memcpy(df->name, data, n);
    data += n; size -= n;

    for(size_t i=0;i<df->ncols;i++) {
        DataframeCol *col = &df->cols[i];

        if(size < sizeof n) return -1;
        memcpy(&n, data, sizeof n); data += sizeof n; size -= sizeof n;

        if(size < n) return -1;
        col->name = malloc(n);
        memcpy(col->name, data, n);
        data += n; size -= n;

        if(size < sizeof col->type) return -1;
        memcpy(&col->type, data, sizeof col->type);
        data += sizeof col->type; size -= sizeof col->type;
        col->item_size = (uint32_t)dataframe_typesize(col->type);
        col->i = i;
        col->vals.s = calloc(col->item_size, df->nrows);
    }
    for(size_t i=0;i<df->ncols;i++) {
        DataframeCol *col = &df->cols[i];
        size_t sz = col->item_size;
        if(col->type == dataframe_str) {
            for(size_t j=0;j<df->nrows;j++) {
                if(size < sizeof n) goto error;
                memcpy(&n, data, sizeof n); data += sizeof n; size -= sizeof n;
                if(size < n) goto error;
                col->vals.s[j] = malloc(n);
                memcpy(col->vals.s[j], data, n);
                data += n; size -= n;
            }
        } else {
            size_t x = sz * df->nrows;
            if(size < x) return -1;
            memcpy(col->vals.v, data, x);
            data += x; size -= x;
        }
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
    dataframe_addcol(&df, "id", dataframe_i32);
    dataframe_addcol(&df, "name", dataframe_str);
    dataframe_reserve(&df, 10);
    for(int i=0;i<5;i++) {
        size_t x = dataframe_add(&df);
        dataframe_set(&df, 0, i, &i);
        dataframe_set(&df, 1, i, "hello");
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
            assert(!memcmp(c->vals.v, c2->vals.v, c2->item_size * df.nrows));
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