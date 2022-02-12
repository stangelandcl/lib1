/* SPDX-License-Identifier: Unlicense */
#ifndef JSON_H
#define JSON_H

/*
   From https://github.com/stangelandcl/lib1/json.h
   Zero memory overhead iterative json parser.
   license and example at end of file. Search for JSON_EXAMPLE

   Not actually so much a parser as a fast way to extract data
   from a JSON document. Runs at about 65-70% of the speed of simdjson.
   lazily parses numbers and strings.
   call json_str() or json_strdup() on a token to decode escape sequences.
   call json_int() or json_float() to decode numbers. Will parse numbers
   from either json numbers or json strings.
   call json_bool() to decode bools
   check JsonTok->type == JSON_NULL for nulls.

   #define JSON_IMPLEMENTATION in one C file before including json.h.
   Use json.h in other files normally.
   Alternatively define JSON_STATIC before each use of json.h for
   static definitions
*/

#if defined(JSON_STATIC) || defined(JSON_EXAMPLE) || defined(JSON_FUZZ)
#define JSON_API static
#define JSON_IMPLEMENTATION
#else
#define JSON_API extern
#endif

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
	JSON_OBJECT=1, /* object start */
	JSON_OBJECT_END,
	JSON_ARRAY, /* array start */
	JSON_ARRAY_END,
	JSON_STRING,
	JSON_NUMBER,
	JSON_BOOL,
	JSON_NULL
};

/* parser. call json_init before using with json_next */
typedef struct Json {
	char *s, *e;
} Json;

typedef struct JsonTok {
	char *start, *end;
	int type;
} JsonTok;

/* compare JsonTok tok to string literal and return 1 if
   tok is a string and matches the literal */
#define JSON_EQ(tok, literal) \
	 ((((tok)->end - (tok)->start) == sizeof(literal) - 1) && \
	 !memcmp((tok)->start, literal, sizeof(literal)-1))

/* initialize parser with json text. json must be non-const.
   string escapes are rewritten in place */
JSON_API void json_init(Json *p, char *json, size_t n);
/* decode string and return length in n. May return null */
JSON_API char* json_str(JsonTok *t, size_t *n);
/* malloc and return a C string of token.
   returns 0 on not a string or out of memory */
JSON_API char* json_strdup(JsonTok*);
/* return 1 on JsonTok == true or 0 otherwise */
JSON_API int json_bool(JsonTok*);
/* return 1 on have token and can call again or 0 on
   error or end of stream */
JSON_API int json_next(Json*, JsonTok*);
/* parses a double from a number or string token. returns 0 on can't parse */
JSON_API double json_float(JsonTok*);
/* parses an int64 from a number or string token. returns 0 on can't parse */
JSON_API int64_t json_int(JsonTok*);
/* skip to end of current object or array. This only works
   if the parser is at start of object or array,
   last token from json_next was JSON_OBJECT or JSON_ARRAY.
   Otherwise will mess up parsing and lead to invalid state.
   To skip arrays and objects and properly handle nesting
   always call this function as part of every "if" chain dealing
   with tokens. It is safe to call whether last JsonTok was
   actually a composite or not */
JSON_API void json_skip(Json*p, JsonTok *t);
/* return 1 if token is object or array else 0. */
static int
json_composite(JsonTok *t) {
	return t->type == JSON_OBJECT || t->type == JSON_ARRAY;
}
/* iterate a key-value pair in a json object.
   return 1 on have key, value or 0 on end of object or error */
static int
json_object(Json *p, JsonTok *k, JsonTok *v) {
	if(!json_next(p, k) || k->type == JSON_OBJECT_END) return 0;
	return json_next(p, v);
}
/* iterate array value until error, or end of json array.
   return 1 on have item. 0 on end of array or error */
static int
json_array(Json *p, JsonTok *t) {
	return json_next(p, t) && t->type != JSON_ARRAY_END;
}

#ifdef __cplusplus
}
#endif

#endif

#ifdef JSON_IMPLEMENTATION

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

JSON_API int
json_error(Json *p) {
	return !p->e;
}

static inline int
json_white(Json *p) {
	while(p->s < p->e && (*p->s == ' ' || *p->s == '\t' || *p->s == '\n' || *p->s == '\r'))
		++p->s;
	return p->s < p->e;
}

static int
json_unicode(char **pp) {
	char *p = *pp;
	int i, ch = 0;
	for(i=4;i!=0;i--,p++) {
		ch *= 16;
		if(*p >= 'a' && *p <= 'f') ch += 10 + *p - 'a';
		else if(*p >= 'A' && *p <= 'F') ch += 10 + *p - 'A';
		else if(*p >= '0' && *p <= '9') ch += *p - '0';
		else {
			ch = 0;
			break;
		};
	}
	*pp = p;
	return ch;
}

JSON_API int
json_next(Json *p, JsonTok *t) {
	if(!json_white(p)) return 0;
	t->start = t->end = 0;
	for(;;) {
		switch(*p->s++) {
		case '{': t->type = JSON_OBJECT; return 1;
		case '}': t->type = JSON_OBJECT_END; return 1;
		case '[': t->type = JSON_ARRAY; return 1;
		case ']': t->type = JSON_ARRAY_END; return 1;
		case ',': case ':':
			if(!json_white(p)) goto error;
			continue;
		case '"':
			t->start = p->s;
			while(p->s < p->e-1 && *p->s != '"') {
				if(*p->s == '\\') ++p->s;
				++p->s;
			}
			t->end = p->s++;
			t->type = JSON_STRING;
			return 1;
		case 'n':
		case 'N': /* for NaN */
			t->start = p->s-1;
			if(p->e - t->start < 3) goto error;
			p->s += 2 + (p->s[0] == 'u'); /* handle nan or null */
			if(p->s > p->e) p->s = p->e; /* bounds check for invalid json */
			t->end = p->s;
			t->type = JSON_NULL;
			return 1;
		case 't':
			t->start = p->s-1; p->s += 3;
			if(p->s > p->e) p->s = p->e; /* bounds check for invalid json */
			t->end = p->s;
			t->type = JSON_BOOL;
			return 1;
		case 'f':
			t->start = p->s-1; p->s += 4;
			if(p->s > p->e) p->s = p->e; /* bounds check for invalid json */
			t->end = p->s;
			t->type = JSON_BOOL;
			return 1;
		default:
			t->start = --p->s;
			while(p->s != p->e && ((*p->s >= '0' && *p->s <= '9') || *p->s == '-' || *p->s == '+' || *p->s == '.' || *p->s == 'e' || *p->s == 'E'))
				++p->s;
			t->end = p->s;
			if(t->start == t->end) goto error;
			t->type = JSON_NUMBER;
			return 1;
		}

	}
error:
	p->e = 0;
	return 0;
}

JSON_API void
json_init(Json *p, char *json, size_t n) {
	p->s = json;
	p->e = json + n;
}

JSON_API int
json_decode(char **pp, char **dp, char *e) {
	char *p = *pp, *d = *dp;
	int ch, w1, w2;
	++p;
	switch(*p++) {
	case '"': *d = '"'; break;
	case '\\': *d = '\\'; break;
	case 'n': *d = '\n'; break;
	case 't': *d = '\t'; break;
	case 'r': *d = '\r'; break;
	case '/': *d = '/'; break;
	case 'b': *d = 'b'; break;
	case 'f': *d = 'f'; break;
	case 'u':
		if(e - p < 4) return 0;;
		if(!(ch = json_unicode(&p))) return 0;
		if(ch >= 0xD800 && ch <= 0xDBFF) {
			if(e - p < 6) return 0;
			if(*p++ != '\\' || *p++ != 'u') return 0;
			w1 = ch - 0xD800;
			if(!(ch = json_unicode(&p))) return 0;
			if(ch < 0xDC00 || ch > 0xDFFF) return 0;
			w2 = ch - 0xDC00;
			ch = 0x10000 + (w1 << 10) + w2;
		}

		if(ch < 0x80) *(unsigned char*)d++ = ch;
		else if(ch < 0x800) {
			*(unsigned char*)d++ = 0xC0 | (ch >> 6);
			*(unsigned char*)d++ = 0x80 | (ch & 0x3F);
		} else if(ch < 0x10000) {
			*(unsigned char*)d++ = 0xE0 | (ch >> 12);
			*(unsigned char*)d++ = 0x80 | ((ch >> 6) & 0x3F);
			*(unsigned char*)d++ = 0x80 | (ch & 0x3F);
		} else if(ch < 0x110000) {
			*(unsigned char*)d++ = 0xF0 | (ch >> 18);
			*(unsigned char*)d++ = 0x80 | ((ch >> 12) & 0x3F);
			*(unsigned char*)d++ = 0x80 | ((ch >> 6) & 0x3F);
			*(unsigned char*)d++ = 0x80 | (ch & 0x3F);
		} else {
			*(unsigned char*)d++ = 0xEF;
			*(unsigned char*)d++ = 0xBF;
			*(unsigned char*)d++ = 0xBD;
		}
		--d;
		break;
	default: return 0;
	}
	*pp = p; *dp = ++d;
	return 1;
}

JSON_API char*
json_str(JsonTok *t, size_t *n) {
	char *d, *p, *e;

	*n = 0;
	d = p = t->start;
	e = t->end;

	if(p == e || t->type == JSON_NULL) return 0;

	while(p != e) {
		if(*p == '\\') {
			if(!json_decode(&p, &d, e)) return 0;
		} else {
			if(d != p) *d = *p;
			++d; ++p;
		}
	}

	*n = d - t->start;
	t->end = d;
	return t->start;
}

JSON_API char*
json_strdup(JsonTok *t) {
	size_t n;
	char *c = json_str(t, &n), *c2;
	if(!c) return 0;
	c2 = (char*)malloc(n + 1);
	if(!c) return 0;
	memcpy(c2, c, n);
	c2[n] = 0;
	return c2;
}

JSON_API int
json_bool(JsonTok *t) {
	int n = t->end - t->start;
	/* check for 4 in case this is actually a non-bool token */
	return n == 4 && t->start[0] == 't';
}

JSON_API int64_t
json_int(JsonTok *t) {
	char *p = t->start, *e = t->end;
	int64_t i = 0, s = 1;

	if(p != e) {
		if(*p == '+') ++p; /* sign */
		else if(*p == '-') { ++p; s = -1; } /* sign */
		for(;p != e && *p>='0' && *p<='9';++p) /* int */
			i = i * 10 + (*p - '0');
	}
	return i * s;
}

JSON_API double
json_float(JsonTok *t) {
	char *p = t->start, *e = t->end;
	int64_t i = 0, j=0, jj=1, k=0, s = 1, ks=1;
	double f = 0.0;
	if(p != e) {
		if(*p == 'n' || *p == 'N') return 0.0/0.0; /* nan (or null). not valid json but handle anyway */
		if(*p == '+') ++p; /* sign. not valid json but handle anyway */
		else if(*p == '-') {++p; s = -1; } /* sign */
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

JSON_API void
json_skip(Json *p, JsonTok *last) {
	int n;
	JsonTok t;

	if(last->type == JSON_OBJECT || last->type == JSON_ARRAY) {
		n = 1;
		while(json_next(p, &t)) {
			switch(t.type) {
			case JSON_OBJECT: case JSON_ARRAY: ++n; break;
			case JSON_OBJECT_END: case JSON_ARRAY_END: if(!--n) return; break;
			}
		}
	}
}

#if JSON_EXAMPLE
#include <stdio.h>
#include <string.h>
int main() {
	char json[] =
		"{\n"
		"\"a_key\":\"a value\",\n"
		"\"values\":\n"
		"  [\n"
		"      {\"stat\": -123.45e7,\n"
		"       \"flag\":false,\n"
		"       \"status\":\"on\\t-going\",\n"
		"       \"count\"  :   +49991 }  ,\n"
		"      {  \"stat\":null,\n"
		"       \"flag\":true,\n"
		"       \"status\":\"\\uD83D\\ude03 done\",\n"
		"       \"count\": -10 },\n"
		"      {\"stat\": nan   ,\n"
		"       \"flag\":true  ,\n"
		"       \"status\" :  \"what???\",\n"
		"       \"count\" : -200 },\n"
		"      {\"stat\": nan   ,\n"
		"       \"flag\":true  ,\n"
		"       \"status\" :  \"what???\",\n"
		"       \"count\" : -200 }\n"
		"  ]\n"
		"}";
	Json p;
	JsonTok k, v;
	struct Val { double stat; int flag; char *status; int count; };
	struct Val vals[100], val;
	int nvals = 0, i;

	printf("parsing:\n%s\n", json);

	json_init(&p, json, sizeof json - 1);
	while(json_next(&p, &v)) {
		/* always handle all possible object/array values.
		   primitives are automatically skipped but arrays and objects
		   must be manually skipped. The simplest rule is
		   always call json_skip() as part of every
		   if statement chain. It is safe to call whether
		   token is actually a composite type or not */
		if(v.type != JSON_OBJECT) json_skip(&p, &v);
		else while(json_object(&p, &k, &v)) {
			if(!JSON_EQ(&k, "values") || v.type != JSON_ARRAY)
				json_skip(&p, &v);
			else while(json_array(&p, &v)) {
				if(v.type != JSON_OBJECT) {
					json_skip(&p, &v);
					continue;
				}

				memset(&val, 0, sizeof val);
				while(json_object(&p, &k, &v)) {
					if(JSON_EQ(&k, "stat"))
						val.stat = json_float(&v);
					else if(JSON_EQ(&k, "flag"))
						val.flag = json_bool(&v);
					else if(JSON_EQ(&k, "status"))
						val.status = json_strdup(&v);
					else if(JSON_EQ(&k, "count")) {
						printf("count='%.*s'\n", (int)(v.end - v.start), v.start);
						val.count = (int)json_int(&v);
					}
					else json_skip(&p, &v); /* always call this
					function in if chains */
				}
				vals[nvals++] = val;
			}
		}
	}

	printf("parsed %d values\n", nvals);
	for(i=0;i<nvals;i++) {
		struct Val *v = &vals[i];
		printf("i=%d stat=%f flag=%s status=%s count=%d\n",
			i, v->stat, v->flag ? "true" : "false", v->status, v->count);
	}

	return 0;
}
#endif

#ifdef JSON_FUZZ
/* build and run with:
 	clang -x c -O3 -ggdb3 -fno-inline-functions -DJSON_FUZZ json2.h -fsanitize=address,undefined,fuzzer -lm
	./a.out -max_len=100000000
*/
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t n) {
	Json p;
	JsonTok t;
	char *s = malloc(n);
	assert(s);
	memcpy(s, data, n);
	json_init(&p, s, n);
	while(json_next(&p, &t)) {}
        free(s);
	return 0;
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
#endif

