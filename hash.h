#ifndef HASH_H
#define HASH_H

#include <string.h>

#define HASH_DECLARE(name, key, value, size, hash) \
	struct name##_kvp { key k; value v; };\
	typedef struct name { struct name##_kvp table[size]; } name;\
	static char name##_zero[sizeof(key)];\
	static size_t name##_hashmod(key k) { return hash(k) % size; }\
	static size_t name##_empty(key k) { return !memcmp(&k, name##_zero, sizeof(k)); }\
	static size_t name##_find(name* t, key k) { \
		size_t h = name##_hashmod(k);\
		while(!name##_empty(t->table[h].k) && memcmp(&t->table[h].k, &k, sizeof(key)))\
			if(++h == size) h = 0;\
		return h;\
	}\
	static void name##_put(name *t, key k, value v) {\
		struct name##_kvp *kvp = &t->table[name##_find(t, k)];\
		kvp->k = k; kvp->v = v;\
	}\
	/* delete algorithm from wikipedia open addressing */ \
	static void name##_del(name *t, key k) {\
		size_t i,j,h; \
		i = name##_find(t, k); \
		j = i;\
		for(;;) {\
			memset(&t->table[i].k, 0, sizeof(key));\
		next: \
			if(++j == size) j = 0;\
			if(name##_empty(t->table[i].k)) break; \
			h = name##_hashmod(t->table[j].k);\
			/* check if h in (i, j]. if so skip. it is in correct position */\
			if((i<=j) ? (i<h && h<=j) : (i<h || h<=j)) goto next;\
			t->table[i] = t->table[j];\
			i = j;\
		}\
	}\
	static int name##_get(name *t, key k, value *v) {\
		struct name##_kvp *kvp = &t->table[name##_find(t, k)];\
		*v = kvp->v;\
		return !name##_empty(kvp->k);\
	}

#endif

#ifdef HASH_EXAMPLE
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#define SIZE 1000000
static size_t hashii_hash(int i) { return (unsigned)i; }
HASH_DECLARE(hashii, int, int, SIZE, hashii_hash)
static hashii hash;

int main(int argc, char **argv) {
	int j;
	int *keys = (int*)malloc(SIZE * sizeof(int));
	for(int i=0;i<SIZE;i++) while(!(keys[i] = rand())) {}
	for(int i=0;i<SIZE / 2;i++) hashii_put(&hash, keys[i], keys[i]);
	for(int i=0;i<SIZE / 2;i++) {
		assert(hashii_get(&hash, keys[i], &j));
		assert(j == keys[i]);
	}
	for(int i=0;i<SIZE /2;i++) hashii_del(&hash, keys[i]);
	for(int i=0;i<SIZE / 2;i++)
		assert(!hashii_get(&hash, keys[i], &j));

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
