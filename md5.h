#ifndef HAVE_MD5_H
#define HAVE_MD5_H

/* 
   From https://github.com/stangelandcl/lib1/md5.h
   License and example at end of file. Search MD5_EXAMPLE
   #define MD5_IMPLEMENTATION in one file unless
   MD5_STATIC is defined then it can be used in multiple files
   Example at end of file. Search for MD5_EXAMPLE
 * */

#if defined(MD5_STATIC) || defined(MD5_EXAMPLE)
#define MD5_API static
#define MD5_IMPLEMENTATION
#else
#define MD5_API extern
#endif

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MD5_BLOCK_SIZE 64
#define MD5_HASH_SIZE 16

typedef struct MD5 {
	uint8_t block[MD5_BLOCK_SIZE];
	uint8_t hash[MD5_HASH_SIZE];
	uint64_t size;
	uint16_t block_pos;
} MD5;

MD5_API void md5_init(MD5*);
MD5_API void md5_add(MD5*, const void *bytes, size_t count);
MD5_API void md5_finish(MD5*);
MD5_API size_t md5_hash(void *hash, size_t hash_size, const void *data, size_t size);

#ifdef __cplusplus
}
#endif

#endif

#ifdef MD5_IMPLEMENTATION

/* calculated
   for(i=1;i<=64;i++)
       md5_T[i] = (uint32_t)(4294967296.0 * abs(sin(i)));
*/
static uint32_t 
md5_rotl32(uint64_t x, size_t count)
{
	return (uint32_t)((x >> (32 - count)) | (x << count));
}

MD5_API void 
md5_init(MD5 *md5)
{
	uint32_t *i;

	memset(md5, 0, sizeof(*md5));
	/* TODO: fix endianness */
	i = (uint32_t*)md5->hash;
	i[0] = 0x67452301;
	i[1] = 0xefcdab89;
	i[2] = 0x98badcfe;
	i[3] = 0x10325476;
}
MD5_API void 
md5_add(MD5 *md5, const void *bytes, size_t count)
{
	int add, remain;
	const uint8_t *data = (const uint8_t*)bytes;
	static const uint32_t s[] = {
	    /* F */
	    0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501, /* 0 - 7 */
	    0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,0x6b901122,0xfd987193,0xa679438e,0x49b40821, /* 8 - 15 */
	    /* G */
	    0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x2441453,0xd8a1e681,0xe7d3fbc8, /* 16 - 23 */
	    0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a, /* 24 - 31 */
	    /* H */
	    0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70, /* 32 - 39 */
	    0x289b7ec6,0xeaa127fa,0xd4ef3085,0x4881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665, /* 40 - 47 */
	    /* I */
	    0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1, /* 48 - 55 */
	    0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391 /* 56 - 63 */
	};
	static const uint32_t rot[] = {
	    /* F */
	    7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22,
	    /* G */
	    5,9,14,20,5,9,14,20,5,9,14,20,5,9,14,20,
	    /* H */
	    4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23,
	    /* I */
	    6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21
	};

	while(count){
		uint32_t i, i4, t;
		uint32_t w[16];
		uint32_t a, b, c, d, *h, f, k;

		add = (int)count;
		remain = MD5_BLOCK_SIZE - md5->block_pos;
		if(add > remain) add = remain;
		memcpy(md5->block + md5->block_pos, data, (uint32_t)add);
		md5->block_pos = (uint16_t)(md5->block_pos + add);
		md5->size += (uint32_t)add;
		count -= (uint32_t)add;
		data += add;

		if(md5->block_pos != MD5_BLOCK_SIZE) break;

		h = (uint32_t*)md5->hash;

		for(i=i4=0;i<16;i++,i4+=4){
			uint8_t *m = md5->block;
			w[i] = (uint32_t)m[i4] |
			       ((uint32_t)m[i4+1] << 8) |
			       ((uint32_t)m[i4+2] << 16) |
			       ((uint32_t)m[i4+3] << 24);
		}

		a = h[0];
		b = h[1];
		c = h[2];
		d = h[3];

		for(i=0;i<64;i++) {
			if(i < 16) {
				f = (b & c) | (~b & d);
				k = i;
			} else if(i < 32) {
				f = (b & d) | (c & ~d);
				k = (5 * i + 1) & 15;
			} else if(i < 48) {
				f = b ^ c ^ d;
				k = (3 * i + 5) & 15;
			} else {
				f = c ^ (b | ~d);
				k = (7 * i) & 15;
			}		

			t = d;
			d = c;
			c = b;
			b += md5_rotl32(a + f + w[k] + s[i], rot[i]);
			a = t;	
		}

		h[0] += a;
		h[1] += b;
		h[2] += c;
		h[3] += d;
	}
}
/* hash value in md5->hash is now usable */
MD5_API void 
md5_finish(MD5 *md5)
{
	/* pad final message and append big endian length,
	   then process the final block

	   padding rules are add a 1 bit, add zeros,
	   add big endian length (128 bit for 384/512, 64 bit for others
	   to fill out a 1024 bit block for 384/512 or 512 bit block for others
	*/

	uint8_t x;
	uint64_t size;
	int remain; /* size of length in bytes */
	uint8_t b[8];

	size = md5->size; /* save original message size because adding
						 padding adds to size */
	remain = MD5_BLOCK_SIZE - md5->block_pos;

	x = (uint8_t)(1 << 7);
	md5_add(md5, &x, 1);
	if(--remain < 0) remain = MD5_BLOCK_SIZE;
	/* not enough room to encode length so pad with zeros to end of block */
	if(remain < 8 ) {
		memset(md5->block + md5->block_pos, 0, (uint32_t)remain);
		md5->block_pos = (uint16_t)(MD5_BLOCK_SIZE - 1);
		x = 0;
		md5_add(md5, &x, 1); /* force block process to run */
		remain = MD5_BLOCK_SIZE;
	}
	/* now at start of a block. pad with zeros until we are at the end
	   of the block + length */
	memset(md5->block + md5->block_pos, 0, (uint32_t)remain); /* could be remain - len instead */
	md5->block_pos = (uint16_t)(MD5_BLOCK_SIZE - sizeof(size));

	/* append big endian size in bits */
	size *= 8; /* we store length in bits */
	b[0] = (uint8_t)size;
	b[1] = (uint8_t)(size >> 8);
	b[2] = (uint8_t)(size >> 16);
	b[3] = (uint8_t)(size >> 24);
	b[4] = (uint8_t)(size >> 32);
	b[5] = (uint8_t)(size >> 40);
	b[6] = (uint8_t)(size >> 48);
	b[7] = (uint8_t)(size >> 56);
	md5_add(md5, b, sizeof(b));
}

/* copies min(hash_size, MD5_hash_size) bytes of hash into hash calculated from
   data and returns the number of bytes copied */
MD5_API size_t 
md5_hash(void *hash, size_t hash_size, const void *data, size_t size)
{
	MD5 md5;
	md5_init(&md5);
	md5_add(&md5, data, size);
	md5_finish(&md5);
	if(MD5_HASH_SIZE < hash_size) hash_size = MD5_HASH_SIZE;
	memcpy(hash, md5.hash, hash_size);
	return hash_size;
}

#ifdef MD5_EXAMPLE
#include <assert.h>
#include <stdio.h>
int main() {
	int i;
	uint8_t hash[MD5_HASH_SIZE];
	const char text[] = "The quick brown fox jumps over the lazy dog";
	assert(md5_hash(hash, sizeof hash, text, sizeof text - 1) == MD5_HASH_SIZE);
	printf("actual   ");
	for(i=0;i<sizeof hash;i++) printf("%02x", hash[i]);
	printf("\n");
	printf("expected 9e107d9d372bb6826bd81d3542a419d6\n");
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

