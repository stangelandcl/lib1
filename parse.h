/* SPDX-License-Identifier: Unlicense */
#ifndef PARSE_H
#define PARSE_H

#if defined(PARSE_STATIC) || defined(PARSE_EXAMPLE)
#define PARSE_API static
#define PARSE_IMPLEMENTATION
#else
#define PARSE_API extern
#endif

#include <stddef.h>
#include <stdint.h>

PARSE_API int parse_bool(const char *start, size_t n);
PARSE_API int64_t parse_int(const char *start, size_t n);
PARSE_API uint64_t parse_uint(const char *start, size_t n);
PARSE_API float parse_float(const char *start, size_t n);
PARSE_API double parse_double(const char *start, size_t n);
/* return 0 on success, -1 on error and uuid will be zeroed */
PARSE_API int parse_uuid(uint8_t uuid[16], const char *start, size_t n);
PARSE_API int64_t parse_time(const char *start, size_t n);
PARSE_API int32_t parse_date(const char *start, size_t n);
#endif

#ifdef PARSE_IMPLEMENTATION
#include <ctype.h>
#include <math.h>
#include <string.h>

/* nanoseconds per second = 1 billion */
#define PARSE_NS_PER_SEC ((int64_t)1000*1000*1000)

static int
parse_eqnocase(const char *x, const char *lower, size_t n) {
	for(size_t i=0;i<n;i++)
		if(tolower(x[i]) != lower[i])
			return 0;
	return 1;
}

PARSE_API int
parse_bool(const char *text, size_t ntext) {
	int x = 1;
	if(ntext == 5 && parse_eqnocase(text, "false", ntext)) x = 0;
	else if(ntext == 2 && parse_eqnocase(text, "no", ntext)) x = 0;
	else if(ntext == 4 && parse_eqnocase(text, "none", ntext)) x = 0;
	else if(ntext == 1 && *text == '0') x = 0;
	else if(ntext == 1 && *text == ' ') x = 0;
	else if(ntext == 0) x = 0;
	return x;
}

PARSE_API int64_t
parse_int(const char *start, size_t n) {
	const char *p = start, *e = start + n;
	int64_t i = 0, s = 1;

	if(p != e) {
		if(*p == '-') { ++p; s = -1; } /* sign */
		for(;p != e && *p>='0' && *p<='9';++p) /* int */
			i = i * 10 + (*p - '0');
	}
	return i * s;
}

PARSE_API uint64_t
parse_uint(const char *start, size_t n) {
	const char *p = start, *e = start + n;
	uint64_t i = 0, s = 1;

	if(p != e) {
		for(;p != e && *p>='0' && *p<='9';++p) /* int */
			i = i * 10 + (*p - '0');
	}
	return i * s;
}

PARSE_API float
parse_float(const char *start, size_t n) {
	const char *p = start, *e = start + n;
	int64_t i = 0, j=0, jj=1, k=0, s = 1, ks=1;
	float f = 0.0f;
	if(p != e) {
		if(*p == '-') {++p; s = -1; } /* sign */
		for(;p != e && *p>='0' && *p<='9';++p) /* int */
			i = i * 10 + (*p - '0');
		f = (float)i;
		if(p != e && *p == '.') { /* fraction */
			for(++p;p != e && *p>='0' && *p<='9';++p) {
				j = j * 10 + (*p - '0');
				jj *= 10;
			}
			f += (float)j / (float)jj;
		}
		f *= s; /* must be done after adding in the fraction */
		if(p != e && (*p == 'e' || *p=='E')) { /* exponent */
			if(++p < e) {
				if(*p == '+') ++p; /* exponent sign */
				else if(*p == '-') {
					ks = -1;
					++p;
				}
				for(;p != e && *p>='0' && *p<='9';++p)
					k = k * 10 + (*p - '0');
				f *= powf(10, (float)(k * ks));
			}
		}
	}
	return f;
}

PARSE_API double
parse_double(const char *start, size_t n) {
	const char *p = start, *e = start + n;
	int64_t i = 0, j=0, jj=1, k=0, s = 1, ks=1;
	double f = 0.0;
	if(p != e) {
		if(*p == '-') {++p; s = -1; } /* sign */
		for(;p != e && *p>='0' && *p<='9';++p) /* int */
			i = i * 10 + (*p - '0');
		f = (double)i;
		if(p != e && *p == '.') { /* fraction */
			for(++p;p != e && *p>='0' && *p<='9';++p) {
				j = j * 10 + (*p - '0');
				jj *= 10;
			}
			f += (double)j / (double)jj;
		}
		f *= s; /* must be done after adding in the fraction */
		if(p != e && (*p == 'e' || *p=='E')) { /* exponent */
			if(++p < e) {
				if(*p == '+') ++p; /* exponent sign */
				else if(*p == '-') {
					ks = -1;
					++p;
				}
				for(;p != e && *p>='0' && *p<='9';++p)
					k = k * 10 + (*p - '0');
				f *= pow(10, (double)(k * ks));
			}
		}
	}
	return f;
}

static int parse_unhex(char c) {
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    if(c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}


/* return 0 on success, -1 on error */
PARSE_API int
parse_uuid(uint8_t uuid[16], const char *start, size_t n) {
    const char *end = start + n;
    size_t i = 0;

  	static const int H[] =
	{
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // ........
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // ........
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // ........
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // ........
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, //  !"#$%&'
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // ()*+,-./
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, // 01234567
		0x08, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 89:;<=>?
		0x00, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x00, // @ABCDEFG
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // HIJKLMNO
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // PQRSTUVW
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // XYZ[\]^_
		0x00, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x00, // `abcdefg
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // hijklmno
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // pqrstuvw
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // xyz{|}~.
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // ........
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // ........
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // ........
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // ........
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // ........
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // ........
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // ........
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // ........
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // ........
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // ........
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // ........
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // ........
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // ........
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // ........
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // ........
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  // ........
	};

    while(start + 1 < end && i < 16) {
        int c = *start++;
        if(c == '-') continue;
        int x = (H[c] << 4) | H[(int)*start++];
        uuid[i++] = x;
    }

    int rc = -(i != 16);
    if(rc) memset(uuid, 0, 16);
    return rc;
}

static int64_t date(int year, int month, int day) {
	static const int month_days[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
	int leap_years, is_leap_year;  // is_leap_day;
	const int seconds_per_day = 86400;
	int64_t seconds;

	if((unsigned)year < 1970) return 0;
	if(month < 1 || month > 12) return 0;

	leap_years = (year - 1968) / 4;
	is_leap_year = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
	if(is_leap_year && month < 3) --leap_years;

	// is_leap_day = month == 2 && day == 29 && is_leap_year;

	seconds = (year - 1970) * 365 * seconds_per_day;
	seconds += leap_years * seconds_per_day;
	seconds += (month_days[--month] + day - 1) * seconds_per_day;
	return seconds * PARSE_NS_PER_SEC;
}

static int64_t
parse_datetime(int year, int month, int day, int hour, int minute, int second, int ms, int us, int ns) {
	int64_t result = date(year, month, day);

	if((unsigned)hour >= 24) return 0;
	if((unsigned)minute >= 60) return 0;
	if((unsigned)second >= 60) return 0;

	result += (int64_t)hour * 60 *60 * PARSE_NS_PER_SEC;
	result += (int64_t)minute * 60 * PARSE_NS_PER_SEC;
	result += ms * 1000000;
	result += us * 1000;
	result += ns;
	return result;
}

PARSE_API int64_t
parse_time(const char *text, size_t ntext) {
	int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0, ms = 0, us = 0, ns = 0;
	int x, i;
	const char *end = text + ntext, *p = text;

	x = 0;
	if(p != end && isdigit(*p)) {
		x *= 10;
		x += *p++ - '0';
	}
	if(p != end && isdigit(*p)) {
		x *= 10;
		x += *p++ - '0';
	}
	if(p != end && isdigit(*p)) {
		x *= 10;
		x += *p++ - '0';
	}
	if(p != end && isdigit(*p)) {
		x *= 10;
		x += *p++ - '0';
	}
	year = x;

	if(p != end && *p == '-') ++p;

	x = 0;
	if(p != end && isdigit(*p)) {
		x *= 10;
		x += *p++ - '0';
	}
	if(p != end && isdigit(*p)) {
		x *= 10;
		x += *p++ - '0';
	}
	month = x;

	if(p != end && *p == '-') ++p;

	x = 0;
	if(p != end && isdigit(*p)) {
		x *= 10;
		x += *p++ - '0';
	}
	if(p != end && isdigit(*p)) {
		x *= 10;
		x += *p++ - '0';
	}
	day = x;

	if(p != end && (*p == 'T' || *p == ' ')) {
		++p; /* skip space or T */

		x = 0;
		if(p != end && isdigit(*p)) {
			x *= 10;
			x += *p++ - '0';
		}
		if(p != end && isdigit(*p)) {
			x *= 10;
			x += *p++ - '0';
		}
		hour = x;

		if(p != end && *p == ':') ++p;

		x = 0;
		if(p != end && isdigit(*p)) {
			x *= 10;
			x += *p++ - '0';
		}
		if(p != end && isdigit(*p)) {
			x *= 10;
			x += *p++ - '0';
		}
		minute = x;

		if(p != end && *p == ':') ++p;

		x = 0;
		if(p != end && isdigit(*p)) {
			x *= 10;
			x += *p++ - '0';
		}
		if(p != end && isdigit(*p)) {
			x *= 10;
			x += *p++ - '0';
		}
		second = x;
		if(p != end && *p == '.') {
			++p;
			for(i=0;i<3 && p != end && isdigit(*p); ++i, ++p) {
				ms *= 10;
				ms += *p - '0';
			}

			for(i=0;i<3 && p != end && isdigit(*p); ++i, ++p) {
				us *= 10;
				us += *p - '0';
			}

			for(i=0;i<3 && p != end && isdigit(*p); ++i, ++p) {
				ns *= 10;
				ns += *p - '0';
			}
		}
	}

	/* don't use mktime because it only works in local time not UTC */
	return parse_datetime(year, month, day, hour, minute, second, ms, us, ns);
}

PARSE_API int32_t
parse_date(const char *text, size_t ntext) {
	int64_t ns = parse_time(text, ntext);
	ns /= 1000000000; /* to seconds */
	ns /= 86400; /* to days */
	return (int32_t)ns;
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
