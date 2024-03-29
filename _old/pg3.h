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

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

/* error return values */
enum {
	PG_ROW=-100,
	PG_SEND,
	PG_RECV,
	PG_MEMORY,
	PG_PARSE,
	PG_ERROR,
	PG_STATE,
	PG_FORMAT,
	PG_MSG,
	PG_UNKNOWN
};

typedef struct PgBuf {
	char *p;
	size_t n, capacity;
} PgBuf;

typedef struct PgConn {
	char error[1024];
	int fd;
	char salt[4], state, type;
	PgBuf buf;
} PgConn;

PG_API const char *pg_strerror(PgConn*, int return_code);

#ifdef __cplusplus
}
#endif

#endif

#ifdef PG_IMPLEMENTATION
#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
	#include <winsock2.h>
	#include <ws2tcpip.h>

	#define SHUT_RDWR SD_BOTH
	#define close closesocket
#else
	#include <fcntl.h>
	#include <netdb.h>
	#include <netinet/in.h>
	#include <netinet/tcp.h>
	#include <sys/socket.h>
	#include <sys/time.h>
	#include <sys/types.h>
	#include <unistd.h>
#endif

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


typedef struct PgMsg {
	char *p;
	size_t n;
} PgMsg;

PG_API const char *
pg_strerror(PgConn* pg, int return_code) {
	switch(return_code) {
	case 0: return "OK";
	case PG_ROW: return "OK: call pg_next";
	case PG_RECV: return "OK: recv next msg";
	case PG_SEND: return "Send failed";
	case PG_MEMORY: return "Out of memory of buffer too small";
	case PG_PARSE: return "Parsing error";
	case PG_ERROR: return pg->error;
	case PG_STATE: return "invalid state";
	case PG_FORMAT: return "format invalid. only text supported not binary";
	case PG_MSG: return "unexpected message type";
	case PG_UNKNOWN:
	default: return "Unknown error";
	}
}

static void pg_destroy(PgConn *pg) {
	shutdown(pg->fd, SHUT_RDWR);
	close(pg->fd);
	free(pg->buf.p);
}

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
pg_socket_keepalive(int fd) {
	int opt = 1;
	setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (char*)&opt, sizeof opt);

#ifdef _WIN32
	DWORD cnt;
	struct tcp_keepalive alive;
	alive.keepalivetime = 15*60*1000;
	alive.keepaliveinterval = 60*1000;
	alive.onoff = TRUE;

	if(WSAIoctl(fd, SIO_KEEPALIVE_VALS, &alive, sizeof alive, 0, 0, &cnt, 0, 0) == SOCKET_ERROR)
		printf("setting keepalive failed\n");
#else
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

static int
pg_socket_connect(const char* host, int port, int timeout) {
	int fd, rc;
	struct addrinfo hints = {0}, *res, *addr;
	char ports[16];

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

	if(!addr) {
		freeaddrinfo(res);
		return -2; /* connection failed */
	}


	pg_socket_timeout(fd, timeout);
	pg_socket_keepalive(fd);

	rc = connect(fd, addr->ai_addr, addr->ai_addrlen);
	if(rc < 0) {
		freeaddrinfo(res);
		close(fd);
		return -3;
	}
	freeaddrinfo(res);
	return fd;
}

static int
pg_reserve(PgBuf *pg, size_t n) {
        int sz;
	char *p;
	if(pg->capacity < pg->n + n) {
                sz = pg->capacity * 2;
                if(sz < n) sz = n;
		if(sz < 1024) sz = 1024;
                p = (char*)realloc(pg->p, sz);
                if(!p) {
			free(pg->p);
			pg->p = 0;
			pg->capacity = pg->n = 0;
			return PG_MEMORY;
		}
                pg->p = p;
                pg->capacity = n;
        }
        return 0;
}

static int
pg_acquire(PgBuf *pg, int n, char **p) {
	int rc;
	if((rc = pg_reserve(pg, n))) return rc;
	*p = pg->p + pg->n;
	pg->n += n;
	return 0;
}


PG_API void
pg_connect1(PgConn *pg, int fd) {
	memset(pg, 0, sizeof *pg);
	pg->fd = fd;
}

PG_API int
pg_connect(PgConn *pg, const char* host, int port, int timeout_sec) {
	int fd = pg_socket_connect(host, port, timeout_sec);
	if(fd < 0) return -1;
	pg_connect1(pg, fd);
	return 0;
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
#ifndef _WIN32
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
static int
pg_recvmsg(PgConn *pg) {
	int rc, n;
	char *p;

	pg->buf.n = 0;
	/* receive header */
	if(pg_acquire(&pg->buf, 5, &p)) return PG_MEMORY;
	if(pg_socket_recvall(pg->fd, p, 5) != 5) return PG_UNKNOWN;

	pg->type = *p++;
	n = pg_readint(&p, 4) - 4;
	pg->buf.n = 0;
	if(pg_acquire(&pg->buf, n, &p)) return PG_MEMORY;
	if(pg_socket_recvall(pg->fd, p, pg->buf.n) != pg->buf.n) return PG_UNKNOWN;
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


/* write null terminated string. return 1 on error 0 on success */
static int
pg_writestr(PgBuf *s, const char *text) {
	char *p;
	int n;
	assert(text);
	n = strlen(text) + 1;
	if(pg_acquire(s, n, &p)) return PG_MEMORY;
	memcpy(p, text, n);
	return 0;
}

/* return 1 on error 0 on success */
static int
pg_writekv(PgBuf *s, const char *key, const char *value) {
	assert(key);
	assert(value);

	if(pg_writestr(s, key)) return PG_MEMORY;
	if(pg_writestr(s, value)) return PG_MEMORY;
	return 0;
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
static int
pg_checkerr(PgConn *pg) {
	char *p, *err=0;
	const char*text;
	int n;
	PgMsg msg;

	if(pg->type != 'E') return 0;
	pg->state = pg_state_error;
	msg.p = pg->buf.p;
	msg.n = pg->buf.n;
	/* error message */
	while(msg.n && *msg.p) {
		if(pg_slice(&msg, 1, &p)) return PG_PARSE;
		if(*p) {
			text = pg_readstr(&msg);
			err = pg_append(err, text);
			err = pg_append(err, " ");
		} else break;
	}
	n = strlen(err) + 1;
	pg->buf.n = 0;
	if(pg_acquire(&pg->buf, n, &p)) return PG_MEMORY;
	memcpy(p, err, n);
	free(err);
	return PG_ERROR;
}

static int
pg_puterr(PgConn *pg, const char *format, ...) {
	va_list arg;
	char prefix[] = "Postgres: ";
	int n, nprefix = sizeof prefix - 1;
	char *p;

	va_start(arg, format);
	n = vsnprintf(0, 0, format, arg);
	va_end(arg);
	if(n < 0) return PG_UNKNOWN;
	n += 1 + nprefix;
	pg->buf.n = 0;
	if(pg_acquire(&pg->buf, n, &p)) return PG_MEMORY;
	memcpy(p, prefix, nprefix);
	va_start(arg, format);
	vsnprintf(p + nprefix, n - nprefix, format, arg);
	va_end(arg);
	return PG_ERROR;
}


PG_API int
pg_recvbody(PgConn *pg) {
	char *p;
	int rc;
	PgMsg msg;

	msg.p = pg->buf.p;
	msg.n = pg->buf.n;

	if((rc = pg_checkerr(pg))) return rc;

	switch(pg->state) {
	case pg_state_error: return PG_ERROR;
	case pg_state_startup:
		if(pg->type != 'R') return PG_MSG;
		if(pg_slice(&msg, 4, &p)) return PG_PARSE;
		if(pg_readint(&p, 4) != 5) return PG_PARSE;
		if(pg_slice(&msg, 4, &p)) return PG_PARSE;
		memcpy(pg->salt, p, 4);
		pg->state = pg_state_authreq;
		break;
	case pg_state_auth:
		if(pg->type != 'R') return PG_MSG;
		if(msg.n != 4) return PG_PARSE;
		if(pg_slice(&msg, 4, &p)) return PG_PARSE;
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

PG_API int
pg_login(PgConn *pg, const char *database, const char *user, const char *password) {
	char md5[16 * 2 + 3 + 1];
	char *p;
	int rc;

	pg->buf.n = 0;
	pg->state = pg_state_startup;
	if(pg_acquire(&pg->buf, 8, &p)) return PG_PARSE;
	pg_writeint(&p, 4, 0); /* length placeholder */
	pg_writeint(&p, 4, 3 << 16); /* protocol version */
	if(pg_writekv(&pg->buf, "user", user)) return PG_PARSE;
	if(pg_writekv(&pg->buf, "database", database)) return PG_PARSE;
	if(pg_writekv(&pg->buf, "client_encoding", "UTF8")) return PG_PARSE;
	if(pg_writekv(&pg->buf, "DateStyle", "ISO, YMD")) return PG_PARSE;
	if(pg_writekv(&pg->buf, "TimeZone", "America/Chicago")) return PG_PARSE;
	if(pg_writekv(&pg->buf, "extra_float_digits", "3")) return PG_PARSE; /* must be 3 for round tripping */
	if(pg_acquire(&pg->buf, 1, &p)) return PG_PARSE;
	*p = 0; /* end */
	p = pg->buf.p;;
	pg_writeint(&p, 4, pg->buf.n);

	if(pg_socket_send(pg->fd, pg->buf.p, pg->buf.n) != pg->buf.n) return PG_SEND;

	for(;;) {
		if((rc = pg_recvmsg(pg))) return rc;
		rc = pg_recvbody(pg);
		if(rc == PG_RECV) continue;
		else if(rc) return rc;
		else break;
	}

	pg->state = pg_state_auth;
	pg_md5(user, password, pg->salt, md5);

	pg->buf.n = 0;
	if(pg_acquire(&pg->buf, 1, &p)) return PG_MEMORY;
	*p = 'p'; /* password auth */
	if(pg_acquire(&pg->buf, 4, &p)) return PG_MEMORY;
	pg_writeint(&p, 4, sizeof md5 + 4);
	if(pg_acquire(&pg->buf, sizeof md5, &p)) return PG_MEMORY;
	memcpy(p, md5, sizeof md5);

	if(pg_socket_send(pg->fd, pg->buf.p, pg->buf.n) != pg->buf.n) return PG_SEND;

	for(;;) {
		if((rc = pg_recvmsg(pg))) return rc;
		rc = pg_recvbody(pg);
		if(rc == PG_RECV) continue;
		else if(rc) return rc;
		else break;
	}

	return 0;
}

#endif


#ifdef PG_EXAMPLE
#include <assert.h>
int main() {
	char *name, *desc, *type;
	PgConn pg = {0}; /* important. zero init */
	int id, rc;


	assert(!pg_connect(&pg, "localhost", 5432, 5));
	assert(!pg_login(&pg, "animals", "clayton", "password"));
	//assert(!pg_query(&pg, "SELECT id, %s, name, descryption FROM dogs", "type"));

#if 0
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
#endif
	pg_destroy(&pg);
	return 0;
error:
	printf("Error %s\n", pg_strerror(&pg, rc));
	return 1;
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

