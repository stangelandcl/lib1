/* SPDX-License-Identifier: Unlicense */

#ifndef THREADPOOL_H
#define THREADPOOL_H

#if defined(THREADPOOL_STATIC) || defined(THREADPOOL_EXAMPLE)
#define THREADPOOL_API static
#define THREADPOOL_IMPLEMENTATION
#else
#define THREADPOOL_API extern
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*threadpool_cb)(void *ctx);

THREADPOOL_API void threadpool_run(threadpool_cb, void *ctx);

#ifdef __cplusplus
}
#endif

#endif

#ifdef THREADPOOL_IMPLEMENTATION
#include "thread.h"
#ifndef _WIN32
#include <unistd.h>
#endif
typedef struct sem_t {
        mtx_t mtx;
        cnd_t cnd;
        int count;
} sem_t;
int sem_init(sem_t *sem, int unused, int count) {
    (void)unused;
    sem->count = count;
    int res = mtx_init(&sem->mtx, mtx_plain);
    if(!res) res = cnd_init(&sem->cnd);
    return res;
}

void sem_destroy(sem_t *sem) {
    mtx_destroy(&sem->mtx);
    cnd_destroy(&sem->cnd);
}

void sem_post(sem_t *sem) {
    mtx_lock(&sem->mtx);
    ++sem->count;
    cnd_signal(&sem->cnd);
    mtx_unlock(&sem->mtx);
}

void sem_wait(sem_t *sem) {
    mtx_lock(&sem->mtx);
    while (sem->count == 0) cnd_wait(&sem->cnd, &sem->mtx);
    --sem->count;
    mtx_unlock(&sem->mtx);
}

/*************************************
 *      Minimalist threadpool
 * ***********************************/
#define THREADPOOL_NCALLBACKS 4096
typedef struct ThreadpoolCallback {
        threadpool_cb cb;
        void *ctx;
} ThreadpoolCallback;

static ThreadpoolCallback threadpool_callbacks[THREADPOOL_NCALLBACKS];
static int threadpool_callback_head;
static int threadpool_callback_count;
static once_flag threadpool_callback_lock_init;
static sem_t threadpool_callback_sem;
static cnd_t threadpool_callback_cond;
static mtx_t threadpool_callback_lock;
static int threadpool_started;

static int
threadpool_dequeue(ThreadpoolCallback *func) {
        int run = threadpool_callback_count > 0;
        if(run) {
                *func = threadpool_callbacks[threadpool_callback_head];
                if(++threadpool_callback_head == THREADPOOL_NCALLBACKS)
                    threadpool_callback_head = 0;
                --threadpool_callback_count;
                sem_post(&threadpool_callback_sem);
        }
        return run;
}

static int
threadpool_thread_run(void *ctx) {
        (void) ctx;
        ThreadpoolCallback cb;
        int ready;
        for(;;) {
                mtx_lock(&threadpool_callback_lock);
                ready = threadpool_dequeue(&cb);
                while(!ready) {
                        cnd_wait(&threadpool_callback_cond, &threadpool_callback_lock);
                        ready = threadpool_dequeue(&cb);
                }
                mtx_unlock(&threadpool_callback_lock);
                if(ready) cb.cb(cb.ctx);
        }
        return 0;
}

static void threadpool_lock_init(void) {
        mtx_init(&threadpool_callback_lock, mtx_plain);
}

/* safe to call multiple times */
static void
threadpool_start() {
        if(threadpool_started) return;
        call_once(&threadpool_callback_lock_init, threadpool_lock_init);
        mtx_lock(&threadpool_callback_lock);
        if(!threadpool_started) {
            cnd_init(&threadpool_callback_cond);
            sem_init(&threadpool_callback_sem, 0, THREADPOOL_NCALLBACKS);
#ifdef _WIN32
            SYSTEM_INFO info = {0};
            GetSystemInfo(&info);
            int cpus = (int)info.dwNumberOfProcessors;
#else
            int cpus = sysconf(_SC_NPROCESSORS_ONLN);
#endif
            if(cpus < 1) cpus = 1;
            while(cpus--) {
                    thrd_t t;
                    thrd_create(&t, threadpool_thread_run, 0);
            }
            threadpool_started = 1;
        }
        mtx_unlock(&threadpool_callback_lock);
}

THREADPOOL_API void
threadpool_run(threadpool_cb cb, void *ctx) {
        if(!threadpool_started) threadpool_start();

        ThreadpoolCallback callback;
        int i;
        callback.cb = cb;
        callback.ctx = ctx;

        sem_wait(&threadpool_callback_sem);
        mtx_lock(&threadpool_callback_lock);
        i = (threadpool_callback_head + threadpool_callback_count++) % THREADPOOL_NCALLBACKS;
        threadpool_callbacks[i] = callback;
        cnd_signal(&threadpool_callback_cond);
        mtx_unlock(&threadpool_callback_lock);
}

#endif
#ifdef THREADPOOL_EXAMPLE
#include <stdio.h>
static void run(void *ctx) {
	fprintf(stderr, "running\n");
    int *i = (int*)ctx;
    *i = 1;
}
int main(int argc, char **argv) {
    int i=0;
	threadpool_start();
    threadpool_run(run, &i);
    while(i != 1) {}
    return 0;
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

