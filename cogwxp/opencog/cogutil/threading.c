/**
 * @file threading.c
 * @brief pthread-backed CogUtil threading primitives.
 */

#include "cogutil.h"

#include <errno.h>
#include <pthread.h>
#ifdef _WIN32
/* pthread.h (pthreads-win32) already pulls in windows.h; SwitchToThread()
 * is therefore available without an extra include. */
#else
#include <sched.h>
#endif
#include <stdlib.h>
#include <time.h>

struct cog_mutex {
    pthread_mutex_t mutex;
};

struct cog_rwlock {
    pthread_rwlock_t rwlock;
};

struct cog_cond {
    pthread_cond_t cond;
};

struct cog_thread {
    pthread_t thread;
    bool joinable;
};

typedef struct cog_threadpool_task {
    cog_thread_func_t func;
    void* arg;
    struct cog_threadpool_task* next;
} cog_threadpool_task_t;

struct cog_threadpool {
    pthread_t* threads;
    size_t thread_count;
    cog_threadpool_task_t* head;
    cog_threadpool_task_t* tail;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    bool stopping;
};

COGUTIL_API cog_mutex_t cog_mutex_create(void) {
    cog_mutex_t mutex = (cog_mutex_t)malloc(sizeof(struct cog_mutex));
    if (!mutex) return NULL;
    if (pthread_mutex_init(&mutex->mutex, NULL) != 0) {
        free(mutex);
        return NULL;
    }
    return mutex;
}

COGUTIL_API void cog_mutex_destroy(cog_mutex_t mutex) {
    if (!mutex) return;
    pthread_mutex_destroy(&mutex->mutex);
    free(mutex);
}

COGUTIL_API void cog_mutex_lock(cog_mutex_t mutex) {
    if (!mutex) return;
    pthread_mutex_lock(&mutex->mutex);
}

COGUTIL_API bool cog_mutex_trylock(cog_mutex_t mutex) {
    if (!mutex) return false;
    return pthread_mutex_trylock(&mutex->mutex) == 0;
}

COGUTIL_API void cog_mutex_unlock(cog_mutex_t mutex) {
    if (!mutex) return;
    pthread_mutex_unlock(&mutex->mutex);
}

COGUTIL_API cog_rwlock_t cog_rwlock_create(void) {
    cog_rwlock_t rwlock = (cog_rwlock_t)malloc(sizeof(struct cog_rwlock));
    if (!rwlock) return NULL;
    if (pthread_rwlock_init(&rwlock->rwlock, NULL) != 0) {
        free(rwlock);
        return NULL;
    }
    return rwlock;
}

COGUTIL_API void cog_rwlock_destroy(cog_rwlock_t rwlock) {
    if (!rwlock) return;
    pthread_rwlock_destroy(&rwlock->rwlock);
    free(rwlock);
}

COGUTIL_API void cog_rwlock_read_lock(cog_rwlock_t rwlock) {
    if (!rwlock) return;
    pthread_rwlock_rdlock(&rwlock->rwlock);
}

COGUTIL_API void cog_rwlock_read_unlock(cog_rwlock_t rwlock) {
    if (!rwlock) return;
    pthread_rwlock_unlock(&rwlock->rwlock);
}

COGUTIL_API void cog_rwlock_write_lock(cog_rwlock_t rwlock) {
    if (!rwlock) return;
    pthread_rwlock_wrlock(&rwlock->rwlock);
}

COGUTIL_API void cog_rwlock_write_unlock(cog_rwlock_t rwlock) {
    if (!rwlock) return;
    pthread_rwlock_unlock(&rwlock->rwlock);
}

COGUTIL_API cog_cond_t cog_cond_create(void) {
    cog_cond_t cond = (cog_cond_t)malloc(sizeof(struct cog_cond));
    if (!cond) return NULL;
    if (pthread_cond_init(&cond->cond, NULL) != 0) {
        free(cond);
        return NULL;
    }
    return cond;
}

COGUTIL_API void cog_cond_destroy(cog_cond_t cond) {
    if (!cond) return;
    pthread_cond_destroy(&cond->cond);
    free(cond);
}

COGUTIL_API void cog_cond_wait(cog_cond_t cond, cog_mutex_t mutex) {
    if (!cond || !mutex) return;
    pthread_cond_wait(&cond->cond, &mutex->mutex);
}

COGUTIL_API bool cog_cond_timedwait(cog_cond_t cond, cog_mutex_t mutex, uint32_t timeout_ms) {
    if (!cond || !mutex) return false;

    struct timespec ts;
#if defined(CLOCK_REALTIME)
    clock_gettime(CLOCK_REALTIME, &ts);
#elif defined(_WIN32)
    timespec_get(&ts, TIME_UTC);
#else
    ts.tv_sec = time(NULL);
    ts.tv_nsec = 0;
#endif
    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (long)(timeout_ms % 1000) * 1000000L;
    if (ts.tv_nsec >= 1000000000L) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000L;
    }

    return pthread_cond_timedwait(&cond->cond, &mutex->mutex, &ts) == 0;
}

COGUTIL_API void cog_cond_signal(cog_cond_t cond) {
    if (!cond) return;
    pthread_cond_signal(&cond->cond);
}

COGUTIL_API void cog_cond_broadcast(cog_cond_t cond) {
    if (!cond) return;
    pthread_cond_broadcast(&cond->cond);
}

COGUTIL_API cog_thread_t cog_thread_create(cog_thread_func_t func, void* arg) {
    if (!func) return NULL;

    cog_thread_t thread = (cog_thread_t)calloc(1, sizeof(struct cog_thread));
    if (!thread) return NULL;

    if (pthread_create(&thread->thread, NULL, func, arg) != 0) {
        free(thread);
        return NULL;
    }

    thread->joinable = true;
    return thread;
}

COGUTIL_API void cog_thread_join(cog_thread_t thread) {
    if (!thread) return;
    if (thread->joinable) pthread_join(thread->thread, NULL);
    free(thread);
}

COGUTIL_API void cog_thread_detach(cog_thread_t thread) {
    if (!thread) return;
    if (thread->joinable) pthread_detach(thread->thread);
    free(thread);
}

COGUTIL_API void cog_thread_yield(void) {
#ifdef _WIN32
    SwitchToThread();
#else
    sched_yield();
#endif
}

COGUTIL_API uint64_t cog_thread_id(void) {
    return (uint64_t)(uintptr_t)pthread_self();
}

static void* threadpool_worker(void* arg) {
    cog_threadpool_t pool = (cog_threadpool_t)arg;

    while (true) {
        pthread_mutex_lock(&pool->lock);
        while (!pool->head && !pool->stopping) {
            pthread_cond_wait(&pool->cond, &pool->lock);
        }

        if (!pool->head && pool->stopping) {
            pthread_mutex_unlock(&pool->lock);
            break;
        }

        cog_threadpool_task_t* task = pool->head;
        pool->head = task->next;
        if (!pool->head) pool->tail = NULL;
        pthread_mutex_unlock(&pool->lock);

        task->func(task->arg);
        free(task);
    }

    return NULL;
}

COGUTIL_API cog_threadpool_t cog_threadpool_create(size_t num_threads) {
    if (num_threads == 0) num_threads = 1;

    cog_threadpool_t pool = (cog_threadpool_t)calloc(1, sizeof(struct cog_threadpool));
    if (!pool) return NULL;

    pool->threads = (pthread_t*)calloc(num_threads, sizeof(pthread_t));
    if (!pool->threads) {
        free(pool);
        return NULL;
    }

    pthread_mutex_init(&pool->lock, NULL);
    pthread_cond_init(&pool->cond, NULL);
    pool->thread_count = num_threads;

    for (size_t i = 0; i < num_threads; i++) {
        if (pthread_create(&pool->threads[i], NULL, threadpool_worker, pool) != 0) {
            pthread_mutex_lock(&pool->lock);
            pool->stopping = true;
            pthread_cond_broadcast(&pool->cond);
            pthread_mutex_unlock(&pool->lock);
            for (size_t j = 0; j < i; j++) pthread_join(pool->threads[j], NULL);
            pthread_cond_destroy(&pool->cond);
            pthread_mutex_destroy(&pool->lock);
            free(pool->threads);
            free(pool);
            return NULL;
        }
    }

    return pool;
}

COGUTIL_API void cog_threadpool_destroy(cog_threadpool_t pool) {
    if (!pool) return;

    pthread_mutex_lock(&pool->lock);
    pool->stopping = true;
    pthread_cond_broadcast(&pool->cond);
    pthread_mutex_unlock(&pool->lock);

    for (size_t i = 0; i < pool->thread_count; i++) {
        pthread_join(pool->threads[i], NULL);
    }

    cog_threadpool_task_t* task = pool->head;
    while (task) {
        cog_threadpool_task_t* next = task->next;
        free(task);
        task = next;
    }

    pthread_cond_destroy(&pool->cond);
    pthread_mutex_destroy(&pool->lock);
    free(pool->threads);
    free(pool);
}

COGUTIL_API cog_result_t cog_threadpool_submit(cog_threadpool_t pool,
                                               cog_thread_func_t func,
                                               void* arg) {
    if (!pool || !func) return COG_ERROR_INVALID_ARG;

    cog_threadpool_task_t* task =
        (cog_threadpool_task_t*)calloc(1, sizeof(cog_threadpool_task_t));
    if (!task) return COG_ERROR_MEMORY;

    task->func = func;
    task->arg = arg;

    pthread_mutex_lock(&pool->lock);
    if (pool->stopping) {
        pthread_mutex_unlock(&pool->lock);
        free(task);
        return COG_ERROR_STATE;
    }

    if (pool->tail) pool->tail->next = task;
    else pool->head = task;
    pool->tail = task;

    pthread_cond_signal(&pool->cond);
    pthread_mutex_unlock(&pool->lock);

    return COG_OK;
}
