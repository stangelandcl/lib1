#ifndef JSON_H
#define JSON_H

/*
   From https://github.com/stangelandcl/lib1/json.h
   Zero memory overhead iterative json parser
   license and example at end of file. Search for JSON_EXAMPLE

   #define JSON_IMPLEMENTATION in one .C file before including json.h.
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
	char s[127]; /* stack of states for arrays/objects */
	unsigned char n; /* size size */
	char *p, *end;
} Json;

typedef struct JsonTok {
	char *start, *end;
	int type;
} JsonTok;

/* compare JsonTok tok to string literal and return 1 if
   tok is a string and matches the literal */
#define JSON_EQ(tok, literal) \
	((tok)->type == JSON_STRING && \
	 (((tok)->end - (tok)->start) == sizeof(literal) - 1) && \
	 !memcmp((tok)->start, literal, sizeof(literal)-1))

/* initialize parser with json text. json must be non-const.
   string escapes are rewritten in place */
JSON_API void json_init(Json *p, char *json, size_t n);
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
/* return 1 if token is object or array else 0. */
JSON_API int json_composite(JsonTok*);
/* skip to end of current object or array. This only works
   if the parser is at start of object or array,
   last token from json_next was JSON_OBJECT or JSON_ARRAY.
   Otherwise will mess up parsing and lead to invalid state.
   To skip arrays and objects and properly handle nesting
   always call this function as part of every "if" chain dealing
   with tokens. It is safe to call whether last JsonTok was
   actually a composite or not */
JSON_API int json_skip(Json*p);
/* iterate a key-value pair in a json object.
   return 1 on have key, value or 0 on end of object or error */
JSON_API int json_object(Json *p, JsonTok *k, JsonTok *v);
/* iterate array value until error, or end of json array.
   return 1 on have item. 0 on end of array or error */
JSON_API int json_array(Json *p, JsonTok *t);
/* returns text if parser is in error state or null if not */
JSON_API const char *json_error(Json *p);

#ifdef __cplusplus
}
#endif

#endif

#ifdef JSON_IMPLEMENTATION

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define JSON_ERROR(msg) json_make_error(p, msg, sizeof msg)
#define JSON_SUCCESS (p->s[p->n-1]=JSON_S_DONE,0)

enum {
	JSON_S_START,
	JSON_S_OBJECT,
	JSON_S_KEY,
	JSON_S_VALUE,
	JSON_S_NEXT,
	JSON_S_ARRAY,
	JSON_S_ELEMENT,
	JSON_S_ERROR,
	JSON_S_DONE
};

static int
json_make_error(Json *p, const char *msg, size_t n) {
	assert(p->n > 0);
	p->p = (char*)msg;
	p->end = p->p + n;
	p->s[p->n > 0 ? p->n - 1 : p->n++ ] = JSON_S_ERROR;
	return 0;
}


JSON_API const char*
json_error(Json *p) {
	assert(p->n > 0);
	assert(p->n <= sizeof p->s / sizeof p->s[0]);
	if(p->n <= 0) return "json.h: stack underflow";
	if(p->s[p->n-1] == JSON_S_ERROR) return p->p;
	return 0;
}

static void
json_white(Json *p) {
	assert(p->p <= p->end);
	while(p->p != p->end &&
	     (*p->p == ' ' || *p->p == '\t' || *p->p == '\n' || *p->p == '\r'))
	     ++p->p;
}

static int
json_unicode(Json *p) {
	int i, ch = 0;
	for(i=4;i!=0;i--,p->p++) {
		ch *= 16;
		if(*p->p >= 'a' && *p->p <= 'f') ch += 10 + *p->p - 'a';
		else if(*p->p >= 'A' && *p->p <= 'F') ch += 10 + *p->p - 'A';
		else if(*p->p >= '0' && *p->p <= '9') ch += *p->p - '0';
		else {
			JSON_ERROR("json.h: invalid unicode hex");
			return 0;
		};
	}
	if(!ch) JSON_ERROR("json.h: null char in string");
	return ch;
}

static int
json_escape(Json *p, char **offset) {
	int ch, w1, w2;
	char *d = *offset;
	if(++p->p == p->end)
		return JSON_ERROR("json.h: unterminated string");
	switch(*p->p++) {
	case '"': *d = '"'; break;
	case '\\': *d = '\\'; break;
	case 'n': *d = '\n'; break;
	case 't': *d = '\t'; break;
	case 'r': *d = '\r'; break;
	case '/': *d = '/'; break;
	case 'b': *d = 'b'; break;
	case 'f': *d = 'f'; break;
	case 'u':
		if(p->end - p->p < 4)
			return JSON_ERROR("json.h: invalid unicode escape");
		if(!(ch = json_unicode(p))) return 0;
		if(ch >= 0xD800 && ch <= 0xDBFF) {
			if(p->end - p->p < 6)
				return JSON_ERROR("json.h: unterminated surrogate pair");
			if(*p->p++ != '\\' || *p->p++ != 'u')
				return JSON_ERROR("json.h: missing surrogate pair");
			w1 = ch - 0xD800;
			if(!(ch = json_unicode(p))) return 0;
			if(ch < 0xDC00 || ch > 0xDFFF)
				return JSON_ERROR("json.h: invalid second surrogate pair");
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
		--p->p;
		--d;
		break;
	default: return JSON_ERROR("json.h: invalid escape");
	}
	++p->p;
	*offset = ++d;
	return 1;
}

static int
json_str(Json *p, JsonTok *t) {
	char *d;

	assert(p->p < p->end);
	t->start = d = ++p->p;
	while(p->p != p->end) {
		switch(*p->p) {
		case '\\':
			if(!json_escape(p, &d)) return 0;
			if(p->p == p->end) goto done;
			break;
		case '"': goto done;
		default:
			if(d != p->p) *d = *p->p;
			d++; p->p++;
			break;
		}
	}
done:
	if(p->p == p->end) return JSON_ERROR("json.h: unterminated string");
	++p->p;
	t->end = d;
	t->type = JSON_STRING;
	assert(p->p <= p->end);
	return 1;
}

/* add item char to stack. retrun 1 on success.
   set to error and return 0 on out of stack space
   or invalid state char */
static int
json_push(Json *p, char c) {
	assert(p->n > 0);
	if(c > JSON_S_START && c <= JSON_S_DONE &&
	   p->n < sizeof p->s / sizeof p->s[0]) {
			p->s[p->n++] = c;
			return 1;
	}
	p->s[p->n-1] = JSON_S_ERROR;
	return JSON_ERROR("json.h: stack overflow");
}

static int
json_null(Json *p, JsonTok *t, const char *text, int n) {
	assert(n == 4 || n == 5);
	assert(*p->p == 'n' || *p->p == 't' || *p->p == 'f');
	if(p->end - p->p < n || memcmp(text, p->p, n))
		return JSON_ERROR("json.h: expected null, true, or false");
	t->start = p->p;
	p->p += n;
	t->end = p->p;
	t->type = *text == 'n' ? JSON_NULL : JSON_BOOL;
	assert(p->p <= p->end);
	return 1;
}

static int
json_any(Json *p, JsonTok *t) {
	switch(*p->p) {
	case '{':
		if(!json_push(p, JSON_S_OBJECT)) return 0;
		t->start = t->end = 0;
		t->type = JSON_OBJECT;
		++p->p;
		assert(p->p <= p->end);
		return 1;
	case '[':
		if(!json_push(p, JSON_S_ARRAY)) return 0;
		t->start = t->end = 0;
		t->type = JSON_ARRAY;
		++p->p;
		assert(p->p <= p->end);
		return 1;
	case '-':case '0':case '1':case '2':case '3':case '4':
	case '5':case '6':case '7':case '8':case '9':
		t->start = p->p;
		if(*p->p == '-') ++p->p;
		while(p->p != p->end && *p->p >= '0' && *p->p <= '9') ++p->p;
		if(p->p != p->end && *p->p == '.') {
			++p->p;
			while(p->p != p->end && *p->p >= '0' && *p->p <= '9') ++p->p;
		}
		if(p->p != p->end && (*p->p == 'e' || *p->p == 'E')) {
			++p->p;
			if(p->p != p->end && (*p->p == '-' || *p->p == '+')) ++p->p;
			while(p->p != p->end && *p->p >= '0' && *p->p <= '9') ++p->p;
		}

		t->end = p->p;
		t->type = JSON_NUMBER;
		assert(p->p <= p->end);
		return 1;
	case '"': return json_str(p, t);
	case 'n':
		t->start = p->p;
		p->p += 4;
		if(p->p > p->end) return JSON_ERROR("json.h: null");
		t->end = p->p;
		t->type = JSON_NULL;
		return 1;
	case 't':
		t->start = p->p;
		p->p += 4;
		if(p->p > p->end) return JSON_ERROR("json.h: expected true");
		t->end = p->p;
		t->type = JSON_BOOL;
		return 1;
	case 'f':
		t->start = p->p;
		p->p += 5;
		if(p->p > p->end) return JSON_ERROR("json.h: expected false");
		t->end = p->p;
		t->type = JSON_BOOL;
		return 1;
	default:
		return JSON_ERROR("json.h: invalid starting character");
	}
	return 0;
}

JSON_API int
json_next(Json *p, JsonTok *t) {
	assert(p->p <= p->end);
	assert(p->n > 0);
	assert(p->n <= sizeof p->s / sizeof p->s[0]);

	json_white(p);
	if(p->p >= p->end) return JSON_SUCCESS;
	switch(p->s[p->n-1]) {
	case JSON_S_START:  return json_any(p, t);
	case JSON_S_OBJECT:
		assert(p->p <= p->end);
		if(*p->p == '}') {
			++p->p;
			--p->n;
			t->type = JSON_OBJECT_END;
			return 1;
		}
		/* fall through */
	case JSON_S_KEY:
key:
		if(*p->p != '"')
			return JSON_ERROR("json.h: only string keys allowed");
		t->start = ++p->p;
		while(p->p < p->end && *p->p != '"') {
			if(*p->p == '\\') ++p->p;
			++p->p;
		}
		t->end = p->p++;
		if(p->p >= p->end) return JSON_ERROR("json.h: invalid string key");
		t->type = JSON_STRING;
		//if(!json_str(p, t)) return 0;
		assert(p->n > 0);
		assert(p->n <= sizeof p->s / sizeof p->s[0]);
		if(!json_push(p, JSON_S_VALUE)) return 0;
		assert(p->p <= p->end);
		return 1;
	case JSON_S_VALUE:
		if(*p->p != ':')
			return JSON_ERROR("json.h: missing ':' separator");
		++p->p;
		p->s[p->n-1] = JSON_S_NEXT;
		json_white(p);
		if(p->p == p->end) return JSON_ERROR("json.h: missing key-value pair value");
		return json_any(p, t);
	case JSON_S_NEXT:
		--p->n;
		if(*p->p == ',') {
			++p->p;
			json_white(p);
			if(p->p == p->end)
				return JSON_ERROR("json.h: unterminated object");
			goto key;
		}
		if(*p->p == '}') {
			++p->p;
			--p->n;
			t->type = JSON_OBJECT_END;
			return 1;
		}
		return JSON_ERROR("json.h: invalid next object value");
	case JSON_S_ARRAY:
		p->s[p->n-1] = JSON_S_ELEMENT;
		return json_any(p, t);
	case JSON_S_ELEMENT:
		if(*p->p == ']') {
			++p->p;
			--p->n;
			t->type = JSON_ARRAY_END;
			return 1;
		}
		if(*p->p != ',')
			return JSON_ERROR("json.h: invalid next array value");
		++p->p;
		json_white(p);
		if(p->p == p->end) return JSON_ERROR("json.h: missing next array value");
		return json_any(p, t);
	case JSON_S_ERROR: return 0; /* error state */
	case JSON_S_DONE: return 0; /* completed successfully state */
	default:
		assert(0);
		return JSON_ERROR("json.h: invalid state");
	}
	return 0;
}

JSON_API void
json_init(Json *p, char *json, size_t n) {
	p->s[0] = JSON_S_START;
	p->n = 1;
	p->p = json;
	p->end = json + n;
}

JSON_API char*
json_strdup(JsonTok *t) {
	char *c;
	size_t n;
	if(t->type != JSON_STRING) return 0;
	n = t->end - t->start;
	c = (char*)malloc(n + 1);
	if(!c) return 0;
	memcpy(c, t->start, n);
	c[n] = 0;
	return c;
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
		if(*p == '-') { ++p; s = -1; } /* sign */
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

JSON_API int
json_composite(JsonTok *t) {
	return t->type == JSON_OBJECT || t->type == JSON_ARRAY;
}

JSON_API int
json_skip(Json *p) {
	int n = p->n, rc;
	JsonTok t;
	if(p->s[p->n-1] != JSON_S_OBJECT && p->s[p->n-1] != JSON_S_ARRAY) return 0;
	while(p->n >= n && ((rc = json_next(p, &t))));
	assert(!rc || t.type == JSON_OBJECT_END || t.type == JSON_ARRAY_END);
	return rc;
}

JSON_API int
json_object(Json *p, JsonTok *k, JsonTok *v) {
	if(!json_next(p, k) || k->type == JSON_OBJECT_END) return 0;
	return json_next(p, v);
}

JSON_API int
json_array(Json *p, JsonTok *t) {
	return json_next(p, t) && t->type != JSON_ARRAY_END;
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
		"       \"count\":49991 },\n"
		"      {\"stat\": null,\n"
		"       \"flag\":true,\n"
		"       \"status\":\"\\uD83D\\ude03 done\",\n"
		"       \"count\": -10 }\n"
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
		if(v.type != JSON_OBJECT) json_skip(&p);
		else while(json_object(&p, &k, &v)) {
			if(!JSON_EQ(&k, "values") || v.type != JSON_ARRAY)
				json_skip(&p);
			else while(json_array(&p, &v)) {
				if(v.type != JSON_OBJECT) {
					json_skip(&p);
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
					else if(JSON_EQ(&k, "count"))
						val.count = (int)json_int(&v);
					else json_skip(&p); /* always call this
					function in if chains */
				}
				vals[nvals++] = val;
			}
		}
	}

	if(json_error(&p)) {
		printf("Error parsing: %s\n", json_error(&p));
		return -1;
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
#endif

