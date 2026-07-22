/**
 * @file osal.c
 * @brief Operating System Abstraction Layer - Implementation
 */

#include "osal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#if OSAL_PLATFORM_WINDOWS
#  include <windows.h>
#  include <process.h>
#  include <io.h>
#else
#  include <pthread.h>
#  include <unistd.h>
#  include <sys/time.h>
#  include <dlfcn.h>
#  include <errno.h>
#  include <time.h>
#  include <sched.h>
#  if OSAL_PLATFORM_LINUX
#    include <sys/random.h>
#  endif
#endif

/* ============================================================================
 * Thread Implementation
 * ========================================================================== */

struct osal_thread {
#if OSAL_PLATFORM_WINDOWS
    HANDLE handle;
    unsigned int id;
#else
    pthread_t handle;
#endif
    osal_thread_func_t func;
    void* arg;
    void* result;
};

#if OSAL_PLATFORM_WINDOWS
static unsigned __stdcall osal_thread_wrapper(void* arg) {
    struct osal_thread* t = (struct osal_thread*)arg;
    t->result = t->func(t->arg);
    return 0;
}
#else
static void* osal_thread_wrapper(void* arg) {
    struct osal_thread* t = (struct osal_thread*)arg;
    t->result = t->func(t->arg);
    return t->result;
}
#endif

OSAL_API osal_result_t osal_thread_create(
    osal_thread_t* thread,
    const osal_thread_attr_t* attr,
    osal_thread_func_t func,
    void* arg
) {
    if (!thread || !func) return OSAL_ERROR_INVALID_PARAM;
    
    struct osal_thread* t = (struct osal_thread*)malloc(sizeof(struct osal_thread));
    if (!t) return OSAL_ERROR_MEMORY;
    
    t->func = func;
    t->arg = arg;
    t->result = NULL;
    
#if OSAL_PLATFORM_WINDOWS
    t->handle = (HANDLE)_beginthreadex(NULL, 
        attr ? (unsigned)attr->stack_size : 0,
        osal_thread_wrapper, t, 0, &t->id);
    if (!t->handle) {
        free(t);
        return OSAL_ERROR_UNKNOWN;
    }
    if (attr && attr->detached) {
        CloseHandle(t->handle);
        t->handle = NULL;
    }
#else
    pthread_attr_t pattr;
    pthread_attr_init(&pattr);
    if (attr) {
        if (attr->stack_size > 0) {
            pthread_attr_setstacksize(&pattr, attr->stack_size);
        }
        if (attr->detached) {
            pthread_attr_setdetachstate(&pattr, PTHREAD_CREATE_DETACHED);
        }
    }
    if (pthread_create(&t->handle, &pattr, osal_thread_wrapper, t) != 0) {
        pthread_attr_destroy(&pattr);
        free(t);
        return OSAL_ERROR_UNKNOWN;
    }
    pthread_attr_destroy(&pattr);
#endif
    
    *thread = t;
    return OSAL_OK;
}

OSAL_API osal_result_t osal_thread_join(osal_thread_t thread, void** retval) {
    if (!thread) return OSAL_ERROR_INVALID_PARAM;
    
#if OSAL_PLATFORM_WINDOWS
    if (!thread->handle) return OSAL_ERROR_INVALID_PARAM;
    DWORD result = WaitForSingleObject(thread->handle, INFINITE);
    if (result != WAIT_OBJECT_0) {
        return OSAL_ERROR_UNKNOWN;
    }
    CloseHandle(thread->handle);
    if (retval) *retval = thread->result;
#else
    void* res = NULL;
    if (pthread_join(thread->handle, &res) != 0) {
        return OSAL_ERROR_UNKNOWN;
    }
    if (retval) *retval = res;
#endif
    
    free(thread);
    return OSAL_OK;
}

OSAL_API osal_result_t osal_thread_detach(osal_thread_t thread) {
    if (!thread) return OSAL_ERROR_INVALID_PARAM;
    
#if OSAL_PLATFORM_WINDOWS
    if (thread->handle) {
        CloseHandle(thread->handle);
    }
#else
    if (pthread_detach(thread->handle) != 0) {
        return OSAL_ERROR_UNKNOWN;
    }
#endif
    
    free(thread);
    return OSAL_OK;
}

OSAL_API void osal_thread_exit(void* retval) {
#if OSAL_PLATFORM_WINDOWS
    _endthreadex((unsigned)(uintptr_t)retval);
#else
    pthread_exit(retval);
#endif
}

OSAL_API uint64_t osal_thread_id(void) {
#if OSAL_PLATFORM_WINDOWS
    return (uint64_t)GetCurrentThreadId();
#else
    return (uint64_t)(uintptr_t)pthread_self();
#endif
}

OSAL_API void osal_thread_yield(void) {
#if OSAL_PLATFORM_WINDOWS
    SwitchToThread();
#else
    sched_yield();
#endif
}

/* ============================================================================
 * Mutex Implementation
 * ========================================================================== */

struct osal_mutex {
#if OSAL_PLATFORM_WINDOWS
    SRWLOCK lock;
#else
    pthread_mutex_t lock;
#endif
};

OSAL_API osal_result_t osal_mutex_create(osal_mutex_t* mutex, const osal_mutex_attr_t* attr) {
    (void)attr; /* TODO: support recursive mutexes */
    if (!mutex) return OSAL_ERROR_INVALID_PARAM;
    
    struct osal_mutex* m = (struct osal_mutex*)malloc(sizeof(struct osal_mutex));
    if (!m) return OSAL_ERROR_MEMORY;
    
#if OSAL_PLATFORM_WINDOWS
    InitializeSRWLock(&m->lock);
#else
    if (pthread_mutex_init(&m->lock, NULL) != 0) {
        free(m);
        return OSAL_ERROR_UNKNOWN;
    }
#endif
    
    *mutex = m;
    return OSAL_OK;
}

OSAL_API osal_result_t osal_mutex_destroy(osal_mutex_t mutex) {
    if (!mutex) return OSAL_ERROR_INVALID_PARAM;
#if !OSAL_PLATFORM_WINDOWS
    pthread_mutex_destroy(&mutex->lock);
#endif
    free(mutex);
    return OSAL_OK;
}

OSAL_API osal_result_t osal_mutex_lock(osal_mutex_t mutex) {
    if (!mutex) return OSAL_ERROR_INVALID_PARAM;
#if OSAL_PLATFORM_WINDOWS
    AcquireSRWLockExclusive(&mutex->lock);
    return OSAL_OK;
#else
    return (pthread_mutex_lock(&mutex->lock) == 0) ? OSAL_OK : OSAL_ERROR_UNKNOWN;
#endif
}

OSAL_API osal_result_t osal_mutex_trylock(osal_mutex_t mutex) {
    if (!mutex) return OSAL_ERROR_INVALID_PARAM;
#if OSAL_PLATFORM_WINDOWS
    return TryAcquireSRWLockExclusive(&mutex->lock) ? OSAL_OK : OSAL_ERROR_BUSY;
#else
    int r = pthread_mutex_trylock(&mutex->lock);
    if (r == 0) return OSAL_OK;
    if (r == EBUSY) return OSAL_ERROR_BUSY;
    return OSAL_ERROR_UNKNOWN;
#endif
}

OSAL_API osal_result_t osal_mutex_unlock(osal_mutex_t mutex) {
    if (!mutex) return OSAL_ERROR_INVALID_PARAM;
#if OSAL_PLATFORM_WINDOWS
    ReleaseSRWLockExclusive(&mutex->lock);
    return OSAL_OK;
#else
    return (pthread_mutex_unlock(&mutex->lock) == 0) ? OSAL_OK : OSAL_ERROR_UNKNOWN;
#endif
}

/* ============================================================================
 * Read-Write Lock Implementation
 * ========================================================================== */

struct osal_rwlock {
#if OSAL_PLATFORM_WINDOWS
    SRWLOCK lock;
#else
    pthread_rwlock_t lock;
#endif
};

OSAL_API osal_result_t osal_rwlock_create(osal_rwlock_t* rwlock) {
    if (!rwlock) return OSAL_ERROR_INVALID_PARAM;
    
    struct osal_rwlock* rw = (struct osal_rwlock*)malloc(sizeof(struct osal_rwlock));
    if (!rw) return OSAL_ERROR_MEMORY;
    
#if OSAL_PLATFORM_WINDOWS
    InitializeSRWLock(&rw->lock);
#else
    if (pthread_rwlock_init(&rw->lock, NULL) != 0) {
        free(rw);
        return OSAL_ERROR_UNKNOWN;
    }
#endif
    
    *rwlock = rw;
    return OSAL_OK;
}

OSAL_API osal_result_t osal_rwlock_destroy(osal_rwlock_t rwlock) {
    if (!rwlock) return OSAL_ERROR_INVALID_PARAM;
#if !OSAL_PLATFORM_WINDOWS
    pthread_rwlock_destroy(&rwlock->lock);
#endif
    free(rwlock);
    return OSAL_OK;
}

OSAL_API osal_result_t osal_rwlock_rdlock(osal_rwlock_t rwlock) {
    if (!rwlock) return OSAL_ERROR_INVALID_PARAM;
#if OSAL_PLATFORM_WINDOWS
    AcquireSRWLockShared(&rwlock->lock);
    return OSAL_OK;
#else
    return (pthread_rwlock_rdlock(&rwlock->lock) == 0) ? OSAL_OK : OSAL_ERROR_UNKNOWN;
#endif
}

OSAL_API osal_result_t osal_rwlock_tryrdlock(osal_rwlock_t rwlock) {
    if (!rwlock) return OSAL_ERROR_INVALID_PARAM;
#if OSAL_PLATFORM_WINDOWS
    return TryAcquireSRWLockShared(&rwlock->lock) ? OSAL_OK : OSAL_ERROR_BUSY;
#else
    int r = pthread_rwlock_tryrdlock(&rwlock->lock);
    if (r == 0) return OSAL_OK;
    if (r == EBUSY) return OSAL_ERROR_BUSY;
    return OSAL_ERROR_UNKNOWN;
#endif
}

OSAL_API osal_result_t osal_rwlock_wrlock(osal_rwlock_t rwlock) {
    if (!rwlock) return OSAL_ERROR_INVALID_PARAM;
#if OSAL_PLATFORM_WINDOWS
    AcquireSRWLockExclusive(&rwlock->lock);
    return OSAL_OK;
#else
    return (pthread_rwlock_wrlock(&rwlock->lock) == 0) ? OSAL_OK : OSAL_ERROR_UNKNOWN;
#endif
}

OSAL_API osal_result_t osal_rwlock_trywrlock(osal_rwlock_t rwlock) {
    if (!rwlock) return OSAL_ERROR_INVALID_PARAM;
#if OSAL_PLATFORM_WINDOWS
    return TryAcquireSRWLockExclusive(&rwlock->lock) ? OSAL_OK : OSAL_ERROR_BUSY;
#else
    int r = pthread_rwlock_trywrlock(&rwlock->lock);
    if (r == 0) return OSAL_OK;
    if (r == EBUSY) return OSAL_ERROR_BUSY;
    return OSAL_ERROR_UNKNOWN;
#endif
}

OSAL_API osal_result_t osal_rwlock_unlock(osal_rwlock_t rwlock) {
    if (!rwlock) return OSAL_ERROR_INVALID_PARAM;
#if OSAL_PLATFORM_WINDOWS
    /* Note: SRWLOCK needs separate release for shared/exclusive. 
       For simplicity, try exclusive first. Caller should track mode. */
    ReleaseSRWLockExclusive(&rwlock->lock);
    return OSAL_OK;
#else
    return (pthread_rwlock_unlock(&rwlock->lock) == 0) ? OSAL_OK : OSAL_ERROR_UNKNOWN;
#endif
}

/* ============================================================================
 * Condition Variable Implementation
 * ========================================================================== */

struct osal_cond {
#if OSAL_PLATFORM_WINDOWS
    CONDITION_VARIABLE cond;
#else
    pthread_cond_t cond;
#endif
};

OSAL_API osal_result_t osal_cond_create(osal_cond_t* cond) {
    if (!cond) return OSAL_ERROR_INVALID_PARAM;
    
    struct osal_cond* c = (struct osal_cond*)malloc(sizeof(struct osal_cond));
    if (!c) return OSAL_ERROR_MEMORY;
    
#if OSAL_PLATFORM_WINDOWS
    InitializeConditionVariable(&c->cond);
#else
    if (pthread_cond_init(&c->cond, NULL) != 0) {
        free(c);
        return OSAL_ERROR_UNKNOWN;
    }
#endif
    
    *cond = c;
    return OSAL_OK;
}

OSAL_API osal_result_t osal_cond_destroy(osal_cond_t cond) {
    if (!cond) return OSAL_ERROR_INVALID_PARAM;
#if !OSAL_PLATFORM_WINDOWS
    pthread_cond_destroy(&cond->cond);
#endif
    free(cond);
    return OSAL_OK;
}

OSAL_API osal_result_t osal_cond_wait(osal_cond_t cond, osal_mutex_t mutex) {
    if (!cond || !mutex) return OSAL_ERROR_INVALID_PARAM;
#if OSAL_PLATFORM_WINDOWS
    if (!SleepConditionVariableSRW(&cond->cond, &mutex->lock, INFINITE, 0)) {
        return OSAL_ERROR_UNKNOWN;
    }
    return OSAL_OK;
#else
    return (pthread_cond_wait(&cond->cond, &mutex->lock) == 0) ? OSAL_OK : OSAL_ERROR_UNKNOWN;
#endif
}

OSAL_API osal_result_t osal_cond_timedwait(osal_cond_t cond, osal_mutex_t mutex, uint64_t timeout_ms) {
    if (!cond || !mutex) return OSAL_ERROR_INVALID_PARAM;
    
#if OSAL_PLATFORM_WINDOWS
    if (!SleepConditionVariableSRW(&cond->cond, &mutex->lock, (DWORD)timeout_ms, 0)) {
        if (GetLastError() == ERROR_TIMEOUT) return OSAL_ERROR_TIMEOUT;
        return OSAL_ERROR_UNKNOWN;
    }
    return OSAL_OK;
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += (time_t)(timeout_ms / 1000);
    ts.tv_nsec += (long)((timeout_ms % 1000) * 1000000);
    if (ts.tv_nsec >= 1000000000L) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000L;
    }
    int r = pthread_cond_timedwait(&cond->cond, &mutex->lock, &ts);
    if (r == 0) return OSAL_OK;
    if (r == ETIMEDOUT) return OSAL_ERROR_TIMEOUT;
    return OSAL_ERROR_UNKNOWN;
#endif
}

OSAL_API osal_result_t osal_cond_signal(osal_cond_t cond) {
    if (!cond) return OSAL_ERROR_INVALID_PARAM;
#if OSAL_PLATFORM_WINDOWS
    WakeConditionVariable(&cond->cond);
    return OSAL_OK;
#else
    return (pthread_cond_signal(&cond->cond) == 0) ? OSAL_OK : OSAL_ERROR_UNKNOWN;
#endif
}

OSAL_API osal_result_t osal_cond_broadcast(osal_cond_t cond) {
    if (!cond) return OSAL_ERROR_INVALID_PARAM;
#if OSAL_PLATFORM_WINDOWS
    WakeAllConditionVariable(&cond->cond);
    return OSAL_OK;
#else
    return (pthread_cond_broadcast(&cond->cond) == 0) ? OSAL_OK : OSAL_ERROR_UNKNOWN;
#endif
}

/* ============================================================================
 * Time Functions
 * ========================================================================== */

OSAL_API uint64_t osal_time_monotonic_ns(void) {
#if OSAL_PLATFORM_WINDOWS
    static LARGE_INTEGER freq = {0};
    LARGE_INTEGER count;
    if (freq.QuadPart == 0) {
        QueryPerformanceFrequency(&freq);
    }
    QueryPerformanceCounter(&count);
    return (uint64_t)(count.QuadPart * 1000000000ULL / freq.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#endif
}

OSAL_API uint64_t osal_time_monotonic_ms(void) {
#if OSAL_PLATFORM_WINDOWS
    return GetTickCount64();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
#endif
}

OSAL_API uint64_t osal_time_epoch_sec(void) {
#if OSAL_PLATFORM_WINDOWS
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    return (uli.QuadPart - 116444736000000000ULL) / 10000000ULL;
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec;
#endif
}

OSAL_API uint64_t osal_time_epoch_ms(void) {
#if OSAL_PLATFORM_WINDOWS
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    return (uli.QuadPart - 116444736000000000ULL) / 10000ULL;
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
#endif
}

OSAL_API void osal_sleep_ms(uint64_t ms) {
#if OSAL_PLATFORM_WINDOWS
    Sleep((DWORD)ms);
#else
    usleep((useconds_t)(ms * 1000));
#endif
}

OSAL_API void osal_sleep_us(uint64_t us) {
#if OSAL_PLATFORM_WINDOWS
    if (us >= 1000) {
        Sleep((DWORD)(us / 1000));
    } else {
        uint64_t start = osal_time_monotonic_ns();
        while ((osal_time_monotonic_ns() - start) < us * 1000) {
            /* spin */
        }
    }
#else
    usleep((useconds_t)us);
#endif
}

/* ============================================================================
 * Process Functions
 * ========================================================================== */

OSAL_API uint64_t osal_getpid(void) {
#if OSAL_PLATFORM_WINDOWS
    return (uint64_t)GetCurrentProcessId();
#else
    return (uint64_t)getpid();
#endif
}

OSAL_API char* osal_getcwd(void) {
#if OSAL_PLATFORM_WINDOWS
    DWORD len = GetCurrentDirectoryA(0, NULL);
    if (len == 0) return NULL;
    char* buf = (char*)malloc(len);
    if (!buf) return NULL;
    if (GetCurrentDirectoryA(len, buf) == 0) {
        free(buf);
        return NULL;
    }
    return buf;
#else
    char* buf = (char*)malloc(4096);
    if (!buf) return NULL;
    if (getcwd(buf, 4096) == NULL) {
        free(buf);
        return NULL;
    }
    return buf;
#endif
}

OSAL_API bool osal_isatty(int fd) {
#if OSAL_PLATFORM_WINDOWS
    return _isatty(fd) != 0;
#else
    return isatty(fd) != 0;
#endif
}

/* ============================================================================
 * Dynamic Loading
 * ========================================================================== */

#if OSAL_PLATFORM_WINDOWS
static __declspec(thread) DWORD osal_dl_last_error = 0;
#else
static __thread char* osal_dl_last_error_msg = NULL;
#endif

OSAL_API osal_dl_handle_t osal_dl_open(const char* path) {
    if (!path) return NULL;
#if OSAL_PLATFORM_WINDOWS
    HMODULE h = LoadLibraryA(path);
    if (!h) osal_dl_last_error = GetLastError();
    return (osal_dl_handle_t)h;
#else
    void* h = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!h && osal_dl_last_error_msg) {
        free(osal_dl_last_error_msg);
        osal_dl_last_error_msg = NULL;
    }
    if (!h) {
        const char* e = dlerror();
        if (e) osal_dl_last_error_msg = strdup(e);
    }
    return (osal_dl_handle_t)h;
#endif
}

OSAL_API void* osal_dl_sym(osal_dl_handle_t handle, const char* symbol) {
    if (!handle || !symbol) return NULL;
#if OSAL_PLATFORM_WINDOWS
    void* s = (void*)GetProcAddress((HMODULE)handle, symbol);
    if (!s) osal_dl_last_error = GetLastError();
    return s;
#else
    void* s = dlsym(handle, symbol);
    if (!s) {
        if (osal_dl_last_error_msg) {
            free(osal_dl_last_error_msg);
            osal_dl_last_error_msg = NULL;
        }
        const char* e = dlerror();
        if (e) osal_dl_last_error_msg = strdup(e);
    }
    return s;
#endif
}

OSAL_API osal_result_t osal_dl_close(osal_dl_handle_t handle) {
    if (!handle) return OSAL_ERROR_INVALID_PARAM;
#if OSAL_PLATFORM_WINDOWS
    return FreeLibrary((HMODULE)handle) ? OSAL_OK : OSAL_ERROR_UNKNOWN;
#else
    return (dlclose(handle) == 0) ? OSAL_OK : OSAL_ERROR_UNKNOWN;
#endif
}

OSAL_API const char* osal_dl_error(void) {
#if OSAL_PLATFORM_WINDOWS
    static __declspec(thread) char msg[256];
    if (osal_dl_last_error == 0) return NULL;
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, NULL, osal_dl_last_error,
                   0, msg, sizeof(msg), NULL);
    return msg;
#else
    return osal_dl_last_error_msg;
#endif
}

/* ============================================================================
 * Socket Functions
 * ========================================================================== */

OSAL_API osal_result_t osal_socket_init(void) {
#if OSAL_PLATFORM_WINDOWS
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        return OSAL_ERROR_UNKNOWN;
    }
    return OSAL_OK;
#else
    return OSAL_OK; /* No-op on POSIX */
#endif
}

OSAL_API void osal_socket_cleanup(void) {
#if OSAL_PLATFORM_WINDOWS
    WSACleanup();
#endif
}

OSAL_API osal_result_t osal_socket_close(osal_socket_t sock) {
    if (sock == OSAL_INVALID_SOCKET) return OSAL_ERROR_INVALID_PARAM;
#if OSAL_PLATFORM_WINDOWS
    return (closesocket(sock) == 0) ? OSAL_OK : OSAL_ERROR_UNKNOWN;
#else
    return (close(sock) == 0) ? OSAL_OK : OSAL_ERROR_UNKNOWN;
#endif
}

OSAL_API int osal_socket_error(void) {
#if OSAL_PLATFORM_WINDOWS
    return WSAGetLastError();
#else
    return errno;
#endif
}

/* ============================================================================
 * Path Functions
 * ========================================================================== */

OSAL_API char osal_path_separator(void) {
#if OSAL_PLATFORM_WINDOWS
    return '\\';
#else
    return '/';
#endif
}

OSAL_API char* osal_path_normalize(const char* path) {
    if (!path) return NULL;
    
    size_t len = strlen(path);
    char* out = (char*)malloc(len + 1);
    if (!out) return NULL;
    
    memcpy(out, path, len + 1);
    
    /* Normalize separators to platform default */
    char sep = osal_path_separator();
    char other = (sep == '/') ? '\\' : '/';
    for (size_t i = 0; i < len; i++) {
        if (out[i] == other) out[i] = sep;
    }
    
    return out;
}

OSAL_API char* osal_path_join(const char* base, const char* component) {
    if (!base || !component) return NULL;
    
    size_t base_len = strlen(base);
    size_t comp_len = strlen(component);
    char sep = osal_path_separator();
    
    /* Check if we need separator */
    int need_sep = (base_len > 0 && base[base_len-1] != '/' && base[base_len-1] != '\\');
    
    char* out = (char*)malloc(base_len + need_sep + comp_len + 1);
    if (!out) return NULL;
    
    memcpy(out, base, base_len);
    if (need_sep) out[base_len] = sep;
    memcpy(out + base_len + need_sep, component, comp_len + 1);
    
    return out;
}

/* ============================================================================
 * Atomic Operations
 * ========================================================================== */

OSAL_API int32_t osal_atomic_inc32(volatile int32_t* value) {
    if (!value) return 0;
#if OSAL_PLATFORM_WINDOWS
    return InterlockedIncrement((volatile LONG*)value);
#else
    return __sync_add_and_fetch(value, 1);
#endif
}

OSAL_API int64_t osal_atomic_inc64(volatile int64_t* value) {
    if (!value) return 0;
#if OSAL_PLATFORM_WINDOWS
    return InterlockedIncrement64(value);
#else
    return __sync_add_and_fetch(value, 1);
#endif
}

OSAL_API int32_t osal_atomic_dec32(volatile int32_t* value) {
    if (!value) return 0;
#if OSAL_PLATFORM_WINDOWS
    return InterlockedDecrement((volatile LONG*)value);
#else
    return __sync_sub_and_fetch(value, 1);
#endif
}

OSAL_API int64_t osal_atomic_dec64(volatile int64_t* value) {
    if (!value) return 0;
#if OSAL_PLATFORM_WINDOWS
    return InterlockedDecrement64(value);
#else
    return __sync_sub_and_fetch(value, 1);
#endif
}

OSAL_API bool osal_atomic_cas32(volatile int32_t* value, int32_t expected, int32_t desired) {
    if (!value) return false;
#if OSAL_PLATFORM_WINDOWS
    return InterlockedCompareExchange((volatile LONG*)value, desired, expected) == expected;
#else
    return __sync_bool_compare_and_swap(value, expected, desired);
#endif
}

OSAL_API bool osal_atomic_cas64(volatile int64_t* value, int64_t expected, int64_t desired) {
    if (!value) return false;
#if OSAL_PLATFORM_WINDOWS
    return InterlockedCompareExchange64(value, desired, expected) == expected;
#else
    return __sync_bool_compare_and_swap(value, expected, desired);
#endif
}

OSAL_API bool osal_atomic_cas_ptr(volatile void** value, void* expected, void* desired) {
    if (!value) return false;
#if OSAL_PLATFORM_WINDOWS
    return InterlockedCompareExchangePointer((volatile PVOID*)value, desired, expected) == expected;
#else
    return __sync_bool_compare_and_swap(value, expected, desired);
#endif
}

OSAL_API void osal_memory_barrier(void) {
#if OSAL_PLATFORM_WINDOWS
    MemoryBarrier();
#else
    __sync_synchronize();
#endif
}

/* ============================================================================
 * Random Functions
 * ========================================================================== */

OSAL_API uint32_t osal_random_u32(uint32_t* state) {
    if (!state) return 0;
    /* xorshift32 */
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

OSAL_API osal_result_t osal_random_bytes(void* buffer, size_t length) {
    if (!buffer || length == 0) return OSAL_ERROR_INVALID_PARAM;
    
#if OSAL_PLATFORM_WINDOWS
    /* Use RtlGenRandom (SystemFunction036) */
    typedef BOOLEAN (APIENTRY *RtlGenRandomFunc)(PVOID, ULONG);
    static RtlGenRandomFunc fn = NULL;
    if (!fn) {
        HMODULE advapi = LoadLibraryA("advapi32.dll");
        if (advapi) {
            fn = (RtlGenRandomFunc)(void*)GetProcAddress(advapi, "SystemFunction036");
        }
    }
    if (fn && fn(buffer, (ULONG)length)) {
        return OSAL_OK;
    }
    return OSAL_ERROR_UNKNOWN;
#elif OSAL_PLATFORM_LINUX
    ssize_t r = getrandom(buffer, length, 0);
    return (r == (ssize_t)length) ? OSAL_OK : OSAL_ERROR_UNKNOWN;
#else
    /* Fallback to /dev/urandom */
    FILE* f = fopen("/dev/urandom", "rb");
    if (!f) return OSAL_ERROR_UNKNOWN;
    size_t r = fread(buffer, 1, length, f);
    fclose(f);
    return (r == length) ? OSAL_OK : OSAL_ERROR_UNKNOWN;
#endif
}
