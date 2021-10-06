/* SPDX-License-Identifier: Unlicense */

#ifndef SB_H
#define SB_H

/* C string builder */

#include <stddef.h>

#if defined(SB_STATIC) || defined(SB_EXAMPLE)
#define SB_API static
#define SB_IMPLEMENTATION
#else
#define SB_API extern
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SB {
	char *str;
	size_t n, cap;
} SB;

SB_API void sb_init(SB *s);
SB_API int sb_reserve(SB *s, size_t n);
SB_API void sb_addn(SB *s, const char *text, size_t n);
SB_API void sb_adds(SB *s, const char *text);
SB_API void sb_add(SB *s, const char *format, ...);
SB_API void sb_clear(SB *s);
SB_API void sb_free(SB *s);


#ifdef __cplusplus
}
#endif

#endif

#ifdef SB_IMPLEMENTATION

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

SB_API void sb_init(SB *s) {
	s->str = (char*)"";
	s->n = s->cap = 0;
}

SB_API int
sb_reserve(SB *s, size_t n) {
	++n; /* for zero terminator */
	if(s->n + n > s->cap) {
		size_t cap = s->cap * 2;
		char *p;
		if(cap < 4096) cap = 4096;
		if(cap - s->n < n) cap = s->n + n;
		p = (char*)realloc(s->cap ? s->str : 0, cap);
		if(!p) return -1;
		s->cap = cap;
		s->str = p;
	}
	return 0;
}

SB_API void
sb_addn(SB *s, const char *text, size_t n) {
	if(sb_reserve(s, n)) return;
	memcpy(s->str + s->n, text, n);
	s->n += n;
	s->str[s->n] = 0;
}

SB_API void
sb_adds(SB *s, const char *text) {
	if(text) sb_addn(s, text, strlen(text));
}

SB_API void
sb_add(SB *s, const char *format, ...) {
	char buf[128];
	int n;
	va_list arg;
	char *p;

	va_start(arg, format);
	n = vsnprintf(buf, sizeof buf, format, arg);
	va_end(arg);

	if(n >= (int)sizeof buf) {
		p = (char*)malloc(n + 1);
		if(p) {
			va_start(arg, format);
			n = vsnprintf(p, n + 1, format, arg);
			va_end(arg);
			sb_addn(s, p, (unsigned)n);
			free(p);
		}
	} else sb_addn(s, buf, (unsigned)n);
}

SB_API void
sb_clear(SB *s) {
	s->n = 0;
	if(s->cap) s->str[0] = 0;
	else s->str = (char*)"";
}

SB_API void
sb_free(SB *s) {
	if(s->cap) free(s->str);
}
#endif

#ifdef SB_EXAMPLE
int main(int argc, char **argv) {
	SB sb;
	sb_init(&sb);
	sb_add(&sb, "%s %d\n", "testing", 1);
	printf("%s", sb.str);
	sb_free(&sb);
	return 0;
}
#endif

