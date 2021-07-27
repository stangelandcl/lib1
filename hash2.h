#ifndef HASH2_H
#define HASH2_H

#if defined(HASH_STATIC) || defined(HASH_EXAMPLE)
#define HASH_API static
#define HASH_IMPLEMENTATION
#else
#define HASH_API extern
#endif

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef size_t (*hash_func)(const void *key, size_t n);
/* return 0 on not equal and non-0 on equals */
typedef int (*hash_equals)(const void *key1, const void *key2, size_t n);
typedef struct Hash {
	unsigned char *keys, *values, *set, *empty;
	hash_func hash;
	hash_equals equals;
	size_t n, n_table, n_key, n_value;
} Hash;

HASH_API void hash_init(Hash *h, size_t n_key, size_t n_value, hash_func hash, hash_equals equals);
HASH_API void* hash_get(Hash *h, const void *key);
HASH_API int hash_get_copy(Hash *h, const void *key, void *value);
HASH_API void hash_put(Hash *h, const void *key, const void *value);
HASH_API void hash_destroy(Hash *h);
HASH_API int hash_del(Hash *h, const void *key);
HASH_API void hash_destroy(Hash *h);
HASH_API size_t hash_string(const void *key, size_t n);
HASH_API int hash_equals_string(const void *a, const void *b, size_t n);
HASH_API size_t hash_i32(const void *k, size_t n);
HASH_API int hash_equals_i32(const void *k, const void *k2, size_t n);
HASH_API size_t hash_default(const void *key, size_t n);
HASH_API int hash_equals_default(const void *a, const void *b, size_t n);

#ifdef __cplusplus
}
#endif
#endif

#ifdef HASH_IMPLEMENTATION
/* FNV-1a */
HASH_API size_t hash_string(const void *key, size_t n) {
	unsigned long long hash = 14695981039346656037ULL;
	unsigned char *k = *(unsigned char**)key;
	while(*k) {
		hash ^= *k++;
		hash *= 1099511628211ULL;
	}
	return (size_t)hash;
}
HASH_API int hash_equals_string(const void *a, const void *b, size_t n) {
	return !strcmp(*(const char**)a, *(const char**)b);
}
HASH_API size_t hash_i32(const void *k, size_t n) {
	return *(unsigned*)k;
}

HASH_API int hash_equals_i32(const void *k, const void *k2, size_t n) {
	return *(int*)k == *(int*)k2;
}
HASH_API size_t hash_default(const void *key, size_t n) {
	size_t h = 0;
	memcpy(&h, key, n < sizeof h ? n : h);
	return h;
}
HASH_API int hash_equals_default(const void *a, const void *b, size_t n) {
	return !memcmp(a, b, n);
}

HASH_API void
hash_init(Hash *h, size_t n_key, size_t n_value, hash_func hash, hash_equals equals) {
	h->keys = h->values = h->set = 0;
	h->n = h->n_table = 0;
	h->hash = hash ? hash : hash_default;
	h->equals = equals ? equals : hash_equals_default;
	h->n_key = n_key;
	h->n_value = n_value;
}

/* returns 0 on success */
static int
hash_grow(Hash *h) {
	size_t i, n;
	Hash h2;

	hash_init(&h2, h->n_key, h->n_value, h->hash, h->equals);

	n = h->n_table * 2;
	if(!n) n = 64;

	h2.keys = (unsigned char*)malloc(n * h->n_key);
	h2.values = (unsigned char*)malloc(n * h->n_value);
	h2.set = (unsigned char*)calloc(1, (n + 7) / 8);
	h2.empty = (unsigned char*)calloc(1, h->n_key);

	if(!h2.keys || !h2.values || !h2.set ||!h2.empty) {
		hash_destroy(&h2);
		return -1;
	}
	h2.n_table = n;

	for(i=0;i<h->n_table;i++) {
		if(h->set[i/8] & (1<<(i % 8)))
			hash_put(&h2, &h->keys[i*h->n_key], &h->values[i*h->n_value]);
	}
	hash_destroy(h);
	*h = h2;
	return 0;
}

static size_t
hash_mod(Hash *h, const void *key) {
	return h->hash(key, h->n_key) & (h->n_table - 1);
}
HASH_API void
hash_put(Hash *h, const void *key, const void *value) {
	size_t i;
	if(h->n >= h->n_table * 70 / 100 && hash_grow(h)) return;
	i = hash_mod(h, key);
	for(;;) {
		if(!(h->set[i/8] & (1<<(i % 8)))) { ++h->n; break; }
		if(h->equals(key, &h->keys[i*h->n_key], h->n_key)) break;
		if(++i == h->n_table) i = 0;
	}
	memcpy(&h->keys[i*h->n_key], key, h->n_key);
	memcpy(&h->values[i*h->n_value], value, h->n_value);
	h->set[i/8] |= 1 << (i % 8);
}

HASH_API void*
hash_get(Hash *h, const void *key) {
	size_t i = hash_mod(h, key);
	for(;;) {
		if(!(h->set[i/8] & (1<<(i % 8)))) return 0;
		if(h->equals(key, &h->keys[i*h->n_key], h->n_key))
			return &h->values[i*h->n_value];
		if(++i == h->n_table) i = 0;
	}
}

static void*
hash_value(Hash *h, size_t i) {
	if(h->set[i/8] & (1<<(i%8))) return &h->values[i*h->n_value];
	return 0;
}
static void*
hash_key(Hash *h, size_t i) {
	if(h->set[i/8] & (1<<(i%8))) return &h->values[i*h->n_key];
	return 0;
}

HASH_API int
hash_get_copy(Hash *h, const void *key, void *value) {
	void *p = hash_get(h, key);
	if(p) {
		memcpy(value, p, h->n_value);
		return 1;
	}
	return 0;
}

HASH_API int
hash_del(Hash *h, const void *key) {
	size_t j,w,i = hash_mod(h, key);
	for(;;) {
		if(!(h->set[i/8] & (1<<(i % 8)))) return 0;
		if(h->equals(key, &h->keys[i*h->n_key], h->n_key)) break;
		if(++i == h->n_table) i = 0;
	}
	j=i;
	for(;;) {
		memset(&h->keys[i*h->n_key], 0, h->n_key);
		h->set[i/8] &= ~(1 << (i%8));
	next:
		if(++j == h->n_table) j = 0;
		if(!(h->set[j/8] & (1<<(j % 8)))) break;
		w = hash_mod(h, &h->keys[j*h->n_key]);
		/* check if w already in (i, j]. if so skip moving it */
		if((i<=j) ? (i<w && w<=j) : (i<w || w<=j)) goto next;
		memcpy(&h->keys[i*h->n_key], &h->keys[j*h->n_key], h->n_key);
		i = j;
	}
	return 1;
}

HASH_API void
hash_destroy(Hash *h) {
	free(h->keys);
	free(h->values);
	free(h->set);
	free(h->empty);
}

#endif

#ifdef HASH_EXAMPLE
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#define SIZE 1000000

int main(int argc, char **argv) {
	Hash hash;

	hash_init(&hash, sizeof(int), sizeof(int), hash_i32, hash_equals_i32);
	int j, found, *keys = (int*)malloc(SIZE * sizeof(int));
	for(int i=0;i<SIZE;i++) keys[i] = rand();
	for(int i=0;i<SIZE;i++) hash_put(&hash, &keys[i], &keys[i]);
	for(int i=0;i<SIZE;i++) {
		assert(hash_get_copy(&hash, &keys[i], &j));
		assert(j == keys[i]);
	}
	for(int i=0;i<SIZE;i++) hash_del(&hash, &keys[i]);
	for(int i=0;i<SIZE;i++)
		assert(!hash_get_copy(&hash, &keys[i], &j));

	found = 0;
	for(size_t i=0;i<hash.n;i++) {
		int *p = hash_value(&hash, i);
		if(p) ++found;
	}
	printf("found %d\n", found);
	hash_destroy(&hash);
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
