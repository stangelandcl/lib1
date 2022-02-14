#ifndef HASHG_H
#define HASHG_H

/* Generic hashmap and hashset. Faster than hash2.h.
 * Uses separate chaining but implemented in an array instead of separate allocations
 * for each one. Similar to .NET Dictionary implementation but without the prime number complexity */

#include <ctype.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HASHG_INT_EQUALS(x, y) ((x) == (y))
#define HASHG_INT_HASH(x) ((size_t)x)
#define HASHG_STRING_EQUALS(x, y) (!strcmp(x, y))
#define HASHG_STRING_HASH(x) hashg_string(x)
#define HASHG_STRING_LOWER_EQUALS(x, y) hashg_string_lower_equals(x, y)
#define HASHG_STRING_LOWER_HASH(x) hashg_string_lower(x)

#define HASHG_DECLARE(name, TKEY, TVALUE, HASH, EQUALS) \
typedef struct { \
	TKEY key; \
	TVALUE value; \
	size_t next;	\
} name##Entry;\
typedef struct { \
	size_t *buckets; \
	name##Entry *entries;\
	size_t capacity, n; \
} name; \
static void \
name##_init(name*h, size_t n) { \
	memset(h, 0, sizeof *h); \
	if(!n) n = 16; /* n cannot be zero otherwise errors happen on get before put */\
	n = (n * 128 + 89) / 90; \
	h->entries = (name##Entry*)calloc(n, sizeof(name##Entry)); \
	h->buckets = (size_t*)malloc(n * sizeof(size_t)); \
	memset(h->buckets, 255, n * sizeof(size_t)); \
	h->capacity = n; \
} \
static void \
name##_destroy(name*h) { \
	free(h->buckets); \
	free(h->entries); \
	memset(h, 0, sizeof *h); \
} \
static int name##_put(name*, TKEY, TVALUE); \
static void \
name##_grow(name*h, size_t n) { \
	size_t i; \
	name h2; \
	name##_init(&h2, n); \
	if(n) \
		for(i=0;i<h->n;i++) { \
			name##Entry *e = &h->entries[i]; \
			name##_put(&h2, e->key, e->value); \
		}\
	name##_destroy(h); \
	*h = h2; \
}\
static void \
name##_clear(name*h) { \
	if(h->buckets) memset(h->buckets, 255, h->capacity * sizeof(size_t)); \
	h->n = 0; \
}\
/* return 0 on already exists. 1 on added */ \
static int \
name##_put(name*h, TKEY key, TVALUE value) { \
	if(h->n >= h->capacity * 90 / 128) \
		name##_grow(h, h->capacity ? h->capacity * 2 : 65536); \
	size_t i = HASH(key) % h->capacity; \
	size_t idx = h->buckets[i]; \
	while(idx != SIZE_MAX) { \
		name##Entry *e = &h->entries[idx]; \
		/* key matches */ \
		if(EQUALS(e->key, key)) return 0; \
		idx = e->next; \
	} \
	name##Entry *kp = &h->entries[h->n]; \
	kp->key = key; \
	kp->value = value; \
	kp->next = h->buckets[i]; \
	h->buckets[i] = h->n++; \
	return 1; \
}\
/* return 0 on missing, 1 on found */ \
static TVALUE* \
name##_get(name*h, TKEY key) { \
	size_t i = HASH(key) % h->capacity; \
	size_t idx = h->buckets[i]; \
	while(idx != SIZE_MAX) { \
		name##Entry *e = &h->entries[idx]; \
		if(EQUALS(e->key, key)) return &e->value; \
		idx = e->next; \
	} \
	return 0; \
}

#define HASHSETG_DECLARE(name, TKEY, HASH, EQUALS) \
typedef struct { \
	TKEY key; \
	size_t next;	\
} name##Entry;\
typedef struct { \
	size_t *buckets; \
	name##Entry *entries;\
	size_t capacity, n; \
} name; \
static void \
name##_init(name*h, size_t n) { \
	memset(h, 0, sizeof *h); \
	if(!n) n = 16; /* n cannot be zero otherwise errors happen on get before put */\
	n = (n * 128 + 89) / 90; \
	h->entries = (name##Entry*)calloc(n, sizeof(name##Entry)); \
	h->buckets = (size_t*)malloc(n * sizeof(size_t)); \
	memset(h->buckets, 255, n * sizeof(size_t)); \
	h->capacity = n; \
} \
static void \
name##_destroy(name*h) { \
	free(h->buckets); \
	free(h->entries); \
	memset(h, 0, sizeof *h); \
} \
static int name##_put(name*, TKEY); \
static void \
name##_grow(name*h, size_t n) { \
	size_t i; \
	name h2; \
	name##_init(&h2, n); \
	if(n) \
		for(i=0;i<h->n;i++) { \
			name##Entry *e = &h->entries[i]; \
			name##_put(&h2, e->key); \
		}\
	name##_destroy(h); \
	*h = h2; \
}\
static void \
name##_clear(name*h) { \
	if(h->buckets) memset(h->buckets, 255, h->capacity * sizeof(size_t)); \
	h->n = 0; \
}\
/* return 0 on already exists. 1 on added */ \
static int \
name##_put(name*h, TKEY key) { \
	if(h->n >= h->capacity * 90 / 128) \
		name##_grow(h, h->capacity ? h->capacity * 2 : 65536); \
	size_t i = HASH(key) % h->capacity; \
	size_t idx = h->buckets[i]; \
	while(idx != SIZE_MAX) { \
		name##Entry *e = &h->entries[idx]; \
		/* key matches */ \
		if(EQUALS(e->key, key)) return 0; \
		idx = e->next; \
	} \
	name##Entry *kp = &h->entries[h->n]; \
	kp->key = key; \
	kp->next = h->buckets[i]; \
	h->buckets[i] = h->n++; \
	return 1; \
}\
/* return 0 on missing, 1 on found */ \
static int \
name##_contains(name*h, TKEY key) { \
	size_t i = HASH(key) % h->capacity; \
	size_t idx = h->buckets[i]; \
	while(idx != SIZE_MAX) { \
		name##Entry *e = &h->entries[idx]; \
		if(EQUALS(e->key, key)) return 1; \
		idx = e->next; \
	} \
	return 0; \
}

static size_t hashg_bytes(const uint8_t *bytes, size_t nbytes) {
	/* fnv1a64 */
    uint64_t hash = (uint64_t)14695981039346656037ULL;
    for(size_t i=0;i<nbytes;i++) {
        hash ^= bytes[i];
        hash *= 1099511628211;
    }
    return (size_t)hash;
}

static size_t hashg_string(const char *str) {
	/* fnv1a64 */
    uint64_t hash = (uint64_t)14695981039346656037ULL;
	if(!str) return hash;
    for(;*str;++str) {
        hash ^= (uint8_t)*str;
        hash *= 1099511628211;
    }
    return (size_t)hash;
}

static size_t hashg_string_lower(const char *str) {
	/* fnv1a64 */
    uint64_t hash = (uint64_t)14695981039346656037ULL;
	if(!str) return hash;
    for(;*str;++str) {
        hash ^= (uint8_t)tolower(*str);
        hash *= 1099511628211;
    }
    return (size_t)hash;
}

static int hashg_string_lower_equals(const char *x, const char *y) {
	if(!x) return y ? 0 : 1;
	if(!y) return 0;

	while(*x && *y) {
		int a = tolower(*x++), b = tolower(*y++);
		if(a != b) return 0;
	}
	return tolower(*x) == tolower(*y);
}

#ifdef __cplusplus
}
#endif

#endif

#ifdef HASHG_EXAMPLE
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/random.h>
#define SIZE 10*1000*1000

HASH_DECLARE(inthash, int, size_t, INT_HASH, INT_EQUALS)

static double now() {
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec + t.tv_nsec / 1000000000.0;
}

static void shuffle(int *ids, size_t n) {
    srand(time(0));
    for(size_t i = 0;i<n-2;i++) {
            size_t j = rand() % (n - i) + i;
            int tmp = ids[i];
            ids[i] = ids[j];
            ids[j] = tmp;
    }
}

static int* genkeys() {
	int *k = (int*)malloc(SIZE * sizeof(int));
	for(size_t i=0;i<SIZE;i++)
		k[i] = (int)i;
	shuffle(k, SIZE);
	return k;
}

int main(int argc, char **argv) {
	double t = now();
	int *k = genkeys();
	printf("generate in %f\n", now() - t);

	inthash hash;
	inthash_init(&hash, SIZE);

	t = now();
	for(size_t i=0;i<SIZE;i++) {
		int rc = inthash_put(&hash, k[i], k[i]);
		assert(rc);
	}
	printf("hash in %f\n", now() - t);
	printf("nhash=%zu\n", hash.n);

	shuffle(k, SIZE);

	t = now();
	for(size_t i=0;i<SIZE;i++) {
		size_t v;
		int rc = inthash_get(&hash, k[i], &v);
		assert(rc);
		assert(v == k[i]);
	}
	printf("hash find in %f\n", now() - t);
	printf("nhash=%zu\n", hash.n);

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
