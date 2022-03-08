#ifndef URL_H
#define URL_H

#include <stddef.h>

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
	size_t nscheme;
	const char *user;
	size_t nuser;
	const char *password;
	size_t npassword;
	const char *host;
	size_t nhost;
	const char *port;
	size_t nport;
	const char *path; /* excludes leading / */
	size_t npath;
	const char *query;
	size_t nquery;
	const char *fragment;
	size_t nfragment;
} Url;

typedef struct UrlParam {
	const char *param;
	size_t nparam;
} UrlParam;

/* return number of parameters passed on success, < 0 on failure.
   if url != NULL && nurl == 0 then nurl will be set to strlen(url).
   param and nparam may be zero */
URL_API int url_parse(Url *result, UrlParam *param, size_t nparam, const char *url, size_t nurl);

#ifdef __cplusplus
}
#endif

#endif

#ifdef URL_IMPLEMENTATION
#include <string.h>
#include <stdio.h>


static int url_strnstr(const char *haystack, size_t nhaystack, const char *needle) {
	size_t n = strlen(needle);
	for(int i=0;i<(int)nhaystack && (size_t)i <= nhaystack - n;i++) {
		if(!strncmp(haystack + i, needle, n))
			return i;
	}
	return -1;
}

/* return number of parameters passed on success, < 0 on failure */
URL_API int url_parse(Url *u, UrlParam *param, size_t nparam, const char *url, size_t nurl) {
	memset(u, 0, sizeof *u);
	if(!nurl) {
		if(url) nurl = strlen(url);
		else return -1;
	}

	/* scheme */
	int pos = url_strnstr(url, nurl, "://");
	if(!pos) return -2;
	if(pos > 0) {
		u->scheme = url;
		u->nscheme = (unsigned)pos;
		url += pos + 3;
		nurl -= pos + 3;
	}

	/* user and optional password */
	pos = url_strnstr(url, nurl, "@");
	if(!pos) return -3;
	if(pos > 0) {
		u->user = url;
		int pos2 = url_strnstr(url, (unsigned)pos, ":");
		if(pos2 >= 0) {
			u->nuser = pos2;
			u->password = url + pos2 + 1;
			u->npassword = pos - (pos2 + 1);
		} else {
			u->nuser = pos;
		}
		url += pos + 1;
		nurl -= pos + 1;
	}

	u->host = url;
	pos = url_strnstr(url, nurl, "/");
	if(pos < 0) pos = nurl;
	if(pos >= 0) {
		int pos2 = url_strnstr(url, (unsigned)pos, ":");
		if(pos2 >= 0) {
			u->nhost = pos2;
			u->port = url + pos2 + 1;
			u->nport = pos - (pos2 + 1);
		} else {
			u->nhost = pos;
		}
		url += pos;
		nurl -= pos;
		if(nurl) {
			u->path = url;
		}
	}

	pos = url_strnstr(url, nurl, "?");
	if(pos >=0) {
		if(u->path) u->npath = (unsigned)pos;
		url += pos + 1;
		nurl -= pos + 1;
		u->query = url;
	}

	pos = url_strnstr(url, nurl, "#");
	if(pos >= 0) {
		if(u->query) u->nquery = (unsigned)pos;
		else if(!u->npath) u->npath = (unsigned)pos;
		url += pos + 1;
		nurl -= pos + 1;
		u->fragment = url;
		u->nfragment = nurl;
	}

	/* no query or fragment */
	if(u->path && !u->npath) u->npath = nurl;
	else if(u->query && !u->nquery) u->nquery = nurl;

	const char *query = u->query;
	size_t nquery = u->nquery;
	size_t i;
	for(i=0;i<nparam && nquery;i++) {
		pos = url_strnstr(query, nquery, "&");
		if(pos < 0) break;
		param[i].param = query;
		param[i].nparam = (unsigned)pos;
		query += pos + 1;
		nquery -= pos + 1;
	}
	if(i < nparam && nquery) {
		param[i].param = query;
		param[i++].nparam = nquery;
	}

	return (int)i;
}

#endif

#ifdef URL_EXAMPLE
#include <assert.h>
static int EQ(const char *x, size_t nx, const char *y) {
	return strlen(y) == nx && !strncmp(x, y, nx);
}


int main(int argc, char **argv) {
	char a[] = "https://google.com";
	char b[] = "http://bob@example.com:1000";
	char c[] = "http://bob:pass@example.com";
	char d[] = "tcp://x:y@test.com:8433/where/at?id=9&id2=4#remainder";
	
	assert(url_strnstr(d, strlen(d), "://") == 3);
	Url u;
	UrlParam param[16];
	int nparam = url_parse(&u, param, 16, d, 0);
	assert(nparam == 2);
	assert(EQ(u.scheme, u.nscheme, "tcp"));
	assert(EQ(u.user, u.nuser, "x"));
	assert(EQ(u.password, u.npassword, "y"));
	assert(EQ(u.host, u.nhost, "test.com"));
	assert(EQ(u.port, u.nport, "8433"));
	assert(EQ(u.path, u.npath, "where/at"));
	assert(EQ(u.query, u.nquery, "id=9&id2=4"));
	assert(EQ(u.fragment, u.nfragment, "remainder"));
	assert(EQ(param[0].param, param[0].nparam, "id=9"));
	assert(EQ(param[1].param, param[1].nparam, "id2=4"));

	assert(url_parse(&u, 0, 0, a, 0) >= 0);
	assert(EQ(u.scheme, u.nscheme, "https"));
	assert(EQ(u.host, u.nhost, "google.com"));
	assert(!u.port);
	assert(!u.nport);
	assert(!u.user);
	assert(!u.nuser);
	assert(!u.password);
	assert(!u.npassword);
	assert(!u.path);
	assert(!u.npath);
	assert(!u.query);
	assert(!u.nquery);
	assert(!u.fragment);
	assert(!u.nfragment);

	assert(url_parse(&u, 0, 0, b, 0) >= 0);
	assert(EQ(u.scheme, u.nscheme, "http"));
	assert(EQ(u.host, u.nhost, "example.com"));
	assert(EQ(u.user, u.nuser, "bob"));
	assert(EQ(u.port, u.nport, "1000"));
	assert(!u.password);
	assert(!u.npassword);
	assert(!u.path);
	assert(!u.npath);
	assert(!u.query);
	assert(!u.nquery);
	assert(!u.fragment);
	assert(!u.fragment);

	assert(url_parse(&u, 0, 0, c, 0) >= 0);
	assert(EQ(u.scheme, u.nscheme, "http"));
	assert(EQ(u.host, u.nhost, "example.com"));
	assert(EQ(u.user, u.nuser, "bob"));
	assert(EQ(u.password, u.npassword, "pass"));
	assert(!u.port);
	assert(!u.nport);
	assert(!u.path);
	assert(!u.npath);
	assert(!u.query);
	assert(!u.nquery);
	assert(!u.fragment);
	assert(!u.nfragment);
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

