/* SPDX-License-Identifier: Unlicense */
#ifndef STREAM_H
#define STREAM_H

/*
   C Stream implementation

   #define STREAM_IMPLEMENTATION in one C file before including json.h.
   Use json.h in other files normally.
   Alternatively define STREAM_STATIC before each use of json.h for
   static definitions
*/

#if defined(STREAM_STATIC) || defined(STREAM_EXAMPLE) || defined(STREAM_FUZZ)
#define STREAM_API static
#define STREAM_IMPLEMENTATION
#else
#define STREAM_API extern
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define STREAM_OF(type, ptr, member) \
    ((type*)((char*)(ptr) - offsetof(type, member)))

typedef struct Stream Stream;
struct Stream {
    ssize_t (*read)(Stream*, void *buf, size_t nbuf);
    ssize_t (*write)(Stream*, const void *buf, size_t nbuf);
    ssize_t (*writef)(Stream*, const char *format, ...);
    size_t (*offset)(Stream*);
    size_t (*size)(Stream*);
    /* may return null for non-memory streams */
    void* (*bytes)(Stream*);
    void (*reset)(Stream*);
    void (*destroy)(Stream*);
};

/* mode = 'w' for write */
/* set buf to null for dynamically allocated memory */
STREAM_API Stream* streammem_init(const void *buf, size_t nbuf, char mode);
STREAM_API Stream* streamfile_init(FILE *f, char mode);


#ifdef __cplusplus
}
#endif

#endif

#ifdef STREAM_IMPLEMENTATION
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct StreamMem {
    Stream stream;
    uint8_t *buf;
    size_t n, cap;
    char owns, mode;
} StreamMem;

typedef struct StreamFile {
    Stream stream;
    FILE *f;
    char owns, mode;
} StreamFile;

static ssize_t streammem_read(Stream *s, void *buf, size_t nbuf) {
    StreamMem *ms = STREAM_OF(StreamMem, s, stream);
    size_t n = ms->cap - ms->n;
    if(n > nbuf) n = nbuf;
    memcpy(buf, ms->buf + ms->n, n);
    return (ssize_t)n;
}
static ssize_t streammem_write(Stream *s, const void *buf, size_t nbuf) {
    StreamMem *ms = STREAM_OF(StreamMem, s, stream);
    size_t req = nbuf + ms->n;
    if(ms->cap < req) {
        if(ms->owns) {
            size_t cap = ms->cap * 2;
            if(cap < req) cap = req;
            ms->buf = (uint8_t*)realloc(ms->buf, cap);
            ms->cap = cap;
        } else nbuf = ms->cap - ms->n;
    }
    memcpy(ms->buf + ms->n, buf, nbuf);
    ms->n += nbuf;
    return nbuf;
}
static ssize_t streammem_writef(Stream *s, const char *format, ...) {
    char buf[16384], *p;
    va_list arg;
    va_start(arg, format);
    size_t n = vsnprintf(buf, sizeof buf, format, arg);
    va_end(arg);
    p = buf;
    if(n > sizeof buf) {
        p = (char*)malloc(n);
        va_start(arg, format);
        vsnprintf(p, n, format, arg);
        va_end(arg);
    }
    n = streammem_write(s, p, n);
    if(p != buf) free(p);
    return n;
}
static size_t streammem_offset(Stream *s) {
    StreamMem *ms = STREAM_OF(StreamMem, s, stream);
    return ms->n;
}
static size_t streammem_size(Stream *s) {
    StreamMem *ms = STREAM_OF(StreamMem, s, stream);
    return ms->cap;
}
static void* streammem_bytes(Stream *s) {
    StreamMem *ms = STREAM_OF(StreamMem, s, stream);
    return ms->buf;
}
static void streammem_reset(Stream *s) {
    StreamMem *ms = STREAM_OF(StreamMem, s, stream);
    ms->n = 0;
}
static void streammem_destroy(Stream *s) {
    StreamMem *ms = STREAM_OF(StreamMem, s, stream);
    if(ms->owns) free(ms->buf);
    free(ms);
}
static Stream streammem = {
    streammem_read,
    streammem_write,
    streammem_writef,
    streammem_offset,
    streammem_size,
    streammem_bytes,
    streammem_reset,
    streammem_destroy
};
STREAM_API Stream* streammem_init(const void *buf, size_t nbuf, char mode) {
    StreamMem *s = (StreamMem*)calloc(1, sizeof *s);
    if(buf) {
        s->buf = (uint8_t*)buf;
        s->cap = nbuf;
        s->owns = 0;
    } else {
        s->cap = nbuf ? nbuf : 1024;
        s->buf = (uint8_t*)malloc(s->cap);
        s->owns = 0;
    }
    s->n = 0;
    s->mode = mode;
    s->stream = streammem;
    return &s->stream;
}


static ssize_t streamfile_read(Stream *s, void *buf, size_t nbuf) {
    StreamFile *f = STREAM_OF(StreamFile, s, stream);
    return fread(buf, 1, nbuf, f->f);
}
static ssize_t streamfile_write(Stream *s, const void *buf, size_t nbuf) {
    StreamFile *f = STREAM_OF(StreamFile, s, stream);
    return fwrite(buf, 1, nbuf, f->f);
}
static ssize_t streamfile_writef(Stream *s, const char *format, ...) {
    StreamFile *f = STREAM_OF(StreamFile, s, stream);
    va_list arg;
    va_start(arg, format);
    size_t n = vfprintf(f->f, format, arg);
    va_end(arg);
    return n;
}
static size_t streamfile_offset(Stream *s) {
    StreamFile *f = STREAM_OF(StreamFile, s, stream);
    return ftell(f->f);
}
static void* streamfile_bytes(Stream *s) {
    return 0;
}
static size_t streamfile_size(Stream *s) {
    StreamFile *f = STREAM_OF(StreamFile, s, stream);
    size_t pos = ftell(f->f);
    fseek(f->f, 0, SEEK_END);
    size_t end = ftell(f->f);
    fseek(f->f, pos, SEEK_SET);
    return end - pos;
}
static void streamfile_reset(Stream *s) {
    StreamFile *f = STREAM_OF(StreamFile, s, stream);
    fseek(f->f, 0, SEEK_SET);
}
static void streamfile_destroy(Stream *s) {
    StreamFile *f = STREAM_OF(StreamFile, s, stream);
    fclose(f->f);
    free(f);
}
static Stream streamfile = {
    streamfile_read,
    streamfile_write,
    streamfile_writef,
    streamfile_offset,
    streamfile_size,
    streamfile_bytes,
    streamfile_reset,
    streamfile_destroy
};
STREAM_API Stream* streamfile_init(FILE* f, char mode) {
    StreamFile *s = (StreamFile*)calloc(1, sizeof *s);
    s->stream = streamfile;
    s->f = f;
    s->mode = mode;
    return &s->stream;
}



#endif

#if STREAM_EXAMPLE
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main() {
    Stream* s = streammem_init(0, 0, 'w');
    s->writef(s, "%s %d!", "test", 10);
    printf("len=%zu\n", s->size(s));
    printf("bytes=%.*s\n", (int)s->size(s), (char*)s->bytes(s));
    s->destroy(s);
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
