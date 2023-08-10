/* SPDX-License-Identifier: Unlicense */
#ifndef CSV_H
#define CSV_H

#if defined(CSV_STATIC) || defined(CSV_EXAMPLE)
#define CSV_API static
#define CSV_IMPLEMENTATION
#else
#define CSV_API extern
#endif


#include <stddef.h>

typedef struct Csv {
    char *text;
    size_t i, ntext;
} Csv;

typedef struct CsvStr {
    char *text;
    size_t ntext;
} CsvStr;

CSV_API void csv_init(Csv *csv, char *text, size_t ntext);
/* returns number of columns parsed or zero for complete or -1 for error */
CSV_API int csv_next(Csv *csv, CsvStr *cols, int ncols);


#endif

#ifdef CSV_IMPLEMENTATION

#include <string.h>

CSV_API void
csv_init(Csv *csv, char *text, size_t ntext) {
    csv->i = 0;
    csv->text = text;
    csv->ntext = ntext;
}

CSV_API int
csv_next(Csv *csv, CsvStr *cols, int ncols) {
    int in_escape = 0, prior_escape = 0, nstrs = 0;
    size_t start, ibuf = 0;
    char buf[65536];

    start = csv->i;
    while(csv->i <= csv->ntext) {
        char c = csv->text[csv->i];
        switch(c) {
        case '"':
            if (!in_escape) in_escape = 1;
            else if (!prior_escape)
            {
                if (csv->i + 1 < csv->ntext && csv->text[csv->i + 1] == '"')
                    prior_escape = 1;
                else in_escape = 0;
            }
            else prior_escape = 0;
            ++csv->i;
            break;
        case ',':
        case '\n':
            if(in_escape) {
                if(ibuf >= sizeof buf) return -1;
                buf[ibuf++] = csv->text[csv->i];
            } else {
                if(start + ibuf != csv->i)
                    memcpy(csv->text + start, buf, ibuf);
                if(nstrs < ncols) {
                    cols[nstrs].text = csv->text + start;
                    cols[nstrs++].ntext = ibuf;
                }
                start = csv->i + 1;
                ibuf = 0;
            }
            ++csv->i;
            if(c == '\n') return nstrs;
            break;
        case '\r':
            if(in_escape) {
                if(ibuf == sizeof buf) return -1;
                buf[ibuf++] = csv->text[csv->i];
            }
            ++csv->i;
            break;
        default:
            if(ibuf == sizeof buf) return -1;
            buf[ibuf++] = csv->text[csv->i++];
            break;
        }
    }
    return nstrs;
}
#endif
/* Public Domain (www.unlicense.org)
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
