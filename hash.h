#ifndef HASH_H
#define HASH_H

#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#define HASH_FOREACH(name, var, t) \
	for(name##_kvp *var=(t)->table;var != (t)->table+(t)->n;++var)

#define HASH_DECLARE(name, key, value, hash) \
	struct name##_kvp { key k; value v; };\
	typedef struct name { struct name##_kvp *table; size_t n_table, n; } name;\
	static char name##_zero[sizeof(key)];\
	static void name##_put(name *t, key k, value v);\
	static size_t name##_hashmod(name* t, key k) { return hash(k) & (t->n_table-1); }\
	static size_t name##_empty(key k) { return !memcmp(&k, name##_zero, sizeof(k)); }\
	static size_t name##_find(name* t, key k) { \
		size_t h = name##_hashmod(t, k);\
		while(!name##_empty(t->table[h].k) && memcmp(&t->table[h].k, &k, sizeof(key)))\
			if(++h == t->n_table) h = 0;\
		return h;\
	}\
	static void name##_grow(name *t) {\
		if(t->n >= t->n_table * 70 / 100) {\
			struct name x;\
			size_t i,n = t->n_table * 2;\
			if(!n) n = 1024;\
			x.table = (struct name##_kvp*)calloc(sizeof(struct name##_kvp), n);\
			if(!x.table) return;\
			x.n_table = n;x.n = 0;\
			for(i=0;i<t->n_table;i++) {\
				struct name##_kvp *kvp = &t->table[i]; \
				if(!name##_empty(kvp->k)) name##_put(&x, kvp->k, kvp->v);\
			}\
			free(t->table);\
			*t = x;\
		}\
	}\
	static void name##_put(name *t, key k, value v) {\
		size_t h;\
		name##_grow(t); \
		h = name##_hashmod(t, k);\
		for(;;) {\
			if(name##_empty(t->table[h].k)) { ++t->n; break; }\
			if(!memcmp(&t->table[h].k, &k, sizeof(key))) break;\
			if(++h == t->n_table) h = 0;\
		}\
		struct name##_kvp *kvp = &t->table[h];\
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
			if(++j == t->n_table) j = 0;\
			if(name##_empty(t->table[i].k)) break; \
			h = name##_hashmod(t, t->table[j].k);\
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
	}\
	static void name##_destroy(name *t) {\
		free(t->table); \
	}

#endif

#ifdef HASH_EXAMPLE
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#define SIZE 1000000
static size_t hashii_hash(int i) { return (unsigned)i; }
HASH_DECLARE(hashii, int, int, hashii_hash)

int main(int argc, char **argv) {
	hashii hash = {0};
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

	hashii_destroy(&hash);
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
