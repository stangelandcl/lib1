#ifndef NOW_H
#define NOW_H

#include <time.h>
#include <string.h>
#include <stdint.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
static double now() {
    static double freq;
    if(!freq) {
        long long l;
        QueryPerformanceFrequency((LARGE_INTEGER*)&l);
        freq = (double)l;
    }
    long long ticks;
    QueryPerformanceCounter((LARGE_INTEGER*)&ticks);
    return (double)ticks / freq;
}

static uint64_t realtime() {
    FILETIME f;
    GetSystemTimePreciseAsFileTime(&f);
    uint64_t t = f.dwLowDateTime + ((uint64_t)f.dwHighDateTime << 32);
    return t * 100;
}


#else
#ifndef CLOCK_REALTIME_COARSE
#define CLOCK_REALTIME_COARSE CLOCK_REALTIME
#endif

static double now() {
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec + t.tv_nsec / 1e9;
}

/* returns real wall-clock time in nanoseconds */
static uint64_t realtime() {
    uint64_t now;
    struct timespec ts;
    if(clock_gettime(CLOCK_REALTIME_COARSE, &ts)) memset(&ts, 0, sizeof ts);
    now = (uint64_t)ts.tv_sec * (uint64_t)1000000000 + (uint64_t)ts.tv_nsec;
    return now;
}
#endif

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
