#ifndef _COGWXP_PTHREAD_COMPAT_H_
#define _COGWXP_PTHREAD_COMPAT_H_

#if defined(_WIN32) || defined(_WIN64)

#include <windows.h>
#include <process.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>

typedef HANDLE pthread_t;
typedef SRWLOCK pthread_mutex_t;
typedef CRITICAL_SECTION pthread_rwlock_t;
typedef CONDITION_VARIABLE pthread_cond_t;
typedef void pthread_attr_t;
typedef void pthread_mutexattr_t;
typedef void pthread_rwlockattr_t;
typedef void pthread_condattr_t;

#define PTHREAD_MUTEX_INITIALIZER SRWLOCK_INIT
#define PTHREAD_RWLOCK_INITIALIZER {0}
#define PTHREAD_COND_INITIALIZER CONDITION_VARIABLE_INIT

typedef struct {
    void* (*start_routine)(void*);
    void* arg;
} cogwxp_pthread_start_info_t;

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
    InitializeCriticalSection(rwlock);
    return 0;
}

static inline int pthread_rwlock_destroy(pthread_rwlock_t* rwlock) {
    if (!rwlock) {
        return EINVAL;
    }
    DeleteCriticalSection(rwlock);
    return 0;
}

static inline int pthread_rwlock_rdlock(pthread_rwlock_t* rwlock) {
    if (!rwlock) {
        return EINVAL;
    }
    EnterCriticalSection(rwlock);
    return 0;
}

static inline int pthread_rwlock_wrlock(pthread_rwlock_t* rwlock) {
    if (!rwlock) {
        return EINVAL;
    }
    EnterCriticalSection(rwlock);
    return 0;
}

static inline int pthread_rwlock_unlock(pthread_rwlock_t* rwlock) {
    if (!rwlock) {
        return EINVAL;
    }
    LeaveCriticalSection(rwlock);
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
