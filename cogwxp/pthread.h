#ifndef _COGWXP_PTHREAD_COMPAT_H_
#define _COGWXP_PTHREAD_COMPAT_H_

#if defined(_WIN32) || defined(_WIN64)

#include <windows.h>
#include <process.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <time.h>

typedef HANDLE pthread_t;
typedef SRWLOCK pthread_mutex_t;
typedef struct {
    SRWLOCK lock;
} pthread_rwlock_t;
typedef CONDITION_VARIABLE pthread_cond_t;
typedef void pthread_attr_t;
typedef void pthread_mutexattr_t;
typedef void pthread_rwlockattr_t;
typedef void pthread_condattr_t;

#define PTHREAD_MUTEX_INITIALIZER SRWLOCK_INIT
#define PTHREAD_RWLOCK_INITIALIZER { SRWLOCK_INIT }
#define PTHREAD_COND_INITIALIZER CONDITION_VARIABLE_INIT

#define COGWXP_NSEC_PER_SEC               1000000000ULL
#define COGWXP_NSEC_PER_MSEC              1000000ULL
#define COGWXP_MAX_TIMEOUT_MS             ((uint64_t)(INFINITE - 1))

typedef struct {
    void* (*start_routine)(void*);
    void* arg;
} cogwxp_pthread_start_info_t;

typedef struct {
    const pthread_rwlock_t* lock;
    int mode; /* 1 = shared, 2 = exclusive */
} cogwxp_rwlock_entry_t;

#define COGWXP_RWLOCK_STACK_MAX 64

typedef struct {
    cogwxp_rwlock_entry_t entries[COGWXP_RWLOCK_STACK_MAX];
    int count;
} cogwxp_rwlock_state_t;

#if defined(_MSC_VER)
static __declspec(thread) cogwxp_rwlock_state_t g_cogwxp_rwlock_state = {0};
#else
static __thread cogwxp_rwlock_state_t g_cogwxp_rwlock_state = {0};
#endif

static unsigned __stdcall cogwxp_pthread_start(void* arg) {
    cogwxp_pthread_start_info_t* info = (cogwxp_pthread_start_info_t*)arg;
    if (info) {
        void* (*start_routine)(void*) = info->start_routine;
        void* start_arg = info->arg;
        free(info);
        if (start_routine) {
            (void)start_routine(start_arg);
        }
    }
    return 0;
}

static inline int pthread_create(
    pthread_t* thread,
    const pthread_attr_t* attr,
    void* (*start_routine)(void*),
    void* arg
) {
    (void)attr;
    if (!thread || !start_routine) {
        return EINVAL;
    }

    cogwxp_pthread_start_info_t* info = (cogwxp_pthread_start_info_t*)malloc(sizeof(*info));
    if (!info) {
        return ENOMEM;
    }

    info->start_routine = start_routine;
    info->arg = arg;

    uintptr_t handle = _beginthreadex(NULL, 0, cogwxp_pthread_start, info, 0, NULL);
    if (!handle) {
        free(info);
        return EAGAIN;
    }

    *thread = (HANDLE)handle;
    return 0;
}

static inline int pthread_join(pthread_t thread, void** retval) {
    DWORD wait_result = WaitForSingleObject(thread, INFINITE);
    if (wait_result != WAIT_OBJECT_0) {
        return EINVAL;
    }
    if (retval) {
        *retval = NULL;
    }
    return CloseHandle(thread) ? 0 : EINVAL;
}

static inline int pthread_detach(pthread_t thread) {
    return CloseHandle(thread) ? 0 : EINVAL;
}

static inline int pthread_mutex_init(pthread_mutex_t* mutex, const pthread_mutexattr_t* attr) {
    (void)attr;
    if (!mutex) {
        return EINVAL;
    }
    InitializeSRWLock(mutex);
    return 0;
}

static inline int pthread_mutex_destroy(pthread_mutex_t* mutex) {
    (void)mutex;
    return 0;
}

static inline int pthread_mutex_lock(pthread_mutex_t* mutex) {
    if (!mutex) {
        return EINVAL;
    }
    AcquireSRWLockExclusive(mutex);
    return 0;
}

static inline int pthread_mutex_trylock(pthread_mutex_t* mutex) {
    if (!mutex) {
        return EINVAL;
    }
    return TryAcquireSRWLockExclusive(mutex) ? 0 : EBUSY;
}

static inline int pthread_mutex_unlock(pthread_mutex_t* mutex) {
    if (!mutex) {
        return EINVAL;
    }
    ReleaseSRWLockExclusive(mutex);
    return 0;
}

static inline int pthread_rwlock_init(pthread_rwlock_t* rwlock, const pthread_rwlockattr_t* attr) {
    (void)attr;
    if (!rwlock) {
        return EINVAL;
    }
    InitializeSRWLock(&rwlock->lock);
    return 0;
}

static inline int pthread_rwlock_destroy(pthread_rwlock_t* rwlock) {
    (void)rwlock;
    return 0;
}

static inline int pthread_rwlock_rdlock(pthread_rwlock_t* rwlock) {
    if (!rwlock) {
        return EINVAL;
    }
    AcquireSRWLockShared(&rwlock->lock);
    if (g_cogwxp_rwlock_state.count >= COGWXP_RWLOCK_STACK_MAX) {
        ReleaseSRWLockShared(&rwlock->lock);
        return EAGAIN;
    }
    g_cogwxp_rwlock_state.entries[g_cogwxp_rwlock_state.count].lock = rwlock;
    g_cogwxp_rwlock_state.entries[g_cogwxp_rwlock_state.count].mode = 1;
    g_cogwxp_rwlock_state.count++;
    return 0;
}

static inline int pthread_rwlock_wrlock(pthread_rwlock_t* rwlock) {
    if (!rwlock) {
        return EINVAL;
    }
    AcquireSRWLockExclusive(&rwlock->lock);
    if (g_cogwxp_rwlock_state.count >= COGWXP_RWLOCK_STACK_MAX) {
        ReleaseSRWLockExclusive(&rwlock->lock);
        return EAGAIN;
    }
    g_cogwxp_rwlock_state.entries[g_cogwxp_rwlock_state.count].lock = rwlock;
    g_cogwxp_rwlock_state.entries[g_cogwxp_rwlock_state.count].mode = 2;
    g_cogwxp_rwlock_state.count++;
    return 0;
}

static inline int pthread_rwlock_unlock(pthread_rwlock_t* rwlock) {
    int idx = -1;
    int mode = 0;
    if (!rwlock) {
        return EINVAL;
    }

    for (int i = g_cogwxp_rwlock_state.count - 1; i >= 0; --i) {
        if (g_cogwxp_rwlock_state.entries[i].lock == rwlock) {
            idx = i;
            mode = g_cogwxp_rwlock_state.entries[i].mode;
            break;
        }
    }

    if (idx < 0 || (mode != 1 && mode != 2)) {
        return EINVAL;
    }

    if (mode == 2) {
        ReleaseSRWLockExclusive(&rwlock->lock);
    } else {
        ReleaseSRWLockShared(&rwlock->lock);
    }

    g_cogwxp_rwlock_state.count--;
    for (int i = idx; i < g_cogwxp_rwlock_state.count; ++i) {
        g_cogwxp_rwlock_state.entries[i] = g_cogwxp_rwlock_state.entries[i + 1];
    }
    return 0;
}

static inline int pthread_cond_init(pthread_cond_t* cond, const pthread_condattr_t* attr) {
    (void)attr;
    if (!cond) {
        return EINVAL;
    }
    InitializeConditionVariable(cond);
    return 0;
}

static inline int pthread_cond_destroy(pthread_cond_t* cond) {
    (void)cond;
    return 0;
}

static inline int pthread_cond_wait(pthread_cond_t* cond, pthread_mutex_t* mutex) {
    if (!cond || !mutex) {
        return EINVAL;
    }
    return SleepConditionVariableSRW(cond, mutex, INFINITE, 0) ? 0 : EINVAL;
}

/*
 * Wait until signalled or until the absolute UTC deadline in `abstime`.
 * Returns 0 on success, ETIMEDOUT on timeout, and EINVAL for invalid input
 * or other wait failures, matching pthread_cond_timedwait semantics.
 */
static inline int pthread_cond_timedwait(
    pthread_cond_t* cond,
    pthread_mutex_t* mutex,
    const struct timespec* abstime
) {
    DWORD timeout_ms;
    struct timespec now;
    uint64_t delta_ms;
    uint64_t delta_ns;
    uint64_t now_ns;
    uint64_t target_ns;

    if (!cond || !mutex || !abstime) {
        return EINVAL;
    }

    if (timespec_get(&now, TIME_UTC) != TIME_UTC) {
        return EINVAL;
    }
    now_ns = ((uint64_t)now.tv_sec * COGWXP_NSEC_PER_SEC) + (uint64_t)now.tv_nsec;
    target_ns = ((uint64_t)abstime->tv_sec * COGWXP_NSEC_PER_SEC) +
                (uint64_t)abstime->tv_nsec;

    if (target_ns <= now_ns) {
        timeout_ms = 0;
    } else {
        delta_ns = target_ns - now_ns;
        delta_ms = delta_ns / COGWXP_NSEC_PER_MSEC;
        if ((delta_ns % COGWXP_NSEC_PER_MSEC) != 0 &&
            delta_ms <= COGWXP_MAX_TIMEOUT_MS) {
            delta_ms++;
        }
        timeout_ms = (delta_ms > COGWXP_MAX_TIMEOUT_MS)
            ? (DWORD)COGWXP_MAX_TIMEOUT_MS
            : (DWORD)delta_ms;
    }

    if (SleepConditionVariableSRW(cond, mutex, timeout_ms, 0)) {
        return 0;
    }

    return GetLastError() == ERROR_TIMEOUT ? ETIMEDOUT : EINVAL;
}

static inline int pthread_cond_signal(pthread_cond_t* cond) {
    if (!cond) {
        return EINVAL;
    }
    WakeConditionVariable(cond);
    return 0;
}

static inline int pthread_cond_broadcast(pthread_cond_t* cond) {
    if (!cond) {
        return EINVAL;
    }
    WakeAllConditionVariable(cond);
    return 0;
}

#else
#include_next <pthread.h>
#endif

#endif /* _COGWXP_PTHREAD_COMPAT_H_ */
