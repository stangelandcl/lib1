#ifndef JWT_H
#define JWT_H

/*
 * #define JWT_IMPLEMENTATION in one file before including or
 * #define JWT_STATIC before each include
 * example at bottom of file
 * public domain license at end of file
 */

#include <stddef.h>

#if defined(JWT_STATIC) || defined(JWT_EXAMPLE)
#define JWT_API static
#define JWT_IMPLEMENTATION
#else
#define JWT_API extern
#endif

#ifdef __cplusplus
extern "C" {
#endif

enum {
	JWT_UNKNOWN=-100,
	JWT_PARSE_FAILED,
	JWT_UNSUPPORTED_ALGORITHM,
	JWT_INVALID_TIME,
	JWT_SIGNATURE_MISMATCH,
	JWT_AUDIENCE_MISMATCH,
	JWT_HASH_FAILED,
	JWT_BUFFER_SIZE
};

/* values point to decoded buffer */
typedef struct Jwt {
	char sig[512]; /* signature bytes */
	char aud[32]; /* audience */
	char oid[32]; /* user id */
	char iss[128]; /* issuer url */
	char kid[64]; /* key id */
	char alg[10]; /* algorithm */
	unsigned nbf; /* not before time in unix seconds */
	unsigned exp; /* expires at in unix seconds */
	int nsig; /* signature length */
} Jwt;

typedef struct JwtKey {
	char m[512]; /* modulus. needs to be big enough to hold base 64 version of key */
	char e[32]; /* exponent */
	int nm, ne;
} JwtKey;

/* modifies token and null terminates header, payload and signature. sets any not found to null */
JWT_API void jwt_split(char *token, char **header, char **payload, char **signature);
/* return number of bytes written to json on success. negative number on failure */
JWT_API int jwt_decode(char *json, size_t njson, const char *base64, size_t nbase64);
/* return 0 on success, negative error code on failure */
JWT_API int jwt_parse(const char *token, char *buf, size_t nbuf, Jwt *jwt);
/* put wellknown url in buffer. returns 0 on success. else # of bytes required in buffer */
JWT_API int jwt_wellknown(char *dst, size_t ndst, const char *iss);
/* returns non-null terminated URI + size on success else null on not found */
JWT_API const char *jwt_ks_uri(const char *well_known_config_json, int *size);
/* return 0 on success, negative error code on failure */
JWT_API int jwt_find_key(const char *keys_json, const char *kid, JwtKey *key);
/* return 0 on success, negative error code on failure */
JWT_API int jwt_verify_sig(const char *hash, int nhash, const char *sig, int nsig, const char *e, int ne, const char *m, int nm);
/* return 0 on success, negative error code on failure */
JWT_API int jwt_verify(const char *token, const JwtKey *key, const char *aud);
/* convert return code to error text */
JWT_API const char* jwt_strerror(int return_code);

#ifdef __cplusplus
}
#endif

#endif

#ifdef JWT_IMPLEMENTATION
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/********************************************************************
 *
 *                           Base64
 *
 ********************************************************************/

/* returns decoded byte size if >=0 else error */
static int
jwt_base64_decode(void* dst, int ndst, const char *src, int nsrc) {
	static const uint8_t r[] = {
		255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255, /* 0-15 */
		255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255, /* 16-31 */
		255,255,255,255,255,255,255,255,255,255,255, 62,255,62,255, 63, /* 32-47 */
		 52, 53, 54, 55, 56, 57, 58, 59, 60, 61,255,255,255,254,255,255, /* 48-63 */
		255,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, /* 64-79 */
		 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,255,255,255,255,63, /* 80-95 */
		255, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, /* 96-111 */
		 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51,255,255,255,255,255, /* 112-127 */
		255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255, /* 128-143 */
		255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255, /* 144-159 */
		255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255, /* 160-175 */
		255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255, /* 176-192 */
		255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255, /* 192-207 */
		255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255, /* 208-223 */
		255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255, /* 224-239 */
		255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255, /* 239-255 */
	};
        int i, j;
        uint8_t *p = (uint8_t*)dst;
	const uint8_t *s = (const uint8_t*)src;
        uint8_t x0, x1, x2, x3;

	i = j = 0;
	if(i >= nsrc) return -1;
        while(i < nsrc) {
		x0 = r[s[i++]];
		if(x0 & 0x80) return -2;
		if(i == nsrc) return -3;
		x1 = r[s[i++]];
		if(x1 & 0x80) return -4;
		if(j == ndst) return -5;
		p[j++] = (x0 << 2) | (x1 >> 4);
		if(i == nsrc) break;
		x2 = r[s[i++]];
		if(x2 & 0x80) {
			if(x2 == 254) break;
			return -5;
		}
		if(j == ndst) return -6;
		p[j++] = (x1 << 4) | (x2 >> 2);
		if(i == nsrc) break;
		x3 = r[s[i++]];
		if(x3 & 0x80) {
			if(x3 == 254) break;
			return -7;
		}
		if(j == ndst) return -8;
		p[j++] = (x2 << 6) | x3;
        }
        return j;
}

/********************************************************************
 *
 *                               JWT_SHA
 *
 ********************************************************************/

#define JWT_SHA1_BLOCK_SIZE 64
#define JWT_SHA224_BLOCK_SIZE 64
#define JWT_SHA256_BLOCK_SIZE 64
#define JWT_SHA384_BLOCK_SIZE 128
#define JWT_SHA512_BLOCK_SIZE 128
#define JWT_SHA_MAX_BLOCK_SIZE JWT_SHA512_BLOCK_SIZE

#define JWT_SHA1_HASH_SIZE 20
#define JWT_SHA224_HASH_SIZE 28
#define JWT_SHA256_HASH_SIZE 32
#define JWT_SHA384_HASH_SIZE 48
#define JWT_SHA512_HASH_SIZE 64
#define JWT_SHA_MAX_HASH_SIZE JWT_SHA512_HASH_SIZE

typedef uint8_t JwtShaType;
#define JWT_SHA1 2
#define JWT_SHA224 3
#define JWT_SHA256 4
#define JWT_SHA384 5
#define JWT_SHA512 6

typedef struct {
        uint8_t block[JWT_SHA_MAX_BLOCK_SIZE];
		uint8_t sha[JWT_SHA_MAX_HASH_SIZE];
        /* actually sha 384/512 can handle 128 bytes of length but we don't
           need that yet so not implemented */
        uint64_t size;
        uint16_t block_size;
        uint16_t block_pos;
		uint8_t nhash;
        JwtShaType type;
} JwtSha;

#include <assert.h>
#include <string.h>

/* calculated
   for(i=1;i<=64;i++)
       md5_T[i] = (uint32_t)(4294967296.0 * abs(sin(i)));
*/
static uint32_t
jwt_sha_swap_u32(uint32_t x) {
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
jwt_sha_swap_u64(uint64_t x) {
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
jwt_sha_rotr64(uint64_t x, size_t count) {
        return (x << (64 - count)) | (x >> count);
}
static uint32_t
jwt_sha_rotr32(uint32_t x, size_t count) {
        return (uint32_t)((x << (32 - count)) | (x >> count));
}
static uint32_t
jwt_sha_rotl32(uint64_t x, size_t count) {
        return (uint32_t)((x >> (32 - count)) | (x << count));
}
static uint32_t
jwt_sha_ch32(uint32_t x, uint32_t y, uint32_t z) {
        /* return (x & y) ^ (~x & z); */
        return (x & (y ^ z)) ^ z;
}
static uint64_t
jwt_sha_ch64(uint64_t x, uint64_t y, uint64_t z) {
        return (x & (y ^ z)) ^ z;
}
static uint32_t
jwt_sha_maj32(uint32_t x, uint32_t y, uint32_t z) {
        /* return (x & y) ^ (x & z) ^ (y & z); */
        return (x & (y | z)) | (y & z);
}
static uint64_t
jwt_sha_maj64(uint64_t x, uint64_t y, uint64_t z) {
        return (x & (y | z)) | (y & z);
}
static uint32_t
jwt_sha_parity(uint32_t x, uint32_t y, uint32_t z) {
        return x ^ y ^ z;
}
static uint32_t
jwt_sha256_sigma0(uint32_t x) {
        return jwt_sha_rotr32(x, 7) ^ jwt_sha_rotr32(x, 18) ^ (x >> 3);
}
static uint32_t
jwt_sha256_sigma1(uint32_t x) {
        return jwt_sha_rotr32(x, 17) ^ jwt_sha_rotr32(x, 19) ^ (x >> 10);
}
static uint32_t
jwt_sha256_sigma2(uint32_t x) {
        return jwt_sha_rotr32(x, 2) ^ jwt_sha_rotr32(x, 13) ^ jwt_sha_rotr32(x, 22);
}
static uint32_t
jwt_sha256_sigma3(uint32_t x) {
        return jwt_sha_rotr32(x, 6) ^ jwt_sha_rotr32(x, 11) ^ jwt_sha_rotr32(x, 25);
}
static uint64_t
jwt_sha512_sigma0(uint64_t x) {
        return jwt_sha_rotr64(x, 1) ^ jwt_sha_rotr64(x, 8) ^ (x >> 7);
}
static uint64_t
jwt_sha512_sigma1(uint64_t x) {
        return jwt_sha_rotr64(x, 19) ^ jwt_sha_rotr64(x, 61) ^ (x >> 6);
}
static uint64_t
jwt_sha512_sigma2(uint64_t x) {
        return jwt_sha_rotr64(x, 28) ^ jwt_sha_rotr64(x, 34) ^ jwt_sha_rotr64(x, 39);
}
static uint64_t
jwt_sha512_sigma3(uint64_t x) {
        return jwt_sha_rotr64(x, 14) ^ jwt_sha_rotr64(x, 18) ^ jwt_sha_rotr64(x, 41);
}
static void
jwt_sha_init(JwtSha *ctx, JwtShaType type) {
        uint32_t *i;

        memset(ctx, 0, sizeof(*ctx));
        ctx->type = type;
        /* TODO: fix endianness */
		i = (uint32_t*)ctx->sha;

        switch(type) {
        case JWT_SHA1:
                ctx->block_size = JWT_SHA1_BLOCK_SIZE;
				ctx->nhash = JWT_SHA1_HASH_SIZE;
                i[0] = 0x67452301;
                i[1] = 0xEFCDAB89;
                i[2] = 0x98BADCFE;
                i[3] = 0x10325476;
                i[4] = 0xC3D2E1F0;
                break;
        case JWT_SHA224:
                ctx->block_size = JWT_SHA224_BLOCK_SIZE;
				ctx->nhash = JWT_SHA224_HASH_SIZE;
                i[0] = 0xC1059ED8;
                i[1] = 0x367CD507;
                i[2] = 0x3070DD17;
                i[3] = 0xF70E5939;
                i[4] = 0xFFC00B31;
                i[5] = 0x68581511;
                i[6] = 0x64F98FA7;
                i[7] = 0xBEFA4FA4;
                break;
        case JWT_SHA256:
                ctx->block_size = JWT_SHA256_BLOCK_SIZE;
				ctx->nhash = JWT_SHA256_HASH_SIZE;
                i[0] = 0x6A09E667;
                i[1] = 0xBB67AE85;
                i[2] = 0x3C6EF372;
                i[3] = 0xA54FF53A;
                i[4] = 0x510E527F;
                i[5] = 0x9B05688C;
                i[6] = 0x1F83D9AB;
                i[7] = 0x5BE0CD19;
                break;
        case JWT_SHA384:
                ctx->block_size = JWT_SHA384_BLOCK_SIZE;
				ctx->nhash = JWT_SHA384_HASH_SIZE;
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
        case JWT_SHA512:
                ctx->block_size = JWT_SHA512_BLOCK_SIZE;
				ctx->nhash = JWT_SHA512_HASH_SIZE;
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
jwt_sha_swap_hash(JwtSha *ctx){
        size_t i;
		uint32_t *h = (uint32_t*)ctx->sha;
		for(i=0;i<ctx->nhash/4;i++) h[i] = jwt_sha_swap_u32(h[i]);
}


static void
jwt_sha_add(JwtSha *ctx, const void *bytes, size_t count) {
        static const uint64_t jwt_sha512_K[] = {
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
        static const uint32_t jwt_sha256_K[] = {
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
                case JWT_SHA1:
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
                                w[i] = jwt_sha_rotl32(tmp, 1);
                        }
                        a = h[0];
                        b = h[1];
                        c = h[2];
                        d = h[3];
                        e = h[4];

                        for(i=0;i<80;i++) {
                                tmp = jwt_sha_rotl32(a, 5) + e + w[i] + sha1_K[i/20];
                                if(i < 20) tmp += jwt_sha_ch32(b, c, d);
                                else if(i < 40) tmp += jwt_sha_parity(b, c, d);
                                else if(i < 60) tmp += jwt_sha_maj32(b, c, d);
                                else if(i < 80) tmp += jwt_sha_parity(b, c, d);

                                e = d;
                                d = c;
                                c = jwt_sha_rotl32(b, 30);
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
                case JWT_SHA224:
                case JWT_SHA256:
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
                                s0 = jwt_sha256_sigma0(w[i-15]);
                                s1 = jwt_sha256_sigma1(w[i-2]);

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
                                tmp1 = h + jwt_sha256_sigma3(e) + jwt_sha_ch32(e,f,g) +
                                    jwt_sha256_K[i] + w[i];
                                tmp2 = jwt_sha256_sigma2(a) + jwt_sha_maj32(a, b, c);
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
                case JWT_SHA384:
                case JWT_SHA512:
                {
                        int i, i8; /* loop counter */
                        uint64_t tmp1, tmp2;
                        uint64_t w[80];
                        uint64_t a, b, c, d, e, f, g, h; /* buffers */
                        const uint64_t *k = (const uint64_t*)jwt_sha512_K;
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
                                w[i] = jwt_sha512_sigma1(w[i-2]) + w[i-7] +
                                       jwt_sha512_sigma0(w[i-15]) + w[i-16];
                        a = hash[0];
                        b = hash[1];
                        c = hash[2];
                        d = hash[3];
                        e = hash[4];
                        f = hash[5];
                        g = hash[6];
                        h = hash[7];

                        for(i=0;i<80;i++) {
                                tmp1 = h + jwt_sha512_sigma3(e) + jwt_sha_ch64(e,f,g) +
                                       k[i] + w[i];
                                tmp2 = jwt_sha512_sigma2(a) + jwt_sha_maj64(a,b,c);
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
static void
jwt_sha_finish(JwtSha *ctx) {
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
        if(ctx->type == JWT_SHA384 || ctx->type == JWT_SHA512) len = 128 / 8;
        else len = 64 / 8;

        remain = ctx->block_size - ctx->block_pos;

        x = (uint8_t)(1 << 7);
        jwt_sha_add(ctx, &x, 1);
        if(--remain < 0) remain = ctx->block_size;
        /* not enough room to encode length so pad with zeros to end of block */
        if(remain < len) {
                memset(ctx->block + ctx->block_pos, 0, (uint32_t)remain);
                ctx->block_pos = (uint16_t)(ctx->block_size - 1);
                x = 0;
                jwt_sha_add(ctx, &x, 1); /* force block process to run */
                remain = ctx->block_size;
        }
        /* now at start of a block. pad with zeros until we are at the end
           of the block + length */
        memset(ctx->block + ctx->block_pos, 0, (uint32_t)remain); /* could be remain - len instead */
        ctx->block_pos = (uint16_t)(ctx->block_size - sizeof(size));

        /* append big endian size in bits */
        size *= 8; /* we store length in bits */
	size = jwt_sha_swap_u64(size);
	jwt_sha_add(ctx, &size, sizeof(size));

	assert(!ctx->block_pos);

	/* hash is little endian. we need big endian */
	    hp = (uint32_t*)ctx->sha;
		h = (uint64_t*)ctx->sha;
	if(ctx->type == JWT_SHA384 || ctx->type == JWT_SHA512)
		        for(i=0;i<ctx->nhash/8;i++)
			h[i] = jwt_sha_swap_u64(h[i]);
	else
		        for(i=0;i<ctx->nhash/4;i++)
			hp[i] = jwt_sha_swap_u32(hp[i]);
}

/* copies min(hash_size, jwt_sha_hash_size) bytes of hash into hash calculated from
   data and returns the number of bytes copied */
static int
jwt_sha_hash(JwtShaType type, void *hash, int hash_size,
	const void *data, int size)
{
        JwtSha ctx;
        jwt_sha_init(&ctx, type);
        jwt_sha_add(&ctx, data, size);
        jwt_sha_finish(&ctx);
		if(ctx.nhash < hash_size) hash_size = ctx.nhash;
		memcpy(hash, ctx.sha, hash_size);
        return hash_size;
}



/********************************************************************
 *
 *                               JWT_RSA
 *
 ********************************************************************/

#if defined(__GNUC__) && defined(__LP64__)
	#define JWT_RSA_INT_MASK 7
	#define JWT_RSA_INT64
	#define JWT_RSA_INT_MAX 0xFFFFFFFFFFFFFFFF

	typedef uint64_t jwt_rsa_digit;
	typedef __uint128_t jwt_rsa_longdigit;
	typedef __int128_t jwt_rsa_ilongdigit;
#else
	#define JWT_RSA_INT_MASK 3
	#define JWT_RSA_INT32
	#define JWT_RSA_INT_MAX 0xFFFFFFFF

	typedef uint32_t jwt_rsa_digit;
	typedef uint64_t jwt_rsa_longdigit;
	typedef int64_t jwt_rsa_ilongdigit;
#endif

#define JWT_RSA_INT_BITS (sizeof(jwt_rsa_digit)*8)
#define JWT_RSA_DIGITS (8192/8/sizeof(jwt_rsa_digit) + 2)

/* little endian - d[0] is least significant. d[n-1] is most significant */
typedef struct { jwt_rsa_digit d[JWT_RSA_DIGITS]; int n; } JwtRsaInt;

static void jwt_rsa_int_print(const char* name, JwtRsaInt *b) {}

/* init from big endian - data[0] = most significant byte */
static void
jwt_rsa_int_init(JwtRsaInt *b, const uint8_t *d, int n) {
	int i,j,k;
	while(!*d && n) { ++d; --n; }
	b->n = (n + sizeof(jwt_rsa_digit) - 1) / sizeof(jwt_rsa_digit);
	memset(b->d, 0, b->n * sizeof(jwt_rsa_digit));
	assert(b->n <= (int)JWT_RSA_DIGITS);
	for(i=0,j=n-1;j>=0;i++,j--) {
		k = i / sizeof(jwt_rsa_digit);
		b->d[k] |= (jwt_rsa_digit)d[j] << (8 * (i & JWT_RSA_INT_MASK));
	}
}

/* to big endian bytes */
static int
jwt_rsa_int2bytes(JwtRsaInt *b, uint8_t *d, int n) {
        int i, j;
		for(i=(int)(b->n * sizeof(jwt_rsa_digit))-1,j=0;i>=0&&n;i--,n--) {
               	uint8_t x = b->d[i / sizeof(jwt_rsa_digit)] >> (8 * (i & JWT_RSA_INT_MASK));
        	if(j || x) {
			*d++ = x;
			j++;
		} /* else skip leading zeros */
	}
        return j;
}

static int
jwt_rsa_int_iszero(JwtRsaInt *x) {
        int i;
		for(i=0;i<x->n;i++)
		if(x->d[i]) return 0;
        return 1;
}

/* x = x + y */
static void
jwt_rsa_int_add(JwtRsaInt *x, JwtRsaInt *y) {
	int i;
	jwt_rsa_digit carry = 0;
        for(i=0;i<(int)JWT_RSA_DIGITS;i++) {
		jwt_rsa_digit n1 = i < x->n ? x->d[i] : 0;
		jwt_rsa_digit n2 = i < y->n ? y->d[i] : 0;
		if(i > x->n && i > y->n && !carry) break;
                jwt_rsa_longdigit d = (jwt_rsa_longdigit)n1 + n2 + carry;
                x->d[i] = d;
		if(d) x->n = i + 1;
                carry = d >> JWT_RSA_INT_BITS;
	}
}

/* x = x - y */
static void
jwt_rsa_int_sub(JwtRsaInt *x, JwtRsaInt *y)
{
        int i;
        jwt_rsa_digit d, borrow = 0;
	assert(x->n >= y->n);
	/* subtract y from x */
	    for(i=0;i<y->n;i++) {
                d = x->d[i] - y->d[i] - borrow;
                borrow = d > x->d[i];
                x->d[i] = d;
        }
	/* keep borrowing from x */
	for(;i<x->n && borrow;i++) {
		d = x->d[i] - borrow;
		borrow = d > x->d[i];
		x->d[i] = d;
	}
}

/* x * y % mod*/
static void
jwt_rsa_int_mult(JwtRsaInt *dst, const JwtRsaInt *x, const JwtRsaInt *y) {
        int i, j;
	jwt_rsa_longdigit n, k;
	n = x->n + y->n + 1;
	memset(dst->d, 0, (n > JWT_RSA_DIGITS ? JWT_RSA_DIGITS : n) * sizeof(jwt_rsa_digit));
	dst->n = 0;
	    for(i=0;i<y->n;i++) {
                jwt_rsa_digit carry = 0;
				for(j=0;i + j < (int)JWT_RSA_DIGITS && j < x->n;j++) {
                        assert(i + j < (int)JWT_RSA_DIGITS * 2);
                        n = (jwt_rsa_longdigit)x->d[j] * y->d[i] + carry;
                        k = n + dst->d[i + j];
                        dst->d[i + j] = (jwt_rsa_digit)k;
						if(i + j > dst->n && k) dst->n = i + j;
                        carry = (jwt_rsa_digit)(k >> JWT_RSA_INT_BITS);
                }

                dst->d[i + j] += carry;
				if(carry && i + j > dst->n) dst->n = i + j;
        }
		++dst->n;
		assert(dst->n <= (int)JWT_RSA_DIGITS * 2);
}

static int
jwt_rsa_int_leadzero(jwt_rsa_digit x) {
   int n;
   if (x == 0) return sizeof(x * 8);
   n = 0;
#ifdef JWT_RSA_INT64
   if (x <= 0x00000000FFFFFFFF) {n += 32; x <<= 32;}
   if (x <= 0x0000FFFFFFFFFFFF) {n += 16; x <<= 16;}
   if (x <= 0x00FFFFFFFFFFFFFF) {n +=  8; x <<=  8;}
   if (x <= 0x0FFFFFFFFFFFFFFF) {n +=  4; x <<=  4;}
   if (x <= 0x3FFFFFFFFFFFFFFF) {n +=  2; x <<=  2;}
   if (x <= 0x7FFFFFFFFFFFFFFF) n++;
#else
   if (x <= 0x0000FFFF) {n += 16; x <<= 16;}
   if (x <= 0x00FFFFFF) {n +=  8; x <<=  8;}
   if (x <= 0x0FFFFFFF) {n +=  4; x <<=  4;}
   if (x <= 0x3FFFFFFF) {n +=  2; x <<=  2;}
   if (x <= 0x7FFFFFFF) n++;
#endif
   return n;
}

/* from Hacker's Delight */
static void
jwt_rsa_int_mod(JwtRsaInt *remainder, JwtRsaInt *dividend, JwtRsaInt *divisor) {
   const jwt_rsa_longdigit b = (jwt_rsa_longdigit)1 << JWT_RSA_INT_BITS;
   jwt_rsa_longdigit qhat; /* estimated quotient digit */
   jwt_rsa_longdigit rhat; /* remainder */
   jwt_rsa_longdigit p; /* product */
   jwt_rsa_ilongdigit t, k;
   int s, i, j;
   JwtRsaInt t1, t2;
   jwt_rsa_digit *r=remainder->d, *u=dividend->d, *v=divisor->d;
   jwt_rsa_digit *un=t1.d, *vn=t2.d; /* normalized dividend and divisor */
   int n, m=dividend->n;

   while(divisor->n && !divisor->d[divisor->n-1])
	--divisor->n; /* strip leading zeros */

   /* if dividend > divisor then return dividend */
   if(dividend->n < divisor->n) {
	*remainder = *dividend;
	return;
   }
   n=divisor->n;

   assert(m < (int)JWT_RSA_DIGITS - 1);
   assert(m >= n);
   assert(n > 0);
   assert(v[n-1] != 0);

   /* Normalize by shifting v left just enough so that its high-order
   bit is on, and shift u left the same amount. We may have to append a
   high-order digit on the dividend; we do that unconditionally. */

   s = jwt_rsa_int_leadzero(v[n-1]);
   for (i = n - 1; i > 0; i--)
      vn[i] = (v[i] << s) | ((jwt_rsa_longdigit)v[i-1] >> (JWT_RSA_INT_BITS-s));
   vn[0] = v[0] << s;

   /*  un = (jwt_rsa_digit *)alloca(4* (m + 1)); */
   un[m] = (jwt_rsa_longdigit)u[m-1] >> (JWT_RSA_INT_BITS-s);
   for (i = m - 1; i > 0; i--)
      un[i] = (u[i] << s) | ((jwt_rsa_longdigit)u[i-1] >> (JWT_RSA_INT_BITS-s));
   un[0] = u[0] << s;

   for (j = m - n; j >= 0; j--) {
      /* Compute estimate qhat of q[j] */
      qhat = (un[j+n]*b + un[j+n-1])/vn[n-1];
      rhat = (un[j+n]*b + un[j+n-1]) - qhat*vn[n-1];
again:
      if (qhat >= b ||
        qhat*vn[n-2] > b*rhat + un[j+n-2]) {
        qhat = qhat - 1;
        rhat = rhat + vn[n-1];
        if (rhat < b) goto again;
      }

      /* Multiply and subtract */
      k = 0;
      for (i = 0; i < n; i++) {
         p = qhat*vn[i];
         t = un[i+j] - k - (p & (jwt_rsa_ilongdigit)JWT_RSA_INT_MAX);
         un[i+j] = t;
         k = (p >> JWT_RSA_INT_BITS) - (t >> JWT_RSA_INT_BITS);
      }
      t = un[j+n] - k;
      un[j+n] = t;

      if (t < 0) {              /* If we subtracted */
         k = 0;
         for (i = 0; i < n; i++) {
            t = (jwt_rsa_longdigit)un[i+j] + vn[i] + k;
            un[i+j] = t;
            k = t >> JWT_RSA_INT_BITS;
         }
         un[j+n] = un[j+n] + k;
      }
   }

   /* unnormalize remainder */
      for (i = 0; i < n-1; i++)
         r[i] = (un[i] >> s) | ((jwt_rsa_longdigit)un[i+1] << (JWT_RSA_INT_BITS-s));
      r[n-1] = un[n-1] >> s;

	  remainder->n = n;
	  while(remainder->n && !remainder->d[remainder->n-1])
		--remainder->n; /* strip leading zeros */
}

/* dst = pow(x, e) % m */
static void
jwt_rsa_int_pow(JwtRsaInt *dst, JwtRsaInt *x, JwtRsaInt *e, JwtRsaInt *m)
{
        JwtRsaInt mult;
        int start_digit, i;
        jwt_rsa_digit high_bit, bit;

	assert(e->n);
	assert(e->d[e->n-1]);

        /* find highest non-zero digit */
	    for(start_digit=e->n-1;start_digit > 0;--start_digit)
                if(e->d[start_digit])
                        break;

        high_bit = (jwt_rsa_digit)1 << (JWT_RSA_INT_BITS-1);
        while(high_bit && !(e->d[start_digit] & high_bit))
                high_bit >>= 1;

        if(!start_digit && !high_bit) {
                /* anything but zero raised to zero is one */
                if(jwt_rsa_int_iszero(x)) dst->d[0] = 0;
                else dst->d[0] = 1;
				dst->n = 1;
                return;
        }

        /* we now have the highest one bit */
		dst->n = 1;
        dst->d[0] = 1;
        mult = *x;
	//printf("start=%d hi=%d\n", start_digit, high_bit);
        for(i=0;i<=start_digit;i++) {
                jwt_rsa_digit digit = e->d[i];
                bit = 1;
                do {
                        JwtRsaInt tmp;
                        if(digit & bit) {
                                jwt_rsa_int_mult(&tmp, dst, &mult);
				jwt_rsa_int_mod(dst, &tmp, m);
                        }
                        jwt_rsa_int_mult(&tmp, &mult, &mult);
			jwt_rsa_int_mod(&mult, &tmp, m);
                        bit <<= 1;
                } while(bit && (i != start_digit || bit <= high_bit));
        }
}

/*******************************************************************
*
*               JWT_RSA sign, verify, encrypt, decrypt
*
********************************************************************/

/* PKCS 1.5 JWT_RSA signing from RFC 3447 */
static int
jwt_rsa_sign1(uint8_t *dst, uint8_t *hash, int nhash, int sha, JwtRsaInt *e, JwtRsaInt *m) {
	uint8_t tmp[512], *end;
	/* ASN.1 encoding DigestInfo from RFC 3447 */
	uint8_t sha1[]={0x30,0x21,0x30,0x09,0x06,0x05,0x2b,0x0e,
	           0x03,0x02,0x1a,0x05,0x00,0x04,0x14};
	uint8_t sha256[]={0x30,0x31,0x30,0x0d,0x06,0x09,0x60,0x86,0x48,0x01,
	             0x65,0x03,0x04,0x02,0x01,0x05,0x00,0x04,0x20};
	uint8_t sha384[]={0x30,0x41,0x30,0x0d,0x06,0x09,0x60,0x86,0x48,0x01,
	             0x65,0x03,0x04,0x02,0x02,0x05,0x00,0x04,0x30};
	uint8_t sha512[]={0x30,0x51,0x30,0x0d,0x06,0x09,0x60,0x86,0x48,0x01,
	             0x65,0x03,0x04,0x02,0x03,0x05,0x00,0x04,0x40};
	int n = m->n * sizeof(jwt_rsa_digit);
	JwtRsaInt msg, sig;

	/* format is 0x00 0x01 0xFF 0xFF 0xFF ... 0x00 (DigestInfo) (hash) */
	memset(tmp, 0xFF, n);
	tmp[0] = 0;
	tmp[1] = 1;
	end = tmp + n - nhash;
	memcpy(end, hash, nhash);
	switch(sha) {
	case JWT_SHA1: end-=sizeof sha1; memcpy(end, sha1, sizeof sha1); break;
	case JWT_SHA256: end-=sizeof sha256; memcpy(end, sha256, sizeof sha256); break;
	case JWT_SHA384: end-=sizeof sha384; memcpy(end, sha384, sizeof sha384); break;
	case JWT_SHA512: end-=sizeof sha512; memcpy(end, sha512, sizeof sha512); break;
	}
	--end; *end = 0;
	jwt_rsa_int_init(&msg, tmp, n);
	/* printf("premsg2="); tls_int_print(&msg); */
	jwt_rsa_int_pow(&sig, &msg, e, m);
	/* printf("sig="); tls_int_print(&sig); */
	jwt_rsa_int2bytes(&sig, dst, n);
	/* tls_dumphex("sig2=", dst, n); */
	return n;
}

static int
jwt_rsa_sign(uint8_t *dst, uint8_t *hash, int nhash, int sha, const uint8_t *e, int ne, const uint8_t *m, int me) {
	JwtRsaInt ie, im;
	jwt_rsa_int_init(&ie, e, ne);
	jwt_rsa_int_init(&im, m, me);

	return jwt_rsa_sign1(dst, hash, nhash, sha, &ie, &im);
}
static int
jwt_rsa_verify1(const uint8_t *hash, int nhash, const uint8_t *sig, int nsig, JwtRsaInt *e, JwtRsaInt *m) {
	JwtRsaInt i, sig2;
	uint8_t buf[1024];
	int n;

	jwt_rsa_int_init(&i, sig, nsig);
	jwt_rsa_int_pow(&sig2, &i, e, m);
	n = jwt_rsa_int2bytes(&sig2, buf, sizeof buf);
	if(n < nhash) return -1;
	return !memcmp(buf + n - nhash, hash, nhash) ? 0 : -1;
}
static int
jwt_rsa_verify(const uint8_t *hash, int nhash, const uint8_t *sig, int nsig,
	const uint8_t *e, int ne, const uint8_t *m, int nm) {
	JwtRsaInt ie, im;
	jwt_rsa_int_init(&ie, e, ne);
	jwt_rsa_int_init(&im, m, nm);

	return jwt_rsa_verify1(hash, nhash, sig, nsig, &ie, &im);
}



/********************************************************************
 *
 *                               JWT
 *
 ********************************************************************/


JWT_API const char* jwt_strerror(int return_code) {
	switch(return_code) {
	case 0: return "OK";
	default:
	case JWT_UNKNOWN: return "Unknown";
	case JWT_PARSE_FAILED: return "Parse failed";
	case JWT_UNSUPPORTED_ALGORITHM: return "Unsupported algorithm";
	case JWT_INVALID_TIME: return "Invalid time";
	case JWT_SIGNATURE_MISMATCH: return "Signature mismatch";
	case JWT_AUDIENCE_MISMATCH: return "Audience mismatch";
	case JWT_HASH_FAILED: return "Hash failed";
	case JWT_BUFFER_SIZE: return "Buffer too small";
	}
}

JWT_API void
jwt_split(char *token, char **header, char **payload, char **signature) {
	char *tmp;
	*header = *payload = *signature = 0;
	*header = strstr(token, ".");
	if(!*header) return;
	**header = 0;
	tmp = *header + 1;
	*header = token;
	token = tmp;
	*payload = strstr(tmp, ".");
	if(!*payload) return;
	**payload = 0;
	tmp = *payload + 1;
	*payload = token;
	token = tmp;
	*signature = token;
}

JWT_API int
jwt_decode(char *json, size_t njson, const char *base64, size_t nbase64) {
	int rc, ok;

	rc = jwt_base64_decode(json, njson, base64, nbase64);
	ok = rc > 0 && rc < (int)njson;
	if(ok) json[rc] = 0;
	else json[0] = 0;
	return ok ? rc : JWT_PARSE_FAILED;
}

/* returns null on not found */
static const char*
jwt_find(const char *json, const char *key, int *size) {
	char buf[16];
	const char *s, *e;
	*size = 0;

	/* find key */
	snprintf(buf, sizeof buf, "\"%s\"", key);
	s = strstr(json, buf);
	if(!s) return 0;
	while(*s && *s != ':') ++s;
	if(*s != ':') return 0;

	/* find start of value */
	++s;
	while(*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') ++s;

	/* find end of value */
	e = s;
	while(*e != ',' && *e != ']' && *e != '}') ++e;

	/* trim trailing whitespace */
	while((*e == ' ' || *e == '\t' || *e == '\n' || *e == '\r') && e > s) --e;

	/* trim quotes */
	if(s < e && *s == '"') ++s;
	if(s < e - 1 && e[-1] == '"') --e;

	*size = e - s;
	return s;
}
JWT_API int
jwt_wellknown(char *dst, size_t ndst, const char *iss) {
	int rc = snprintf(dst, ndst, "%s/.well-known/openid-configuration", iss);
	if(rc < (int)ndst) return 0;
	return snprintf(0, 0, "%s/.well-known/openid-configuration", iss);
}

JWT_API int
jwt_parse(const char *token, char *buf, size_t nbuf, Jwt *jwt) {
	const char *x, *s;
	int rc, n;

	memset(jwt, 0, sizeof *jwt);

	/* header */

	x = strstr(token, ".");
	if(!x) return JWT_PARSE_FAILED;
	rc = jwt_decode(buf, nbuf, token, x - token);
	if(rc <= 0) return JWT_PARSE_FAILED;

	s = jwt_find(buf, "alg", &n);
	if(s) snprintf(jwt->alg, sizeof jwt->alg, "%.*s", n, s);
	s = jwt_find(buf, "kid", &n);
	if(s) snprintf(jwt->kid, sizeof jwt->kid, "%.*s", n, s);

	token = x + 1;
	buf += rc;
	nbuf -= rc;

	/* body */

	x = strstr(token, ".");
	if(!x) return JWT_PARSE_FAILED;
	rc = jwt_decode(buf, nbuf, token, x - token);

	s = jwt_find(buf, "aud", &n);
	if(s) snprintf(jwt->aud, sizeof jwt->aud, "%.*s", n, s);
	s = jwt_find(buf, "oid", &n);
	if(s) snprintf(jwt->oid, sizeof jwt->oid, "%.*s", n, s);
	s = jwt_find(buf, "iss", &n);
	if(s) snprintf(jwt->iss, sizeof jwt->iss, "%.*s", n, s);
	s = jwt_find(buf, "nbf", &n);
	if(s) jwt->nbf = atoi(s);
	s = jwt_find(buf, "exp", &n);
	if(s) jwt->exp = atoi(s);


	token = x + 1;
	buf += rc;
	nbuf -= rc;

	/* signature */
	rc = jwt_decode(jwt->sig, sizeof jwt->sig, token, strlen(token));
	jwt->nsig = rc;

	return 0;
}
JWT_API const char*
jwt_ks_uri(const char *well_known_config_json, int *size) {
	return jwt_find(well_known_config_json, "jwks_uri", size);
}

JWT_API int
jwt_find_key(const char *keys_json, const char *kid, JwtKey *key) {
	int n, nns, nne;
	const char *s, *ns, *ne;

	for(;;) {
		s = jwt_find(keys_json, "kid", &n);
		if(!s) return JWT_PARSE_FAILED;
		keys_json = s;
		if(!strncmp(s, kid, n)) {
			ns = jwt_find(s, "n", &nns);
			ne = jwt_find(s, "e", &nne);

			if(ns && ne && nns <= (int)sizeof key->m) {
				key->nm = jwt_base64_decode(key->m, sizeof key->m, ns, nns);
				key->ne = jwt_base64_decode(key->e, sizeof key->e, ne, nne);
				if(key->nm > 0 && key->ne > 0) break;
			}
		}
	}
	return 0;
}

JWT_API int
jwt_verify_sig(const char *hash, int nhash, const char *sig, int nsig, const char *e, int ne, const char *m, int nm) {
	int rc = jwt_rsa_verify((const uint8_t*)hash, nhash, (const uint8_t*)sig, nsig, (const uint8_t*)e, ne, (const uint8_t*)m, nm);
	if(rc) return JWT_SIGNATURE_MISMATCH;
	return 0;
}

JWT_API int
jwt_verify(const char *token, const JwtKey *key, const char *aud) {
	char buf[4096];
	Jwt jwt;
	const char *end;
	JwtShaType type = JWT_SHA1;
	char hash[JWT_SHA_MAX_HASH_SIZE];
	int nhash;
	time_t now;
	int rc;

	if(jwt_parse(token, buf, sizeof buf, &jwt)) return JWT_PARSE_FAILED;
	if(aud && strcmp(jwt.aud, aud)) return JWT_AUDIENCE_MISMATCH;

	end = strstr(token, ".");
	if(!end) return JWT_PARSE_FAILED;
	end = strstr(end + 1, ".");
	if(!end) return JWT_PARSE_FAILED;

	if(!strcmp(jwt.alg, "RS256")) type = JWT_SHA256;
	else if(!strcmp(jwt.alg, "RS384")) type = JWT_SHA384;
	else if(!strcmp(jwt.alg, "RS512")) type = JWT_SHA512;
	else return JWT_UNSUPPORTED_ALGORITHM;

	nhash = jwt_sha_hash(type, hash, sizeof hash, token, end - token);
	if(nhash <= 0) return JWT_HASH_FAILED;

	rc = jwt_verify_sig(hash, nhash, jwt.sig, jwt.nsig, key->e, key->ne, key->m, key->nm);
	if(rc) return rc;

	if(jwt.nbf || jwt.exp) {
		now = time(0);
		if(now < jwt.nbf || now > jwt.exp) return JWT_INVALID_TIME;
	}

	return 0;
}

#endif

#ifdef JWT_EXAMPLE
#include <assert.h>
#include <stdio.h>
int main(int argc, char **argv) {
	/* key example from https://blog.angular-university.io/angular-jwt/ */
	char token[] = "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiIxMjM0NTY3ODkwIiwibmFtZSI6IkpvaG4gRG9lIiwiYWRtaW4iOnRydWV9.EkN-DOsnsuRjRO6BxXemmJDm3HbxrbRzXglbN2S4sOkopdU4IsDxTI8jO19W_A4K8ZPJijNLis4EZsHeY559a4DFOd50_OqgHGuERTqYZyuhtF39yxJPAjUESwxk2J5k_4zM3O-vtd1Ghyo4IbqKKSy6J9mTniYJPenn5-HIirE";

	char buf[4096], url[256];
	const uint8_t m[] = {
		0x00,0xdd,0x95,0xab,0x51,0x8d,0x18,0xe8,0x82,0x8d,0xd6,0xa2,0x38,0x06,0x1c,
		0x51,0xd8,0x2e,0xe8,0x1d,0x51,0x60,0x18,0xf6,0x24,0x77,0x7f,0x2e,0x1a,0xad,
	        0x63,0x40,0xd4,0xaa,0x12,0xf2,0x45,0x70,0xdf,0x77,0x09,0x89,0xb5,0xeb,0xf1,
		0xbb,0xf0,0x50,0x05,0x29,0x6a,0xb0,0xb0,0x96,0xf7,0x5b,0x1f,0xa7,0x6f,0x10,
		0xe7,0xe8,0xbb,0x4f,0xe0,0x08,0x54,0x2c,0x1d,0x47,0xd0,0xad,0x20,0xef,0xf8,
		0xcb,0x92,0x50,0xc0,0x1e,0xf2,0x3c,0xca,0x13,0x8a,0x96,0xfa,0x32,0xbe,0xc5,
		0x05,0x3d,0x6b,0x4d,0xc6,0x52,0x72,0x87,0x92,0x49,0x5e,0xf9,0x0d,0x29,0x5f,
		0xf8,0x3a,0x8d,0x76,0x7b,0xaf,0x5f,0xf1,0x00,0xae,0x43,0xa3,0x69,0x10,0xf9,
		0x7e,0x71,0x2b,0xd7,0x22,0xa5,0x18,0x04,0x2b
	};
	const uint8_t e[] = {0x01,0x00,0x01};
	Jwt jwt;
	JwtKey key;
	int rc;
	/* FILE *f = fopen("jwks", "rb");
           size_t nkeys = fread(keys, 1, sizeof keys, f);
   	   keys[nkeys] = 0;
	   fclose(f); */

	jwt_parse(token, buf, sizeof buf, &jwt);
	printf("alg=%s\n", jwt.alg);
	printf("kid=%s\n", jwt.kid);
	printf("nbf=%u\n", jwt.nbf);
	printf("exp=%u\n", jwt.exp);
	printf("iss=%s\n", jwt.iss);
	printf("aud=%s\n", jwt.aud);
	printf("oid=%s\n", jwt.oid);
	printf("signature len=%d\n", jwt.nsig);

	jwt_wellknown(url, sizeof url, jwt.iss);
	printf("well-known url=%s\n", url);


#if 0
	rc = jwt_find_key(keys, jwt.kid, &key);
	assert(rc == 0);
	printf("mlen=%d elen=%d\n", key.nm, key.ne);
#endif


	memcpy(key.m, m, sizeof m);
	key.nm = sizeof m;
	memcpy(key.e, e, sizeof e);
	key.ne = sizeof e;


#if 0
	end = strstr(token, ".");
	end = strstr(end + 1, ".");
	nhash = jwt_sha_hash(JWT_SHA256, hash, sizeof hash, token, end - token);
	rc = jwt_verify_sig(hash, nhash, jwt.sig, jwt.nsig, key.e, key.ne, key.m, key.nm);
#endif

	rc = jwt_verify(token, &key, 0);
	printf("verified? %s\n", jwt_strerror(rc));

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
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT JWT_SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
