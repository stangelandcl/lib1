#ifndef TEAB_H
#define TEAB_H

/*
 * #define TEAB_IMPLEMENTATION in one file before including or
 * #define TEAB_STATIC before each include
 * example at bottom of file
 * public domain license at end of file
 */

#if defined(TEAB_STATIC) || defined(TEAB_EXAMPLE)
#define TEAB_API static
#define TEAB_IMPLEMENTATION
#else
#define TEAB_API extern
#endif

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

TEAB_API void teab_encrypt(uint32_t *data, uint32_t ndata, uint32_t key[4]);
TEAB_API void teab_decrypt(uint32_t *data, uint32_t ndata, uint32_t key[4]);

#ifdef __cplusplus
}
#endif

#endif

#ifdef TEAB_IMPLEMENTATION
/* Based on http://www.cix.co.uk/~klockstone/xtea.pdf */
#include <assert.h>


/* teab is a block version of tean.
It will encode or decode n words as a single block where n > 1.
data is the n word data vector,
k is the 4 word key */

TEAB_API void
teab_encrypt(uint32_t *data, uint32_t ndata, uint32_t key[4]) {
    assert(ndata);
    if(!ndata) return;
    uint32_t z=data[ndata-1], sum=0,e, DELTA=0x9e3779b9;
    uint32_t p, q;
    q=6+52/ndata ;
    while (q-- > 0) {
        sum += DELTA;
        e = sum>>2&3;
        for (p=0; p<ndata; p++ )
            z=data[p] += (((z<<4) ^ (z>>5)) + z) ^ (key[(p & 3) ^ e] + sum);
    }
}

TEAB_API void
teab_decrypt(uint32_t *data, uint32_t ndata, uint32_t key[4]) {
    assert(ndata);
    if(!ndata) return;
    uint32_t z=data[ndata-1], sum=0,e, DELTA=0x9e3779b9;
    uint32_t p, q;
    q=6+52/ndata;
    sum=q*DELTA ;
    while (sum != 0) {
        e= sum>>2 & 3;
        for (p=ndata-1; p>0; p--) {
            z=data[p-1];
            data[p] -= (((z<<4) ^ (z>>5)) + z) ^ (key[(p & 3) ^ e] + sum);
        }
        z=data[ndata-1];
        data[0] -= (((z<<4) ^ (z>>5)) + z) ^ (key[(p & 3) ^ e] + sum);
        sum-=DELTA ;
    }
}

#endif

#ifdef TEAB_EXAMPLE
#include <stdio.h>
#include <string.h>
int main(int argc, char **argv) {
    char data[16] = "testing";
    char control[16] = "testing";
    char key[16] = "key";
    uint32_t *p = (uint32_t*)key;

    teab_encrypt((uint32_t*)data, sizeof data / sizeof(uint32_t), p);
    assert(memcmp(data, control, sizeof data));
    teab_decrypt((uint32_t*)data, sizeof data / sizeof(uint32_t), p);
    assert(!memcmp(data, control, sizeof data));

    printf("Success\n");
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
