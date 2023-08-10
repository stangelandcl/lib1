#ifndef MSG_H
#define MSG_H

/* license (public domain) and example at bottom of file */

#if defined(MSG_STATIC) || defined(MSG_EXAMPLE)
#define MSG_API static
#define MSG_IMPLEMENTATION
#else
#define MSG_API extern
#endif

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MsgPair {
    size_t offset, n;
} MsgPair;

typedef struct Msg {
    MsgPair offset[128];
    uint8_t *p;
    size_t i, n, noffset;
} Msg;


MSG_API void msg_copyfixed(Msg *msg, void *dst, size_t n);
MSG_API void msg_dump(const char *text, const void *data, size_t n);
MSG_API double msg_getdouble(Msg *msg);
MSG_API void msg_free(Msg *msg);
MSG_API void* msg_getbytes(Msg *msg, size_t *n);
MSG_API void* msg_getfixed(Msg *msg, size_t n);
MSG_API float msg_getfloat(Msg *msg);
MSG_API int8_t msg_geti8(Msg *msg);
MSG_API int16_t msg_geti16(Msg *msg);
MSG_API int32_t msg_geti32(Msg *msg);
MSG_API int64_t msg_geti64(Msg *msg);
MSG_API void msg_getpop(Msg *msg);
MSG_API void msg_getpush(Msg *msg);
MSG_API size_t msg_getpusha(Msg *msg);
MSG_API const char* msg_getstr(Msg *msg);
MSG_API const char* msg_getstrn(Msg *msg, size_t *n);
MSG_API uint8_t msg_getu8(Msg *msg);
MSG_API uint16_t msg_getu16(Msg *msg);
MSG_API uint32_t msg_getu32(Msg *msg);
MSG_API uint64_t msg_getu64(Msg *msg);
MSG_API uint64_t msg_getvarint(Msg *msg);
MSG_API void msg_init(Msg *msg, void *data, size_t n);
MSG_API void msg_init0(Msg *msg);
MSG_API void msg_putbytes(Msg *msg, const void *data, size_t n);
MSG_API void msg_putdouble(Msg *msg, double i);
MSG_API void msg_putfixed(Msg *msg, const void *p, size_t n);
MSG_API void msg_putfloat(Msg *msg, float i);
MSG_API void msg_puti64(Msg *msg, int64_t i);
MSG_API void msg_puti32(Msg *msg, int32_t i);
MSG_API void msg_puti16(Msg *msg, int16_t i);
MSG_API void msg_puti8(Msg *msg, int8_t i);
MSG_API void msg_putpop(Msg *msg);
MSG_API void msg_putpopa(Msg *msg, uint32_t n);
MSG_API void msg_putpush(Msg *msg);
MSG_API void msg_putpusha(Msg *msg);
MSG_API void msg_putstr(Msg *msg, const char *c);
MSG_API void msg_putstrn(Msg *msg, const char *c, size_t n);
MSG_API void msg_putvarint(Msg *msg, uint64_t x);
MSG_API void msg_putu64(Msg *msg, uint64_t i);
MSG_API void msg_putu32(Msg *msg, uint32_t i);
MSG_API void msg_putu16(Msg *msg, uint16_t i);
MSG_API void msg_putu8(Msg *msg, uint8_t i);
MSG_API void msg_reserve(Msg *msg, size_t n);
MSG_API void msg_reset(Msg *msg);

#ifdef __cplusplus
}
#endif

#endif

#ifdef MSG_IMPLEMENTATION
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>


MSG_API void
msg_copyfixed(Msg *msg, void *dst, size_t n) {
    if(msg->i + n > msg->n) {
    	memset(dst, 0, n);
	msg->i = msg->n;
    } else {
    	memcpy(dst, msg->p + msg->i, n);
	msg->i += n;
    }
}

MSG_API void
msg_dump(const char *text, const void *data, size_t n) {
    const uint8_t *c = data;
    printf("%s(%zu)=", text, n);
    assert(c || !n);
    for(size_t i=0;i<n;i++,c++) {
        if(*c >= 33 && *c <= 126) printf("%c", (char)*c);
        else printf("\\%02x", *c);
    }
    printf("\n");
}

MSG_API void
msg_free(Msg *msg) {
    free(msg->p);
    msg->p = 0; /* important. code relies on being able to double free */
}

MSG_API double
msg_getdouble(Msg *msg) {
	double x;
	msg_copyfixed(msg, &x, sizeof x);
	return x;
}

MSG_API void*
msg_getfixed(Msg *msg, size_t n) {
    if(!n) return 0;

    if(msg->i + n > msg->n) {
    	msg->i = msg->n;
    	return 0;
    }
    char *p = (char*)msg->p + msg->i;
    msg->i += n;
    return p;
}

MSG_API float
msg_getfloat(Msg *msg) {
	float x;
	msg_copyfixed(msg, &x, sizeof x);
	return x;
}

MSG_API int8_t
msg_geti8(Msg *msg) {
	int8_t x;
	msg_copyfixed(msg, &x, sizeof x);
	return x;
}

MSG_API int16_t
msg_geti16(Msg *msg) {
	int16_t x;
	msg_copyfixed(msg, &x, sizeof x);
	return x;
}

MSG_API int32_t
msg_geti32(Msg *msg) {
	int32_t x;
	msg_copyfixed(msg, &x, sizeof x);
	return x;
}

MSG_API int64_t
msg_geti64(Msg *msg) {
	int64_t x;
	msg_copyfixed(msg, &x, sizeof x);
	return x;
}

MSG_API void
msg_getpop(Msg *msg) {
    MsgPair p;
    p = msg->offset[--msg->noffset];
    msg->i = p.offset;
    msg->n = p.n;
}

MSG_API void
msg_getpush(Msg *msg) {
    MsgPair p;
    p.n = msg->n; /* old size */
    /* struct/array size + current offset */
    size_t n = msg_getu32(msg);
    assert(n >= sizeof(uint32_t)); /* length of n */
    n -= sizeof(uint32_t); /* skip length of n */
    size_t end = msg->i + n; /* end of this sub message */
    assert(end <= msg->n);
    if(end > msg->n) {
        msg->i = msg->n;
        return;
    }
    msg->n = msg->i + n;
    p.offset = end; /* end of struct/array */
    msg->offset[msg->noffset++] = p;
}
MSG_API size_t
msg_getpusha(Msg *msg) {
    msg_getpush(msg);
    return msg_getu32(msg);
}

MSG_API const char*
msg_getstr(Msg *msg) {
    size_t n;
    return msg_getstrn(msg, &n);
}

MSG_API const char*
msg_getstrn(Msg *msg, size_t *n) {
    *n = msg_getu32(msg);
    if(!*n) return "";
    const char *p = (const char*)msg_getfixed(msg, *n);
    if(p[*n-1]) {
        *n = 0;
        return ""; /* msgformat error not zero terminated */
    }
    *n = *n - 1; /* don't include null terminator */
    return p; /* zero terminated string */
}

MSG_API void*
msg_getbytes(Msg *msg, size_t *n) {
    *n = msg_getu32(msg);
    if(!*n) return "";
    return msg_getfixed(msg, *n);
}

MSG_API uint8_t
msg_getu8(Msg *msg) {
	uint8_t x;
	msg_copyfixed(msg, &x, sizeof x);
	return x;
}
MSG_API uint16_t
msg_getu16(Msg *msg) {
	uint16_t x;
	msg_copyfixed(msg, &x, sizeof x);
	return x;
}

MSG_API uint32_t
msg_getu32(Msg *msg) {
	uint32_t x;
	msg_copyfixed(msg, &x, sizeof x);
	return x;
}

MSG_API uint64_t
msg_getu64(Msg *msg) {
	uint64_t x;
	msg_copyfixed(msg, &x, sizeof x);
	return x;
}

MSG_API uint64_t
msg_getvarint(Msg *msg) {
    uint64_t x = 0, n;
    int bits = 0;
    for(;;){
        n = msg_getu8(msg);
        x |= (n & 0x7F) << bits;
        if(!(n & 128)) break;
        bits += 7;
    }
    return x;
}

MSG_API void
msg_init0(Msg *msg) {
    memset(msg, 0, sizeof *msg);
}

MSG_API void msg_init(Msg *msg, void *data, size_t n) {
    msg_init0(msg);
    msg->p = data;
    msg->n = n;
}

MSG_API void
msg_putfixed(Msg *msg, const void *p, size_t n) {
    msg_reserve(msg, n);
    if(n) memcpy(msg->p + msg->i, p, n);
    msg->i += n;
}

MSG_API void
msg_puti64(Msg *msg, int64_t i) {
	msg_putfixed(msg, &i, sizeof i);
}

MSG_API void
msg_puti32(Msg *msg, int32_t i) {
	msg_putfixed(msg, &i, sizeof i);
}

MSG_API void
msg_puti16(Msg *msg, int16_t i) {
	msg_putfixed(msg, &i, sizeof i);
}

MSG_API void
msg_puti8(Msg *msg, int8_t i) {
	msg_putfixed(msg, &i, sizeof i);
}

MSG_API void
msg_putpop(Msg *msg) {
    size_t start = msg->offset[--msg->noffset].n;
    uint32_t n = msg->i - start;
    memcpy(msg->p + start, &n, sizeof n);
}

MSG_API void
msg_putpopa(Msg *msg, uint32_t n) {
    memcpy(msg->p + msg->offset[msg->noffset - 1].n + sizeof(uint32_t), &n, sizeof n);
    msg_putpop(msg);
}

MSG_API void
msg_putpush(Msg *msg) {
    msg->offset[msg->noffset++].n = msg->i;
    msg->i += sizeof(uint32_t);
}

MSG_API void
msg_putpusha(Msg *msg) {
    msg_putpush(msg);
    msg->i += sizeof(uint32_t); /* skip count */
}

MSG_API void
msg_putbytes(Msg *msg, const void *data, size_t n) {
    msg_reserve(msg, n);
    msg_putu32(msg, (uint32_t)n);
    msg_putfixed(msg, data, n);
}

MSG_API void
msg_putstr(Msg *msg, const char *c) {
    if(!c) c = "";
    return msg_putstrn(msg, c, strlen(c));
}

MSG_API void
msg_putstrn(Msg *msg, const char *c, size_t n) {
    if(!c) c = "";
    msg_reserve(msg, n + 1);
    msg_putu32(msg, (uint32_t)n + 1);
    msg_putfixed(msg, c, n);
    msg_putu8(msg, 0); /* add null terminator */
}

MSG_API void
msg_putvarint(Msg *msg, uint64_t x) {
    while(x > 127) {
        uint8_t n = (x & 0x7F) | 128;
        x >>= 7;
        msg_putu8(msg, n);
    }
    msg_putu8(msg, (uint8_t)x);
}

MSG_API void
msg_putu64(Msg *msg, uint64_t i) {
	msg_putfixed(msg, &i, sizeof i);
}
MSG_API void
msg_putu32(Msg *msg, uint32_t i) {
	msg_putfixed(msg, &i, sizeof i);
}
MSG_API void
msg_putu16(Msg *msg, uint16_t i) {
	msg_putfixed(msg, &i, sizeof i);
}
MSG_API void
msg_putu8(Msg *msg, uint8_t i) {
	msg_putfixed(msg, &i, sizeof i);
}

MSG_API void
msg_reserve(Msg *msg, size_t n) {
    size_t req = msg->i + n;
    if(msg->n < req) {
        size_t cap = msg->n * 2;
        if(cap < req) cap = req;
        if(cap < 65536) cap = 65536;
        msg->p = (uint8_t*)realloc(msg->p, cap);
        msg->n = cap;
    }
}

MSG_API void
msg_reset(Msg *msg) {
	msg->i = 0;
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

