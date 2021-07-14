#ifndef RSA_H
#define RSA_H

#include <stdint.h>

#if defined(RSA_STATIC) || defined(RSA_EXAMPLE)
#define RSA_API static
#define RSA_IMPLEMENTATION
#else
#define RSA_API extern
#endif

RSA_API int
rsa_sign(uint8_t *dst, uint8_t *hash, int nhash, int sha,
	const uint8_t *e, int ne, const uint8_t *m, int me);

RSA_API int
rsa_verify(const uint8_t *hash, int nhash, const uint8_t *sig, int nsig,
	const uint8_t *e, int ne, const uint8_t *m, int nm);

#endif

#ifdef RSA_IMPLEMENTATION

#include <assert.h>
#include <stdint.h>
#include <string.h>

#define RSA_SHA1 2
#define RSA_SHA224 3
#define RSA_SHA256 4
#define RSA_SHA384 5
#define RSA_SHA512 6

/*******************************************************************
*
*                  Big integer math for RSA
*
********************************************************************/

#if defined(__GNUC__) && defined(__LP64__)
	#define RSA_INT_MASK 7
	#define RSA_INT64
	#define RSA_INT_MAX 0xFFFFFFFFFFFFFFFF

	typedef uint64_t rsa_digit;
	typedef __uint128_t rsa_longdigit;
	typedef __int128_t rsa_ilongdigit;
#else
	#define RSA_INT_MASK 3
	#define RSA_INT32
	#define RSA_INT_MAX 0xFFFFFFFF

	typedef uint32_t rsa_digit;
	typedef uint64_t rsa_longdigit;
	typedef int64_t rsa_ilongdigit;
#endif

#define RSA_INT_BITS (sizeof(rsa_digit)*8)
#define RSA_DIGITS (8192/8/sizeof(rsa_digit) + 2)

/* little endian - d[0] is least significant. d[n-1] is most significant */
typedef struct { rsa_digit d[RSA_DIGITS]; int n; } RsaInt;

static void rsa_int_print(const char* name, RsaInt *b) {}

/* init from big endian - data[0] = most significant byte */
static void
rsa_int_init(RsaInt *b, const uint8_t *d, int n) {
	int i,j,k;
	while(!*d && n) { ++d; --n; }
	b->n = (n + sizeof(rsa_digit) - 1) / sizeof(rsa_digit);
	memset(b->d, 0, b->n * sizeof(rsa_digit));
	assert(b->n <= RSA_DIGITS);
	for(i=0,j=n-1;j>=0;i++,j--) {
		k = i / sizeof(rsa_digit);
		b->d[k] |= (rsa_digit)d[j] << (8 * (i & RSA_INT_MASK));
	}
}

/* to big endian bytes */
static int
rsa_int2bytes(RsaInt *b, uint8_t *d, int n) {
        int i, j;
		for(i=(int)(b->n * sizeof(rsa_digit))-1,j=0;i>=0&&n;i--,n--) {
               	uint8_t x = b->d[i / sizeof(rsa_digit)] >> (8 * (i & RSA_INT_MASK));
        	if(j || x) {
			*d++ = x;
			j++;
		} /* else skip leading zeros */
	}
        return j;
}

static int
rsa_int_iszero(RsaInt *x) {
        int i;
		for(i=0;i<x->n;i++)
		if(x->d[i]) return 0;
        return 1;
}

/* x = x + y */
static void
rsa_int_add(RsaInt *x, RsaInt *y) {
	int i, n = x->n < y->n ? x->n : y->n;
	rsa_digit carry = 0;
        for(i=0;i<RSA_DIGITS;i++) {
		rsa_digit n1 = i < x->n ? x->d[i] : 0;
		rsa_digit n2 = i < y->n ? y->d[i] : 0;
		if(i > x->n && i > y->n && !carry) break;
                rsa_longdigit d = (rsa_longdigit)n1 + n2 + carry;
                x->d[i] = d;
		if(d) x->n = i + 1;
                carry = d >> RSA_INT_BITS;
	}
}

/* x = x - y */
static void
rsa_int_sub(RsaInt *x, RsaInt *y)
{
        int i, n, count = 0;
        rsa_digit d, borrow = 0;
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
rsa_int_mult(RsaInt *dst, const RsaInt *x, const RsaInt *y) {
        int i, j;
	rsa_longdigit n, k;
	n = x->n + y->n + 1;
	memset(dst->d, 0, (n > RSA_DIGITS ? RSA_DIGITS : n) * sizeof(rsa_digit));
	dst->n = 0;
	    for(i=0;i<y->n;i++) {
                rsa_digit carry = 0;
				for(j=0;i + j < RSA_DIGITS && j < x->n;j++) {
                        assert(i + j < RSA_DIGITS * 2);
                        n = (rsa_longdigit)x->d[j] * y->d[i] + carry;
                        k = n + dst->d[i + j];
                        dst->d[i + j] = (rsa_digit)k;
						if(i + j > dst->n && k) dst->n = i + j;
                        carry = (rsa_digit)(k >> RSA_INT_BITS);
                }

                dst->d[i + j] += carry;
				if(carry && i + j > dst->n) dst->n = i + j;
        }
		++dst->n;
		assert(dst->n <= RSA_DIGITS * 2);
}

static int
rsa_int_leadzero(rsa_digit x) {
   int n;
   if (x == 0) return sizeof(x * 8);
   n = 0;
#ifdef RSA_INT64
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
rsa_int_mod(RsaInt *remainder, RsaInt *dividend, RsaInt *divisor) {
   const rsa_longdigit b = (rsa_longdigit)1 << RSA_INT_BITS;
   rsa_longdigit qhat; /* estimated quotient digit */
   rsa_longdigit rhat; /* remainder */
   rsa_longdigit p; /* product */
   rsa_ilongdigit t, k;
   int s, i, j;
   RsaInt t1, t2;
   rsa_digit *r=remainder->d, *u=dividend->d, *v=divisor->d;
   rsa_digit *un=t1.d, *vn=t2.d; /* normalized dividend and divisor */
   int n, m=dividend->n;

   while(divisor->n && !divisor->d[divisor->n-1])
	--divisor->n; /* strip leading zeros */

   /* if dividend > divisor then return dividend */
   if(dividend->n < divisor->n) {
	*remainder = *dividend;
	return;
   }
   n=divisor->n;

   assert(m < RSA_DIGITS - 1);
   assert(m >= n);
   assert(n > 0);
   assert(v[n-1] != 0);

   /* Normalize by shifting v left just enough so that its high-order
   bit is on, and shift u left the same amount. We may have to append a
   high-order digit on the dividend; we do that unconditionally. */

   s = rsa_int_leadzero(v[n-1]);
   for (i = n - 1; i > 0; i--)
      vn[i] = (v[i] << s) | ((rsa_longdigit)v[i-1] >> (RSA_INT_BITS-s));
   vn[0] = v[0] << s;

   /*  un = (rsa_digit *)alloca(4* (m + 1)); */
   un[m] = (rsa_longdigit)u[m-1] >> (RSA_INT_BITS-s);
   for (i = m - 1; i > 0; i--)
      un[i] = (u[i] << s) | ((rsa_longdigit)u[i-1] >> (RSA_INT_BITS-s));
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
         t = un[i+j] - k - (p & (rsa_ilongdigit)RSA_INT_MAX);
         un[i+j] = t;
         k = (p >> RSA_INT_BITS) - (t >> RSA_INT_BITS);
      }
      t = un[j+n] - k;
      un[j+n] = t;

      if (t < 0) {              /* If we subtracted */
         k = 0;
         for (i = 0; i < n; i++) {
            t = (rsa_longdigit)un[i+j] + vn[i] + k;
            un[i+j] = t;
            k = t >> RSA_INT_BITS;
         }
         un[j+n] = un[j+n] + k;
      }
   }

   /* unnormalize remainder */
      for (i = 0; i < n-1; i++)
         r[i] = (un[i] >> s) | ((rsa_longdigit)un[i+1] << (RSA_INT_BITS-s));
      r[n-1] = un[n-1] >> s;

	  remainder->n = n;
	  while(remainder->n && !remainder->d[remainder->n-1])
		--remainder->n; /* strip leading zeros */
}

/* dst = pow(x, e) % m */
static void
rsa_int_pow(RsaInt *dst, RsaInt *x, RsaInt *e, RsaInt *m)
{
        RsaInt mult;
        int start_digit, i;
        rsa_digit high_bit, bit;

	assert(e->n);
	assert(e->d[e->n-1]);

        /* find highest non-zero digit */
	    for(start_digit=e->n-1;start_digit > 0;--start_digit)
                if(e->d[start_digit])
                        break;

        high_bit = (rsa_digit)1 << (RSA_INT_BITS-1);
        while(high_bit && !(e->d[start_digit] & high_bit))
                high_bit >>= 1;

        if(!start_digit && !high_bit) {
                /* anything but zero raised to zero is one */
                if(rsa_int_iszero(x)) dst->d[0] = 0;
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
                rsa_digit digit = e->d[i];
                bit = 1;
                do {
                        RsaInt tmp;
                        if(digit & bit) {
                                rsa_int_mult(&tmp, dst, &mult);
				rsa_int_mod(dst, &tmp, m);
                        }
                        rsa_int_mult(&tmp, &mult, &mult);
			rsa_int_mod(&mult, &tmp, m);
                        bit <<= 1;
                } while(bit && (i != start_digit || bit <= high_bit));
        }
}

/*******************************************************************
*
*               RSA sign, verify, encrypt, decrypt
*
********************************************************************/

/* PKCS 1.5 RSA signing from RFC 3447 */
static int
rsa_sign1(uint8_t *dst, uint8_t *hash, int nhash, int sha, RsaInt *e, RsaInt *m) {
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
	int n = m->n * sizeof(rsa_digit);
	RsaInt msg, sig;

	/* format is 0x00 0x01 0xFF 0xFF 0xFF ... 0x00 (DigestInfo) (hash) */
	memset(tmp, 0xFF, n);
	tmp[0] = 0;
	tmp[1] = 1;
	end = tmp + n - nhash;
	memcpy(end, hash, nhash);
	switch(sha) {
	case RSA_SHA1: end-=sizeof sha1; memcpy(end, sha1, sizeof sha1); break;
	case RSA_SHA256: end-=sizeof sha256; memcpy(end, sha256, sizeof sha256); break;
	case RSA_SHA384: end-=sizeof sha384; memcpy(end, sha384, sizeof sha384); break;
	case RSA_SHA512: end-=sizeof sha512; memcpy(end, sha512, sizeof sha512); break;
	}
	--end; *end = 0;
	rsa_int_init(&msg, tmp, n);
	/* printf("premsg2="); tls_int_print(&msg); */
	rsa_int_pow(&sig, &msg, e, m);
	/* printf("sig="); tls_int_print(&sig); */
	rsa_int2bytes(&sig, dst, n);
	/* tls_dumphex("sig2=", dst, n); */
	return n;
}

RSA_API int
rsa_sign(uint8_t *dst, uint8_t *hash, int nhash, int sha, const uint8_t *e, int ne, const uint8_t *m, int me) {
	RsaInt ie, im;
	rsa_int_init(&ie, e, ne);
	rsa_int_init(&im, m, me);

	return rsa_sign1(dst, hash, nhash, sha, &ie, &im);
}

#if 0
static int
rsa_sign_with_key(uint8_t *dst, uint8_t *hash, int nhash, int sha, const void *key, int nkey) {
	RsaAsn1 nodes[40];
	int count = rsa_asn1_parse(nodes, sizeof nodes / sizeof nodes[0], key, nkey);
	RsaInt e, m;
	printf("count=%d\n", count);
	//asn1_print(nodes, count);
	/* format is nodes[0] = seq */
	/* nodes[0].child[0] = version */
	/* nodes[0].child[1] = modulus */
	/* nodes[0].child[2] = public exponent */
	/* nodes[0].child[3] = private exponent */
	rsa_int_init(&m, nodes[0].child->next->data, nodes[0].child->next->size);
	rsa_int_init(&e, nodes[0].child->next->next->next->data, nodes[0].child->next->next->next->size);

	printf("child size=%d\n", nodes[0].child->next->size);
	rsa_int_print("exponent", &e);
	rsa_int_print("modulus", &m);
	rsa_debug("mlen=%d\n", (int)(m.n * sizeof(rsa_digit)));
	return rsa_sign(dst, hash, nhash, sha, &e, &m);
}
#endif

static int
rsa_verify1(const uint8_t *hash, int nhash, const uint8_t *sig, int nsig, RsaInt *e, RsaInt *m) {
	RsaInt i, sig2;
	uint8_t buf[1024];
	int n;

	rsa_int_init(&i, sig, nsig);
	rsa_int_pow(&sig2, &i, e, m);
	n = rsa_int2bytes(&sig2, buf, sizeof buf);
	if(n < nhash) return -1;
	return !memcmp(buf + n - nhash, hash, nhash) ? 0 : -1;
}

RSA_API int
rsa_verify(const uint8_t *hash, int nhash, const uint8_t *sig, int nsig,
	const uint8_t *e, int ne, const uint8_t *m, int nm) {
	RsaInt ie, im;
	rsa_int_init(&ie, e, ne);
	rsa_int_init(&im, m, nm);

	return rsa_verify1(hash, nhash, sig, nsig, &ie, &im);
}

#endif

#ifdef RSA_EXAMPLE
int main(int argc, char **argv) {


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

