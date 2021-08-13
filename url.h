#ifndef URL_H
#define URL_H

#if defined(URL_STATIC) || defined(URL_EXAMPLE)
#define URL_API static
#define URL_IMPLEMENTATION
#else
#define URL_API extern
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* url format according to wikipedia
URI = scheme:[//authority]path[?query][#fragment]
authority = [user[:password]@]host[:port]
*/

typedef struct Url {
	const char *scheme;
	const char *user;
	const char *password;
	const char *host;
	const char *port;
	const char *path; /* excludes leading / */
	const char *query;
	const char *fragment;
} Url;

typedef struct UrlBuf {
	/* strings return pointers to buf so much live as long as Url */
	char buf[4096];
	Url url;
} UrlBuf;

/** return 0 on success */
URL_API int url_parse(Url *result, char *url);
URL_API int url_parse_buf(UrlBuf *buf, const char *url);

#ifdef __cplusplus
}
#endif

#endif

#ifdef URL_IMPLEMENTATION
#include <string.h>
#include <stdio.h>

URL_API int url_parse_buf(UrlBuf *buf, const char *url) {
	int n = snprintf(buf->buf, sizeof buf->buf, "%s", url);
	if(n >= (int)sizeof buf->buf) return -2;
	return url_parse(&buf->url, buf->buf);
}

/* return 0 on success. in place modifies url and returns pointers to it in result. */
URL_API int url_parse(Url *result, char *url) {
	char *s, *e, *e2;
	Url *u = result;
	u->scheme = u->user = u->password = u->host = u->port = u->path = u->query = u->fragment = "";

	/* scheme */
	s = url;
	e = strstr(s, ":");
	if(!e) return -1;
	*e++ = 0;
	result->scheme = s;
	s = e;

	/* authorithy */
	if(s[0] == '/' && s[1] == '/') {
		s += 2;
		/* user/password */
		e = strstr(s, "@");
		if(e) {
			result->user = s;
			*e++ = 0;
			e2 = strstr(s, ":");
			if(e2) {
				result->user = s;
				*e2++ = 0;
				result->password = e2;
			}
			s = e;
		}

		/* host and port */
		result->host = s;
		e = strstr(s, ":");
		if(e) {
			*e++ = 0;
			result->port = s = e;
		}
		e = strstr(s, "/");
		if(e) {
			*e++ = 0;
			s = e;
		} else while(*s && (*s != '?' && *s != '#')) ++s;
	}

	result->path = s;
	/* query */
	e = strstr(s, "?");
	if(e) {
		*e++ = 0;
		result->query = s = e;
	}

	/* fragment */
	e = strstr(s, "#");
	if(e) {
		*e++ = 0;
		result->fragment = s = e;
	}

	return 0;
}

#endif

#ifdef URL_EXAMPLE
#include <assert.h>
int main(int argc, char **argv) {
	char a[] = "https://google.com";
	char b[] = "http://bob@example.com:1000";
	char c[] = "http://bob:pass@example.com";
	char d[] = "tcp://x:y@test.com:8433/where/at?id=9&id2=4#remainder";
	Url u;
	url_parse(&u, d);
	assert(!strcmp(u.scheme, "tcp"));
	assert(!strcmp(u.user, "x"));
	assert(!strcmp(u.password, "y"));
	assert(!strcmp(u.host, "test.com"));
	assert(!strcmp(u.port, "8433"));
	assert(!strcmp(u.path, "where/at"));
	assert(!strcmp(u.query, "id=9&id2=4"));
	assert(!strcmp(u.fragment, "remainder"));

	url_parse(&u, a);
	assert(!strcmp(u.scheme, "https"));
	assert(!strcmp(u.host, "google.com"));
	assert(!strcmp(u.port, ""));
	assert(!strcmp(u.user, ""));
	assert(!strcmp(u.password, ""));
	assert(!strcmp(u.path, ""));
	assert(!strcmp(u.query, ""));
	assert(!strcmp(u.fragment, ""));

	url_parse(&u, b);
	assert(!strcmp(u.scheme, "http"));
	assert(!strcmp(u.host, "example.com"));
	assert(!strcmp(u.user, "bob"));
	assert(!strcmp(u.port, "1000"));
	assert(!strcmp(u.password, ""));
	assert(!strcmp(u.path, ""));
	assert(!strcmp(u.query, ""));
	assert(!strcmp(u.fragment, ""));

	url_parse(&u, c);
	assert(!strcmp(u.scheme, "http"));
	assert(!strcmp(u.host, "example.com"));
	assert(!strcmp(u.user, "bob"));
	assert(!strcmp(u.password, "pass"));
	assert(!strcmp(u.port, ""));
	assert(!strcmp(u.path, ""));
	assert(!strcmp(u.query, ""));
	assert(!strcmp(u.fragment, ""));
	printf("Success\n");
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

