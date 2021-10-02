#ifndef FILE_H
#define FILE_H

#if defined(FILE_STATIC) || defined(FILE_EXAMPLE)
#define FILE_API static
#define FILE_IMPLEMENTATION
#else
#define FILE_API extern

#ifdef __cplusplus
extern "C" {
#endif

/* free result */
FILE_API char *file_readtext(const char *filename);
/* returns 0 on success. <0 on error */
FILE_API int file_writetext(const char *filename, const char *text);


#ifdef __cplusplus
}
#endif

#endif


#endif

#ifdef FILE_IMPLEMENTATION
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

FILE_API int
file_writetext(const char *filename, const char *text) {
	FILE *f = fopen(filename, "wb");
	if(!f) return -1;
	size_t n = strlen(text);
	int err = fwrite(text, 1, n, f) != n;
	fclose(f);
	return -err;
}

FILE_API char*
file_readtext(const char *filename) {
	FILE *f = fopen(filename, "rb");
	if(!f) return 0;
	size_t n;
	fseek(f, 0, SEEK_END);
#ifdef _WIN32
	n = (size_t)_ftelli64(f);
#else
	n = (size_t)ftell(f);
#endif
	rewind(f);
	char *buf = (char*)malloc(n + 1);
	if(!buf) {
		fclose(f);
		return 0;
	}
	int ok = fread(buf, 1, n, f) == n;
	fclose(f);
	if(!ok) {
		free(buf);
		return 0;
	}
	buf[n] = 0;
	return buf;
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
