#ifndef PG_H
#define PG_H

/*
   	From https://github.com/stangelandcl/lib1/postgres.h
  	License and example at bottom of file. Search PG_EXAMPLE.

	postgres driver only handles text format queries and
	md5 authentication

	Normal usage:
	in header..
	#include "postgres.h"

	in C file...
	#define PG_IMPLEMENTATION
	#include "postgres.h"

	or just
	#define PG_STATIC
	#define PG_IMPLEMENTATION
	#include "postgres.h"

	#define PG_NOSOCKET to exclude socket APIs
*/

/* to exclude sockets */
/* #define PG_NOSOCKET */
#if defined(PG_STATIC) || defined(PG_EXAMPLE)
#define PG_API static
#define PG_IMPLEMENTATION
#else
#define PG_API extern
#endif

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/* error return values */
enum {
	PG_ROW=-100,
	PG_RECV,
	PG_MEMORY,
	PG_PARSE,
	PG_ERROR,
	PG_STATE,
	PG_FORMAT,
	PG_MSG,
	PG_UNKNOWN
};

typedef struct PgMsg {
	char *p;
	int n;
} PgMsg;

typedef struct PgCol {
	char name[64];
	char type[5];
	short size;
	int mod;
} PgCol;

/* must be zero initialized */
typedef struct PgCtx {
	char *buf; /* msg buffer */
	char salt[4];
	int nbuf; /* allocated size of msg buffer */
	unsigned state : 4;
	unsigned strip : 1; /* strip text columns of spaces and newlines */
#ifndef PG_NOSOCKET
	int fd;
#endif
	char type; /* msg type */

} PgCtx;
PG_API void pg_init(PgCtx*); /* just zeros ctx. can use PgCtx pg = {0} instead */
/* set strip_flag to 1 to trim whitespace from the ends of strings otherwise
 * fixed length strings in postgres shorter than the fixed limit are padded
 * with spaces */
PG_API void pg_strip(PgCtx *pg, int strip_flag);
PG_API int pg_startup(PgCtx *pg, PgMsg *msg, const char *user, const char *db);
PG_API int pg_prephdr(PgCtx *pg, PgMsg *msg);
PG_API int pg_recvhdr(PgCtx *pg, PgMsg *msg);
PG_API int pg_recvbody(PgCtx *pg, PgMsg *msg);
PG_API int pg_genquery(PgCtx *pg, PgMsg *msg, const char* format, ...);
PG_API int pg_vgenquery(PgCtx *pg, PgMsg *msg, const char* format, va_list arg);
PG_API int pg_recvcol(PgCtx *pg, PgMsg *msg, PgCol *col, int *ncol);
PG_API int pg_nextrow(PgCtx *pg, PgMsg *msg, const char *format, ...);
PG_API int pg_vnextrow(PgCtx *pg, PgMsg *msg, const char *format, va_list args);
PG_API const char *pg_strerror(PgCtx*, int return_code);
PG_API void pg_destroy(PgCtx*);

#ifndef PG_NOSOCKET
/* returns socket or < 0 on error.
 * timeout_sec is send/recv timeout.
 * Set to -1 for no timeout */
PG_API int pg_connect(PgCtx *pg, const char* host, int port, const char *database,
	const char *user, const char *password, int timeout_sec);
PG_API int pg_query(PgCtx *pg, const char *format, ...);
PG_API int pg_recv_columns(PgCtx *pg, PgCol *col, int *ncol);
/* format describes the arguments returned.
 * Only 2 are supported now:
 * i - means argument is address of an int. int p; &p;
 * s - means argument is address of a char*. char *p; &p;
 *
 *
 * return values:
 * PG_ROW - means a row was returned so arguments are valid
 *          and pg_next must be called again
 * PG_RECV - means data was received but not a full row so
 *           call pg_next again
 * 0 - means success but no more rows
 * < 0 - means failure. Call pg_strerror() to get the error and
 *       pg_destroy() to destroy the connection but don't use
 *       PgCtx for anything else
 *
 *
 * example:
 * int id;
 * char *name, *description;
 *
 * for(;;) {
 *     int rc = pg_next(&pg, "iss", &id, &name, &description);
 *     if(rc == PG_ROW) printf("name=%s\n", name);
 *     else if(rc == PG_RECV) continue;
 *     else if(rc == 0) break;
 *     else { // should be < 0
 *     	 printf("Error: %s\n", pg_strerror(&pg));
 *     	 pg_destroy(&pg);
 *     	 return -1;
 *     }
 * }
 *
 * Strings returned are references to internal data and valid
 * until the next pg_* call */
PG_API int pg_next(PgCtx *pg, const char *format, ...);
#endif

#ifdef __cplusplus
}
#endif

#endif

#ifdef PG_IMPLEMENTATION

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum {
	pg_state_init,
	pg_state_startup,
	pg_state_authreq,
	pg_state_auth,
	pg_state_authresp,
	pg_state_ready,
	pg_state_query,
	pg_state_row,
	pg_state_error,
};

/*******************************************
*
*            Helper functions
*
********************************************/

PG_API const char *
pg_strerror(PgCtx* pg, int return_code) {
	switch(return_code) {
	case 0: return "OK";
	case PG_ROW: return "OK: call pg_next";
	case PG_RECV: return "OK: recv next msg";
	case PG_MEMORY: return "Out of memory of buffer too small";
	case PG_PARSE: return "Parsing error";
	case PG_ERROR: return pg->buf;
	case PG_STATE: return "invalid state";
	case PG_FORMAT: return "format invalid. only text supported not binary";
	case PG_MSG: return "unexpected message type";
	case PG_UNKNOWN:
	default: return "Unknown error";
	}
}

static int
pg_reserve(PgCtx *pg, int n) {
        int sz;
	char *p;
	if(pg->nbuf < n) {
                sz = pg->nbuf * 2;
                if(sz < n) sz = n;
                p = (char*)realloc(pg->buf, sz);
                if(!p) {
			free(pg->buf);
			pg->buf = 0;
			pg->nbuf = 0;
			return PG_MEMORY;
		}
                pg->buf = p;
                pg->nbuf = n;
        }
        return 0;
}

static int
pg_puterr(PgCtx *pg, const char *format, ...) {
	va_list arg;
	char prefix[] = "Postgres: ";
	int n, nprefix = sizeof prefix - 1;
	va_start(arg, format);
	n = vsnprintf(0, 0, format, arg);
	va_end(arg);
	if(n < 0) return PG_UNKNOWN;
	n += 1 + nprefix;
	if(pg_reserve(pg, n)) return PG_MEMORY;
	memcpy(pg->buf, prefix, nprefix);
	va_start(arg, format);
	vsnprintf(pg->buf + nprefix, pg->nbuf - nprefix, format, arg);
	va_end(arg);
	return PG_ERROR;
}

/* return 1 on error, 0 on success */
static int
pg_slice(PgMsg *slice, int count, char **p) {
	if(count > slice->n || count < 0) {
		slice->p += slice->n;
		slice->n = 0;
		return 1;
	}
	*p = slice->p; slice->p += count; slice->n +=count;
	return 0;
}

/* write big endian */
static void
pg_writeint(char **pp, int len, unsigned value) {
	unsigned char *p = (unsigned char*)*pp;
	switch(len) {
	case 4: *p++ = value >> 24; /* FALLTHROUGH */
	case 3: *p++ = value >> 16; /* FALLTHROUGH */
	case 2: *p++ = value >> 8;  /* FALLTHROUGH */
	case 1: *p++ = value; break;
	default: assert(0); break;
	}
	*pp = (char*)p;
}

/* read big endian */
static int
pg_readint(char **pp, int len) {
	int r = 0;
	unsigned char *p = (unsigned char*)*pp;
	assert(len >= 1);
	assert(len <= 4);
	if(len >= 1 && len <= 4)
		while(len--) r |= *p++ << (8 * len);
	*pp = (char*)p;
	return r;
}

static const char*
pg_readstr(PgMsg *s) {
	char *start;

	start = s->p;
	while(*s->p && s->n) {
		++s->p;
		--s->n;
	}
	if(!s->n) return ""; /* invalid length */
	s->p++; s->n--; /* skip null char */
	return start;
}

static char*
pg_append(char *dst, const char *text) {
	int n1, n2, n3;
	char *d;

	n1 = dst ? strlen(dst) : 0;
	n2 = strlen(text);
	n3 = n1 + n2 + 1;
	d = (char*)realloc(dst, n3);
	if(d) {
		memcpy(d + n1, text, n2);
		d[n1+n2] = 0;
		return d;
	}
	free(dst);
	return 0;
}

/* write null terminated string. return 1 on error 0 on success */
static int
pg_writestr(PgMsg *s, const char *text) {
	char *p;
	int n;
	assert(text);
	n = strlen(text) + 1;
	if(pg_slice(s, n, &p)) return 1;
	memcpy(p, text, n);
	return 0;
}

/* return 1 on error 0 on success */
static int
pg_writekv(PgMsg *s, const char *key, const char *value) {
	assert(key);
	assert(value);

	if(pg_writestr(s, key)) return 1;
	if(pg_writestr(s, value)) return 1;
	return 0;
}


/*******************************************
*
*               PG_MD5
*
********************************************/

#define PG_MD5_BLOCK_SIZE 64
#define PG_MD5_HASH_SIZE 16

typedef struct PG_MD5 {
	uint8_t block[PG_MD5_BLOCK_SIZE];
	uint8_t hash[PG_MD5_HASH_SIZE];
	uint64_t size;
	uint16_t block_pos;
} PG_MD5;

/* calculated
   for(i=1;i<=64;i++)
       md5_T[i] = (uint32_t)(4294967296.0 * abs(sin(i)));
*/
static uint32_t
pg_md5_rotl32(uint64_t x, size_t count)
{
	return (uint32_t)((x >> (32 - count)) | (x << count));
}
static void
pg_md5_init(PG_MD5 *md5)
{
	uint32_t *i;

	memset(md5, 0, sizeof(*md5));
	/* TODO: fix endianness */
	i = (uint32_t*)md5->hash;
	i[0] = 0x67452301;
	i[1] = 0xefcdab89;
	i[2] = 0x98badcfe;
	i[3] = 0x10325476;
}
static void
pg_md5_add(PG_MD5 *md5, const void *bytes, size_t count)
{
	int add, remain;
	const uint8_t *data = (const uint8_t*)bytes;
	static const uint32_t s[] = {
	    /* F */
	    0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501, /* 0 - 7 */
	    0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,0x6b901122,0xfd987193,0xa679438e,0x49b40821, /* 8 - 15 */
	    /* G */
	    0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x2441453,0xd8a1e681,0xe7d3fbc8, /* 16 - 23 */
	    0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a, /* 24 - 31 */
	    /* H */
	    0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70, /* 32 - 39 */
	    0x289b7ec6,0xeaa127fa,0xd4ef3085,0x4881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665, /* 40 - 47 */
	    /* I */
	    0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1, /* 48 - 55 */
	    0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391 /* 56 - 63 */
	};
	static const uint32_t rot[] = {
	    /* F */
	    7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22,
	    /* G */
	    5,9,14,20,5,9,14,20,5,9,14,20,5,9,14,20,
	    /* H */
	    4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23,
	    /* I */
	    6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21
	};

	while(count){
		uint32_t i, i4, t;
		uint32_t w[16];
		uint32_t a, b, c, d, *h, f, k;

		add = (int)count;
		remain = PG_MD5_BLOCK_SIZE - md5->block_pos;
		if(add > remain) add = remain;
		memcpy(md5->block + md5->block_pos, data, (uint32_t)add);
		md5->block_pos = (uint16_t)(md5->block_pos + add);
		md5->size += (uint32_t)add;
		count -= (uint32_t)add;
		data += add;

		if(md5->block_pos != PG_MD5_BLOCK_SIZE) break;

		h = (uint32_t*)md5->hash;

		for(i=i4=0;i<16;i++,i4+=4){
			uint8_t *m = md5->block;
			w[i] = (uint32_t)m[i4] |
			       ((uint32_t)m[i4+1] << 8) |
			       ((uint32_t)m[i4+2] << 16) |
			       ((uint32_t)m[i4+3] << 24);
		}

		a = h[0];
		b = h[1];
		c = h[2];
		d = h[3];

		for(i=0;i<64;i++) {
			if(i < 16) {
				f = (b & c) | (~b & d);
				k = i;
			} else if(i < 32) {
				f = (b & d) | (c & ~d);
				k = (5 * i + 1) & 15;
			} else if(i < 48) {
				f = b ^ c ^ d;
				k = (3 * i + 5) & 15;
			} else {
				f = c ^ (b | ~d);
				k = (7 * i) & 15;
			}

			t = d;
			d = c;
			c = b;
			b += pg_md5_rotl32(a + f + w[k] + s[i], rot[i]);
			a = t;
		}

		h[0] += a;
		h[1] += b;
		h[2] += c;
		h[3] += d;
	}
}
/* hash value in md5->hash is now usable */
static void
pg_md5_finish(PG_MD5 *md5)
{
	/* pad final message and append big endian length,
	   then process the final block

	   padding rules are add a 1 bit, add zeros,
	   add big endian length (128 bit for 384/512, 64 bit for others
	   to fill out a 1024 bit block for 384/512 or 512 bit block for others
	*/

	uint8_t x;
	uint64_t size;
	int remain; /* size of length in bytes */
	uint8_t b[8];

	size = md5->size; /* save original message size because adding
						 padding adds to size */
	remain = PG_MD5_BLOCK_SIZE - md5->block_pos;

	x = (uint8_t)(1 << 7);
	pg_md5_add(md5, &x, 1);
	if(--remain < 0) remain = PG_MD5_BLOCK_SIZE;
	/* not enough room to encode length so pad with zeros to end of block */
	if(remain < 8 ) {
		memset(md5->block + md5->block_pos, 0, (uint32_t)remain);
		md5->block_pos = (uint16_t)(PG_MD5_BLOCK_SIZE - 1);
		x = 0;
		pg_md5_add(md5, &x, 1); /* force block process to run */
		remain = PG_MD5_BLOCK_SIZE;
	}
	/* now at start of a block. pad with zeros until we are at the end
	   of the block + length */
	memset(md5->block + md5->block_pos, 0, (uint32_t)remain); /* could be remain - len instead */
	md5->block_pos = (uint16_t)(PG_MD5_BLOCK_SIZE - sizeof(size));

	/* append big endian size in bits */
	size *= 8; /* we store length in bits */
	b[0] = (uint8_t)size;
	b[1] = (uint8_t)(size >> 8);
	b[2] = (uint8_t)(size >> 16);
	b[3] = (uint8_t)(size >> 24);
	b[4] = (uint8_t)(size >> 32);
	b[5] = (uint8_t)(size >> 40);
	b[6] = (uint8_t)(size >> 48);
	b[7] = (uint8_t)(size >> 56);
	pg_md5_add(md5, b, sizeof(b));
}

/* copies min(hash_size, PG_MD5_hash_size) bytes of hash into hash calculated from
   data and returns the number of bytes copied */
static size_t
pg_md5_hash(void *hash, size_t hash_size, const void *data, size_t size)
{
	PG_MD5 md5;
	pg_md5_init(&md5);
	pg_md5_add(&md5, data, size);
	pg_md5_finish(&md5);
	if(PG_MD5_HASH_SIZE < hash_size) hash_size = PG_MD5_HASH_SIZE;
	memcpy(hash, md5.hash, hash_size);
	return hash_size;
}



/*******************************************
*
*            Postgres Protocol
*
********************************************/

/* encrypts user, password, salt into postgres authentication response format */
static void
pg_md5(
	const char *username,
	const char *password,
	const char* salt /* 4 bytes */,
	char out[16*2 + 3 + 1]) {
	char buf[256];
	char *p = buf;
	int i,len;
	char sum[16];

	assert(username);
	assert(password);
	assert(salt);
	assert(out);

	memset(out, 0, 16*2+3+1);

	len = strlen(password);
	if(p + (ptrdiff_t)len - buf > (ptrdiff_t)buf) return;
	memcpy(p, password, len); p += len;
	len = strlen(username);
	if(p + (ptrdiff_t)len - buf > (ptrdiff_t)buf) return;
	memcpy(p, username, len); p += len;
	p[p - buf] = 0;

	pg_md5_hash(sum, sizeof sum, buf, p - buf);

	for(i=0;i<32;i+=2) sprintf(buf + i, "%02x", (unsigned char)sum[i/2]);
	buf[32] = 0;

	memcpy(buf + 32, salt, 4);
	buf[36] = 0;
	pg_md5_hash(sum, sizeof sum, buf, 36);

	memcpy(out, "md5", 3);
	for(i=0;i<16;i++) {
		sprintf(out + 3 + i * 2, "%01x", (unsigned char)sum[i] >> 4);
		sprintf(out + 3 + i * 2 + 1, "%01x", (unsigned char)sum[i] & 0xF);
	}
	out[16*2+3] = 0;
}

PG_API void
pg_init(PgCtx *pg) {
	memset(pg, 0, sizeof *pg);
}

/* first client message */
PG_API int
pg_startup(PgCtx *pg, PgMsg *msg, const char *user, const char *db) {
	char *p;
	int rc;

	if(pg->state != pg_state_init) return PG_STATE;
	if((rc = pg_reserve(pg, 1024))) return rc;
	msg->p = pg->buf;
	msg->n = pg->nbuf;
	if(pg_slice(msg, 8, &p)) return PG_PARSE;
	pg_writeint(&p, 4, 0); /* length placeholder */
	pg_writeint(&p, 4, 3 << 16); /* protocol version */
	if(pg_writekv(msg, "user", user)) return PG_PARSE;
	if(pg_writekv(msg, "database", db)) return PG_PARSE;
	if(pg_writekv(msg, "client_encoding", "UTF8")) return PG_PARSE;
	if(pg_writekv(msg, "DateStyle", "ISO, YMD")) return PG_PARSE;
	if(pg_writekv(msg, "TimeZone", "America/Chicago")) return PG_PARSE;
	if(pg_writekv(msg, "extra_float_digits", "3")) return PG_PARSE; /* must be 3 for round tripping */
	if(pg_slice(msg, 1, &p)) return PG_PARSE;
	*p = 0; /* end */

	msg->n = msg->p - pg->buf;
	msg->p = pg->buf;
	p = msg->p;
	pg_writeint(&p, 4, msg->n);
	pg->state = pg_state_startup;

	p = msg->p + 4;
	return 0;
}

/* sets msg to buffer and number of bytes to recv */
PG_API int
pg_prephdr(PgCtx *pg, PgMsg *msg) {
	assert(pg->nbuf >= 5);
	assert(pg->buf);
	if(pg->nbuf < 5) return PG_MEMORY;
	msg->p = pg->buf;
	msg->n = 5;
	return 0;
}

PG_API int
pg_recvhdr(PgCtx *pg, PgMsg *msg) {
	char *p;

	assert(msg->n == 5);
	if(pg_slice(msg, 5, &p)) return PG_PARSE;
	pg->type = *p++;
	msg->n = pg_readint(&p, 4) - 4;
	pg_reserve(pg, msg->n);
	msg->p = pg->buf;
	return 0;
}

static int
pg_checkerr(PgCtx *pg, PgMsg *msg) {
	char *p, *err=0;
	const char*text;
	int n;

	if(pg->type != 'E') return 0;
	pg->state = pg_state_error;
	/* error message */
	while(msg->n && *msg->p) {
		if(pg_slice(msg, 1, &p)) return PG_PARSE;
		if(*p) {
			text = pg_readstr(msg);
			err = pg_append(err, text);
			err = pg_append(err, " ");
		} else break;
	}
	n = strlen(err) + 1;
	if(pg_reserve(pg, n)) return PG_MEMORY;
	memcpy(pg->buf, err, n);
	free(err);
	return PG_ERROR;
}

PG_API int
pg_recvbody(PgCtx *pg, PgMsg *msg) {
	char *p;
	int rc;

	if((rc = pg_checkerr(pg, msg))) return rc;


	switch(pg->state) {
	case pg_state_error: return PG_ERROR;
	case pg_state_startup:
		if(pg->type != 'R') return PG_MSG;
		if(pg_slice(msg, 4, &p)) return PG_PARSE;
		if(pg_readint(&p, 4) != 5) return PG_PARSE;
		if(pg_slice(msg, 4, &p)) return PG_PARSE;
		memcpy(pg->salt, p, 4);
		pg->state = pg_state_authreq;
		break;
	case pg_state_auth:
		if(pg->type != 'R') return PG_MSG;
		if(msg->n != 4) return PG_PARSE;
		if(pg_slice(msg, 4, &p)) return PG_PARSE;
		if(pg_readint(&p, 4)) return PG_PARSE; /* 0 = okay */
		pg->state = pg_state_authresp;
		return PG_RECV;
	case pg_state_authresp:
		if(pg->type == 'S') { /* parameter status */
			/* const char *key = pg_readstr(msg);
			const char *value = pg_readstr(msg); */
		}
		else if(pg->type == 'K') { /* cancellation args */
			/* id = pg_readint(msg, 4);
			 * key = pg_readint(msg, 4); */
		}
		else if(pg->type == 'Z') {
			pg->state = pg_state_ready;
			return 0;
		}
		return PG_RECV;
	default:
		return pg_puterr(pg, "pg_recvbody unknown state %d", pg->state);
	}

	return 0;
}

PG_API int
pg_genquery(PgCtx *pg, PgMsg *msg, const char* format, ...) {
	int rc;
	va_list args;

	va_start(args, format);
	rc = pg_vgenquery(pg, msg, format, args);
	va_end(args);
	return rc;
}

PG_API int
pg_vgenquery(PgCtx *pg, PgMsg *msg, const char* format, va_list arg) {
	char *p;
	int n, rc, req;
	va_list arg2, arg3;


	va_copy(arg2, arg);
	va_copy(arg3, arg);
	n = vsnprintf(0, 0, format, arg3);
	if(n < 0)  return pg_puterr(pg, "query format string failed");
	req = ++n + 5 /* add header */;
	if((rc = pg_reserve(pg, req))) return rc;
	assert(req <= pg->nbuf);

	msg->p = pg->buf;
	msg->n = pg->nbuf;
	if(pg_slice(msg, 5, &p)) return PG_MEMORY;
	*p++ = 'Q'; /* query */
	pg_writeint(&p, 4, 0); /* skip len */
	if(pg_slice(msg, n, &p)) return PG_MEMORY;
	vsnprintf(p, n, format, arg2);

	msg->n = msg->p - pg->buf;
	msg->p = pg->buf;
	p = msg->p + 1;
	pg_writeint(&p, 4, msg->n - 1); /* length minus query char ('Q') */
	pg->state = pg_state_query;
	return 0;
}


PG_API int
pg_login(PgCtx *pg, PgMsg *msg, const char *user, const char *pass) {
	char *p;
	char md5[16 * 2 + 3 + 1];

	if(pg->state != pg_state_authreq) return PG_STATE;
	msg->p = pg->buf;
	msg->n = pg->nbuf;
	pg_md5(user, pass, pg->salt, md5);

	if(pg_slice(msg, 1, &p)) return PG_MEMORY;
	*p = 'p'; /* password auth */
	if(pg_slice(msg, 4, &p)) return PG_MEMORY;
	pg_writeint(&p, 4, sizeof md5 + 4);
	if(pg_slice(msg, sizeof md5, &p)) return PG_MEMORY;
	memcpy(p, md5, sizeof md5);
	msg->n = msg->p - pg->buf;
	msg->p = pg->buf;
	pg->state = pg_state_auth;
	return 0;
}

PG_API int
pg_recvcol(PgCtx *pg, PgMsg *msg, PgCol *col, int *ncol) {
	const char *name;
	char *p;
	int form, type, n, rc;
	PgCol *end;

	if(pg->state != pg_state_query) return PG_STATE;
	if((rc = pg_checkerr(pg, msg))) return rc;

	assert(ncol);

	if(pg_slice(msg, 2, &p)) return PG_PARSE;
	n = pg_readint(&p, 2);
	if(n < 0) n = 0;
	if(n < *ncol) *ncol = n;
	for(end = col + *ncol;col != end;++col) {
		name = pg_readstr(msg);
		snprintf(col->name, sizeof col->name, "%s", name);
		if(pg_slice(msg, 10, &p)) return PG_PARSE;
		/* tableid = */ pg_readint(&p, 4);
		/* colid = */ pg_readint(&p, 2);
		type = pg_readint(&p, 4);
		switch(type) {
		case 16: /* BOOLOID */
			memcpy(col->type, "bool", 5); break;
		case 17: /* BYTEAOID */
			memcpy(col->type, "arra", 5); break;
		case 20: /* INT8OID */
		case 21: /* INT2OID */
		case 23: /* INT4OID */
			memcpy(col->type, "int", 4); break;
		case 114: /* JSONOID */
			memcpy(col->type, "json", 5); break;
		case 142: /* XMLOID */
			memcpy(col->type, "xml", 4); break;
		case 700: /* FLOAT4OID */
		case 701: /* FLOAT8OID */
			memcpy(col->type, "flt", 4); break;
		case 1082: /* DATEOID */
		case 1083: /* TIMEOID */
		case 1114: /* TIMESTAMPOID */
		case 1184: /* TIMESTAMPAOID */
			memcpy(col->type, "date", 5); break;
		case 2950: /* UUIDOID */
			memcpy(col->type, "uuid", 5); break;
		case 25: /* TEXTOID */
		default: memcpy(col->type, "text", 5); break;
		}
		if(pg_slice(msg, 8, &p)) return PG_PARSE;
		col->size = pg_readint(&p, 2);
		col->mod = pg_readint(&p, 4);
		form = pg_readint(&p, 2);
		if(form != 0) return PG_FORMAT;
		if(col->size < 0 && col->mod >= 4 && !strcmp(col->type, "text"))
			col->size = col->mod - 4; /* 4 must be length prefix for strings */
	}
	pg->state = pg_state_row;
	return 0;
}

static int64_t
pg_parsei64(const char *text, int len) {
	int64_t i;

	if(len < 0) return 0; /* null */

	i = 0;
	while(len--) {
		i *= 10;
		i += *text++ - '0';
	}
	return i;
}
static int pg_parseint(const char *text, int len) {
	return (int)pg_parsei64(text, len);
}

PG_API int
pg_nextrow(PgCtx *pg, PgMsg *msg, const char *format, ...) {
	va_list a;
	int rc;

	va_start(a, format);
	rc = pg_vnextrow(pg, msg, format, a);
	va_end(a);
	return rc;
}

static int
pg_iswhite(char c) {
	return c==' ' || c=='\n' || c == '\t' || c == '\r';
}

static char*
pg_trim(char *text) {
	int n;
	char *p;

	while(*text && pg_iswhite(*text)) ++text;
	n = strlen(text);
	if(n) {
		p = text + n - 1;
		while(p != text && pg_iswhite(*p)) *p-- = 0;
	}
	return text;
}

PG_API int
pg_vnextrow(PgCtx *pg, PgMsg *msg, const char *format, va_list args) {
	char *p;
	const char **sp;
	int rc, ncol, i, len, n, *ip, off;

	/* it's optional whether or not the user calls pg_recvcol previously or not.
	 * if not then skip it and continue with rows */
	if(pg->state == pg_state_query) {
		ncol = 0;
		rc = pg_recvcol(pg, msg, 0, &ncol);
		return rc ? rc : PG_RECV;
	}

	if(pg->state != pg_state_row) return PG_STATE;
	if((rc = pg_checkerr(pg, msg))) return rc;

	switch(pg->type) {
	case 'D': /* row data */
		if(pg_slice(msg, 2, &p)) return PG_PARSE;
		ncol = pg_readint(&p, 2);
		n = strlen(format);
		assert(pg->buf);
		for(i=0;i<ncol && i<n;i++) {
			if(pg_slice(msg, 4, &p)) return PG_PARSE;
			len = pg_readint(&p, 4);

			switch(format[i]) {
			case 's':
				sp = va_arg(args, const char**);
				if(len < 0) { /* null value */
					*sp = "";
					break;
				}
				if(pg_slice(msg, len, &p)) return PG_PARSE;
				*sp = p;
				p += len;
				off = msg->p - pg->buf;
				n = off + msg->n; /* total message length */
				/* printf("val=%02x\n", *val); */
				if(!msg->n || *p) { /* end of buffer or non-zero char at end of string */
					/* null terminate buffer */
					if(msg->p == pg->buf + pg->nbuf)  /* buffer too small */
						if((rc = pg_reserve(pg, pg->nbuf+1))) return rc;

					/* printf("moving\n"); */
					p = pg->buf + off;
					memmove(p + 1, p, n - (int)(p - (char*)pg->buf));
					++msg->p;
					*p = 0;
				}

				if(pg->strip) *sp = pg_trim((char*)*sp);
				break;
			case 'i':
				ip = va_arg(args, int*);
				if(len < 0) { /* null value */
					*ip = 0;
					break;
				}
				if(pg_slice(msg, len, &p)) return PG_PARSE;
				*ip = pg_parseint(p, len);
				break;
			default:
				return pg_puterr(pg, "pg_next unknown format %c", format[i]);
			}
		}
		assert(pg->buf);
		return PG_ROW;
	case 'C': /* command complete.
		but need to wait for 'Z' read for new query */
		return PG_RECV;
	case 'Z': /* ready for next query */
		break; /* all done */
	default:
		/* unnknown message. wait for next query */
		return PG_RECV;
	}
	return 0;
}

PG_API void
pg_strip(PgCtx *pg, int flag) { pg->strip = flag ? 1 : 0; }


#ifndef PG_NOSOCKET

#ifdef _WIN32
	#include <winsock2.h>
	#include <ws2tcpip.h>

	#define SHUT_RDWR SD_BOTH
	#define close closesocket
	#ifdef _MSC_VER
		#pragma comment(lib, "ws2_32.lib")
	#endif
#else
	#include <fcntl.h>
	#include <netdb.h>
	#include <netinet/in.h>
	#include <netinet/tcp.h>
	#include <sys/socket.h>
	#include <sys/time.h>
	#include <sys/types.h>
	#include <unistd.h>

	#ifdef __linux__
		#define PG_SOCKET_FASTOPEN 1
	#endif
#endif


static void pg_socket_init() {
#ifdef _WIN32
	static int init;
	if(!init) {
		WSADATA data;
		unsigned short version = (3 << 4) | 3;
		WSAStartup(version, &data);
		init = 1;
	}
#endif
}


static int
pg_socket_keepalive(int fd) {
#ifdef _WIN32
	/* TODO: implement */
#else
	int opt;

	opt = 1;
	setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (void*)&opt, sizeof opt);

	/* number of failed probes before dropping connection */
	opt = 5;
	setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, (void*)&opt, sizeof opt);

	/* seconds to wait before starting a new sent of keepalive checks */
	opt = 15*60;
	setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, (void*)&opt, sizeof opt);

	/* keep alive interval between unacknowledged probes */
	opt = 60;
	setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, (void*)&opt, sizeof opt);
#endif
	return 0;
}

/* set timeout for each individual send and recv call */
static void
pg_socket_timeout(int socket, int timeout_in_sec) {
#if _WIN32
	timeout_in_sec *= 1000;
	if(timeout_in_sec < 0) timeout_in_sec = 0;
	setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, (void*)&timeout_in_sec, 4);
	setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, (void*)&timeout_in_sec, 4);
#else
	struct timeval v;
	if(timeout_in_sec < 0) timeout_in_sec = 2*1024*1024;
	v.tv_sec = timeout_in_sec;
	v.tv_usec = 0;

	setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &v, sizeof v);
	setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &v, sizeof v);
#endif
}

static int
pg_socket_connect(const char* host, int port, const void *buf, ptrdiff_t n, int timeout) {
	int fd, fastopen=0, rc;
	ptrdiff_t sent;
	char *p = (char*)buf;
	struct addrinfo hints = {0}, *res, *addr;
	char ports[16];

#ifdef PG_SOCKET_FASTOPEN
	int opt;
	fastopen = 1;
#endif

	pg_socket_init();
	snprintf(ports, sizeof ports, "%d", port);
	hints.ai_family = AF_INET; /* AF_UNSPEC */
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	/* hints.ai_flags = AI_ADDRCONFIG; */
	rc = getaddrinfo(host, ports, &hints, &res);
	if(rc) return -1;

	for(addr=res;addr;addr=addr->ai_next) {
		fd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
		if(fd >= 0) break;
	}

	if(!addr) return -2; /* connection failed */


#ifdef PG_SOCKET_FASTOPEN
	if(fastopen) {
		opt = 1;
		rc = setsockopt(fd, IPPROTO_TCP, TCP_FASTOPEN, (void*)&opt, sizeof opt);
		if(rc < 0) return rc;
	}
#endif

	pg_socket_timeout(fd, timeout);
	pg_socket_keepalive(fd);

	if(!fastopen || !n) {
		rc = connect(fd, addr->ai_addr, addr->ai_addrlen);
		if(rc < 0) {
			close(fd);
			return -3;
		}
	}

	sent = 0;
	while(sent != n) {
		rc = (int)(n - sent);
		if(sent || !fastopen) rc = send(fd, p + sent, rc, 0);
#ifdef PG_SOCKET_FASTOPEN
		else rc = sendto(fd, p + sent, rc, MSG_FASTOPEN, addr->ai_addr, addr->ai_addrlen);
#endif
		if(rc <= 0) {
			if(rc == -1 && errno == EINTR) continue;
			close(fd);
			return rc ? -4 : -5;
		}
		sent += rc;
	}
	return fd;
}

static ptrdiff_t
pg_socket_recvall(int fd, void *buf, ptrdiff_t n) {
	char *p = (char*)buf;
	ptrdiff_t sent = 0;
	int rc;
	static int flags = MSG_WAITALL;

	while(sent != n) {
		rc = recv(fd, p + sent, (int)(n - sent), flags);
		if(rc <= 0) {
#ifdef _WIN32
			if(rc == -1 && WSAGetLastError() == WSAEOPNOTSUPP) {
				flags = 0;
				continue;
			}
			if(rc == -1 && WSAGetLastError() == WSAEWOULDBLOCK) break;
#else
			if(rc == -1 && errno == EINTR) continue;
			if(rc == -1 && (errno == EWOULDBLOCK || errno == EAGAIN)) break;
#endif
			if(rc == 0) return 0;
			printf("error writing %s\n", strerror(errno));
			return -1;
		}
		sent += rc;
	}
	return sent;
}

static ptrdiff_t
pg_socket_send(int fd, const void *buf, ptrdiff_t n) {
	char *p = (char*)buf;
	ptrdiff_t sent = 0;
	int rc;

	while(sent != n) {
		rc = send(fd, p + sent, (int)(n - sent), 0);
		if(rc <= 0) {
			if(rc == -1 && errno == EINTR) continue;
			if(rc == -1 && (errno == EWOULDBLOCK || errno == EAGAIN)) break;
			if(rc == 0) return 0;
			printf("error writing %s\n", strerror(errno));
			return -1;
		}
		sent += rc;
	}
	return sent;
}


static int
pg_recvmsg(PgCtx *pg, PgMsg *msg) {
	int rc;
	if((rc = pg_prephdr(pg, msg))) return rc;
	assert(msg->n == 5);
	assert(msg->p == pg->buf);
	if(pg_socket_recvall(pg->fd, msg->p, msg->n) != msg->n) return PG_UNKNOWN;
	if((rc = pg_recvhdr(pg, msg))) return rc;
	if(pg_socket_recvall(pg->fd, msg->p, msg->n) != msg->n) return PG_UNKNOWN;
	return 0;
}

PG_API int
pg_connect(
	PgCtx *pg, const char* host, int port, const char *database,
	const char *user, const char *password, int timeout_sec) {
	int fd, rc;
	PgMsg msg;

	if((rc = pg_startup(pg, &msg, user, database))) return rc;
	fd = pg_socket_connect(host, port, msg.p, msg.n, timeout_sec);
	if(fd < 0) return pg_puterr(pg, "socket connect to %s:%d failed", host, port);
	pg->fd = fd;
	for(;;) {
		if((rc = pg_recvmsg(pg, &msg))) goto error;
		rc = pg_recvbody(pg, &msg);
		if(rc == PG_RECV) continue;
		else if(rc) goto error;
		else break;
	}

	if((rc = pg_login(pg, &msg, user, password))) goto error;
	assert(pg_socket_send(fd, msg.p, msg.n) == msg.n);

	for(;;) {
		if((rc = pg_recvmsg(pg, &msg))) goto error;
		rc = pg_recvbody(pg, &msg);
		if(rc == PG_RECV) continue;
		else if(rc) goto error;
		else break;
	}

	assert(pg->state == pg_state_ready);
	return fd;
error:
	if(fd >= 0) close(fd);
	return rc;
}

PG_API int
pg_query(PgCtx *pg, const char *format, ...) {
	int rc;
	PgMsg msg;
	va_list arg;

	va_start(arg, format);
	rc = pg_vgenquery(pg, &msg, format, arg);
	va_end(arg);
	if(rc) return rc;
	if((rc = pg_socket_send(pg->fd, msg.p, msg.n)) != msg.n) {
		if(rc) return rc;
		return PG_UNKNOWN;
	}

	return 0;
}

PG_API int
pg_recv_columns(PgCtx *pg, PgCol *col, int *ncol) {
	int rc;
	PgMsg msg;

	if((rc = pg_recvmsg(pg, &msg))) return rc;
	if((rc = pg_recvcol(pg, &msg, col, ncol))) return rc;
	return rc;
}

PG_API int
pg_next(PgCtx *pg, const char *format, ...) {
	int rc;
	PgMsg msg;
	va_list arg;

	if((rc = pg_recvmsg(pg, &msg))) return rc;
	va_start(arg, format);
	rc = pg_vnextrow(pg, &msg, format, arg);
	va_end(arg);
	return rc;
}

#endif

PG_API void
pg_destroy(PgCtx* pg) {
	free(pg->buf);
#ifndef PG_NOSOCKET
	if(pg->fd >= 0) close(pg->fd);
#endif
}


#ifdef PG_EXAMPLE
int main() {
	char *name, *desc, *type;
	PgCtx pg = {0}; /* important. zero init */
	int id, rc;

	pg_init(&pg);
	pg_strip(&pg, 1);
	if((rc = pg_connect(&pg, "localhost", 5432, "animals", "clayton", "password", 60*5)) < 0) goto error;
	if((rc = pg_query(&pg, "SELECT id, %s, name, description FROM dogs", "type"))) goto error;


#if 0   /* this can be set to one to see the result */
	/* this section is optional. Enable it to get information about each returned column
	 * in the query. if disabled then pg_next will silently ignore it */
	PgCol col[8];
	int ncol = (int)(sizeof col / sizeof col[0]);
	if((rc = pg_recv_columns(&pg, col, &ncol))) goto error;
	for(i=0;i<ncol;i++) printf("%s(%s:%d) ", col[i].name, col[i].type, col[i].size < 0 ? col[i].mod : col[i].size);
	printf("\n");
#endif
	for(;;) {
		rc = pg_next(&pg, "isss", &id, &type, &name, &desc);
		if(rc == PG_ROW) printf("%d '%s' %s %s\n", id, type, name, desc);
		else if(rc == PG_RECV) continue;
		else if(rc) goto error;
		else break;
	}

	pg_destroy(&pg);
	return 0;
error:
	printf("Error %s\n", pg_strerror(&pg, rc));
	return 1;
}
#endif

#endif

/*
------------------------------------------------------------------------------
This software is available under 2 licenses -- choose whichever you prefer.
------------------------------------------------------------------------------
ALTERNATIVE A - MIT License
Copyright (c) 2021 Clayton Stangeland
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
------------------------------------------------------------------------------
ALTERNATIVE B - Public Domain (www.unlicense.org)
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
------------------------------------------------------------------------------
*/


