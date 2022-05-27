/* SPDX-License-Identifier: Unlicense */
#ifndef FILE_H
#define FILE_H

#if defined(FILE_STATIC) || defined(FILE_EXAMPLE)
#define FILE_API static
#define FILE_IMPLEMENTATION
#else
#define FILE_API extern
#endif

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* free result */
FILE_API char *file_readtext(const char *filename);
/* returns 0 on success. <0 on error */
FILE_API int file_writetext(const char *filename, const char *text);
/* still over-allocates by one and zero terminates but n is the non-zero terminated length */
FILE_API char* file_readbytes(const char *filename, size_t *n);
FILE_API int file_writebytes(const char *filename, const void *bytes, size_t n);


#ifdef __cplusplus
}
#endif

#endif


#ifdef FILE_IMPLEMENTATION
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

FILE_API int
file_writetext(const char *filename, const char *text) {
	return file_writebytes(filename, text, text ? strlen(text) : 0);
	FILE *f = fopen(filename, "wb");
	if(!f) return -1;
	size_t n = strlen(text);
	int err = fwrite(text, 1, n, f) != n;
	fclose(f);
	return -err;
}

FILE_API int
file_writebytes(const char *filename, const void *bytes, size_t n) {
	FILE *f = fopen(filename, "wb");
	if(!f) return -1;
	int err = fwrite(bytes, 1, n, f) != n;
	fclose(f);
	return -err;
}

FILE_API char*
file_readbytes(const char *filename, size_t *n) {
	*n = 0;
	FILE *f = fopen(filename, "rb");
	if(!f) return 0;
	fseek(f, 0, SEEK_END);
#ifdef _WIN32
	*n = (size_t)_ftelli64(f);
#else
	*n = (size_t)ftell(f);
#endif
	rewind(f);
	char *buf = (char*)malloc(*n + 1);
	if(!buf) {
		fclose(f);
		return 0;
	}
	int ok = fread(buf, 1, *n, f) == *n;
	fclose(f);
	if(!ok) {
		free(buf);
		return 0;
	}
	buf[*n] = 0;
	return buf;
}

FILE_API char*
file_readtext(const char *filename) {
	size_t n;
	return file_readbytes(filename, &n);
}
#endif

#ifdef FILE_EXAMPLE
#include <assert.h>
int main(int argc, char **argv) {
	const char test[] = "test1";
	assert(!file_writetext("test.txt", test));
	char *t = file_readtext("test.txt");
	assert(t);
	assert(!strcmp(t, test));
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
