#ifndef SHA_H
#define SHA_H

/*
 * #define SHA_IMPLEMENTATION in one file before including or
 * #define SHA_STATIC before each include
 * example at bottom of file
 * public domain license at end of file
 */


#include <stddef.h>
#include <stdint.h>

#if defined(SHA_STATIC) || defined(SHA_EXAMPLE)
#define SHA_API static
#define SHA_IMPLEMENTATION
#else
#define SHA_API extern
#endif

#define SHA1_BLOCK_SIZE 64
#define SHA224_BLOCK_SIZE 64
#define SHA256_BLOCK_SIZE 64
#define SHA384_BLOCK_SIZE 128
#define SHA512_BLOCK_SIZE 128
#define SHA_MAX_BLOCK_SIZE SHA512_BLOCK_SIZE

#define SHA1_HASH_SIZE 20
#define SHA224_HASH_SIZE 28
#define SHA256_HASH_SIZE 32
#define SHA384_HASH_SIZE 48
#define SHA512_HASH_SIZE 64
#define SHA_MAX_HASH_SIZE SHA512_HASH_SIZE

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t ShaType;
#define SHA1 2
#define SHA224 3
#define SHA256 4
#define SHA384 5
#define SHA512 6

typedef struct {
        uint8_t block[SHA_MAX_BLOCK_SIZE];
		uint8_t sha[SHA_MAX_HASH_SIZE];
        /* actually sha 384/512 can handle 128 bytes of length but we don't
           need that yet so not implemented */
        uint64_t size;
        uint16_t block_size;
        uint16_t block_pos;
		uint8_t nhash;
        ShaType type;
} Sha;

SHA_API int sha_hash(ShaType type, void *hash, int hash_size,
	const void *data, int size);
SHA_API void sha_init(Sha *ctx, ShaType type);
SHA_API void sha_add(Sha *ctx, const void *bytes, size_t count);
SHA_API void sha_finish(Sha *ctx);

#ifdef __cplusplus
}
#endif

#endif

#ifdef SHA_IMPLEMENTATION
#include <assert.h>
#include <string.h>

/* calculated
   for(i=1;i<=64;i++)
       md5_T[i] = (uint32_t)(4294967296.0 * abs(sin(i)));
*/
static uint32_t
sha_swap_u32(uint32_t x) {
#if defined(__GNUC__)
        return __builtin_bswap32(x);
#else
        uint8_t *p = (uint8_t*)&x, t;
        t = p[0]; p[0] = p[3]; p[3] = t;
        t = p[1]; p[1] = p[2]; p[2] = t;
        return x;
#endif
}
static uint64_t
sha_swap_u64(uint64_t x) {
#if defined(__GNUC__)
        return __builtin_bswap64(x);
#else
        uint8_t *p = (uint8_t*)&x, t;
        t = p[0]; p[0] = p[7]; p[7] = t;
        t = p[1]; p[1] = p[6]; p[6] = t;
        t = p[2]; p[2] = p[5]; p[5] = t;
        t = p[3]; p[3] = p[4]; p[4] = t;
        return x;
#endif
}
static uint64_t
sha_rotr64(uint64_t x, size_t count) {
        return (x << (64 - count)) | (x >> count);
}
static uint32_t
sha_rotr32(uint32_t x, size_t count) {
        return (uint32_t)((x << (32 - count)) | (x >> count));
}
static uint32_t
sha_rotl32(uint64_t x, size_t count) {
        return (uint32_t)((x >> (32 - count)) | (x << count));
}
static uint32_t
sha_ch32(uint32_t x, uint32_t y, uint32_t z) {
        /* return (x & y) ^ (~x & z); */
        return (x & (y ^ z)) ^ z;
}
static uint64_t
sha_ch64(uint64_t x, uint64_t y, uint64_t z) {
        return (x & (y ^ z)) ^ z;
}
static uint32_t
sha_maj32(uint32_t x, uint32_t y, uint32_t z) {
        /* return (x & y) ^ (x & z) ^ (y & z); */
        return (x & (y | z)) | (y & z);
}
static uint64_t
sha_maj64(uint64_t x, uint64_t y, uint64_t z) {
        return (x & (y | z)) | (y & z);
}
static uint32_t
sha_parity(uint32_t x, uint32_t y, uint32_t z) {
        return x ^ y ^ z;
}
static uint32_t
sha256_sigma0(uint32_t x) {
        return sha_rotr32(x, 7) ^ sha_rotr32(x, 18) ^ (x >> 3);
}
static uint32_t
sha256_sigma1(uint32_t x) {
        return sha_rotr32(x, 17) ^ sha_rotr32(x, 19) ^ (x >> 10);
}
static uint32_t
sha256_sigma2(uint32_t x) {
        return sha_rotr32(x, 2) ^ sha_rotr32(x, 13) ^ sha_rotr32(x, 22);
}
static uint32_t
sha256_sigma3(uint32_t x) {
        return sha_rotr32(x, 6) ^ sha_rotr32(x, 11) ^ sha_rotr32(x, 25);
}
static uint64_t
sha512_sigma0(uint64_t x) {
        return sha_rotr64(x, 1) ^ sha_rotr64(x, 8) ^ (x >> 7);
}
static uint64_t
sha512_sigma1(uint64_t x) {
        return sha_rotr64(x, 19) ^ sha_rotr64(x, 61) ^ (x >> 6);
}
static uint64_t
sha512_sigma2(uint64_t x) {
        return sha_rotr64(x, 28) ^ sha_rotr64(x, 34) ^ sha_rotr64(x, 39);
}
static uint64_t
sha512_sigma3(uint64_t x) {
        return sha_rotr64(x, 14) ^ sha_rotr64(x, 18) ^ sha_rotr64(x, 41);
}
SHA_API void
sha_init(Sha *ctx, ShaType type) {
        uint32_t *i;

        memset(ctx, 0, sizeof(*ctx));
        ctx->type = type;
        /* TODO: fix endianness */
		i = (uint32_t*)ctx->sha;

        switch(type) {
        case SHA1:
                ctx->block_size = SHA1_BLOCK_SIZE;
				ctx->nhash = SHA1_HASH_SIZE;
                i[0] = 0x67452301;
                i[1] = 0xEFCDAB89;
                i[2] = 0x98BADCFE;
                i[3] = 0x10325476;
                i[4] = 0xC3D2E1F0;
                break;
        case SHA224:
                ctx->block_size = SHA224_BLOCK_SIZE;
				ctx->nhash = SHA224_HASH_SIZE;
                i[0] = 0xC1059ED8;
                i[1] = 0x367CD507;
                i[2] = 0x3070DD17;
                i[3] = 0xF70E5939;
                i[4] = 0xFFC00B31;
                i[5] = 0x68581511;
                i[6] = 0x64F98FA7;
                i[7] = 0xBEFA4FA4;
                break;
        case SHA256:
                ctx->block_size = SHA256_BLOCK_SIZE;
				ctx->nhash = SHA256_HASH_SIZE;
                i[0] = 0x6A09E667;
                i[1] = 0xBB67AE85;
                i[2] = 0x3C6EF372;
                i[3] = 0xA54FF53A;
                i[4] = 0x510E527F;
                i[5] = 0x9B05688C;
                i[6] = 0x1F83D9AB;
                i[7] = 0x5BE0CD19;
                break;
        case SHA384:
                ctx->block_size = SHA384_BLOCK_SIZE;
				ctx->nhash = SHA384_HASH_SIZE;
                i[1] = 0xCBBB9D5D;
                i[0] = 0xC1059ED8;
                i[3] = 0x629A292A;
                i[2] = 0x367CD507;
                i[5] = 0x9159015A;
                i[4] = 0x3070DD17;
                i[7] = 0x152FECD8;
                i[6] = 0xF70E5939;
                i[9] = 0x67332667;
                i[8] = 0xFFC00B31;
                i[11] = 0x8EB44A87;
                i[10] = 0x68581511;
                i[13] = 0xDB0C2E0D;
                i[12] = 0x64F98FA7;
                i[15] = 0x47B5481D;
                i[14] = 0xBEFA4FA4;
                break;
        case SHA512:
                ctx->block_size = SHA512_BLOCK_SIZE;
				ctx->nhash = SHA512_HASH_SIZE;
                i[1] = 0x6A09E667;
                i[0] = 0xF3BCC908;
                i[3] = 0xBB67AE85;
                i[2] = 0x84CAA73B;
                i[5] = 0x3C6EF372;
                i[4] = 0xFE94F82B;
                i[7] = 0xA54FF53A;
                i[6] = 0x5F1D36F1;
                i[9] = 0x510E527F;
                i[8] = 0xADE682D1;
                i[11] = 0x9B05688C;
                i[10] = 0x2B3E6C1F;
                i[13] = 0x1F83D9AB;
                i[12] = 0xFB41BD6B;
                i[15] = 0x5BE0CD19;
                i[14] = 0x137E2179;
                break;
        }
}
static void
sha_swap_hash(Sha *ctx){
        size_t i;
		uint32_t *h = (uint32_t*)ctx->sha;
		for(i=0;i<ctx->nhash/4;i++) h[i] = sha_swap_u32(h[i]);
}


SHA_API void
sha_add(Sha *ctx, const void *bytes, size_t count) {
        static const uint64_t sha512_K[] = {
                0x428A2F98D728AE22ll, 0x7137449123EF65CDll, 0xB5C0FBCFEC4D3B2Fll,
                0xE9B5DBA58189DBBCll, 0x3956C25BF348B538ll, 0x59F111F1B605D019ll,
                0x923F82A4AF194F9Bll, 0xAB1C5ED5DA6D8118ll, 0xD807AA98A3030242ll,
                0x12835B0145706FBEll, 0x243185BE4EE4B28Cll, 0x550C7DC3D5FFB4E2ll,
                0x72BE5D74F27B896Fll, 0x80DEB1FE3B1696B1ll, 0x9BDC06A725C71235ll,
                0xC19BF174CF692694ll, 0xE49B69C19EF14AD2ll, 0xEFBE4786384F25E3ll,
                0x0FC19DC68B8CD5B5ll, 0x240CA1CC77AC9C65ll, 0x2DE92C6F592B0275ll,
                0x4A7484AA6EA6E483ll, 0x5CB0A9DCBD41FBD4ll, 0x76F988DA831153B5ll,
                0x983E5152EE66DFABll, 0xA831C66D2DB43210ll, 0xB00327C898FB213Fll,
                0xBF597FC7BEEF0EE4ll, 0xC6E00BF33DA88FC2ll, 0xD5A79147930AA725ll,
                0x06CA6351E003826Fll, 0x142929670A0E6E70ll, 0x27B70A8546D22FFCll,
                0x2E1B21385C26C926ll, 0x4D2C6DFC5AC42AEDll, 0x53380D139D95B3DFll,
                0x650A73548BAF63DEll, 0x766A0ABB3C77B2A8ll, 0x81C2C92E47EDAEE6ll,
                0x92722C851482353Bll, 0xA2BFE8A14CF10364ll, 0xA81A664BBC423001ll,
                0xC24B8B70D0F89791ll, 0xC76C51A30654BE30ll, 0xD192E819D6EF5218ll,
                0xD69906245565A910ll, 0xF40E35855771202All, 0x106AA07032BBD1B8ll,
                0x19A4C116B8D2D0C8ll, 0x1E376C085141AB53ll, 0x2748774CDF8EEB99ll,
                0x34B0BCB5E19B48A8ll, 0x391C0CB3C5C95A63ll, 0x4ED8AA4AE3418ACBll,
                0x5B9CCA4F7763E373ll, 0x682E6FF3D6B2B8A3ll, 0x748F82EE5DEFB2FCll,
                0x78A5636F43172F60ll, 0x84C87814A1F0AB72ll, 0x8CC702081A6439ECll,
                0x90BEFFFA23631E28ll, 0xA4506CEBDE82BDE9ll, 0xBEF9A3F7B2C67915ll,
                0xC67178F2E372532Bll, 0xCA273ECEEA26619Cll, 0xD186B8C721C0C207ll,
                0xEADA7DD6CDE0EB1Ell, 0xF57D4F7FEE6ED178ll, 0x06F067AA72176FBAll,
                0x0A637DC5A2C898A6ll, 0x113F9804BEF90DAEll, 0x1B710B35131C471Bll,
                0x28DB77F523047D84ll, 0x32CAAB7B40C72493ll, 0x3C9EBE0A15C9BEBCll,
                0x431D67C49C100D4Cll, 0x4CC5D4BECB3E42B6ll, 0x597F299CFC657E2All,
                0x5FCB6FAB3AD6FAECll, 0x6C44198C4A475817ll
        };
        static const uint32_t sha256_K[] = {
                0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b,
                0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01,
                0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7,
                0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
                0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152,
                0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
                0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc,
                0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
                0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819,
                0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08,
                0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f,
                0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
                0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
        };
        static const uint32_t sha1_K[] = {
                0x5A827999, 0x6ED9EBA1, 0x8F1BBCDC, 0xCA62C1D6
        };
        int add, remain;
        const uint8_t *data = (const uint8_t*)bytes;

        while(count){
                add = (int)count;
                remain = ctx->block_size - ctx->block_pos;
                if(add > remain) add = remain;
                assert(add > 0);
                memcpy(ctx->block + ctx->block_pos, data, (uint32_t)add);
                ctx->block_pos = (uint16_t)(ctx->block_pos + add);
                ctx->size += (uint32_t)add;
                count -= (uint32_t)add;
                data += add;

                if(ctx->block_pos != ctx->block_size) break;

                switch(ctx->type) {
                default: /* invalid type */ return;
                case SHA1:
                {
                        int i, i4;
                        uint32_t tmp;
                        uint32_t w[80];
                        uint32_t a, b, c, d, e;
                        uint8_t *m = ctx->block;
						uint32_t *h = (uint32_t*)ctx->sha;

                        for(i=i4=0;i<16;i++,i4+=4) {
                                w[i] = ((uint32_t)m[i4] << 24) |
                                       ((uint32_t)m[i4+1] << 16) |
                                       ((uint32_t)m[i4+2] << 8) |
                                        (uint32_t)m[i4+3];
                        }

                        for(i=16;i<80;i++) {
                                tmp = w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16];
                                w[i] = sha_rotl32(tmp, 1);
                        }
                        a = h[0];
                        b = h[1];
                        c = h[2];
                        d = h[3];
                        e = h[4];

                        for(i=0;i<80;i++) {
                                tmp = sha_rotl32(a, 5) + e + w[i] + sha1_K[i/20];
                                if(i < 20) tmp += sha_ch32(b, c, d);
                                else if(i < 40) tmp += sha_parity(b, c, d);
                                else if(i < 60) tmp += sha_maj32(b, c, d);
                                else if(i < 80) tmp += sha_parity(b, c, d);

                                e = d;
                                d = c;
                                c = sha_rotl32(b, 30);
                                b = a;
                                a = tmp;
                        }

                        h[0] += a;
                        h[1] += b;
                        h[2] += c;
                        h[3] += d;
                        h[4] += e;

                        ctx->block_pos = 0;
                        break;
                }
                case SHA224:
                case SHA256:
                {
                        int i, i4;
                        uint32_t tmp1, tmp2;
                        uint32_t w[64];
                        uint32_t a, b, c, d, e, f, g, h, s0, s1;
                        uint8_t *m = ctx->block;
						uint32_t *hash = (uint32_t*)ctx->sha;

                        for(i=i4=0;i<16;i++,i4+=4) {
                                w[i] = ((uint32_t)m[i4] << 24) |
                                       ((uint32_t)m[i4+1] << 16) |
                                       ((uint32_t)m[i4+2] << 8) |
                                        m[i4+3];
                        }

                        for(i=16;i<64;i++) {
                                s0 = sha256_sigma0(w[i-15]);
                                s1 = sha256_sigma1(w[i-2]);

                                w[i] = w[i-7] + w[i-16] + s0 + s1;
                        }

                        a = hash[0];
                        b = hash[1];
                        c = hash[2];
                        d = hash[3];
                        e = hash[4];
                        f = hash[5];
                        g = hash[6];
                        h = hash[7];


                        for(i=0;i<64;i++) {
                                tmp1 = h + sha256_sigma3(e) + sha_ch32(e,f,g) +
                                    sha256_K[i] + w[i];
                                tmp2 = sha256_sigma2(a) + sha_maj32(a, b, c);
                                h = g;
                                g = f;
                                f = e;
                                e = d + tmp1;
                                d = c;
                                c = b;
                                b = a;
                                a = tmp1 + tmp2;
                        }

                        hash[0] += a;
                        hash[1] += b;
                        hash[2] += c;
                        hash[3] += d;
                        hash[4] += e;
                        hash[5] += f;
                        hash[6] += g;
                        hash[7] += h;
                        ctx->block_pos = 0;
                        break;
                }
                case SHA384:
                case SHA512:
                {
                        int i, i8; /* loop counter */
                        uint64_t tmp1, tmp2;
                        uint64_t w[80];
                        uint64_t a, b, c, d, e, f, g, h; /* buffers */
                        const uint64_t *k = (const uint64_t*)sha512_K;
                        uint8_t *m = ctx->block;
						uint64_t *hash = (uint64_t*)ctx->sha;

                        for(i=i8=0;i<16;i++,i8+=8) {
                                w[i] = ((uint64_t)m[i8] << 56) |
                                    ((uint64_t)m[i8 + 1] << 48) |
                                    ((uint64_t)m[i8 + 2] << 40) |
                                    ((uint64_t)m[i8 + 3] << 32) |
                                    ((uint64_t)m[i8 + 4] << 24) |
                                    ((uint64_t)m[i8 + 5] << 16) |
                                    ((uint64_t)m[i8 + 6] << 8) |
                                     (uint64_t)m[i8 + 7];
                        }

                        for(i=16;i<80;i++)
                                w[i] = sha512_sigma1(w[i-2]) + w[i-7] +
                                       sha512_sigma0(w[i-15]) + w[i-16];
                        a = hash[0];
                        b = hash[1];
                        c = hash[2];
                        d = hash[3];
                        e = hash[4];
                        f = hash[5];
                        g = hash[6];
                        h = hash[7];

                        for(i=0;i<80;i++) {
                                tmp1 = h + sha512_sigma3(e) + sha_ch64(e,f,g) +
                                       k[i] + w[i];
                                tmp2 = sha512_sigma2(a) + sha_maj64(a,b,c);
                                h = g;
                                g = f;
                                f = e;
                                e = d + tmp1;
                                d = c;
                                c = b;
                                b = a;
                                a = tmp1 + tmp2;
                        }
                        hash[0] += a;
                        hash[1] += b;
                        hash[2] += c;
                        hash[3] += d;
                        hash[4] += e;
                        hash[5] += f;
                        hash[6] += g;
                        hash[7] += h;
                        ctx->block_pos = 0;
                        break;
                }
                }
        }
}
/* hash value in ctx->hash is now usable */
SHA_API void
sha_finish(Sha *ctx) {
        /* pad final message and append big endian length,
           then process the final block

           padding rules are add a 1 bit, add zeros,
           add big endian length (128 bit for 384/512, 64 bit for others
           to fill out a 1024 bit block for 384/512 or 512 bit block for others
        */

        size_t i;
        uint8_t x;
        uint64_t size, *h;
        uint32_t *hp;
        int len, remain; /* size of length in bytes */

        /* invalid type will have zeroed block_size */
        if(!ctx->block_size) return;

        size = ctx->size; /* save original message size because adding
                             padding adds to size */
        if(ctx->type == SHA384 || ctx->type == SHA512) len = 128 / 8;
        else len = 64 / 8;

        remain = ctx->block_size - ctx->block_pos;

        x = (uint8_t)(1 << 7);
        sha_add(ctx, &x, 1);
        if(--remain < 0) remain = ctx->block_size;
        /* not enough room to encode length so pad with zeros to end of block */
        if(remain < len) {
                memset(ctx->block + ctx->block_pos, 0, (uint32_t)remain);
                ctx->block_pos = (uint16_t)(ctx->block_size - 1);
                x = 0;
                sha_add(ctx, &x, 1); /* force block process to run */
                remain = ctx->block_size;
        }
        /* now at start of a block. pad with zeros until we are at the end
           of the block + length */
        memset(ctx->block + ctx->block_pos, 0, (uint32_t)remain); /* could be remain - len instead */
        ctx->block_pos = (uint16_t)(ctx->block_size - sizeof(size));

        /* append big endian size in bits */
        size *= 8; /* we store length in bits */
	size = sha_swap_u64(size);
	sha_add(ctx, &size, sizeof(size));

	assert(!ctx->block_pos);

	/* hash is little endian. we need big endian */
	    hp = (uint32_t*)ctx->sha;
		h = (uint64_t*)ctx->sha;
	if(ctx->type == SHA384 || ctx->type == SHA512)
		        for(i=0;i<ctx->nhash/8;i++)
			h[i] = sha_swap_u64(h[i]);
	else
		        for(i=0;i<ctx->nhash/4;i++)
			hp[i] = sha_swap_u32(hp[i]);
}

/* copies min(hash_size, sha_hash_size) bytes of hash into hash calculated from
   data and returns the number of bytes copied */
SHA_API int
sha_hash(ShaType type, void *hash, int hash_size,
	const void *data, int size)
{
        Sha ctx;
        sha_init(&ctx, type);
        sha_add(&ctx, data, size);
        sha_finish(&ctx);
		if(ctx.nhash < hash_size) hash_size = ctx.nhash;
		memcpy(hash, ctx.sha, hash_size);
        return hash_size;
}

#endif

#ifdef SHA_EXAMPLE
#include <assert.h>
#include <stdio.h>
static void hex(char *dst, const char *src, int n) {
	int i;
	const char ch[] = "0123456789abcdef";
	for(i=0;i<n;i++) {
		*dst++ = ch[(unsigned char)src[i] >> 4];
		*dst++ = ch[src[i] & 0xF];
	}
}
int main(int argc, char **argv) {
	char a[] = "abc";
	char s1[SHA_MAX_HASH_SIZE], h1[sizeof s1 * 2];
	char o1[] = "a9993e364706816aba3e25717850c26c9cd0d89d";
	char o2[] = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
	int n;

	n = sha_hash(SHA1, s1, sizeof s1, a, sizeof a - 1);
	assert(n == SHA1_HASH_SIZE);
	hex(h1, s1, n); n *= 2;
	h1[n] = 0;
	assert(!strcmp(h1, o1));

	n = sha_hash(SHA256, s1, sizeof s1, a, sizeof a - 1);
	assert(n == SHA256_HASH_SIZE);
	hex(h1, s1, n); n *= 2;
	h1[n] = 0;
	assert(!strcmp(h1, o2));

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
