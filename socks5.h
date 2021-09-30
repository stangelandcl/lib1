#ifndef SOCKS5_H
#define SOCKS5_h

/* either
 * #define SOCKS5_STATIC before each use or
 * #define SOCKS5_IMPLEMENTATION in a single source file
 *
 * See bottom of file for example and license (public domain)
 **/

#if defined(SOCKS5_STATIC) || defined(SOCKS5_EXAMPLE)
	#define SOCKS5_API static
	#define SOCKS5_IMPLEMENTATION
#else
	#define SOCKS5_API extern
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* set user and password to null for no user/pass auth.
 * returns < 0 on failure or a socket on success */
SOCKS5_API int socks5_connect(
	const char *socks5_host,
	int socks5_port,
	const char *real_host,
	int real_port,
	const char *user,
	const char *password);


#ifdef __cplusplus
}
#endif

#endif

#ifdef SOCKS5_IMPLEMENTATION

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
	#include <winsock2.h>
	#include <ws2tcpip.h>

	#define SHUT_RDWR SD_BOTH
	#define close closesocket
	#ifdef _MSC_VER
		#pragma comment(lib, "ws2_32.lib")
	#endif
#else
	#include <errno.h>
	#include <fcntl.h>
	#include <netdb.h>
	#include <netinet/in.h>
	#include <netinet/tcp.h>
	#include <sys/socket.h>
	#include <sys/time.h>
	#include <sys/types.h>
	#include <unistd.h>
#endif

static void socks5_init() {
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
socks5_timeout(int socket, int timeout_in_sec) {
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

static ptrdiff_t
socks5_send(int fd, const void *buf, ptrdiff_t n) {
	char *p = (char*)buf;
	ptrdiff_t sent = 0;
	int rc;

	while(sent != n) {
		rc = send(fd, p + sent, (int)(n - sent), 0);
		if(rc <= 0) {
#ifndef _WIN32
			if(rc == -1 && errno == EINTR) continue;
#endif
			if(rc == 0) return 0;
			return -1;
		}
		sent += rc;
	}
	return sent;
}

static ptrdiff_t
socks5_recv(int fd, void *buf, ptrdiff_t n) {
	ptrdiff_t sent = 0;
	int rc;
	static int flags = MSG_WAITALL;

	while(sent != n) {
		rc = recv(fd, buf, n - (size_t)sent, flags);
		if(rc <= 0) {
#ifdef _WIN32
			if(rc == -1 && WSAGetLastError() == WSAEOPNOTSUPP) {
				flags = 0;
				continue;
			}
#else
			if(rc == -1 && errno == EINTR) continue;
#endif
			if(rc == 0) return 0;
			return -1;
		}
		sent += rc;
	}
	return sent;
}

SOCKS5_API int
socks5_connect(
	const char *socks5_host,
	int socks5_port,
	const char *real_host,
	int real_port,
	const char *user,
	const char *password) {

	struct addrinfo hints = {0}, *res, *addr;
	int fd, rc, n_buf, port;
	unsigned char buf[513];
	size_t n;
	char ports[6];
	const char *host;


	socks5_init();

	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	if(socks5_host) {
		host = socks5_host;
		port = socks5_port;
	} else {
		host = real_host;
		port = real_port;
	}

	snprintf(ports, sizeof ports, "%d", port);
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

	socks5_timeout(fd, 15);

	rc = connect(fd, addr->ai_addr, addr->ai_addrlen);
	freeaddrinfo(res);
	if(rc < 0) {
		close(fd);
		return -1;
	}

	if(socks5_host) {
		n_buf = 0;
		buf[n_buf++] = 5; /* version SOCKS *5* */
		if(user || password) {
			buf[n_buf++] = 2; /* nmethods */
			buf[n_buf++] = 0; /* no authentication */
			buf[n_buf++] = 2; /* user/password auth */
		} else {
			buf[n_buf++] = 1; /* nmethods */
			buf[n_buf++] = 0; /* no authentication */
		}

		if(socks5_send(fd, buf, n_buf) != n_buf) goto error;
		if(socks5_recv(fd, buf, 2) != 2) goto error;

		if(buf[0] != 5) goto error;
		switch(buf[1]) {
		case 0: /* no auth. done */ break;
		case 1: /* gssapi */ goto error;
		case 2: /* user-pass */
			n_buf = 0;
			buf[n_buf++] = 1; /* version of auth protocol */
			user = user ? user : "";
			n = strlen(user);
			if(n > 255) goto error;
			buf[n_buf++] = (unsigned char)n;
			memcpy(buf + n_buf, user, n);
			n_buf += n;
			password = password ? password : "";
			n = strlen(password);
			if(n > 255) goto error;
			buf[n_buf++] = (unsigned char)n;
			memcpy(buf + n_buf, password, n);
			n_buf += n;

			if(socks5_send(fd, buf, n_buf) != n_buf) goto error;
			if(socks5_recv(fd, buf, 2) != 2) goto error;
			if(buf[0] != 1) goto error; /* version number of auth protocol */
			if(buf[1] != 1) goto error; /* status = success */
			break;
		default: goto error;
		}
		/* authorized */
		n_buf = 0;
		buf[n_buf++] = 5; /* version of request protocol */
		buf[n_buf++] = 1; /* 1 = connect. 2 = bind, 3 = udp */
		buf[n_buf++] = 0; /* reserved */
		buf[n_buf++] = 3; /* 1 - IP4 addr (4 bytes), 3 - proxy resolves domain, 4 - IP6 addr (16) bytes */
		/* domain name format is 1 byte length, 1-255 bytes domain name.
		* IPv4 format is just 4 bytes no length prefix.
		* IPv6 format is just 16 bytes no length prefix */
		n = strlen(real_host);
		if(n > 255) goto error;
		buf[n_buf++] = (unsigned char)n;
		memcpy(buf + n_buf, real_host, n);
		n_buf += n;
		buf[n_buf++] = (unsigned char)(real_port >> 8);
		buf[n_buf++] = (unsigned char)real_port;

		if(socks5_send(fd, buf, n_buf) != n_buf) goto error;
		if(socks5_recv(fd, buf, 4) != 4) goto error;
		if(buf[0] != 5) goto error; /* version */
		if(buf[1] != 0) goto error; /* status code */
		/* buf[2] is reserved/meaningless */

		/* next comes address type, address and port.
		* These exist but are usually blank from the server */
		switch(buf[3]) {
		case 1: /* IP v4 */
			if(socks5_recv(fd, buf, 4) != 4) goto error;
			/* buf contains IP v4 address */
			break;
		case 3: /* DNS name */
			if(socks5_recv(fd, buf, 1) != 1) goto error; /* length */
			n = buf[0];
			if(socks5_recv(fd, buf, n) != (ptrdiff_t)n) goto error;
			/* buf contains DNS name */
			break;
		case 4: /* IP v6 */
			if(socks5_recv(fd, buf, 16) != 16) goto error;
			/* buf contains ip v6 addr */
			break;
		default: goto error; /* invalid type */
		}

		/* recv port */
		if(socks5_recv(fd, buf, 2) != 2) goto error;
		/* buf contains big endian port of remote server */
	}
	socks5_timeout(fd, 60*60);
	return fd;
error:
	shutdown(fd, SHUT_RDWR);
	close(fd);
	return -1;
}
#endif

#ifdef SOCKS5_EXAMPLE
#include <stdio.h>
int main(int argc, char **argv) {
	setbuf(stdout, 0);
	int fd = socks5_connect("localhost", 5100, "google.com", 80, 0, 0);
	printf("fd=%d\n", fd);
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
