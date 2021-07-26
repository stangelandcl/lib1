/* performance test of hash table */

#include <unordered_map>
#include "hash.h"
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
			for(int i=0;i<SIZE / 2;i++) hashii_put(&hash, keys[i], keys[i]);
			for(int i=0;i<SIZE / 2;i++) {
				assert(hashii_get(&hash, keys[i], &j));
				assert(j == keys[i]);
			}
			for(int i=0;i<SIZE /2;i++) hashii_del(&hash, keys[i]);
			for(int i=0;i<SIZE / 2;i++)
				assert(!hashii_get(&hash, keys[i], &j));

			hashii_destroy(&hash);
			printf("C=%f\n", now() - t);
		}
		{
			double t = now();
			std::unordered_map<int, int> hash;

			for(int i=0;i<SIZE;i++) while(!(keys[i] = rand())) {}
			for(int i=0;i<SIZE / 2;i++) hash[keys[i]] = keys[i];
			for(int i=0;i<SIZE / 2;i++) assert(hash[keys[i]] == keys[i]);
			for(int i=0;i<SIZE /2;i++) hash.erase(keys[i]);
			for(int i=0;i<SIZE / 2;i++) assert(hash.find(keys[i]) == hash.end());
			printf("C++=%f\n", now() - t);
		}
	}
	return 0;
}

