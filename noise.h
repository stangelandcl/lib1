#ifndef NOISE_H
#define NOISE_H

/*
 * #define NOISE_IMPLEMENTATION in one file before including or
 * #define NOISE_STATIC before each include
 * example at bottom of file
 * public domain license at end of file
 */


#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if defined(NOISE_STATIC) || defined(NOISE_EXAMPLE)
#define NOISE_API static
#define NOISE_IMPLEMENTATION
#else
#define NOISE_API extern
#endif

#define NOISE_DHLEN 32
#define NOISE_PSKLEN 32
#define NOISE_ADLEN 16

#ifdef __cplusplus
extern "C" {
#endif

/* Diffie-Hillman elliptic curve key pair */
typedef struct NoiseDH {
	/* private key */
	uint8_t priv[NOISE_DHLEN];
	/* public key */
	uint8_t pub[NOISE_DHLEN];
} NoiseDH;

/* NoiseNonce. Unique key used for each datagram. Each key
   must be used only once. Noise specifies a 64 bit incrementing
   nonce but aegis takes a 128 bit nonce, so extend it */
typedef union NoiseNonce {
	uint64_t n;
	uint8_t nonce[16];
} NoiseNonce;

/* Noise NoiseCipherState object. */
typedef struct NoiseCipher {
	/* unique key for this connection, generated from the ephemeral
	   Diffie-Hillman shared keys and the pre shared key plus hashing.
	   Used by the aegis symmetric encryption */
	uint8_t k[32];
	NoiseNonce n; /* nonce */
	uint8_t noise_has_key;
} NoiseCipher;

/* Noise NoiseSymmetricState object */
typedef struct NoiseSymmetric {
	NoiseCipher cs;
	uint8_t ck[32];
	/* hash */
	uint8_t h[32];
} NoiseSymmetric;

typedef struct NoiseHandshake {
	NoiseSymmetric ss;
	/* our ephemeral Diffie-Hillman key */
	NoiseDH e;
	/* pre-shared key */
	uint8_t psk[32];
	/* remote public key */
	uint8_t re[NOISE_DHLEN];
	NoiseCipher ours;
	NoiseCipher theirs;
} NoiseHandshake;

NOISE_API size_t noise_write_messageA(NoiseHandshake * hs, uint8_t * buffer,
		     size_t bufsize, uint32_t msgSize);
NOISE_API size_t noise_write_messageB(NoiseHandshake * hs, uint8_t * buffer, size_t size);
NOISE_API size_t noise_write_message(NoiseCipher * cs,
		    const void * data,
		    size_t size, uint8_t * output, size_t outputlen);

NOISE_API int noise_read_messageA(NoiseHandshake * hs,
		    const uint8_t * buffer,
		    size_t size, uint8_t * output, size_t *outputlen);
NOISE_API int noise_read_messageB(NoiseHandshake * hs,
		    const uint8_t * buffer,
		    size_t size, uint8_t * output, size_t *outputlen);
NOISE_API size_t noise_read_message(NoiseCipher * cs, const uint8_t * sent,
		   size_t sent_size, uint8_t * recv, size_t recv_size);
NOISE_API void noise_init(NoiseHandshake * hs, const uint8_t psk[NOISE_PSKLEN]);

#ifdef __cplusplus
}
#endif

#endif


#ifdef NOISE_IMPLEMENTATION

#ifdef _WIN32
/* link with -ladvapi32.
   docs say may be deprecated but used by rand_s, CRT, firefox, chrome, zig, etc */
char __stdcall SystemFunction036(void* buf, unsigned); /* RtlGenRandom */
static int getrandom(void* buf, int len, int x)
{
	return SystemFunction036(buf, len) ? len : 0;
}
#else
#include <sys/random.h>
#endif



//#define NOISE_KEYLEN 32
//#define NOISE_HASHLEN 32
#define NOISE_MSGBITS 3

/* message types: 0-7 (NOISE_MSGBITS bits) */
enum {
	NOISE_CONNECT = 0,
	NOISE_CONNECTED = 1,
};

typedef struct NoiseReader {
	const uint8_t *buf;
	size_t i, size;
} NoiseReader;

typedef struct NoiseBuilder {
	uint8_t *buf;
	size_t i, size;
} NoiseBuilder;

/*******************************************************
*
*                      Blake3
*             CC0 - public domain. from github
*
********************************************************/

#define BLAKE3_KEY_LEN 32
#define BLAKE3_OUT_LEN 32
#define BLAKE3_BLOCK_LEN 64
#define BLAKE3_CHUNK_LEN 1024
#define BLAKE3_MAX_DEPTH 54

// This struct is a private implementation detail. It has to be here because
// it's part of blake3_hasher below.
typedef struct {
	uint32_t cv[8];
	uint64_t chunk_counter;
	uint8_t buf[BLAKE3_BLOCK_LEN];
	uint8_t buf_len;
	uint8_t blocks_compressed;
	uint8_t flags;
} blake3_chunk_state;

typedef struct {
	uint32_t key[8];
	blake3_chunk_state chunk;
	uint8_t cv_stack_len;
	// The stack size is MAX_DEPTH + 1 because we do lazy merging. For example,
	// with 7 chunks, we have 3 entries in the stack. Adding an 8th chunk
	// requires a 4th entry, rather than merging everything down to 1, because we
	// don't know whether more input is coming. This is different from how the
	// reference implementation does things.
	uint8_t cv_stack[(BLAKE3_MAX_DEPTH + 1) * BLAKE3_OUT_LEN];
} blake3_hasher;

static void blake3_hasher_init(blake3_hasher * self);
static void
blake3_hasher_init_keyed(blake3_hasher *self, const uint8_t key[BLAKE3_KEY_LEN]);
static void blake3_hasher_init_derive_key(blake3_hasher * self, const char *context);
static void
blake3_hasher_update(blake3_hasher * self, const void *input, size_t input_len);
static void
blake3_hasher_finalize(const blake3_hasher * self, uint8_t * out, size_t out_len);
static void blake3_hasher_finalize_seek(
	const blake3_hasher * self, uint64_t seek, uint8_t * out, size_t out_len);

// internal flags
enum blake3_flags {
	BLAKE3_CHUNK_START = 1 << 0,
	BLAKE3_CHUNK_END = 1 << 1,
	BLAKE3_PARENT = 1 << 2,
	BLAKE3_ROOT = 1 << 3,
	BLAKE3_KEYED_HASH = 1 << 4,
	BLAKE3_DERIVE_KEY_CONTEXT = 1 << 5,
	BLAKE3_DERIVE_KEY_MATERIAL = 1 << 6,
};

#if defined(_MSC_VER)
#define BLAKE3_INLINE static __forceinline
#else
#define BLAKE3_INLINE static inline __attribute__((always_inline))
#endif

#if defined(__x86_64__) || defined(_M_X64)
#define BLAKE3_IS_X86
#define BLAKE3_IS_X86_64
#endif

#if defined(__i386__) || defined(_M_IX86)
#define BLAKE3_IS_X86
#define BLAKE3_IS_X86_32
#endif

#if defined(BLAKE3_IS_X86) && 0
#if defined(_MSC_VER)
#include <intrin.h>
#endif
#include <immintrin.h>
#endif

#define BLAKE3_MAX_SIMD_DEGREE 1

/* There are some places where we want a static size that's equal to the
	BLAKE3_MAX_SIMD_DEGREE, but also at least 2. */
#define BLAKE3_MAX_SIMD_DEGREE_OR_2 (BLAKE3_MAX_SIMD_DEGREE > 2 ? BLAKE3_MAX_SIMD_DEGREE : 2)

static const uint32_t BLAKE3_IV[8] = { 0x6A09E667UL, 0xBB67AE85UL, 0x3C6EF372UL,
	0xA54FF53AUL, 0x510E527FUL, 0x9B05688CUL,
	0x1F83D9ABUL, 0x5BE0CD19UL
};

static const uint8_t BLAKE3_MSG_SCHEDULE[7][16] = {
	{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
	{2, 6, 3, 10, 7, 0, 4, 13, 1, 11, 12, 5, 9, 14, 15, 8},
	{3, 4, 10, 12, 13, 2, 7, 14, 6, 5, 9, 0, 11, 15, 8, 1},
	{10, 7, 12, 9, 14, 3, 13, 15, 4, 0, 11, 2, 5, 8, 1, 6},
	{12, 13, 9, 11, 15, 10, 14, 8, 7, 2, 5, 3, 0, 1, 6, 4},
	{9, 14, 11, 5, 8, 12, 15, 1, 13, 3, 0, 10, 2, 6, 4, 7},
	{11, 15, 5, 0, 1, 9, 8, 6, 14, 10, 2, 12, 3, 4, 7, 13},
};

/* Find index of the highest set bit */
/* x is assumed to be nonzero.       */
static unsigned int
blake3_highest_one(uint64_t x) {
#if defined(__GNUC__) || defined(__clang__)
	return 63 ^ __builtin_clzll(x);
#elif defined(_MSC_VER) && defined(BLAKE3_IS_X86_64)
	unsigned long index;
	_BitScanReverse64(&index, x);
	return index;
#elif defined(_MSC_VER) && defined(BLAKE3_IS_X86_32)
	if(x >> 32) {
		unsigned long index;
		_BitScanReverse(&index, x >> 32);
		return 32 + index;
	} else {
		unsigned long index;
		_BitScanReverse(&index, x);
		return index;
	}
#else
	unsigned int c = 0;
	if(x & 0xffffffff00000000ULL) {
		x >>= 32;
		c += 32;
	}
	if(x & 0x00000000ffff0000ULL) {
		x >>= 16;
		c += 16;
	}
	if(x & 0x000000000000ff00ULL) {
		x >>= 8;
		c += 8;
	}
	if(x & 0x00000000000000f0ULL) {
		x >>= 4;
		c += 4;
	}
	if(x & 0x000000000000000cULL) {
		x >>= 2;
		c += 2;
	}
	if(x & 0x0000000000000002ULL) {
		c += 1;
	}
	return c;
#endif
}

// Count the number of 1 bits.
BLAKE3_INLINE unsigned int
blake3_popcnt(uint64_t x) {
#if defined(__GNUC__) || defined(__clang__)
	return __builtin_popcountll(x);
#else
	unsigned int count = 0;
	while(x != 0) {
		count += 1;
		x &= x - 1;
	}
	return count;
#endif
}

// Largest power of two less than or equal to x. As a special case, returns 1
// when x is 0.
BLAKE3_INLINE uint64_t
blake3_round_down_to_power_of_2(uint64_t x) {
	return 1ULL << blake3_highest_one(x | 1);
}

BLAKE3_INLINE uint32_t
blake3_counter_low(uint64_t counter) {
	return (uint32_t) counter;
}

BLAKE3_INLINE uint32_t
blake3_counter_high(uint64_t counter) {
	return (uint32_t) (counter >> 32);
}

BLAKE3_INLINE uint32_t
blake3_load32(const void *src) {
	const uint8_t *p = (const uint8_t *)src;
	return ((uint32_t) (p[0]) << 0) | ((uint32_t) (p[1]) << 8) |
		((uint32_t) (p[2]) << 16) | ((uint32_t) (p[3]) << 24);
}

BLAKE3_INLINE void
blake3_load_key_words(const uint8_t key[BLAKE3_KEY_LEN], uint32_t key_words[8]) {
	key_words[0] = blake3_load32(&key[0 * 4]);
	key_words[1] = blake3_load32(&key[1 * 4]);
	key_words[2] = blake3_load32(&key[2 * 4]);
	key_words[3] = blake3_load32(&key[3 * 4]);
	key_words[4] = blake3_load32(&key[4 * 4]);
	key_words[5] = blake3_load32(&key[5 * 4]);
	key_words[6] = blake3_load32(&key[6 * 4]);
	key_words[7] = blake3_load32(&key[7 * 4]);
}

void blake3_compress_in_place(uint32_t cv[8],
			      const uint8_t block[BLAKE3_BLOCK_LEN],
			      uint8_t block_len, uint64_t counter,
			      uint8_t flags);

void blake3_compress_xof(const uint32_t cv[8],
			 const uint8_t block[BLAKE3_BLOCK_LEN],
			 uint8_t block_len, uint64_t counter, uint8_t flags,
			 uint8_t out[64]);

void blake3_hash_many(const uint8_t * const *inputs, size_t num_inputs,
		      size_t blocks, const uint32_t key[8], uint64_t counter,
		      bool increment_counter, uint8_t flags,
		      uint8_t flags_start, uint8_t flags_end, uint8_t * out);

size_t blake3_simd_degree(void);


// Declarations for implementation-specific functions.
void blake3_compress_in_place_portable(uint32_t cv[8],
				       const uint8_t block[BLAKE3_BLOCK_LEN],
				       uint8_t block_len, uint64_t counter,
				       uint8_t flags);

void blake3_compress_xof_portable(const uint32_t cv[8],
				  const uint8_t block[BLAKE3_BLOCK_LEN],
				  uint8_t block_len, uint64_t counter,
				  uint8_t flags, uint8_t out[64]);

void blake3_hash_many_portable(const uint8_t * const *inputs,
			       size_t num_inputs, size_t blocks,
			       const uint32_t key[8], uint64_t counter,
			       bool increment_counter, uint8_t flags,
			       uint8_t flags_start, uint8_t flags_end,
			       uint8_t * out);


BLAKE3_INLINE void
blake3_store32(void *dst, uint32_t w) {
	uint8_t *p = (uint8_t *) dst;
	p[0] = (uint8_t) (w >> 0);
	p[1] = (uint8_t) (w >> 8);
	p[2] = (uint8_t) (w >> 16);
	p[3] = (uint8_t) (w >> 24);
}

BLAKE3_INLINE uint32_t
blake3_rotr32(uint32_t w, uint32_t c) {
	return (w >> c) | (w << (32 - c));
}

BLAKE3_INLINE void
blake3_g(uint32_t * state, size_t a, size_t b, size_t c, size_t d,
  uint32_t x, uint32_t y) {
	state[a] = state[a] + state[b] + x;
	state[d] = blake3_rotr32(state[d] ^ state[a], 16);
	state[c] = state[c] + state[d];
	state[b] = blake3_rotr32(state[b] ^ state[c], 12);
	state[a] = state[a] + state[b] + y;
	state[d] = blake3_rotr32(state[d] ^ state[a], 8);
	state[c] = state[c] + state[d];
	state[b] = blake3_rotr32(state[b] ^ state[c], 7);
}

BLAKE3_INLINE void
blake3_round_fn(uint32_t state[16], const uint32_t * msg, size_t round) {
	// Select the message schedule based on the round.
	const uint8_t *schedule = BLAKE3_MSG_SCHEDULE[round];

	// Mix the columns.
	blake3_g(state, 0, 4, 8, 12, msg[schedule[0]], msg[schedule[1]]);
	blake3_g(state, 1, 5, 9, 13, msg[schedule[2]], msg[schedule[3]]);
	blake3_g(state, 2, 6, 10, 14, msg[schedule[4]], msg[schedule[5]]);
	blake3_g(state, 3, 7, 11, 15, msg[schedule[6]], msg[schedule[7]]);

	// Mix the rows.
	blake3_g(state, 0, 5, 10, 15, msg[schedule[8]], msg[schedule[9]]);
	blake3_g(state, 1, 6, 11, 12, msg[schedule[10]], msg[schedule[11]]);
	blake3_g(state, 2, 7, 8, 13, msg[schedule[12]], msg[schedule[13]]);
	blake3_g(state, 3, 4, 9, 14, msg[schedule[14]], msg[schedule[15]]);
}

BLAKE3_INLINE void
blake3_compress_pre(uint32_t state[16], const uint32_t cv[8],
	     const uint8_t block[BLAKE3_BLOCK_LEN],
	     uint8_t block_len, uint64_t counter, uint8_t flags) {
	uint32_t block_words[16];
	block_words[0] = blake3_load32(block + 4 * 0);
	block_words[1] = blake3_load32(block + 4 * 1);
	block_words[2] = blake3_load32(block + 4 * 2);
	block_words[3] = blake3_load32(block + 4 * 3);
	block_words[4] = blake3_load32(block + 4 * 4);
	block_words[5] = blake3_load32(block + 4 * 5);
	block_words[6] = blake3_load32(block + 4 * 6);
	block_words[7] = blake3_load32(block + 4 * 7);
	block_words[8] = blake3_load32(block + 4 * 8);
	block_words[9] = blake3_load32(block + 4 * 9);
	block_words[10] = blake3_load32(block + 4 * 10);
	block_words[11] = blake3_load32(block + 4 * 11);
	block_words[12] = blake3_load32(block + 4 * 12);
	block_words[13] = blake3_load32(block + 4 * 13);
	block_words[14] = blake3_load32(block + 4 * 14);
	block_words[15] = blake3_load32(block + 4 * 15);

	state[0] = cv[0];
	state[1] = cv[1];
	state[2] = cv[2];
	state[3] = cv[3];
	state[4] = cv[4];
	state[5] = cv[5];
	state[6] = cv[6];
	state[7] = cv[7];
	state[8] = BLAKE3_IV[0];
	state[9] = BLAKE3_IV[1];
	state[10] = BLAKE3_IV[2];
	state[11] = BLAKE3_IV[3];
	state[12] = blake3_counter_low(counter);
	state[13] = blake3_counter_high(counter);
	state[14] = (uint32_t) block_len;
	state[15] = (uint32_t) flags;

	blake3_round_fn(state, &block_words[0], 0);
	blake3_round_fn(state, &block_words[0], 1);
	blake3_round_fn(state, &block_words[0], 2);
	blake3_round_fn(state, &block_words[0], 3);
	blake3_round_fn(state, &block_words[0], 4);
	blake3_round_fn(state, &block_words[0], 5);
	blake3_round_fn(state, &block_words[0], 6);
}

void
blake3_compress_in_place_portable(uint32_t cv[8],
				  const uint8_t block[BLAKE3_BLOCK_LEN],
				  uint8_t block_len, uint64_t counter,
				  uint8_t flags) {
	uint32_t state[16];
	blake3_compress_pre(state, cv, block, block_len, counter, flags);
	cv[0] = state[0] ^ state[8];
	cv[1] = state[1] ^ state[9];
	cv[2] = state[2] ^ state[10];
	cv[3] = state[3] ^ state[11];
	cv[4] = state[4] ^ state[12];
	cv[5] = state[5] ^ state[13];
	cv[6] = state[6] ^ state[14];
	cv[7] = state[7] ^ state[15];
}

void
blake3_compress_xof_portable(const uint32_t cv[8],
			     const uint8_t block[BLAKE3_BLOCK_LEN],
			     uint8_t block_len, uint64_t counter,
			     uint8_t flags, uint8_t out[64]) {
	uint32_t state[16];
	blake3_compress_pre(state, cv, block, block_len, counter, flags);

	blake3_store32(&out[0 * 4], state[0] ^ state[8]);
	blake3_store32(&out[1 * 4], state[1] ^ state[9]);
	blake3_store32(&out[2 * 4], state[2] ^ state[10]);
	blake3_store32(&out[3 * 4], state[3] ^ state[11]);
	blake3_store32(&out[4 * 4], state[4] ^ state[12]);
	blake3_store32(&out[5 * 4], state[5] ^ state[13]);
	blake3_store32(&out[6 * 4], state[6] ^ state[14]);
	blake3_store32(&out[7 * 4], state[7] ^ state[15]);
	blake3_store32(&out[8 * 4], state[8] ^ cv[0]);
	blake3_store32(&out[9 * 4], state[9] ^ cv[1]);
	blake3_store32(&out[10 * 4], state[10] ^ cv[2]);
	blake3_store32(&out[11 * 4], state[11] ^ cv[3]);
	blake3_store32(&out[12 * 4], state[12] ^ cv[4]);
	blake3_store32(&out[13 * 4], state[13] ^ cv[5]);
	blake3_store32(&out[14 * 4], state[14] ^ cv[6]);
	blake3_store32(&out[15 * 4], state[15] ^ cv[7]);
}

BLAKE3_INLINE void
blake3_hash_one_portable(const uint8_t * input, size_t blocks,
		  const uint32_t key[8], uint64_t counter,
		  uint8_t flags, uint8_t flags_start,
		  uint8_t flags_end, uint8_t out[BLAKE3_OUT_LEN]) {
	uint32_t cv[8];
	memcpy(cv, key, BLAKE3_KEY_LEN);
	uint8_t block_flags = flags | flags_start;
	while(blocks > 0) {
		if(blocks == 1) {
			block_flags |= flags_end;
		}
		blake3_compress_in_place_portable(cv, input, BLAKE3_BLOCK_LEN,
						  counter, block_flags);
		input = &input[BLAKE3_BLOCK_LEN];
		blocks -= 1;
		block_flags = flags;
	}
	memcpy(out, cv, 32);
}

void
blake3_hash_many_portable(const uint8_t * const *inputs, size_t num_inputs,
			  size_t blocks, const uint32_t key[8],
			  uint64_t counter, bool increment_counter,
			  uint8_t flags, uint8_t flags_start,
			  uint8_t flags_end, uint8_t * out) {
	while(num_inputs > 0) {
		blake3_hash_one_portable(inputs[0], blocks, key, counter, flags,
				  flags_start, flags_end, out);
		if(increment_counter) {
			counter += 1;
		}
		inputs += 1;
		num_inputs -= 1;
		out = &out[BLAKE3_OUT_LEN];
	}
}


void
blake3_compress_in_place(uint32_t cv[8],
			 const uint8_t block[BLAKE3_BLOCK_LEN],
			 uint8_t block_len, uint64_t counter, uint8_t flags) {
	blake3_compress_in_place_portable(cv, block, block_len, counter,
					  flags);
}

void
blake3_compress_xof(const uint32_t cv[8],
		    const uint8_t block[BLAKE3_BLOCK_LEN],
		    uint8_t block_len, uint64_t counter, uint8_t flags,
		    uint8_t out[64]) {
	blake3_compress_xof_portable(cv, block, block_len, counter, flags,
				     out);
}

void
blake3_hash_many(const uint8_t * const *inputs, size_t num_inputs,
		 size_t blocks, const uint32_t key[8], uint64_t counter,
		 bool increment_counter, uint8_t flags,
		 uint8_t flags_start, uint8_t flags_end, uint8_t * out) {
	blake3_hash_many_portable(inputs, num_inputs, blocks, key, counter,
				  increment_counter, flags, flags_start,
				  flags_end, out);
}

size_t
blake3_simd_degree(void) {
	return 1;
}


BLAKE3_INLINE void
blake3_chunk_state_init(blake3_chunk_state * self, const uint32_t key[8],
		 uint8_t flags) {
	memcpy(self->cv, key, BLAKE3_KEY_LEN);
	self->chunk_counter = 0;
	memset(self->buf, 0, BLAKE3_BLOCK_LEN);
	self->buf_len = 0;
	self->blocks_compressed = 0;
	self->flags = flags;
}

BLAKE3_INLINE void
chunk_state_reset(blake3_chunk_state * self, const uint32_t key[8],
		  uint64_t chunk_counter) {
	memcpy(self->cv, key, BLAKE3_KEY_LEN);
	self->chunk_counter = chunk_counter;
	self->blocks_compressed = 0;
	memset(self->buf, 0, BLAKE3_BLOCK_LEN);
	self->buf_len = 0;
}

BLAKE3_INLINE size_t
blake3_chunk_state_len(const blake3_chunk_state * self) {
	return (BLAKE3_BLOCK_LEN * (size_t)self->blocks_compressed) +
		((size_t)self->buf_len);
}

BLAKE3_INLINE size_t
blake3_chunk_state_fill_buf(blake3_chunk_state * self,
		     const uint8_t * input, size_t input_len) {
	size_t take = BLAKE3_BLOCK_LEN - ((size_t)self->buf_len);
	if(take > input_len) {
		take = input_len;
	}
	uint8_t *dest = self->buf + ((size_t)self->buf_len);
	memcpy(dest, input, take);
	self->buf_len += (uint8_t) take;
	return take;
}

BLAKE3_INLINE uint8_t
blake3_chunk_state_maybe_start_flag(const blake3_chunk_state * self) {
	if(self->blocks_compressed == 0) {
		return BLAKE3_CHUNK_START;
	} else {
		return 0;
	}
}

typedef struct {
	uint32_t input_cv[8];
	uint64_t counter;
	uint8_t block[BLAKE3_BLOCK_LEN];
	uint8_t block_len;
	uint8_t flags;
} blake3_output_t;

BLAKE3_INLINE blake3_output_t
blake3_make_output(const uint32_t input_cv[8],
	    const uint8_t block[BLAKE3_BLOCK_LEN],
	    uint8_t block_len, uint64_t counter, uint8_t flags) {
	blake3_output_t ret;
	memcpy(ret.input_cv, input_cv, 32);
	memcpy(ret.block, block, BLAKE3_BLOCK_LEN);
	ret.block_len = block_len;
	ret.counter = counter;
	ret.flags = flags;
	return ret;
}

// Chaining values within a given chunk (specifically the compress_in_place
// interface) are represented as words. This avoids unnecessary bytes<->words
// conversion overhead in the portable implementation. However, the hash_many
// interface handles both user input and parent node blocks, so it accepts
// bytes. For that reason, chaining values in the CV stack are represented as
// bytes.
BLAKE3_INLINE void
blake3_output_chaining_value(const blake3_output_t * self, uint8_t cv[32]) {
	uint32_t cv_words[8];
	memcpy(cv_words, self->input_cv, 32);
	blake3_compress_in_place(cv_words, self->block, self->block_len,
				 self->counter, self->flags);
	memcpy(cv, cv_words, 32);
}

BLAKE3_INLINE void
blake3_output_root_bytes(const blake3_output_t * self, uint64_t seek, uint8_t * out,
		  size_t out_len) {
	uint64_t output_block_counter = seek / 64;
	size_t offset_within_block = seek % 64;
	uint8_t wide_buf[64];
	while(out_len > 0) {
		blake3_compress_xof(self->input_cv, self->block,
				    self->block_len, output_block_counter,
				    self->flags | BLAKE3_ROOT, wide_buf);
		size_t available_bytes = 64 - offset_within_block;
		size_t memcpy_len;
		if(out_len > available_bytes) {
			memcpy_len = available_bytes;
		} else {
			memcpy_len = out_len;
		}
		memcpy(out, wide_buf + offset_within_block, memcpy_len);
		out += memcpy_len;
		out_len -= memcpy_len;
		output_block_counter += 1;
		offset_within_block = 0;
	}
}

BLAKE3_INLINE void
blake3_chunk_state_update(blake3_chunk_state * self, const uint8_t * input,
		   size_t input_len) {
	if(self->buf_len > 0) {
		size_t take = blake3_chunk_state_fill_buf(self, input, input_len);
		input += take;
		input_len -= take;
		if(input_len > 0) {
			blake3_compress_in_place(self->cv, self->buf,
						 BLAKE3_BLOCK_LEN,
						 self->chunk_counter,
						 self->
						 flags |
						 blake3_chunk_state_maybe_start_flag
						 (self));
			self->blocks_compressed += 1;
			self->buf_len = 0;
			memset(self->buf, 0, BLAKE3_BLOCK_LEN);
		}
	}

	while(input_len > BLAKE3_BLOCK_LEN) {
		blake3_compress_in_place(self->cv, input, BLAKE3_BLOCK_LEN,
					 self->chunk_counter,
					 self->
					 flags |
					 blake3_chunk_state_maybe_start_flag(self));
		self->blocks_compressed += 1;
		input += BLAKE3_BLOCK_LEN;
		input_len -= BLAKE3_BLOCK_LEN;
	}

	size_t take = blake3_chunk_state_fill_buf(self, input, input_len);
	input += take;
	input_len -= take;
}

BLAKE3_INLINE blake3_output_t
blake3_chunk_state_output(const blake3_chunk_state * self) {
	uint8_t block_flags =
		self->flags | blake3_chunk_state_maybe_start_flag(self) | BLAKE3_CHUNK_END;
	return blake3_make_output(self->cv, self->buf, self->buf_len,
			   self->chunk_counter, block_flags);
}

BLAKE3_INLINE blake3_output_t
blake3_parent_output(const uint8_t block[BLAKE3_BLOCK_LEN],
	      const uint32_t key[8], uint8_t flags) {
	return blake3_make_output(key, block, BLAKE3_BLOCK_LEN, 0, flags | BLAKE3_PARENT);
}

// Given some input larger than one chunk, return the number of bytes that
// should go in the left subtree. This is the largest power-of-2 number of
// chunks that leaves at least 1 byte for the right subtree.
BLAKE3_INLINE size_t
blake3_left_len(size_t content_len) {
	// Subtract 1 to reserve at least one byte for the right side. content_len
	// should always be greater than BLAKE3_CHUNK_LEN.
	size_t full_chunks = (content_len - 1) / BLAKE3_CHUNK_LEN;
	return blake3_round_down_to_power_of_2(full_chunks) * BLAKE3_CHUNK_LEN;
}

// Use SIMD parallelism to hash up to BLAKE3_MAX_SIMD_DEGREE chunks at the same time
// on a single thread. Write out the chunk chaining values and return the
// number of chunks hashed. These chunks are never the root and never empty;
// those cases use a different codepath.
BLAKE3_INLINE size_t
blake3_compress_chunks_parallel(const uint8_t * input, size_t input_len,
			 const uint32_t key[8],
			 uint64_t chunk_counter, uint8_t flags,
			 uint8_t * out) {
#if defined(BLAKE3_TESTING)
	assert(0 < input_len);
	assert(input_len <= BLAKE3_MAX_SIMD_DEGREE * BLAKE3_CHUNK_LEN);
#endif

	const uint8_t *chunks_array[BLAKE3_MAX_SIMD_DEGREE];
	size_t input_position = 0;
	size_t chunks_array_len = 0;
	while(input_len - input_position >= BLAKE3_CHUNK_LEN) {
		chunks_array[chunks_array_len] = &input[input_position];
		input_position += BLAKE3_CHUNK_LEN;
		chunks_array_len += 1;
	}

	blake3_hash_many(chunks_array, chunks_array_len,
			 BLAKE3_CHUNK_LEN / BLAKE3_BLOCK_LEN, key,
			 chunk_counter, true, flags, BLAKE3_CHUNK_START, BLAKE3_CHUNK_END,
			 out);

	// Hash the remaining partial chunk, if there is one. Note that the empty
	// chunk (meaning the empty message) is a different codepath.
	if(input_len > input_position) {
		uint64_t counter =
			chunk_counter + (uint64_t) chunks_array_len;
		blake3_chunk_state chunk_state;
		blake3_chunk_state_init(&chunk_state, key, flags);
		chunk_state.chunk_counter = counter;
		blake3_chunk_state_update(&chunk_state, &input[input_position],
				   input_len - input_position);
		blake3_output_t output = blake3_chunk_state_output(&chunk_state);
		blake3_output_chaining_value(&output,
				      &out[chunks_array_len *
					   BLAKE3_OUT_LEN]);
		return chunks_array_len + 1;
	} else {
		return chunks_array_len;
	}
}

// Use SIMD parallelism to hash up to BLAKE3_MAX_SIMD_DEGREE parents at the same time
// on a single thread. Write out the parent chaining values and return the
// number of parents hashed. (If there's an odd input chaining value left over,
// return it as an additional output.) These parents are never the root and
// never empty; those cases use a different codepath.
BLAKE3_INLINE size_t
blake3_compress_parents_parallel(const uint8_t * child_chaining_values,
			  size_t num_chaining_values,
			  const uint32_t key[8], uint8_t flags,
			  uint8_t * out) {
#if defined(BLAKE3_TESTING)
	assert(2 <= num_chaining_values);
	assert(num_chaining_values <= 2 * BLAKE3_MAX_SIMD_DEGREE_OR_2);
#endif

	const uint8_t *parents_array[BLAKE3_MAX_SIMD_DEGREE_OR_2];
	size_t parents_array_len = 0;
	while(num_chaining_values - (2 * parents_array_len) >= 2) {
		parents_array[parents_array_len] =
			&child_chaining_values[2 * parents_array_len *
					       BLAKE3_OUT_LEN];
		parents_array_len += 1;
	}

	blake3_hash_many(parents_array, parents_array_len, 1, key, 0,	// Parents always use counter 0.
			 false, flags | BLAKE3_PARENT, 0,	// Parents have no start flags.
			 0,	// Parents have no end flags.
			 out);

	// If there's an odd child left over, it becomes an output.
	if(num_chaining_values > 2 * parents_array_len) {
		memcpy(&out[parents_array_len * BLAKE3_OUT_LEN],
		       &child_chaining_values[2 * parents_array_len *
					      BLAKE3_OUT_LEN],
		       BLAKE3_OUT_LEN);
		return parents_array_len + 1;
	} else {
		return parents_array_len;
	}
}

// The wide helper function returns (writes out) an array of chaining values
// and returns the length of that array. The number of chaining values returned
// is the dyanmically detected SIMD degree, at most BLAKE3_MAX_SIMD_DEGREE. Or fewer,
// if the input is shorter than that many chunks. The reason for maintaining a
// wide array of chaining values going back up the tree, is to allow the
// implementation to hash as many parents in parallel as possible.
//
// As a special case when the SIMD degree is 1, this function will still return
// at least 2 outputs. This guarantees that this function doesn't perform the
// root compression. (If it did, it would use the wrong flags, and also we
// wouldn't be able to implement exendable ouput.) Note that this function is
// not used when the whole input is only 1 chunk long; that's a different
// codepath.
//
// Why not just have the caller split the input on the first update(), instead
// of implementing this special rule? Because we don't want to limit SIMD or
// multi-threading parallelism for that update().
static size_t
blake3_compress_subtree_wide(const uint8_t * input,
			     size_t input_len,
			     const uint32_t key[8],
			     uint64_t chunk_counter,
			     uint8_t flags, uint8_t * out) {
	// Note that the single chunk case does *not* bump the SIMD degree up to 2
	// when it is 1. If this implementation adds multi-threading in the future,
	// this gives us the option of multi-threading even the 2-chunk case, which
	// can help performance on smaller platforms.
	if(input_len <= blake3_simd_degree() * BLAKE3_CHUNK_LEN) {
		return blake3_compress_chunks_parallel(input, input_len, key,
						chunk_counter, flags, out);
	}
	// With more than simd_degree chunks, we need to recurse. Start by dividing
	// the input into left and right subtrees. (Note that this is only optimal
	// as long as the SIMD degree is a power of 2. If we ever get a SIMD degree
	// of 3 or something, we'll need a more complicated strategy.)
	size_t left_input_len = blake3_left_len(input_len);
	size_t right_input_len = input_len - left_input_len;
	const uint8_t *right_input = &input[left_input_len];
	uint64_t right_chunk_counter =
		chunk_counter +
		(uint64_t) (left_input_len / BLAKE3_CHUNK_LEN);

	// Make space for the child outputs. Here we use BLAKE3_MAX_SIMD_DEGREE_OR_2 to
	// account for the special case of returning 2 outputs when the SIMD degree
	// is 1.
	uint8_t cv_array[2 * BLAKE3_MAX_SIMD_DEGREE_OR_2 * BLAKE3_OUT_LEN];
	size_t degree = blake3_simd_degree();
	if(left_input_len > BLAKE3_CHUNK_LEN && degree == 1) {
		// The special case: We always use a degree of at least two, to make
		// sure there are two outputs. Except, as noted above, at the chunk
		// level, where we allow degree=1. (Note that the 1-chunk-input case is
		// a different codepath.)
		degree = 2;
	}
	uint8_t *right_cvs = &cv_array[degree * BLAKE3_OUT_LEN];

	// Recurse! If this implementation adds multi-threading support in the
	// future, this is where it will go.
	size_t left_n =
		blake3_compress_subtree_wide(input, left_input_len, key,
					     chunk_counter, flags, cv_array);
	size_t right_n =
		blake3_compress_subtree_wide(right_input, right_input_len,
					     key, right_chunk_counter, flags,
					     right_cvs);

	// The special case again. If simd_degree=1, then we'll have left_n=1 and
	// right_n=1. Rather than compressing them into a single output, return
	// them directly, to make sure we always have at least two outputs.
	if(left_n == 1) {
		memcpy(out, cv_array, 2 * BLAKE3_OUT_LEN);
		return 2;
	}
	// Otherwise, do one layer of parent node compression.
	size_t num_chaining_values = left_n + right_n;
	return blake3_compress_parents_parallel(cv_array, num_chaining_values, key,
					 flags, out);
}

// Hash a subtree with compress_subtree_wide(), and then condense the resulting
// list of chaining values down to a single parent node. Don't compress that
// last parent node, however. Instead, return its message bytes (the
// concatenated chaining values of its children). This is necessary when the
// first call to update() supplies a complete subtree, because the topmost
// parent node of that subtree could end up being the root. It's also necessary
// for extended output in the general case.
//
// As with compress_subtree_wide(), this function is not used on inputs of 1
// chunk or less. That's a different codepath.
BLAKE3_INLINE void
blake3_compress_subtree_to_parent_node(const uint8_t * input, size_t input_len,
				const uint32_t key[8], uint64_t chunk_counter,
				uint8_t flags,
				uint8_t out[2 * BLAKE3_OUT_LEN]) {
#if defined(BLAKE3_TESTING)
	assert(input_len > BLAKE3_CHUNK_LEN);
#endif

	uint8_t cv_array[BLAKE3_MAX_SIMD_DEGREE_OR_2 * BLAKE3_OUT_LEN];
	size_t num_cvs = blake3_compress_subtree_wide(input, input_len, key,
						      chunk_counter, flags,
						      cv_array);

	// If BLAKE3_MAX_SIMD_DEGREE is greater than 2 and there's enough input,
	// compress_subtree_wide() returns more than 2 chaining values. Condense
	// them into 2 by forming parent nodes repeatedly.
	uint8_t out_array[BLAKE3_MAX_SIMD_DEGREE_OR_2 * BLAKE3_OUT_LEN / 2];
	while(num_cvs > 2) {
		num_cvs =
			blake3_compress_parents_parallel(cv_array, num_cvs, key,
						  flags, out_array);
		memcpy(cv_array, out_array, num_cvs * BLAKE3_OUT_LEN);
	}
	memcpy(out, cv_array, 2 * BLAKE3_OUT_LEN);
}

BLAKE3_INLINE void
blake3_hasher_init_base(blake3_hasher * self, const uint32_t key[8], uint8_t flags) {
	memcpy(self->key, key, BLAKE3_KEY_LEN);
	blake3_chunk_state_init(&self->chunk, key, flags);
	self->cv_stack_len = 0;
}

static void
blake3_hasher_init(blake3_hasher * self) {
	blake3_hasher_init_base(self, BLAKE3_IV, 0);
}

static void
blake3_hasher_init_keyed(blake3_hasher * self,
			 const uint8_t key[BLAKE3_KEY_LEN]) {
	uint32_t key_words[8];
	blake3_load_key_words(key, key_words);
	blake3_hasher_init_base(self, key_words, BLAKE3_KEYED_HASH);
}

static void
blake3_hasher_init_derive_key(blake3_hasher * self, const char *context) {
	blake3_hasher context_hasher;
	blake3_hasher_init_base(&context_hasher, BLAKE3_IV, BLAKE3_DERIVE_KEY_CONTEXT);
	blake3_hasher_update(&context_hasher, context, strlen(context));
	uint8_t context_key[BLAKE3_KEY_LEN];
	blake3_hasher_finalize(&context_hasher, context_key, BLAKE3_KEY_LEN);
	uint32_t context_key_words[8];
	blake3_load_key_words(context_key, context_key_words);
	blake3_hasher_init_base(self, context_key_words, BLAKE3_DERIVE_KEY_MATERIAL);
}

// As described in blake3_hasher_push_cv() below, we do "lazy merging", delaying
// merges until right before the next CV is about to be added. This is
// different from the reference implementation. Another difference is that we
// aren't always merging 1 chunk at a time. Instead, each CV might represent
// any power-of-two number of chunks, as long as the smaller-above-larger stack
// order is maintained. Instead of the "count the trailing 0-bits" algorithm
// described in the spec, we use a "count the total number of 1-bits" variant
// that doesn't require us to retain the subtree size of the CV on top of the
// stack. The principle is the same: each CV that should remain in the stack is
// represented by a 1-bit in the total number of chunks (or bytes) so far.
BLAKE3_INLINE void
blake3_hasher_merge_cv_stack(blake3_hasher * self, uint64_t total_len) {
	size_t post_merge_stack_len = (size_t)blake3_popcnt(total_len);
	while(self->cv_stack_len > post_merge_stack_len) {
		uint8_t *parent_node =
			&self->cv_stack[(self->cv_stack_len - 2) *
					BLAKE3_OUT_LEN];
		blake3_output_t output =
			blake3_parent_output(parent_node, self->key,
				      self->chunk.flags);
		blake3_output_chaining_value(&output, parent_node);
		self->cv_stack_len -= 1;
	}
}

// In reference_impl.rs, we merge the new CV with existing CVs from the stack
// before pushing it. We can do that because we know more input is coming, so
// we know none of the merges are root.
//
// This setting is different. We want to feed as much input as possible to
// compress_subtree_wide(), without setting aside anything for the chunk_state.
// If the user gives us 64 KiB, we want to parallelize over all 64 KiB at once
// as a single subtree, if at all possible.
//
// This leads to two problems:
// 1) This 64 KiB input might be the only call that ever gets made to update.
//    In this case, the root node of the 64 KiB subtree would be the root node
//    of the whole tree, and it would need to be BLAKE3_ROOT finalized. We can't
//    compress it until we know.
// 2) This 64 KiB input might complete a larger tree, whose root node is
//    similarly going to be the the root of the whole tree. For example, maybe
//    we have 196 KiB (that is, 128 + 64) hashed so far. We can't compress the
//    node at the root of the 256 KiB subtree until we know how to finalize it.
//
// The second problem is solved with "lazy merging". That is, when we're about
// to add a CV to the stack, we don't merge it with anything first, as the
// reference impl does. Instead we do merges using the *previous* CV that was
// added, which is sitting on top of the stack, and we put the new CV
// (unmerged) on top of the stack afterwards. This guarantees that we never
// merge the root node until finalize().
//
// Solving the first problem requires an additional tool,
// blake3_compress_subtree_to_parent_node(). That function always returns the top
// *two* chaining values of the subtree it's compressing. We then do lazy
// merging with each of them separately, so that the second CV will always
// remain unmerged. (That also helps us support extendable output when we're
// hashing an input all-at-once.)
BLAKE3_INLINE void
blake3_hasher_push_cv(blake3_hasher * self, uint8_t new_cv[BLAKE3_OUT_LEN],
	       uint64_t chunk_counter) {
	blake3_hasher_merge_cv_stack(self, chunk_counter);
	memcpy(&self->cv_stack[self->cv_stack_len * BLAKE3_OUT_LEN], new_cv,
	       BLAKE3_OUT_LEN);
	self->cv_stack_len += 1;
}

static void
blake3_hasher_update(blake3_hasher * self, const void *input,
		     size_t input_len) {
	// Explicitly checking for zero avoids causing UB by passing a null pointer
	// to memcpy. This comes up in practice with things like:
	//   std::vector<uint8_t> v;
	//   blake3_hasher_update(&hasher, v.data(), v.size());
	if(input_len == 0) {
		return;
	}

	const uint8_t *input_bytes = (const uint8_t *)input;

	// If we have some partial chunk bytes in the internal chunk_state, we need
	// to finish that chunk first.
	if(blake3_chunk_state_len(&self->chunk) > 0) {
		size_t take =
			BLAKE3_CHUNK_LEN - blake3_chunk_state_len(&self->chunk);
		if(take > input_len) {
			take = input_len;
		}
		blake3_chunk_state_update(&self->chunk, input_bytes, take);
		input_bytes += take;
		input_len -= take;
		// If we've filled the current chunk and there's more coming, finalize this
		// chunk and proceed. In this case we know it's not the root.
		if(input_len > 0) {
			blake3_output_t output = blake3_chunk_state_output(&self->chunk);
			uint8_t chunk_cv[32];
			blake3_output_chaining_value(&output, chunk_cv);
			blake3_hasher_push_cv(self, chunk_cv,
				       self->chunk.chunk_counter);
			chunk_state_reset(&self->chunk, self->key,
					  self->chunk.chunk_counter + 1);
		} else {
			return;
		}
	}
	// Now the chunk_state is clear, and we have more input. If there's more than
	// a single chunk (so, definitely not the root chunk), hash the largest whole
	// subtree we can, with the full benefits of SIMD (and maybe in the future,
	// multi-threading) parallelism. Two restrictions:
	// - The subtree has to be a power-of-2 number of chunks. Only subtrees along
	//   the right edge can be incomplete, and we don't know where the right edge
	//   is going to be until we get to finalize().
	// - The subtree must evenly divide the total number of chunks up until this
	//   point (if total is not 0). If the current incomplete subtree is only
	//   waiting for 1 more chunk, we can't hash a subtree of 4 chunks. We have
	//   to complete the current subtree first.
	// Because we might need to break up the input to form powers of 2, or to
	// evenly divide what we already have, this part runs in a loop.
	while(input_len > BLAKE3_CHUNK_LEN) {
		size_t subtree_len = blake3_round_down_to_power_of_2(input_len);
		uint64_t count_so_far =
			self->chunk.chunk_counter * BLAKE3_CHUNK_LEN;
		// Shrink the subtree_len until it evenly divides the count so far. We know
		// that subtree_len itself is a power of 2, so we can use a bitmasking
		// trick instead of an actual remainder operation. (Note that if the caller
		// consistently passes power-of-2 inputs of the same size, as is hopefully
		// typical, this loop condition will always fail, and subtree_len will
		// always be the full length of the input.)
		//
		// An aside: We don't have to shrink subtree_len quite this much. For
		// example, if count_so_far is 1, we could pass 2 chunks to
		// blake3_compress_subtree_to_parent_node. Since we'll get 2 CVs back, we'll still
		// get the right answer in the end, and we might get to use 2-way SIMD
		// parallelism. The problem with this optimization, is that it gets us
		// stuck always hashing 2 chunks. The total number of chunks will remain
		// odd, and we'll never graduate to higher degrees of parallelism. See
		// https://github.com/BLAKE3-team/BLAKE3/issues/69.
		while((((uint64_t) (subtree_len - 1)) & count_so_far) != 0) {
			subtree_len /= 2;
		}
		// The shrunken subtree_len might now be 1 chunk long. If so, hash that one
		// chunk by itself. Otherwise, compress the subtree into a pair of CVs.
		uint64_t subtree_chunks = subtree_len / BLAKE3_CHUNK_LEN;
		if(subtree_len <= BLAKE3_CHUNK_LEN) {
			blake3_chunk_state chunk_state;
			blake3_chunk_state_init(&chunk_state, self->key,
					 self->chunk.flags);
			chunk_state.chunk_counter = self->chunk.chunk_counter;
			blake3_chunk_state_update(&chunk_state, input_bytes,
					   subtree_len);
			blake3_output_t output = blake3_chunk_state_output(&chunk_state);
			uint8_t cv[BLAKE3_OUT_LEN];
			blake3_output_chaining_value(&output, cv);
			blake3_hasher_push_cv(self, cv, chunk_state.chunk_counter);
		} else {
			// This is the high-performance happy path, though getting here depends
			// on the caller giving us a long enough input.
			uint8_t cv_pair[2 * BLAKE3_OUT_LEN];
			blake3_compress_subtree_to_parent_node(input_bytes,
							subtree_len,
							self->key,
							self->chunk.
							chunk_counter,
							self->chunk.flags,
							cv_pair);
			blake3_hasher_push_cv(self, cv_pair,
				       self->chunk.chunk_counter);
			blake3_hasher_push_cv(self, &cv_pair[BLAKE3_OUT_LEN],
				       self->chunk.chunk_counter +
				       (subtree_chunks / 2));
		}
		self->chunk.chunk_counter += subtree_chunks;
		input_bytes += subtree_len;
		input_len -= subtree_len;
	}

	// If there's any remaining input less than a full chunk, add it to the chunk
	// state. In that case, also do a final merge loop to make sure the subtree
	// stack doesn't contain any unmerged pairs. The remaining input means we
	// know these merges are non-root. This merge loop isn't strictly necessary
	// here, because hasher_push_chunk_cv already does its own merge loop, but it
	// simplifies blake3_hasher_finalize below.
	if(input_len > 0) {
		blake3_chunk_state_update(&self->chunk, input_bytes, input_len);
		blake3_hasher_merge_cv_stack(self, self->chunk.chunk_counter);
	}
}

static void
blake3_hasher_finalize(const blake3_hasher * self, uint8_t * out,
		       size_t out_len) {
	blake3_hasher_finalize_seek(self, 0, out, out_len);
}

static void
blake3_hasher_finalize_seek(const blake3_hasher * self, uint64_t seek,
			    uint8_t * out, size_t out_len) {
	// Explicitly checking for zero avoids causing UB by passing a null pointer
	// to memcpy. This comes up in practice with things like:
	//   std::vector<uint8_t> v;
	//   blake3_hasher_finalize(&hasher, v.data(), v.size());
	if(out_len == 0) {
		return;
	}
	// If the subtree stack is empty, then the current chunk is the root.
	if(self->cv_stack_len == 0) {
		blake3_output_t output = blake3_chunk_state_output(&self->chunk);
		blake3_output_root_bytes(&output, seek, out, out_len);
		return;
	}
	// If there are any bytes in the chunk state, finalize that chunk and do a
	// roll-up merge between that chunk hash and every subtree in the stack. In
	// this case, the extra merge loop at the end of blake3_hasher_update
	// guarantees that none of the subtrees in the stack need to be merged with
	// each other first. Otherwise, if there are no bytes in the chunk state,
	// then the top of the stack is a chunk hash, and we start the merge from
	// that.
	blake3_output_t output;
	size_t cvs_remaining;
	if(blake3_chunk_state_len(&self->chunk) > 0) {
		cvs_remaining = self->cv_stack_len;
		output = blake3_chunk_state_output(&self->chunk);
	} else {
		// There are always at least 2 CVs in the stack in this case.
		cvs_remaining = self->cv_stack_len - 2;
		output = blake3_parent_output(&self->cv_stack[cvs_remaining * 32],
				       self->key, self->chunk.flags);
	}
	while(cvs_remaining > 0) {
		cvs_remaining -= 1;
		uint8_t parent_block[BLAKE3_BLOCK_LEN];
		memcpy(parent_block, &self->cv_stack[cvs_remaining * 32], 32);
		blake3_output_chaining_value(&output, &parent_block[32]);
		output = blake3_parent_output(parent_block, self->key,
				       self->chunk.flags);
	}
	blake3_output_root_bytes(&output, seek, out, out_len);
}

/*******************************************************************
*
*             X25519 (from tweetnacl. public domain)
*
********************************************************************/

typedef int64_t noise_x25519_gf[16];

static const noise_x25519_gf noise_x25519_121665 = {0xDB41, 1};
static void noise_x25519_car25519(noise_x25519_gf o) {
  int i; int64_t c;
  for(i=0;i<16;i++) {
    o[i] += (1LL << 16);
    c = o[i] >> 16;
    o[(i + 1) * (i < 15)] += c - 1 + 37 * (c - 1) * (i == 15);
    o[i] -= c << 16;
  }
}

static void noise_x25519_sel25519(noise_x25519_gf p, noise_x25519_gf q, int b) {
  int64_t t, i, c = ~(b - 1);
  for(i=0;i<16;i++) {
    t = c & (p[i] ^ q[i]);
    p[i] ^= t;
    q[i] ^= t;
  }
}

static void noise_x25519_pack25519(uint8_t *o, const noise_x25519_gf n) {
  int i, j, b;
  noise_x25519_gf m, t;
  for(i=0;i<16;i++) t[i] = n[i];
  noise_x25519_car25519(t);
  noise_x25519_car25519(t);
  noise_x25519_car25519(t);
  for(j=0;j<2;j++) {
    m[0] = t[0] - 0xffed;
    for (i = 1; i < 15; i++) {
      m[i] = t[i] - 0xffff - ((m[i - 1] >> 16) & 1);
      m[i - 1] &= 0xffff;
    }
    m[15] = t[15] - 0x7fff - ((m[14] >> 16) & 1);
    b = (m[15] >> 16) & 1;
    m[14] &= 0xffff;
    noise_x25519_sel25519(t, m, 1 - b);
  }
  for(i=0;i<16;i++) {
    o[2 * i] = t[i] & 0xff;
    o[2 * i + 1] = t[i] >> 8;
  }
}

static void noise_x25519_unpack25519(noise_x25519_gf o, const uint8_t *n) {
  int i;
  for(i=0;i<16;i++) o[i] = n[2 * i] + ((int64_t)n[2 * i + 1] << 8);
  o[15] &= 0x7fff;
}

static void noise_x25519_A(noise_x25519_gf o, const noise_x25519_gf a, const noise_x25519_gf b) {
  int i;
  for(i=0;i<16;i++) o[i] = a[i] + b[i];
}

static void noise_x25519_Z(noise_x25519_gf o, const noise_x25519_gf a, const noise_x25519_gf b) {
  int i;
  for(i=0;i<16;i++) o[i] = a[i] - b[i];
}

static void noise_x25519_M(noise_x25519_gf o, const noise_x25519_gf a, const noise_x25519_gf b) {
  int64_t i, j, t[31];
  for(i=0;i<31;i++) t[i] = 0;
  for(i=0;i<16;i++) for(j=0;j<16;j++) t[i + j] += a[i] * b[j];
  for(i=0;i<15;i++) t[i] += 38 * t[i + 16];
  for(i=0;i<16;i++) o[i] = t[i];
  noise_x25519_car25519(o);
  noise_x25519_car25519(o);
}

static void noise_x25519_S(noise_x25519_gf o, const noise_x25519_gf a) { noise_x25519_M(o, a, a); }

static void noise_x25519_inv25519(noise_x25519_gf o, const noise_x25519_gf i) {
  noise_x25519_gf c;
  int a;
  for(a=0;a<16;a++) c[a] = i[a];
  for (a = 253; a >= 0; a--) {
    noise_x25519_S(c, c);
    if (a != 2 && a != 4) noise_x25519_M(c, c, i);
  }
  for(a=0;a<16;a++) o[a] = c[a];
}

static void noise_x25519_mult(uint8_t *q, const uint8_t *n, const uint8_t *p) {
  uint8_t z[32];
  int64_t x[80], r, i;
  noise_x25519_gf a, b, c, d, e, f;
  for(i=0;i<31;i++) z[i] = n[i];
  z[31] = (n[31] & 127) | 64;
  z[0] &= 248;
  noise_x25519_unpack25519(x, p);
  for(i=0;i<16;i++) {
    b[i] = x[i];
    d[i] = a[i] = c[i] = 0;
  }
  a[0] = d[0] = 1;
  for (i = 254; i >= 0; --i) {
    r = (z[i >> 3] >> (i & 7)) & 1;
    noise_x25519_sel25519(a, b, r);
    noise_x25519_sel25519(c, d, r);
    noise_x25519_A(e, a, c);
    noise_x25519_Z(a, a, c);
    noise_x25519_A(c, b, d);
    noise_x25519_Z(b, b, d);
    noise_x25519_S(d, e);
    noise_x25519_S(f, a);
    noise_x25519_M(a, c, a);
    noise_x25519_M(c, b, e);
    noise_x25519_A(e, a, c);
    noise_x25519_Z(a, a, c);
    noise_x25519_S(b, a);
    noise_x25519_Z(c, d, f);
    noise_x25519_M(a, c, noise_x25519_121665);
    noise_x25519_A(a, a, d);
    noise_x25519_M(c, c, a);
    noise_x25519_M(a, d, f);
    noise_x25519_M(d, b, x);
    noise_x25519_S(b, e);
    noise_x25519_sel25519(a, b, r);
    noise_x25519_sel25519(c, d, r);
  }
  for(i=0;i<16;i++) {
    x[i + 16] = a[i];
    x[i + 32] = c[i];
    x[i + 48] = b[i];
    x[i + 64] = d[i];
  }
  noise_x25519_inv25519(x + 32, x + 32);
  noise_x25519_M(x + 16, x + 16, x + 32);
  noise_x25519_pack25519(q, x + 16);
}

static void noise_x25519_pub(uint8_t *q, const uint8_t *n) {
	static const uint8_t _9[32] = {9};
	noise_x25519_mult(q, n, _9);
}

static void noise_x25519_keypair(uint8_t *y, uint8_t *x) {
	getrandom(x, 32, 0);
	noise_x25519_pub(y, x);
}

/******************************************
*
*              aegis128l
*
*******************************************/

/*
 * AEGIS-128l based on https://bench.cr.yp.to/supercop/supercop-20200409.tar.xz
 From https://raw.githubusercontent.com/jedisct1/libsodium/master/src/libsodium/crypto_aead/aegis128l/aesni/aead_aegis128l_aesni.c

 * ISC License
 *
 * Copyright (c) 2013-2021
 * Frank Denis <j at pureftpd dot org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#if 0
#ifdef __GNUC__
#pragma GCC target("ssse3")
#pragma GCC target("aes")
#pragma GCC target("sse2")
#endif
#endif

#ifdef __cplusplus
#define AEGIS128L_ALIGNAS(bytes) alignas(bytes)
#else
#define AEGIS128L_ALIGNAS(bytes) _Alignas(bytes)
#endif
#define AEGIS128L_MAXSIZE ((1ULL << 61) - 1)

#include <tmmintrin.h>
#include <wmmintrin.h>


#include <emmintrin.h>

static inline int
aegis128l_crypto_verify(const unsigned char *x_, const unsigned char *y_,
			const int n) {
	const __m128i zero = _mm_setzero_si128();
	volatile __m128i v1, v2, z;
	volatile int m;
	int i;

	const volatile __m128i *volatile x =
		(const volatile __m128i * volatile)(const void *)x_;
	const volatile __m128i *volatile y =
		(const volatile __m128i * volatile)(const void *)y_;
	v1 = _mm_loadu_si128((const __m128i *)&x[0]);
	v2 = _mm_loadu_si128((const __m128i *)&y[0]);
	z = _mm_xor_si128(v1, v2);
	for(i = 1; i < n / 16; i++) {
		v1 = _mm_loadu_si128((const __m128i *)&x[i]);
		v2 = _mm_loadu_si128((const __m128i *)&y[i]);
		z = _mm_or_si128(z, _mm_xor_si128(v1, v2));
	}
	m = _mm_movemask_epi8(_mm_cmpeq_epi32(z, zero));
	v1 = zero;
	v2 = zero;
	z = zero;

	return (int)(((uint32_t) m + 1U) >> 16) - 1;
}

static void
aegis128l_memzero(void *p, size_t len) {
	memset(p, 0, len);
	/* memset_s(p, len, 0, len); */
}

static inline void
aegis128l_update(__m128i * const state, const __m128i d1, const __m128i d2) {
	__m128i tmp;

	tmp = state[7];
	state[7] = _mm_aesenc_si128(state[6], state[7]);
	state[6] = _mm_aesenc_si128(state[5], state[6]);
	state[5] = _mm_aesenc_si128(state[4], state[5]);
	state[4] = _mm_aesenc_si128(state[3], state[4]);
	state[3] = _mm_aesenc_si128(state[2], state[3]);
	state[2] = _mm_aesenc_si128(state[1], state[2]);
	state[1] = _mm_aesenc_si128(state[0], state[1]);
	state[0] = _mm_aesenc_si128(tmp, state[0]);

	state[0] = _mm_xor_si128(state[0], d1);
	state[4] = _mm_xor_si128(state[4], d2);
}

static void
aegis128l_init(const unsigned char *key, const unsigned char *nonce,
	       __m128i * const state) {
	const __m128i c1 =
		_mm_set_epi8(0xdd, 0x28, 0xb5, 0x73, 0x42, 0x31, 0x11, 0x20,
			     0xf1, 0x2f, 0xc2, 0x6d,
			     0x55, 0x18, 0x3d, 0xdb);
	const __m128i c2 =
		_mm_set_epi8(0x62, 0x79, 0xe9, 0x90, 0x59, 0x37, 0x22, 0x15,
			     0x0d, 0x08, 0x05, 0x03,
			     0x02, 0x01, 0x01, 0x00);
	__m128i k;
	__m128i n;
	int i;

	k = _mm_loadu_si128((const __m128i *)(const void *)key);
	n = _mm_loadu_si128((const __m128i *)(const void *)nonce);

	state[0] = _mm_xor_si128(k, n);
	state[1] = c1;
	state[2] = c2;
	state[3] = c1;
	state[4] = _mm_xor_si128(k, n);
	state[5] = _mm_xor_si128(k, c2);
	state[6] = _mm_xor_si128(k, c1);
	state[7] = _mm_xor_si128(k, c2);
	for(i = 0; i < 10; i++) {
		aegis128l_update(state, n, k);
	}
}

static void
aegis128l_mac(unsigned char *mac, unsigned long long adlen,
	      unsigned long long mlen, __m128i * const state) {
	__m128i tmp;
	int i;

	tmp = _mm_set_epi64x(mlen << 3, adlen << 3);
	tmp = _mm_xor_si128(tmp, state[2]);

	for(i = 0; i < 7; i++) {
		aegis128l_update(state, tmp, tmp);
	}

	tmp = _mm_xor_si128(state[6], state[5]);
	tmp = _mm_xor_si128(tmp, state[4]);
	tmp = _mm_xor_si128(tmp, state[3]);
	tmp = _mm_xor_si128(tmp, state[2]);
	tmp = _mm_xor_si128(tmp, state[1]);
	tmp = _mm_xor_si128(tmp, state[0]);

	_mm_storeu_si128((__m128i *) (void *)mac, tmp);
}

static void
aegis128l_enc(unsigned char *const dst, const unsigned char *const src,
	      __m128i * const state) {
	__m128i msg0, msg1;
	__m128i tmp0, tmp1;

	msg0 = _mm_loadu_si128((const __m128i *)(const void *)src);
	msg1 = _mm_loadu_si128((const __m128i *)(const void *)(src + 16));
	tmp0 = _mm_xor_si128(msg0, state[6]);
	tmp0 = _mm_xor_si128(tmp0, state[1]);
	tmp1 = _mm_xor_si128(msg1, state[2]);
	tmp1 = _mm_xor_si128(tmp1, state[5]);
	tmp0 = _mm_xor_si128(tmp0, _mm_and_si128(state[2], state[3]));
	tmp1 = _mm_xor_si128(tmp1, _mm_and_si128(state[6], state[7]));
	_mm_storeu_si128((__m128i *) (void *)dst, tmp0);
	_mm_storeu_si128((__m128i *) (void *)(dst + 16), tmp1);

	aegis128l_update(state, msg0, msg1);
}

static void
aegis128l_dec(unsigned char *const dst, const unsigned char *const src,
	      __m128i * const state) {
	__m128i msg0, msg1;

	msg0 = _mm_loadu_si128((const __m128i *)(const void *)src);
	msg1 = _mm_loadu_si128((const __m128i *)(const void *)(src + 16));
	msg0 = _mm_xor_si128(msg0, state[6]);
	msg0 = _mm_xor_si128(msg0, state[1]);
	msg1 = _mm_xor_si128(msg1, state[2]);
	msg1 = _mm_xor_si128(msg1, state[5]);
	msg0 = _mm_xor_si128(msg0, _mm_and_si128(state[2], state[3]));
	msg1 = _mm_xor_si128(msg1, _mm_and_si128(state[6], state[7]));
	_mm_storeu_si128((__m128i *) (void *)dst, msg0);
	_mm_storeu_si128((__m128i *) (void *)(dst + 16), msg1);

	aegis128l_update(state, msg0, msg1);
}

static int
aegis128l_encrypt_detached(unsigned char *c, unsigned char *mac,
			   unsigned long long *maclen_p,
			   const unsigned char *m, unsigned long long mlen,
			   const unsigned char *ad, unsigned long long adlen,
			   const unsigned char *npub,
			   const unsigned char *k) {
	__m128i state[8];
	AEGIS128L_ALIGNAS(16) unsigned char src[32];
	AEGIS128L_ALIGNAS(16) unsigned char dst[32];
	unsigned long long i;

	aegis128l_init(k, npub, state);

	for(i = 0ULL; i + 32ULL <= adlen; i += 32ULL) {
		aegis128l_enc(dst, ad + i, state);
	}
	if(adlen & 0x1f) {
		memset(src, 0, 32);
		memcpy(src, ad + i, adlen & 0x1f);
		aegis128l_enc(dst, src, state);
	}
	for(i = 0ULL; i + 32ULL <= mlen; i += 32ULL) {
		aegis128l_enc(c + i, m + i, state);
	}
	if(mlen & 0x1f) {
		memset(src, 0, 32);
		memcpy(src, m + i, mlen & 0x1f);
		aegis128l_enc(dst, src, state);
		memcpy(c + i, dst, mlen & 0x1f);
	}

	aegis128l_mac(mac, adlen, mlen, state);
	aegis128l_memzero(state, sizeof state);
	aegis128l_memzero(src, sizeof src);
	aegis128l_memzero(dst, sizeof dst);

	if(maclen_p != NULL) {
		*maclen_p = 16ULL;
	}
	return 0;
}

static int
aegis128l_encrypt(unsigned char *c, unsigned long long *clen_p,
		  const unsigned char *m, unsigned long long mlen,
		  const unsigned char *ad, unsigned long long adlen,
		  const unsigned char *npub, const unsigned char *k) {
	unsigned long long clen = 0ULL;
	int ret;

	if(mlen > AEGIS128L_MAXSIZE) {
		return -1;
	}
	ret = aegis128l_encrypt_detached(c, c + mlen, NULL, m, mlen,
					 ad, adlen, npub, k);
	if(clen_p != NULL) {
		if(ret == 0) {
			clen = mlen + 16ULL;
		}
		*clen_p = clen;
	}
	return ret;
}

static int
aegis128l_decrypt_detached(unsigned char *m, const unsigned char *c,
			   unsigned long long clen, const unsigned char *mac,
			   const unsigned char *ad, unsigned long long adlen,
			   const unsigned char *npub, const unsigned char *k)
{
	__m128i state[8];
	AEGIS128L_ALIGNAS(16) unsigned char src[32];
	AEGIS128L_ALIGNAS(16) unsigned char dst[32];
	AEGIS128L_ALIGNAS(16) unsigned char computed_mac[16];
	unsigned long long i;
	unsigned long long mlen;
	int ret;

	mlen = clen;
	aegis128l_init(k, npub, state);

	for(i = 0ULL; i + 32ULL <= adlen; i += 32ULL) {
		aegis128l_enc(dst, ad + i, state);
	}
	if(adlen & 0x1f) {
		memset(src, 0, 32);
		memcpy(src, ad + i, adlen & 0x1f);
		aegis128l_enc(dst, src, state);
	}
	if(m != NULL) {
		for(i = 0ULL; i + 32ULL <= mlen; i += 32ULL) {
			aegis128l_dec(m + i, c + i, state);
		}
	} else {
		for(i = 0ULL; i + 32ULL <= mlen; i += 32ULL) {
			aegis128l_dec(dst, c + i, state);
		}
	}
	if(mlen & 0x1f) {
		memset(src, 0, 32);
		memcpy(src, c + i, mlen & 0x1f);
		aegis128l_dec(dst, src, state);
		if(m != NULL) {
			memcpy(m + i, dst, mlen & 0x1f);
		}
		memset(dst, 0, mlen & 0x1f);
		state[0] = _mm_xor_si128(state[0],
					 _mm_loadu_si128((const __m128i
							  *)(const void *)
							 dst));
		state[4] =
			_mm_xor_si128(state[4],
				      _mm_loadu_si128((const __m128i *)(const
									void
									*)(dst
									   +
									   16)));
	}

	aegis128l_mac(computed_mac, adlen, mlen, state);
	aegis128l_memzero(state, sizeof state);
	aegis128l_memzero(src, sizeof src);
	aegis128l_memzero(dst, sizeof dst);
	ret = aegis128l_crypto_verify(computed_mac, mac, 16);
	aegis128l_memzero(computed_mac, sizeof computed_mac);
	if(m == NULL) {
		return ret;
	}
	if(ret != 0) {
		memset(m, 0, mlen);
		return -1;
	}
	return 0;
}

/* m = plaintext
   mlen_p = input length on input, decrypted length on output
   nsec = unused
   c = encrypted
   clen = encrypted len
   npub = nonce
 */
static int
aegis128l_decrypt(unsigned char *m, unsigned long long *mlen_p,
		  const unsigned char *c, unsigned long long clen,
		  const unsigned char *ad, unsigned long long adlen,
		  const unsigned char *npub, const unsigned char *k) {
	unsigned long long mlen = 0ULL;
	int ret = -1;

	if(clen >= 16ULL) {
		ret = aegis128l_decrypt_detached
			(m, c, clen - 16ULL, c + clen - 16ULL, ad, adlen,
			 npub, k);
	}
	if(mlen_p != NULL) {
		if(ret == 0) {
			mlen = clen - 16ULL;
		}
		*mlen_p = mlen;
	}
	return ret;
}



static void
noise_print_key(const char *text, const uint8_t * key, size_t size) {
	size_t i;

	printf("%s: ", text);
	for(i = 0; i < size; i++) {
		printf("%02x", key[i]);
	}
	printf("\n");
}

static void
noise_init_key(NoiseCipher * c, uint8_t key[32]) {
	assert(sizeof(c->k) == 32);
	memcpy(c->k, key, sizeof(c->k));
	memset(&c->n, 0, sizeof(c->n));
	c->noise_has_key = 1;
}

static bool
noise_has_key(NoiseCipher * c) {
	return c->noise_has_key;
}

/* used for handling out of order transport messages */
static void
noise_set_nonce(NoiseCipher * c, uint64_t nonce) {
	c->n.n = nonce;
}

static void
noise_init_symmetric(NoiseSymmetric * s, const char *name) {
	char n[33] = { 0 };
	size_t len;
	const char *p;
	blake3_hasher h;

	len = strlen(name);
	if(len < 32) {
		memcpy(n, name, len);
		p = n;
	} else {
		p = name;
	}
	blake3_hasher_init(&h);
	blake3_hasher_update(&h, p, strlen(p));
	blake3_hasher_finalize(&h, s->ck, sizeof(s->ck));
}

/* h = HASH(h || data) */
static void
noise_mix_hash(NoiseSymmetric * s, const uint8_t * data, size_t size) {
	uint8_t temp_k[32];
	blake3_hasher h;
	blake3_hasher_init(&h);
	blake3_hasher_update(&h, s->ck, sizeof(s->ck));
	blake3_hasher_update(&h, data, size);
	blake3_hasher_finalize_seek(&h, 0, s->ck, sizeof(s->ck));
	blake3_hasher_finalize_seek(&h, sizeof(s->ck), temp_k,
				    sizeof(temp_k));
	noise_init_key(&s->cs, temp_k);
}

static void
noise_mix_key(NoiseSymmetric * s, const uint8_t * data, size_t size) {
	blake3_hasher h;
	blake3_hasher_init(&h);
	blake3_hasher_update(&h, s->h, sizeof(s->h));
	blake3_hasher_update(&h, data, size);
	blake3_hasher_finalize(&h, s->h, sizeof(s->h));
}

static void
noise_mix_key_and_hash(NoiseSymmetric * s, const uint8_t * data,
		     size_t size) {
	uint8_t temp_h[32], temp_k[32];
	blake3_hasher h;
	blake3_hasher_init(&h);
	blake3_hasher_update(&h, s->ck, sizeof(s->ck));
	blake3_hasher_update(&h, data, size);
	blake3_hasher_finalize_seek(&h, 0, s->ck, sizeof(s->ck));
	blake3_hasher_finalize_seek(&h, sizeof(s->ck), temp_h,
				    sizeof(temp_h));
	blake3_hasher_finalize_seek(&h, sizeof(s->ck) + sizeof(temp_h),
				    temp_k, sizeof(temp_k));

	noise_mix_hash(s, temp_h, sizeof(temp_h));
	noise_init_key(&s->cs, temp_k);
}

static size_t
noise_encrypt_with_ad(NoiseCipher * c,
		      const void *ad,
		      size_t adlen,
		      const uint8_t * plaintext,
		      size_t plainlen,
		      uint8_t * encrypted, size_t encryptedlen) {

	if(noise_has_key(c)) {
		unsigned long long len = (unsigned long long)encryptedlen;
		int rc = aegis128l_encrypt(encrypted,
					   &len,
					   plaintext,
					   plainlen,
					   (const unsigned char*)ad,
					   adlen,
					   c->n.nonce,
					   c->k);
		if(rc) {
			fprintf(stderr, "%s", "encryptedlen too short\n");
			abort();
		}
		encryptedlen = (size_t)len;
		c->n.n++;
	} else {
		if(encryptedlen < plainlen) {
			fprintf(stderr, "%s", "plaintext too short\n");
			abort();
		}
		memcpy(encrypted, plaintext, plainlen);
		encryptedlen = plainlen;
	}
	return encryptedlen;
}

static int
noise_decrypt_with_ad(NoiseCipher * c,
		      const void *ad,
		      size_t adlen,
		      const uint8_t * encrypted,
		      size_t encryptedlen,
		      uint8_t * plaintext, size_t *plaintextlen) {
	if(noise_has_key(c)) {
		unsigned long long len = (unsigned long long)*plaintextlen;
		int rc = aegis128l_decrypt(plaintext,
					   &len,
					   encrypted,
					   encryptedlen,
					   (const unsigned char*)ad,
					   adlen,
					   c->n.nonce,
					   c->k);
		if(rc) {
			fprintf(stderr, "%s",
				"plaintextlen too short or AD failed\n");
			*plaintextlen = 0;
			return rc;
		}
		*plaintextlen = (size_t)len;
		c->n.n++;
	} else {
		if(*plaintextlen < encryptedlen) {
			fprintf(stderr, "%s", "plaintext too short\n");
			*plaintextlen = 0;
			return -1;
		}
		memcpy(plaintext, encrypted, encryptedlen);
		*plaintextlen = encryptedlen;
	}
	return 0;
}

static size_t
noise_encrypt_and_hash(NoiseSymmetric * ss,
	       const uint8_t * plaintext, size_t plaintextlen,
	       uint8_t * encrypted, size_t encryptedlen) {
	encryptedlen = noise_encrypt_with_ad(&ss->cs,
					     ss->h, sizeof(ss->h),
					     plaintext, plaintextlen,
					     encrypted, encryptedlen);
	noise_mix_hash(ss, encrypted, encryptedlen);
	return encryptedlen;
}

static int
noise_decrypt_and_hash(NoiseSymmetric * ss,
		       const uint8_t * encrypted, size_t encryptedlen,
		       uint8_t * plaintext, size_t *plainlen) {
	int rc = noise_decrypt_with_ad(&ss->cs,
				       ss->h, sizeof(ss->h),
				       encrypted, encryptedlen,
				       plaintext, plainlen);
	if(!rc) {
		noise_mix_hash(ss, encrypted, encryptedlen);
	}
	return rc;
}

static void
noise_generate_keypair(NoiseDH * dh) {
	noise_x25519_keypair(dh->pub, dh->priv);
}

static void
noise_dh(const NoiseDH * mine, const uint8_t their_pub[NOISE_DHLEN],
	uint8_t result[NOISE_DHLEN]) {
	noise_x25519_mult(result, mine->priv, their_pub);
}

NOISE_API void
noise_init(NoiseHandshake * hs, const uint8_t psk[NOISE_PSKLEN]) {
	const char name[] = "Version 1";
	const char *prologue = "Noise_NNpsk0_aesgis128l_blake3_x25519";
	memset(hs, 0, sizeof(*hs));

	assert(sizeof(hs->psk) == 32);
	noise_init_symmetric(&hs->ss, name);
	noise_mix_hash(&hs->ss, (const uint8_t *)prologue, strlen(prologue));
	memcpy(hs->psk, psk, sizeof(hs->psk));
}

static void
noise_builder_init(NoiseBuilder * b, uint8_t * buffer, size_t size,
		      int type, int seq) {
	uint32_t n;

	b->buf = buffer;
	b->size = size;
	b->i = 4;
	if(b->size <
	   1 /* type */  + 3 /* msg# */  + 4 /* byte offset */  +
	   16 /* MAC */ ) {
		fprintf(stderr, "buffer too small: %d", (int)size);
		abort();
	}
	n = (uint32_t) seq << 8;
	n = ((uint32_t) type << (8 - NOISE_MSGBITS));
	memcpy(b->buf, &n, sizeof(uint32_t));
}

static void
noise_builder_add(NoiseBuilder * b, const uint8_t * data, size_t size) {
	memcpy(&b->buf[b->i], data, size);
	b->i += size;
}

static void
noise_builder_offset(NoiseBuilder * b, uint32_t offset) {
	memcpy(&b->buf[b->i], &offset, sizeof(uint32_t));
	b->i += sizeof(uint32_t);
}

NOISE_API size_t
noise_write_messageA(NoiseHandshake * hs, uint8_t * buffer,
		     size_t bufsize, uint32_t msgSize) {
	uint8_t encrypted[16];
	NoiseBuilder msg;
	size_t size;

	noise_builder_init(&msg, buffer, bufsize, NOISE_CONNECT, 0);
	noise_builder_offset(&msg, msgSize);

	noise_mix_key_and_hash(&hs->ss, hs->psk, sizeof(hs->psk));
	noise_generate_keypair(&hs->e);
	noise_mix_hash(&hs->ss, hs->e.pub, sizeof(hs->e.pub));
	noise_mix_key(&hs->ss, hs->e.pub, sizeof(hs->e.pub));
	noise_builder_add(&msg, hs->e.pub, sizeof(hs->e.pub));
	//noise_print_key("noise_write_messageA", hs->e.pub, sizeof(hs->e.pub));
	size = noise_encrypt_and_hash(&hs->ss, 0, 0, &msg.buf[msg.i], sizeof(encrypted)); /* payload, len */
	assert(size == 16);
	msg.i += size;
	assert(msg.i < msg.size);
	return msg.i;
}

static void
noise_split(NoiseSymmetric * ss, NoiseCipher * c1,
        NoiseCipher * c2) {
	uint8_t t1[32], t2[32];
	blake3_hasher h;

	blake3_hasher_init(&h);
	blake3_hasher_update(&h, ss->ck, sizeof(ss->ck));
	blake3_hasher_finalize_seek(&h, 0, t1, sizeof(t1));
	blake3_hasher_finalize_seek(&h, sizeof(t1), t2, sizeof(t2));

	noise_init_key(c1, t1);
	noise_init_key(c2, t2);
}

NOISE_API size_t
noise_write_messageB(NoiseHandshake * hs, uint8_t * buffer, size_t size) {
	uint8_t key[32];
	NoiseBuilder m;
	size_t sz;

	noise_builder_init(&m, buffer, size, NOISE_CONNECTED, 1);
	//printf("type=%d\n", m.buf[0]);
	noise_builder_offset(&m, 0);

	noise_generate_keypair(&hs->e);
	noise_mix_hash(&hs->ss, hs->e.pub, sizeof(hs->e.pub));
	noise_mix_key(&hs->ss, hs->e.pub, sizeof(hs->e.pub));
	noise_dh(&hs->e, hs->re, key);
	//noise_print_key("noise_write_messageB remote", hs->re, sizeof(hs->re));
	noise_mix_key(&hs->ss, key, sizeof(key));
	//noise_print_key("noise_write_messageB key", key, sizeof(key));
	noise_builder_add(&m, hs->e.pub, sizeof(hs->e.pub));
	//noise_print_key("noise_write_messageB pub", hs->e.pub, sizeof(hs->e.pub));
	sz = noise_encrypt_and_hash(&hs->ss, 0, 0, &m.buf[m.i], m.size - m.i);
	m.i += sz;
	noise_split(&hs->ss, &hs->ours, &hs->theirs);
	return m.i;
}

NOISE_API size_t
noise_write_message(NoiseCipher * cs,
		    const void * data,
		    size_t size, uint8_t * output, size_t outputlen) {
	size_t result;
	int len;
	len = (int)(size + 16);

	result = noise_encrypt_with_ad(cs, &len, sizeof(len), (const uint8_t*)data, size,
				       output, outputlen);
	return result;
}

static size_t
noise_encrypt(NoiseHandshake *hs, const uint8_t *data, size_t size,
	uint8_t *output, size_t outputlen) {
	return noise_write_message(&hs->ours, data, size, output, outputlen);
}

static void
noise_reader_init(NoiseReader * m, const uint8_t * buffer, size_t size) {
	m->buf = buffer;
	m->size = size;
	m->i = 0;

	uint32_t seq, offset;
	memcpy(&seq, m->buf, 4);
	seq >>= 8;
	memcpy(&offset, m->buf + 4, 4);
}

static int
noise_reader_type(NoiseReader * m) {
	return m->buf[m->i++];
}

static int
noise_reader_seq(NoiseReader * m) {
	uint32_t seq;
	memcpy(&seq, &m->buf[m->i - 1], sizeof(uint32_t));
	seq >>= 8;
	m->i += 3;
	return (int)seq;
}

static uint32_t
noise_reader_offset(NoiseReader * m) {
	uint32_t x;
	memcpy(&x, &m->buf[m->i], sizeof(uint32_t));
	m->i += sizeof(uint32_t);
	return x;
}

static const uint8_t *
noise_reader_data(NoiseReader * m) {
	return &m->buf[m->i];
}

static size_t
noise_reader_size(NoiseReader * m) {
	return m->size - m->i;
}

static void
noise_reader_read(NoiseReader * m, uint8_t * buf, size_t size) {
	memcpy(buf, &m->buf[m->i], size);
	m->i += size;
}

NOISE_API int
noise_read_messageA(NoiseHandshake * hs,
		    const uint8_t * buffer,
		    size_t size, uint8_t * output, size_t *outputlen) {
	NoiseReader r;

	noise_reader_init(&r, buffer, size);
	noise_reader_type(&r);
	noise_reader_seq(&r);
	noise_reader_offset(&r);
	/* read remote ephemeral public key */
	noise_reader_read(&r, hs->re, sizeof(hs->re));
	//noise_print_key("noise_read_messageA remote", hs->re, sizeof(hs->re));
	noise_mix_key_and_hash(&hs->ss, hs->psk, sizeof(hs->psk));
	noise_mix_hash(&hs->ss, hs->re, sizeof(hs->re));
	noise_mix_key(&hs->ss, hs->re, sizeof(hs->re));
	return noise_decrypt_and_hash(&hs->ss, noise_reader_data(&r),
	                  noise_reader_size(&r), output,
				      outputlen);
}

NOISE_API int
noise_read_messageB(NoiseHandshake * hs,
		    const uint8_t * buffer,
		    size_t size, uint8_t * output, size_t *outputlen) {
	NoiseReader r;
	uint8_t key[32];
	int rc;

	noise_reader_init(&r, buffer, size);
	noise_reader_type(&r);
	noise_reader_seq(&r);
	noise_reader_offset(&r);
	noise_reader_read(&r, hs->re, sizeof(hs->re));
	noise_mix_hash(&hs->ss, hs->re, sizeof(hs->re));
	noise_mix_key(&hs->ss, hs->re, sizeof(hs->re));
	noise_dh(&hs->e, hs->re, key);
	//noise_print_key("noise_read_messageB remote", hs->re, sizeof(hs->re));
	noise_mix_key(&hs->ss, key, sizeof(key));
	//noise_print_key("noise_read_messageB key", key, sizeof(key));
	rc = noise_decrypt_and_hash(&hs->ss, noise_reader_data(&r),
	                noise_reader_size(&r), output,
				    outputlen);
	if(!rc) {
		noise_split(&hs->ss, &hs->theirs, &hs->ours);
	}
	return rc;
}

NOISE_API size_t
noise_read_message(NoiseCipher * cs, const uint8_t * sent,
		   size_t sent_size, uint8_t * recv, size_t recv_size) {
	int rc;
	int len = (int)sent_size;

	rc = noise_decrypt_with_ad(cs, &len, sizeof(len), sent, sent_size,
				   recv, &recv_size);
	if(rc) {
		return 0;
	}
	return recv_size;
}

static size_t
noise_decrypt(NoiseHandshake *hs, const uint8_t *sent, size_t sent_size,
	uint8_t *recv, size_t recv_size) {
	return noise_read_message(&hs->theirs, sent, sent_size, recv, recv_size);
}

/* packet format:
 * uint64_t seq#/nonce
 * uint8_t flags
 * uint8_t msg#
 * u48 offset - up to 256 TB
 *
 * or
 *
 * uint8_t flags = Connect, Connected, MsgStart, Msg, Ack
 * u24 msg#
 * uint32_t msg byte offset
 */

static void
noise_generate_psk(uint8_t key[NOISE_PSKLEN]) {
	int i = getrandom(key, NOISE_PSKLEN, 0);
	if(i != NOISE_PSKLEN) {
		fprintf(stderr, "getrandom failed: %s", strerror(errno));
		abort();
	}
}

static void
noise_print_psk(uint8_t key[NOISE_PSKLEN]) {
	printf("uint8_t psk[%d] = {", NOISE_PSKLEN);

	int first = 1;
	for(int j = 0; j < NOISE_PSKLEN / 8; j++) {
		for(int i = 0; i < 8; i++) {
			if(!first) {
				printf(",");
			}
			if(!i) {
				printf("\n\t");
			}
			printf("0x%02x", key[i + j * 8]);
			first = 0;
		}
	}
	printf("};");
}


#ifdef NOISE_EXAMPLE
int
main(int argc, char **argv) {
	uint8_t psk[32] = {
		0xee, 0x43, 0xcf, 0x31, 0x7a, 0x02, 0x8d, 0x84,
		0x60, 0xe1, 0x3a, 0x33, 0x0e, 0x2d, 0x9b, 0x5c,
		0x5e, 0x63, 0xc2, 0x9e, 0x6c, 0xee, 0xe5, 0x6a,
		0x6f, 0x56, 0x92, 0xce, 0xf3, 0x4b, 0x62, 0xec
	};

	if(argc == 2 && !strcmp(argv[1], "psk")) {
		uint8_t psk[NOISE_PSKLEN];
		noise_generate_psk(psk);
		noise_print_psk(psk);
	} else if(argc == 2 && !strcmp(argv[1], "dh")) {
		NoiseDH x, y;
		uint8_t k1[32] = { 0 }, k2[32] = { 0 };
		noise_generate_keypair(&x);
		noise_generate_keypair(&y);
		noise_dh(&x, y.pub, k1);
		noise_dh(&y, x.pub, k2);
		noise_print_key("k1", k1, sizeof(k1));
		noise_print_key("k2", k2, sizeof(k2));
	} else if(argc == 2 && !strcmp(argv[1], "msg")) {
		NoiseHandshake server = { 0 }, client = { 0 };
		uint8_t send[1024], recv[1024];
		size_t sz2;
		int i;
		noise_init(&server, psk);
		noise_init(&client, psk);

		int sz = noise_write_messageA(&client, send, sizeof(send),
					      sizeof("hello"));
		printf("cln sent %d bytes\n", sz);
		sz2 = sizeof(recv);
		printf("srv reading msg A\n");
		int rc = noise_read_messageA(&server, send, sz, recv, &sz2);
		assert(!rc);
		printf("srv received %d bytes\n", (int)sz2);

		rc = noise_write_messageB(&server, send, sizeof(send));
		printf("srv sent %d bytes\n", rc);
		sz2 = sizeof(recv);
		printf("cln reading msg B\n");
		rc = noise_read_messageB(&client, send, rc, recv, &sz2);
		assert(!rc);
		printf("cln received %d bytes\n", (int)sz2);

		for(i = 0; i < 4; i++) {
			sz2 = noise_write_message(&client.ours, "testing",
						  sizeof("testing"), send,
						  sizeof(send));
			assert(sz2 >= sizeof("testing"));
			printf("cln sent %d bytes\n", (int)sz2);

			sz2 = noise_read_message(&server.theirs, send, sz2,
						 recv, sizeof(recv));
			assert(sz2 == sizeof("testing"));
			printf("srv received %s\n", (char *)recv);
		}

		for(i = 0; i < 4; i++) {
			sz2 = noise_write_message(&server.ours,
						  "server-side test",
						  sizeof("server-side test"),
						  send, sizeof(send));
			assert(sz2 >= sizeof("testing"));
			printf("srv sent %d bytes\n", (int)sz2);

			sz2 = noise_read_message(&client.theirs, send, sz2,
						 recv, sizeof(recv));
			printf("cln received %d bytes\n", (int)sz2);
			assert(sz2 == sizeof("server-side test"));
			printf("cln received %s\n", (char *)recv);
		}

	} else {
		printf("usage: cache msg\n");
	}
	return 0;
}
#endif

/*
 License for everything except AEGIS section which has its own license (ISC)

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
#endif
