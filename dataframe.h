#ifndef DATAFRAME_H
#define DATAFRAME_H

/* license (public domain) and example at bottom of file */

#if defined(DATAFRAME_STATIC) || defined(DATAFRAME_EXAMPLE)
#define DATAFRAME_API static
#define DATAFRAME_IMPLEMENTATION
#else
#define DATAFRAME_API extern
#endif

#include <ctype.h>
#include <stddef.h>
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t DataframeType;
typedef struct Dataframe Dataframe;

enum {
    dataframe_i8=1,
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

typedef struct DataframeSort {
    size_t col;
    int reverse;
} DataframeSort;

DATAFRAME_API Dataframe *dataframe_new(const char *name);
DATAFRAME_API const char *dataframe_name(Dataframe*);
DATAFRAME_API const char *dataframe_colname(Dataframe *, size_t col);
/* returns index of column */
DATAFRAME_API size_t dataframe_addcol(Dataframe*, const char *name, DataframeType);
DATAFRAME_API void dataframe_namecol(Dataframe *df, size_t col, const char *name);
DATAFRAME_API void dataframe_dropcol(Dataframe*, size_t i);
DATAFRAME_API void dataframe_dropcolname(Dataframe *, const char *name);
/* add n new rows. they are zero initialized. returns row index for use in setting row values */
DATAFRAME_API size_t dataframe_addrow(Dataframe *df, size_t n);
/* reserve n total rows. dataframe_addrow() still needs to be called before accessing the rows */
DATAFRAME_API void dataframe_reserve(Dataframe *df, size_t n);
/* assign data to cell */
DATAFRAME_API void dataframe_set(Dataframe *df, size_t col, size_t row, const void *data, size_t ndata);
/* returns index of column or -1 on not found */
DATAFRAME_API ptrdiff_t dataframe_col(Dataframe* df, const char *name);
DATAFRAME_API size_t dataframe_ncols(Dataframe *df);
DATAFRAME_API size_t dataframe_nrows(Dataframe *df);
DATAFRAME_API void dataframe_clearrows(Dataframe *df);
DATAFRAME_API size_t dataframe_coltypesize(Dataframe *df, size_t column);
DATAFRAME_API DataframeType dataframe_type(Dataframe *df, size_t column);
/* returns pointer to column data or 0 if column out of bounds */
DATAFRAME_API void* dataframe_getcol(Dataframe *df, size_t column);
DATAFRAME_API void dataframe_zerocol(Dataframe *df, size_t column);
DATAFRAME_API void dataframe_zero(Dataframe *df, size_t column, size_t row);
DATAFRAME_API void dataframe_get(Dataframe *df, size_t column, size_t row, void *dst, size_t ndest);
DATAFRAME_API void dataframe_getname(Dataframe *df, const char* column, size_t row, void *dst, size_t ndest);
DATAFRAME_API size_t dataframe_typesize(DataframeType);
DATAFRAME_API void dataframe_print(Dataframe *df);
DATAFRAME_API void dataframe_sort(Dataframe *df, DataframeSort *, size_t nsort);
DATAFRAME_API void dataframe_merge(Dataframe *dst, size_t dstcol, Dataframe *src, size_t srccol);
/* destroy dataframe */
DATAFRAME_API void dataframe_free(Dataframe*);

/* returns a reference to a cell */
DATAFRAME_API DataframeItem dataframe_item(Dataframe*, size_t col, size_t row);
DATAFRAME_API size_t dataframe_itemsize(DataframeItem*);
DATAFRAME_API void* dataframe_itemptr(DataframeItem*);
DATAFRAME_API int dataframe_itemcmp(DataframeItem *x, DataframeItem *y);


#ifdef __cplusplus
}
#endif

#endif


#ifdef DATAFRAME_IMPLEMENTATION
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

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

typedef struct DataframeCol {
    char *name;
    DataframeVals vals;
    DataframeType type;
    size_t item_size; /* in bytes */
    size_t i;
} DataframeCol;


struct Dataframe {
    DataframeCol *cols;
    size_t ncols, cols_capacity;
    char *name;
    size_t nrows, row_capacity;
};

typedef struct DataframeHeader {
    uint64_t nrows;
    uint32_t ncols;
} DataframeHeader;

DATAFRAME_API const char *
dataframe_name(Dataframe* df) {
	return df->name ? df->name : "";
}

DATAFRAME_API const char *
dataframe_colname(Dataframe *df, size_t col) {
	if(col >= df->ncols) return "";
	return df->cols[col].name ? df->cols[col].name : "";
}

static char*
dataframe_strdup(const char *str) {
    if(!str) str = "";
    size_t n = strlen(str) + 1;
    char * p = (char*)malloc(n);
    memcpy(p, str, n);
    return p;
}

DATAFRAME_API Dataframe*
dataframe_new(const char *name) {
    Dataframe *df = (Dataframe*)calloc(sizeof(Dataframe), 1);
    if(df) df->name = dataframe_strdup(name);
    return df;
}

DATAFRAME_API size_t
dataframe_nrows(Dataframe *df) {
    return df->nrows;
}

DATAFRAME_API size_t
dataframe_ncols(Dataframe *df) {
    return df->ncols;
}

DATAFRAME_API size_t
dataframe_addcol(Dataframe *df, const char *name, DataframeType type) {
    if(df->ncols == df->cols_capacity) {
        if(df->cols_capacity) df->cols_capacity = 16;
        else df->cols_capacity = 16;
        df->cols = (DataframeCol*)realloc(df->cols, df->cols_capacity * sizeof(DataframeCol));
        assert(df->cols);
    }

    DataframeCol *col = &df->cols[df->ncols];
    memset(col, 0, sizeof *col);
    col->name = dataframe_strdup(name);
    col->type = type;
    col->item_size = dataframe_typesize(type);
    col->vals.v = malloc(df->row_capacity * (uint32_t)col->item_size);
    assert(col->vals.v);
    return col->i = df->ncols++;
}

DATAFRAME_API void
dataframe_namecol(Dataframe *df, size_t col, const char *name) {
    if(col >= df->ncols) return;
    free(df->cols[col].name);
    df->cols[col].name = dataframe_strdup(name);
}

DATAFRAME_API void*
dataframe_getcol(Dataframe *df, size_t column) {
    if(column >= df->ncols) return 0;
    return df->cols[column].vals.v;
}

DATAFRAME_API void
dataframe_get(Dataframe *df, size_t column, size_t row, void *dst, size_t ndest) {
    if(column >= df->ncols || row >= df->nrows) {
        memset(dst, 0, ndest);
        return;
    }
    DataframeCol *col = &df->cols[column];
    size_t mn = ndest < col->item_size ? ndest : col->item_size;
    memcpy(dst, col->vals.u8 + col->item_size * row, mn);
}

DATAFRAME_API void
dataframe_getname(Dataframe *df, const char* column, size_t row, void *dst, size_t ndest) {
    ptrdiff_t col = dataframe_col(df, column);
    if(col < 0) {
        memset(dst, 0, ndest);
        return;
    }
    dataframe_get(df, col, row, dst, ndest);
}

DATAFRAME_API size_t
dataframe_coltypesize(Dataframe *df, size_t column) {
    if(column >= df->ncols) return 0;
    return df->cols[column].item_size;
}
DATAFRAME_API DataframeType
dataframe_type(Dataframe *df, size_t column) {
    if(column >= df->ncols) return 0;
    return df->cols[column].type;
}

DATAFRAME_API void
dataframe_dropcol(Dataframe *df, size_t i) {
    if(i >= df->ncols) return;
    DataframeCol *col = &df->cols[i];
    free(col->name);
    if(col->type == dataframe_str)
        for(size_t i = 0;i<df->nrows;i++)
            free(col->vals.s[i]);
    free(col->vals.v);
    for(++i;i<df->ncols;i++)
        df->cols[i-1] = df->cols[i];
    --df->ncols;
}

DATAFRAME_API void
dataframe_dropcolname(Dataframe *df, const char *name) {
    ptrdiff_t col = dataframe_col(df, name);
    if(col >= 0) dataframe_dropcol(df, (size_t)col);
}

DATAFRAME_API void
dataframe_zero(Dataframe *df, size_t column, size_t row) {
    if(column >= df->ncols) return;
    DataframeCol *col = &df->cols[column];
    if(col->type == dataframe_str) {
        free(col->vals.s[row]);
        col->vals.s[row] = 0;
    } else {
        memset(col->vals.u8 + col->item_size * row, 0, col->item_size);
    }
}

DATAFRAME_API void
dataframe_zerocol(Dataframe *df, size_t column) {
    if(column >= df->ncols) return;
    DataframeCol *col = &df->cols[column];
    if(col->type == dataframe_str) {
        for(size_t i=0;i<df->nrows;i++)
            free(col->vals.s[i]);
    }
    memset(col->vals.u8, 0, col->item_size * df->nrows);
}


DATAFRAME_API DataframeCol*
dataframe_column(Dataframe *df, size_t idx) {
    if(idx >= df->ncols) return 0;
    return &df->cols[idx];
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
            case dataframe_time: {
                char buf[21];
                time_t time = c->vals.time[j] / 1000000000;
                struct tm *tm = gmtime(&time);
                strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S", tm);
                printf("%s", buf);
                break;
            }
            case dataframe_i64: printf("%ld", c->vals.i64[j]); break;
            case dataframe_u8: printf("%u", c->vals.u8[j]); break;
            case dataframe_u16: printf("%u", c->vals.u16[j]); break;
            case dataframe_u32: printf("%u", c->vals.u32[j]); break;
            case dataframe_u64: printf("%lu", c->vals.u64[j]); break;
            case dataframe_double: printf("%f", c->vals.d[j]); break;
            case dataframe_float: printf("%f", c->vals.f[j]); break;
            case dataframe_str: printf("%s", c->vals.s[j] ? c->vals.s[j] : "(null)"); break;
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
    if(column >= df->ncols || row >= df->nrows) {
        memset(&v, 0, sizeof v);
        return v;
    }
    DataframeCol *col = &df->cols[column];
    v.v = (DataframeVal*)(col->vals.u8 + col->item_size * row);
    v.type = col->type;
    v.item_size = col->item_size;
    return v;
}

DATAFRAME_API size_t
dataframe_itemsize(DataframeItem *item) {
    if(item->type == dataframe_str)
        return strlen(item->v->s);
    return item->item_size;
}

DATAFRAME_API void*
dataframe_itemptr(DataframeItem *item) {
    if(item->type == dataframe_str)
        return item->v->s;
    else return item->v;
}

DATAFRAME_API void
dataframe_set(Dataframe *df, size_t coli, size_t row, const void *data, size_t n) {
    if(coli >= df->ncols || row >= df->nrows) return;
    DataframeCol *col = &df->cols[coli];
    if(col->type == dataframe_str) {
        free(col->vals.s[row]);
        col->vals.s[row] = (char*)malloc(n + 1);
        assert(col->vals.s[row]);
        memcpy(col->vals.s[row], data, n);
        col->vals.s[row][n] = 0;
    } else {
        uint8_t *p = col->vals.u8 + col->item_size * row;
        /* less or equal so we can pass in bigger numbers and use just the low
            bytes on little endian */
        if(col->item_size <= n) memcpy(p, data, col->item_size);
        else memset(p, 0, col->item_size);
    }
}

DATAFRAME_API ptrdiff_t
dataframe_col(Dataframe* df, const char *name) {
    for(size_t i=0;i<df->ncols;i++) {
        DataframeCol *c = &df->cols[i];
        if(!strcmp(c->name, name)) return (ptrdiff_t)c->i;
    }
    return -1;
}

DATAFRAME_API void
dataframe_reserve(Dataframe *df, size_t req) {
    size_t cap;
    if(df->row_capacity < req) {
        cap = df->row_capacity * 2;
        if(cap < 1024) cap = 1024;
        if(cap < req) cap = req;
        for(size_t i=0;i<df->ncols;i++) {
            DataframeCol *col = &df->cols[i];
            col->vals.v = realloc(col->vals.v, cap * col->item_size);
            memset(col->vals.u8 + df->nrows * col->item_size, 0, (cap - df->nrows) * col->item_size);
        }
        df->row_capacity = cap;
    }
}

DATAFRAME_API size_t
dataframe_addrow(Dataframe *df, size_t n) {
    size_t old = df->nrows;
    dataframe_reserve(df, df->nrows + n);
    df->nrows += n;
    assert(df->nrows <= df->row_capacity);
    return (size_t)old;
}

DATAFRAME_API void
dataframe_clearrows(Dataframe *df) {
    for(size_t i=0;i<df->ncols;i++) {
        DataframeCol *col = &df->cols[i];
        if(col->type == dataframe_str) {
            for(size_t j=0;j<df->nrows;j++)
                free(&col->vals.s[j]);
        }
        memset(col->vals.v, 0, df->nrows * col->item_size);
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

DATAFRAME_API void
dataframe_free(Dataframe *df) {
    free(df->name);
    for(size_t i=0;i<df->ncols;i++) {
        DataframeCol *col = &df->cols[i];
        free(col->name);
        if(col->type == dataframe_str) {
            for(size_t j=0;j<df->nrows;j++)
                free(col->vals.s[j]);
        }
        free(col->vals.v);
    }
    free(df->cols);
    free(df);
}

static void
dataframe_mergesort(
    size_t *vals,
    size_t *tmp,
    size_t count,
    int (*cmp)(const void *, const void*),
    int mult,
    const uint8_t *array,
    size_t size) {
    size_t i,j,k,mid;

    assert(mult == 1 || mult == -1);
#if 0
    if(count <= 64) {
        /* insertion sort */
        for(i=1;i<count;i++) {
            size_t t = vals[i];
            for(j=i;j>0&& mult * cmp(array + vals[j-1]*size, array + t * size) > 0 /* vals[j-1]>t */;--j)
                vals[j] = vals[j-1];
            vals[j] = t;
        }
        return;
    }
#endif

    if(count <= 1) return;
    mid = count / 2;
    dataframe_mergesort(vals, tmp, mid, cmp, mult, array, size);
    dataframe_mergesort(vals + mid, tmp + mid, count - mid, cmp, mult, array, size);
    /* merge sorted halves */
    for(i=0,j=0,k=mid;j<mid && k<count;i++) {
        int c = mult * cmp(array + vals[j]*size, array + vals[k]*size);
        tmp[i] = c <= 0 /* vals[j]<=vals[k] */ ?vals[j++]:vals[k++];
    }
    memcpy(tmp + i, vals + j, (mid - j) * sizeof(size_t));
    //if(j<mid) tmp[i++] = vals[j++];
    //printf("remain=%zu %zu\n", j, mid);
    /* append mid to end of sorted set */
    //for(;j<mid;i++,j++) tmp[i] = vals[j];
    /* move sorted set back into source array */
    //for(i=0;i<k;i++) vals[i] = tmp[i];
    memcpy(vals, tmp, k * sizeof(size_t));
}
static int dataframe_cmp_i8(const void *x, const void *y) {
    const int8_t a = *(int8_t*)x, b = *(int8_t*)y;
    if(a < b) return -1;
    if(a > b) return 1;
    return 0;
}
static int dataframe_cmp_i16(const void *x, const void *y) {
    const int16_t a = *(int16_t*)x, b = *(int16_t*)y;
    if(a < b) return -1;
    if(a > b) return 1;
    return 0;
}
static int dataframe_cmp_i32(const void *x, const void *y) {
    const int32_t a = *(int32_t*)x, b = *(int32_t*)y;
    if(a < b) return -1;
    if(a > b) return 1;
    return 0;
}
static int dataframe_cmp_i64(const void *x, const void *y) {
    const int64_t a = *(int64_t*)x, b = *(int64_t*)y;
    if(a < b) return -1;
    if(a > b) return 1;
    return 0;
}
static int dataframe_cmp_u8(const void *x, const void *y) {
    const uint8_t a = *(uint8_t*)x, b = *(uint8_t*)y;
    if(a < b) return -1;
    if(a > b) return 1;
    return 0;
}
static int dataframe_cmp_u16(const void *x, const void *y) {
    const uint16_t a = *(uint16_t*)x, b = *(uint16_t*)y;
    if(a < b) return -1;
    if(a > b) return 1;
    return 0;
}
static int dataframe_cmp_u32(const void *x, const void *y) {
    const uint32_t a = *(uint32_t*)x, b = *(uint32_t*)y;
    if(a < b) return -1;
    if(a > b) return 1;
    return 0;
}
static int dataframe_cmp_u64(const void *x, const void *y) {
    const uint64_t a = *(uint64_t*)x, b = *(uint64_t*)y;
    if(a < b) return -1;
    if(a > b) return 1;
    return 0;
}
static int dataframe_cmp_uuid(const void *x, const void *y) {
    return !memcmp(x, y, 16);
}
static int dataframe_cmp_str(const void *x, const void *y) {
    const char *a = *(const char**)x;
    const char *b = *(const char**)y;
    if(!a) a = "";
    if(!b) b = "";
    size_t na = strlen(a);
    size_t nb = strlen(b);
    size_t n = na < nb ? na : nb;
    for(size_t i=0;i<n;i++) {
        int c = toupper(a[i]) - toupper(b[i]);
        if(c) return c;
    }
    if(na < nb) return -1;
    if(na > nb) return 1;
    return 0;
}

DATAFRAME_API int
dataframe_itemcmp(DataframeItem *x, DataframeItem *y) {
    int c = x->type - y->type;
    if(c) return c;
    if(x->type == dataframe_str)
        return dataframe_cmp_str(&x->v->s, &y->v->s);
    return memcmp(x->v, y->v, x->item_size);
}

DATAFRAME_API void
dataframe_sort(Dataframe *df, DataframeSort *sort, size_t nsort) {
    size_t *indexes = (size_t*)malloc(df->nrows * sizeof(size_t) * 2);
    size_t *tmp = indexes + df->nrows;

    for(size_t i=0;i<df->nrows;i++) indexes[i] = i;

    /* merge sort is stable so sorting in reverse order sorts
       everything correctly */
    for(int i=(int)nsort-1;i>=0;i--) {
        int (*cmp)(const void*, const void*);
        DataframeCol *col = &df->cols[sort[i].col];
        switch(col->type) {
        case dataframe_str: cmp = dataframe_cmp_str; break;
        case dataframe_i8: cmp = dataframe_cmp_i8; break;
        case dataframe_i16: cmp = dataframe_cmp_i16; break;
        case dataframe_i32: cmp = dataframe_cmp_i32; break;
        case dataframe_time:
        case dataframe_i64: cmp = dataframe_cmp_i64; break;
        case dataframe_u8: cmp = dataframe_cmp_u8; break;
        case dataframe_u16: cmp = dataframe_cmp_u16; break;
        case dataframe_u32: cmp = dataframe_cmp_u32; break;
        case dataframe_u64: cmp = dataframe_cmp_u64; break;
        case dataframe_uuid: cmp = dataframe_cmp_uuid; break;
        default:
            assert(0);
            printf("unknown dataframe type: %d\n", col->type);
            return;
        }
        int rev;
        if(sort[i].reverse) rev = -1;
        else rev = 1;
        dataframe_mergesort(indexes, tmp, df->nrows, cmp, rev, col->vals.u8, col->item_size);
    }

    /* assign keys to new indexes */
    for(size_t i=0;i<df->ncols;i++) {
        DataframeCol *src = &df->cols[i];
        DataframeCol col = *src;
        uint8_t *p = (uint8_t*)malloc(col.item_size * df->nrows);

        for(size_t j=0;j<df->nrows;j++) {
            memcpy(
                p + j * src->item_size,
                src->vals.u8 + indexes[j] * src->item_size,
                src->item_size);
        }
        free(src->vals.v);
        src->vals.u8 = p;
    }

    free(indexes);
}

DATAFRAME_API void
dataframe_merge(Dataframe *dst, size_t dstcol, Dataframe *src, size_t srccol) {
    DataframeSort sort;
    sort.col = dstcol;
    sort.reverse = 0;
    dataframe_sort(dst, &sort, 1);
    sort.col = srccol;
    dataframe_sort(src, &sort, 1);

    size_t colstart = dst->ncols;
    for(size_t i=0;i<src->ncols;i++) {
        if(i == srccol) continue;
        DataframeCol *col = &src->cols[i];
        dataframe_addcol(dst, col->name, col->type);
    }

    size_t end = dst->nrows;
    size_t srci = 0, dsti = 0;
    while(dsti < end && srci < src->nrows) {
        DataframeItem dstitem = dataframe_item(dst, dstcol, dsti);
        DataframeItem srcitem = dataframe_item(src, srccol, srci);
        int c = dataframe_itemcmp(&dstitem, &srcitem);
        if(c < 0) { dsti++; continue; }
        if(c > 0) { srci++; continue; }

        /* copy all src rows matching dst row but stay on src row in case
           next dst row has the same key we may need to copy these src rows
           again */
        for(size_t j=srci;j < src->nrows;j++) {
            DataframeItem srcitem = dataframe_item(src, srccol, j);
            if(dataframe_itemcmp(&dstitem, &srcitem)) break;
            for(size_t i=0,col=colstart;i<src->ncols;i++,col++) {
                if(i == srccol) continue;
                dataframe_set(dst, col, dsti, dataframe_itemptr(&srcitem), dataframe_itemsize(&srcitem));
            }
        }
        ++dsti;
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
static int cmp(const void *x, const void *y) {
    int *a = (int*)x, *b = (int*)y;
    return *a - *b;
}
int main(int argc, char **argv) {
#if 0
    double t;
    size_t count = 10*1000*1000;
    int *vals = (int*)malloc(count * sizeof(size_t));
    size_t *indexes = (size_t*)malloc(count * sizeof(size_t));
    for(size_t i=0;i<count;i++) {
        vals[i] = rand();
        indexes[i] = i;
    }
    t = now();
    size_t *tmp = (size_t*)malloc(count * sizeof(size_t));
    dataframe_mergesort(indexes, tmp, count, cmp, 1, (uint8_t*)vals, sizeof(int));
    //qsort(vals, count, sizeof(int), cmp);
    free(tmp);
    printf("sorted in %f\n", now() - t);

    for(int i=1;i<count;i++) {
        size_t x = indexes[i-1];
        size_t y = indexes[i];
        assert(vals[x] <= vals[y]);
    }
#endif


    Dataframe *df = dataframe_new("values");
    dataframe_addcol(df, "id", dataframe_i32);
    dataframe_addcol(df, "name", dataframe_str);
    dataframe_reserve(df, 10);
    for(int i=0;i<5;i++) {
        size_t x = dataframe_addrow(df, 1);
        dataframe_set(df, 0, x, &i, sizeof i);
        if(i == 1) dataframe_set(df, 1, x, "what???", strlen("what???"));
        else dataframe_set(df, 1, x, "hello", strlen("hello"));
    }

    DataframeSort sort[] = {{1,1},{0,1}};
    dataframe_print(df);
    dataframe_sort(df, sort, 2);
    dataframe_print(df);
    dataframe_free(df);
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
