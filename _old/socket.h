#ifndef SOCKET_H
#define SOCKET_H

#if defined(SOCKET_STATIC) || defined(SOCKET_EXAMPLE)
#define SOCKET_API static
#define SOCKET_IMPLEMENTATION
#else
#define SOCKET_API extern
#endif

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

SOCKET_API int socket_connect(const char* host, int port, const void *buf, ptrdiff_t n, int timeout);
SOCKET_API void socket_nodelay(int fd);
SOCKET_API int socket_keepalive(int fd);
/* receive exactly n bytes except when non-blocking socket is set.
   returns 0 on disconnected, -1 on error, and n on success */
SOCKET_API ptrdiff_t socket_recv(int fd, void *data, size_t n);
/* except when non-blocking socket is set.
   returns 0 on disconnected, -1 on error, and n on success */
SOCKET_API ptrdiff_t socket_recvall(int fd, void *buf, ptrdiff_t n);
/* except when non-blocking socket is set.
   returns 0 on disconnected, -1 on error, and n on success */
SOCKET_API ptrdiff_t socket_sendall(int fd, const void *buf, ptrdiff_t n);
SOCKET_API int socket_listen(const char *host, int port);
/* set timeout for each individual send and recv call */
SOCKET_API void socket_timeout(int socket, int timeout_in_sec);
SOCKET_API int socket_accept(int fd, char *host, int nhost, int *port);
SOCKET_API int socket_nonblock(int fd, int enable);
SOCKET_API void socket_close(int fd);


#ifdef __cplusplus
}
#endif

#endif


#ifdef SOCKET_IMPLEMENTATION

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

#ifdef __linux__
	#define SOCKET_FASTOPEN 1
#endif

#endif

static void socket_init() {
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

#define SOCKET_WAITALL 1

SOCKET_API int
socket_keepalive(int fd) {
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

	/* seconds to wait before starting a new set of keepalive checks */
	opt = 15*60;
	setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, (void*)&opt, sizeof opt);

	/* keep alive interval between unacknowledged probes */
	opt = 60;
	setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, (void*)&opt, sizeof opt);
#endif
	return 0;
}

/* receive exactly n bytes */
SOCKET_API ptrdiff_t
socket_recv(int fd, void *data, size_t n) {
	ptrdiff_t rc;
	size_t recvd = 0;
	char *p = (char*)data;

	while(recvd != n) {
		rc = (ptrdiff_t)recv(fd, p + recvd, n - recvd, 0);
		if(rc <= 0) {
			if(rc == 0) break; /* remote closed connection */
			if(rc == -1 && errno == EINTR) continue;
			if(rc == -1 && (errno == EWOULDBLOCK || errno == EAGAIN)) break;
			return rc;
		}
		recvd += rc;
	}
	return recvd;
}

/* except when non-blocking socket is set */
SOCKET_API ptrdiff_t
socket_recvall(int fd, void *buf, ptrdiff_t n) {
	char *p = (char*)buf;
	ptrdiff_t sent = 0;
	int rc;

	while(sent != n) {
		rc = recv(fd, p + sent, (int)(n - sent), MSG_WAITALL);
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

SOCKET_API ptrdiff_t
socket_sendall(int fd, const void *buf, ptrdiff_t n) {
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

SOCKET_API int
socket_listen(const char *host, int port) {
	struct addrinfo hints = {0}, *res, *a;
	char ports[16];
	int fd, opt, rc;

	socket_init();

	snprintf(ports, sizeof ports, "%d", port);
	hints.ai_family = AF_INET; /* AF_UNSPEC */
	hints.ai_socktype = SOCK_STREAM;
	/* hints.ai_flags = AI_ADDRCONFIG; */
	rc = getaddrinfo(host, ports, &hints, &res);
	if(rc) {
		printf("getaddrinfo failed %d\n", rc);
		return -1;
	}

	for(a=res;a;a=a->ai_next) {
		fd = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
		if(fd >= 0) break;
	}

	if(!a) {
		freeaddrinfo(res);
		return -2; /* connection failed */
	}
#ifdef SOCKET_FASTOPEN
	opt = 1;
	rc = setsockopt(fd, IPPROTO_TCP, TCP_FASTOPEN, &opt, sizeof opt);
	assert(rc == 0);
#endif

	opt = 1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void*)&opt, sizeof opt);

	if(bind(fd, a->ai_addr, a->ai_addrlen) == -1) {
		freeaddrinfo(res);
		close(fd);
		return -3;
	}
	freeaddrinfo(res);

	if(listen(fd, 100) == -1) {
		close(fd);
		return -4;
	}
	return fd;
}

/* set timeout for each individual send and recv call */
SOCKET_API void
socket_timeout(int socket, int timeout_in_sec) {
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

SOCKET_API int
socket_accept(int fd, char *host, int nhost, int *port) {
	struct sockaddr_storage addr;
	socklen_t addrlen;
	char ports[6];
	int s;

	addrlen = sizeof addr;
	s = accept(fd, (struct sockaddr*)&addr, &addrlen);
	if(s == -1) {
		printf("accept failed %s\n", strerror(errno));
		return -1;
	}

#if 1
	if(getnameinfo((struct sockaddr*)&addr, addrlen,
		host, nhost, ports, sizeof ports, NI_NOFQDN) == -1) {
		snprintf(host, nhost, "(unknown)");
		*port = 0;
	} else {
		*port = atoi(ports);
	}
#else
	host[0] = 0;
	*port = 0;
#endif
	socket_keepalive(fd);
	return s;
}

SOCKET_API int
socket_connect(const char* host, int port, const void *buf, ptrdiff_t n, int timeout) {
	int fd, fastopen=0, rc;
	ptrdiff_t sent;
	char *p = (char*)buf;
	struct addrinfo hints = {0}, *res, *addr;
	char ports[16];

	socket_init();

#ifdef SOCKET_FASTOPEN
	fastopen = 1;
#endif

	snprintf(ports, sizeof ports, "%d", port);
	hints.ai_family = AF_INET; /* AF_UNSPEC */
	hints.ai_socktype = SOCK_STREAM;
	/* hints.ai_flags = AI_ADDRCONFIG; */
	rc = getaddrinfo(host, ports, &hints, &res);
	if(rc) {
		printf("getaddrinfo failed %d\n", rc);
		return -1;
	}

	for(addr=res;addr;addr=addr->ai_next) {
		fd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
		if(fd >= 0) break;
		else printf("socket creation error %s\n", strerror(errno));
	}

	if(!addr) return -2; /* connection failed */


#ifdef SOCKET_FASTOPEN
	if(fastopen) {
		int opt = 1;
		rc = setsockopt(fd, IPPROTO_TCP, TCP_FASTOPEN, (void*)&opt, sizeof opt);
		if(rc < 0) {
			freeaddrinfo(addr);
			return rc;
		}
	}
#endif

	socket_timeout(fd, timeout);
	socket_keepalive(fd);

	if(!fastopen || !n) {
		rc = connect(fd, addr->ai_addr, addr->ai_addrlen);
		if(rc < 0) {
			freeaddrinfo(addr);
			close(fd);
			return -3;
		}
	}

	sent = 0;
	while(sent != n) {
		rc = (int)(n - sent);
		if(sent || !fastopen) rc = send(fd, p + sent, rc, 0);
#ifdef SOCKET_FASTOPEN
		else rc = sendto(fd, p + sent, rc, MSG_FASTOPEN, addr->ai_addr, addr->ai_addrlen);
#endif
		if(rc <= 0) {
			if(rc == -1 && errno == EINTR) continue;
			close(fd);
			freeaddrinfo(addr);
			return rc ? -4 : -5;
		}
		sent += rc;
	}
	freeaddrinfo(addr);
	return fd;
}

SOCKET_API int
socket_nonblock(int fd, int enable) {
	int f, s;

#ifdef _WIN32
	f = enable ? 1 : 0;
	s = ioctlsocket(fd, FIONBIO, (void*)&f);
	return s ? -1 : 0;
#else
	f = fcntl(fd, F_GETFL, 0);
	if(f == -1) {
		printf("socket_nonblock get %d error: %s\n", fd, strerror(errno));
		return -1;
	}
	if(enable) {
		if(f & O_NONBLOCK) return 0;
		f |= O_NONBLOCK;
	} else {
		if(!(f & O_NONBLOCK)) return 0;
		f &= ~O_NONBLOCK;
	}
	s = fcntl(fd, F_SETFL, f);
	if(s == -1) {
		printf("socket_nonblock set %d error: %s\n", fd, strerror(errno));
		return -1;
	}
#endif
	return 0;
}

SOCKET_API void
socket_nodelay(int fd) {
	int opt = 1;
	setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char*)&opt, sizeof opt);
}

SOCKET_API void
socket_close(int fd) {
	/* disable non blocking so all data is sent before closing */
	socket_nonblock(fd, 0);
	/* flush any sent data before closing */
	shutdown(fd, SHUT_RDWR);
	close(fd);
}
#endif


#ifdef SOCKET_MAIN
#include <assert.h>
#include <string.h>
int main(int argc, char **argv) {
	const char *get = "HTTP/1.0 GET /\r\nHost: google.com\r\nConnection: close\r\n\r\n";
	int fd, rc;
	char buf[10000];

	fd = socket_connect("google.com", 80, 0, 0, 60);
	assert(fd >= 0);
	rc = socket_send(fd, get, strlen(get));
	assert(!rc);
	rc = socket_recv(fd, buf, sizeof buf, 0);
	assert(rc > 0);
	buf[rc] = 0;
	printf("received:\n%s\n", buf);
	return 0;
}
#endif
