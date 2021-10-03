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

THREADPOOL_API void
threadpool_run(threadpool_cb, void *ctx);

#ifdef __cplusplus
}
#endif

#endif

#ifdef THREADPOOL_IMPLEMENTATION
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

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
static sem_t threadpool_callback_sem;
static pthread_cond_t threadpool_callback_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t threadpool_callback_lock = PTHREAD_MUTEX_INITIALIZER;

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

static void*
threadpool_thread_run(void *ctx) {
        ThreadpoolCallback cb;
        int ready;
        for(;;) {
                pthread_mutex_lock(&threadpool_callback_lock);
                ready = threadpool_dequeue(&cb);
                while(!ready) {
                        pthread_cond_wait(&threadpool_callback_cond, &threadpool_callback_lock);
                        ready = threadpool_dequeue(&cb);
                }
                pthread_mutex_unlock(&threadpool_callback_lock);
                if(ready) cb.cb(cb.ctx);
        }
        return 0;
}

THREADPOOL_API void
threadpool_run(threadpool_cb cb, void *ctx) {
        ThreadpoolCallback callback;
        int i;
        callback.cb = cb;
        callback.ctx = ctx;

        sem_wait(&threadpool_callback_sem);
        pthread_mutex_lock(&threadpool_callback_lock);
        i = (threadpool_callback_head + threadpool_callback_count++) % THREADPOOL_NCALLBACKS;
        threadpool_callbacks[i] = callback;
        pthread_cond_signal(&threadpool_callback_cond);
        pthread_mutex_unlock(&threadpool_callback_lock);
}

static void
threadpool_start() {
        sem_init(&threadpool_callback_sem, 0, THREADPOOL_NCALLBACKS);
        int cpus = sysconf(_SC_NPROCESSORS_ONLN);
        if(cpus < 1) cpus = 1;
        while(cpus--) {
                pthread_t t;
                pthread_create(&t, 0, threadpool_thread_run, 0);
        }
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
