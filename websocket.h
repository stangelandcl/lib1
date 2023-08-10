/* SPDX-License-Identifier: Unlicense */
#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#if defined(WEBSOCKET_STATIC) || defined(WEBSOCKET_EXAMPLE)
#define WEBSOCKET_API static
#define WEBSOCKET_IMPLEMENTATION
#else
#define WEBSOCKET_API extern
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct WsConn;
typedef struct WsConn WsConn;

typedef struct WsStats {
    size_t send, send_uncompressed;
    size_t recv, recv_uncompressed;
	double send_time, recv_time;
} WsStats;

WEBSOCKET_API struct WsConn* wsconnect(int fd, const char *host);
WEBSOCKET_API struct WsConn* wsconnect1(const char *host, int port, const char *socks_host, int socks_port);
WEBSOCKET_API char* wsrecv(struct WsConn*, size_t *n);
WEBSOCKET_API char* wsrecvzlib(struct WsConn*, size_t *n);
WEBSOCKET_API int wssend(struct WsConn*, const void *data, size_t n, int binary);
WEBSOCKET_API int wssendtext(struct WsConn*, const void *data, size_t n);
WEBSOCKET_API int wssendzlib(struct WsConn*, const void *data, size_t n);
WEBSOCKET_API void wsstats(struct WsConn *, WsStats *);
WEBSOCKET_API void wsclose(struct WsConn *conn);

#ifdef __cplusplus
}
#endif

#endif

#ifdef WEBSOCKET_IMPLEMENTATION
#include <tls.h>
#ifdef WEBSOCKET_STATIC
    #define GZIP_STATIC
    #define SB_STATIC
#endif
#include "sb.h"
#include "gzip.h"
#include "now.h"

struct WsConn {
    struct tls *tls;
    int fd;
    size_t send, send_uncompressed;
    size_t recv, recv_uncompressed;
	double send_time, recv_time;
};

static struct tls_config *websocket_config;

static void websocket_init() {
	if(!websocket_config) {
		struct tls_config *conf = tls_config_new();
		tls_config_insecure_noverifycert(conf);
		tls_config_insecure_noverifyname(conf);
		tls_config_insecure_noverifytime(conf);
		websocket_config = conf;
	}
}

WEBSOCKET_API void
wsstats(WsConn *conn, WsStats *stats) {
	stats->recv = conn->recv;
	stats->send = conn->send;
	stats->send_uncompressed = conn->send_uncompressed;
	stats->recv_uncompressed = conn->recv_uncompressed;
	stats->send_time = conn->send_time;
	stats->recv_time = conn->recv_time;
}

static int
wstlssend(struct tls *tls, const void *data, size_t n) {
	const char *p = data;
	while(n) {
		ssize_t rc = tls_write(tls, p, n);
		if(!rc) {
			printf("connection closed by remote\n");
			return -1;
		}
		if(rc == TLS_WANT_POLLIN || rc == TLS_WANT_POLLOUT) continue;
		if(rc == -1) {
			printf("send failed: %s\n", tls_error(tls));
			return -1;
		}
		n -= rc;
		p += rc;
	}
	return 0;
}

static int
wstlsreadn(struct tls *tls, void *buf, size_t n) {
	char *data = buf;
	while(n > 0) {
		ssize_t rc = tls_read(tls, data, n);
		if(rc == -1) return -1;
		if(rc == TLS_WANT_POLLIN || rc == TLS_WANT_POLLOUT) continue;
		data += rc;
		n -= rc;
	}
	return 0;
}

WEBSOCKET_API WsConn*
wsconnect(int fd, const char *host) {
    websocket_init();
    struct tls *conn = tls_client();
    tls_configure(conn, websocket_config);
    tls_connect_socket(conn, fd, host);
	for(;;) {
		int rc = tls_handshake(conn);
		if(!rc) break;
		if(rc == TLS_WANT_POLLIN || rc == TLS_WANT_POLLOUT) continue;
		if(rc == -1) {
			printf("handshake failed: %s\n", tls_error(conn));
			goto error;
		}
	}

    const char query[] =
		"GET /chat HTTP/1.1\r\n"
		"Host: %s\r\n"
		"Upgrade: websocket\r\n"
		"Connection: Upgrade\r\n"
		"Sec-WebSocket-Key: x3JJHMbDL1EzLkh9GBhXDw==\r\n"
		"Sec-WebSocket-Protocol: chat, superchat\r\n"
		"Sec-WebSocket-Version: 13\r\n"
		"Origin: http://%s\r\n"
		"\r\n";

    char buf[1024];
    size_t nbuf = snprintf(buf, sizeof buf, query, host, host);

    if(wstlssend(conn, buf, nbuf)) {
        printf("http send failed: %s\n", tls_error(conn));
        goto error;
    }

    ssize_t rc = tls_read(conn, buf, sizeof buf);
	if(rc == -1) {
        printf("http recv failed: %s\n", tls_error(conn));
		goto error;
	}

    WsConn *c = calloc(1, sizeof *c);
    if(!c) goto error;
    c->tls = conn;
    c->fd = fd;
    return c;
error:
    tls_free(conn);
    return 0;
}

WEBSOCKET_API WsConn*
wsconnect1(const char *host, int port, const char *socks_host, int socks_port) {
    int fd = socks5_connect(
		socks_host, socks_port,
		host, port,
		0, 0);
	if(fd < 0) return 0;


    WsConn *conn = wsconnect(fd, host);
    if(!conn) {
        shutdown(fd, SHUT_RDWR);
        close(fd);
        return 0;
    }
    return conn;
}

WEBSOCKET_API void
wsclose(WsConn *conn) {
	if(!conn) return;
	if(conn->tls) {
		tls_close(conn->tls);
		tls_free(conn->tls);
	}
	if(conn->fd >= 0) {
		shutdown(conn->fd, SHUT_RDWR);
		close(conn->fd);
	}
    free(conn);
}

WEBSOCKET_API int
wssend(WsConn *conn, const void *data, size_t n, int binary) {
    conn->send += n;
	uint8_t header[14];
	size_t i=0;
	header[i++] = (1 << 7) | (binary ? 2 : 1); /* single frame */
	header[i] = (1 << 7);
	if(n < 126) header[i++] |= n;
	else if(n <= UINT16_MAX) {
		header[i++] |= 126;
		header[i++] = n >> 8;
		header[i++] = n;
	} else {
		header[i++] |= 127;
		header[i++] = n >> 56;
		header[i++] = n >> 48;
		header[i++] = n >> 40;
		header[i++] = n >> 32;
		header[i++] = n >> 24;
		header[i++] = n >> 16;
		header[i++] = n >> 8;
		header[i++] = n;
	}

	/* mask */
	header[i++] = 0;
	header[i++] = 0;
	header[i++] = 0;
	header[i++] = 0;

	double t = now();
	if(wstlssend(conn->tls, header, i)) return -1;
	int rc = wstlssend(conn->tls, data, n);
	conn->send_time += now() - t;
	return rc;
}

WEBSOCKET_API int
wssendtext(WsConn *conn, const void *data, size_t n) {
    conn->send_uncompressed = n;
	return wssend(conn, data, n, 0);
}

WEBSOCKET_API int
wssendzlib(WsConn *conn, const void *data, size_t n) {
	size_t ntmp = gzip_bound(GZIP_ZLIB, n);
	char *tmp = malloc(ntmp);
	ssize_t sz = gzip_compress(GZIP_ZLIB, tmp, ntmp, data, n);
	if(sz <= 0) return -1;
	//printf("sending %zu compressed bytes from %zu original\n", sz, n);
	conn->send_uncompressed += n;
	int rc = wssend(conn, tmp, sz, 1);
	free(tmp);
	return rc;
}

WEBSOCKET_API char*
wsrecv(WsConn *conn, size_t *n) {
	SB sb;
	sb_init(&sb);
	*n = 0;
	int first = 1;
	double t = now();
	for(;;) {
		uint8_t buf[125];
		if(wstlsreadn(conn->tls, buf, 2)) {
			fprintf(stderr, "%s:%d ws read failed\n", __FILE__, __LINE__);
			goto error;
		}

		int fin = buf[0] & (1 << 7);
		int op = buf[0] & 15;
		int mask = buf[1] & (1 << 7);
		if(mask) {
			fprintf(stderr, "Websocket expected unmasked data from server\n");
			goto error;
		}
		size_t len = buf[1] & 127;
		if(len == 126) {
			if(wstlsreadn(conn->tls, buf, 2)) {
				fprintf(stderr, "%s:%d ws read len failed\n", __FILE__, __LINE__);
				goto error;
			}
			len = (size_t)buf[0] << 8;
			len += (size_t)buf[1];
		} else if(len == 127) {
			if(wstlsreadn(conn->tls, buf, 8)) {
				fprintf(stderr, "%s:%d ws read long len\n", __FILE__, __LINE__);
				goto error;
			}
			len = (size_t)buf[0] << 56;
			len += (size_t)buf[1] << 48;
			len += (size_t)buf[2] << 40;
			len += (size_t)buf[3] << 32;
			len += (size_t)buf[4] << 24;
			len += (size_t)buf[5] << 16;
			len += (size_t)buf[6] << 8;
			len += (size_t)buf[7];
		}

		if(op > 2) {
			if(len > 125) {
				fprintf(stderr, "%s:%d ws read op failed\n", __FILE__, __LINE__);
				goto error;
			}
			if(wstlsreadn(conn->tls, buf, len)) {
				fprintf(stderr, "%s:%d ws read op failed\n", __FILE__, __LINE__);
				goto error;
			}
			continue;
		}
		if(first) {
			first = 0;
			t = now();
		}
		sb_reserve(&sb, len + 1);
		if(wstlsreadn(conn->tls, sb.str + sb.n, len)) {
			fprintf(stderr, "%s:%d ws read body failed\n", __FILE__, __LINE__);
			goto error;
		}
		sb.n += len;
		sb.str[sb.n] = 0;
		if(fin) break;
	}
	*n = sb.n;
	conn->recv += *n;
	conn->recv_time += now() - t;
	//fprintf(stderr, "ws read received %zu bytes\n", *n);
	return sb.str;
error:
	sb_free(&sb);
	return 0;
}

WEBSOCKET_API char*
wsrecvzlib(WsConn *conn, size_t *n) {
	size_t ndata;
	*n = 0;
	char *data = wsrecv(conn, &ndata);
	if(!data) return 0;
	char *uncomp = gzip_uncompress(GZIP_ZLIB, data, ndata, n);
	free(data);
	//fprintf(stderr, "decompressed %zu into %zu\n", ndata, *n);
	//uncomp = realloc(uncomp, *n + 1);
	//uncomp[*n] = 0;
	conn->recv_uncompressed += *n;
	return uncomp;
}

#endif
