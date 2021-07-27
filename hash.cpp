/* performance test of hash table */

#include <unordered_map>
#include "hash.h"
#define HASH_IMPLEMENTATION
#include "hash2.h"
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <time.h>
#define SIZE 1000000

static double now() {
	double d;
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	d = t.tv_sec;
	d += (double)t.tv_nsec / 1000000000.0;
	return d;
}

static size_t hashii_hash(int i) { return (unsigned)i; }
HASH_DECLARE(hashii, int, int, hashii_hash)
int main(int argc, char **argv) {
	int *keys = (int*)malloc(SIZE * sizeof(int));
	for(int i=0;i<SIZE;i++) while(!(keys[i] = rand())) {}
	for(int k=0;k<3;k++) {
		{
			double t = now();
			hashii hash = {0};
			int j;
			for(int i=0;i<SIZE;i++) hashii_put(&hash, keys[i], keys[i]);
			for(int i=0;i<SIZE;i++) {
				assert(hashii_get(&hash, keys[i], &j));
				assert(j == keys[i]);
			}
			for(int i=0;i<SIZE;i++) hashii_del(&hash, keys[i]);
			for(int i=0;i<SIZE;i++)
				assert(!hashii_get(&hash, keys[i], &j));

			hashii_destroy(&hash);
			printf("C=%f\n", now() - t);
		}
		{
			double t = now();
			Hash hash;
			hash_init(&hash, sizeof(int), sizeof(int), hash_i32, hash_equals_i32);

			for(int i=0;i<SIZE;i++) hash_put(&hash, &keys[i], &keys[i]);
			for(int i=0;i<SIZE;i++) {
				int j;
				assert(hash_get_copy(&hash, &keys[i], &j));
				assert(j == keys[i]);
			}
			for(int i=0;i<SIZE;i++) hash_del(&hash, &keys[i]);
			for(int i=0;i<SIZE;i++) {
				int j;
				assert(!hash_get_copy(&hash, &keys[i], &j));
			}
			printf("C2=%f\n", now() - t);
		}
		{
			double t = now();
			std::unordered_map<int, int> hash;

			for(int i=0;i<SIZE;i++) hash[keys[i]] = keys[i];
			for(int i=0;i<SIZE;i++) assert(hash[keys[i]] == keys[i]);
			for(int i=0;i<SIZE;i++) hash.erase(keys[i]);
			for(int i=0;i<SIZE;i++) assert(hash.find(keys[i]) == hash.end());
			printf("C++=%f\n", now() - t);
		}
	}
	return 0;
}

