/**
 * @file osal.h
 * @brief OS Abstraction Layer for CoGWXP-OS9
 * 
 * Provides platform-independent abstractions for:
 * - Threading and synchronization (pthread vs Win32/SRW)
 * - Time functions (clock_gettime vs GetTickCount64)
 * - Process utilities (getpid, usleep vs Windows equivalents)
 * - Dynamic loading (dlopen vs LoadLibrary)
 * - Console/TTY detection (isatty vs _isatty)
 * - Path handling (POSIX vs Windows paths)
 * 
 * This module consolidates the ad-hoc pthread.h shim and other platform-specific
 * code into a unified, well-documented abstraction layer.
 * 
 * @copyright AGPL-3.0
 */

#ifndef _COGWXP_OSAL_H_
#define _COGWXP_OSAL_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Platform Detection
 *===========================================================================*/

#if defined(_WIN32) || defined(_WIN64)
    #define OSAL_PLATFORM_WINDOWS 1
    #define OSAL_PLATFORM_POSIX 0
#elif defined(__unix__) || defined(__unix) || (defined(__APPLE__) && defined(__MACH__))
    #define OSAL_PLATFORM_WINDOWS 0
    #define OSAL_PLATFORM_POSIX 1
#else
    #error "Unsupported platform"
#endif

#if defined(__APPLE__) && defined(__MACH__)
    #define OSAL_PLATFORM_MACOS 1
#else
    #define OSAL_PLATFORM_MACOS 0
#endif

#if defined(__linux__)
    #define OSAL_PLATFORM_LINUX 1
#else
    #define OSAL_PLATFORM_LINUX 0
#endif

/*===========================================================================
 * DLL Export/Import Macros
 *===========================================================================*/

#if OSAL_PLATFORM_WINDOWS
    #ifdef OSAL_EXPORTS
        #define OSAL_API __declspec(dllexport)
    #else
        #define OSAL_API __declspec(dllimport)
    #endif
#else
    #define OSAL_API
#endif

/*===========================================================================
 * Error Codes
 *===========================================================================*/

typedef enum {
    OSAL_OK = 0,
    OSAL_ERROR_INVALID_PARAM = -1,
    OSAL_ERROR_MEMORY = -2,
    OSAL_ERROR_TIMEOUT = -3,
    OSAL_ERROR_BUSY = -4,
    OSAL_ERROR_NOT_FOUND = -5,
    OSAL_ERROR_PERMISSION = -6,
    OSAL_ERROR_NOT_SUPPORTED = -7,
    OSAL_ERROR_INTERRUPTED = -8,
    OSAL_ERROR_UNKNOWN = -99
} osal_result_t;

/*===========================================================================
 * Thread API
 *===========================================================================*/

typedef struct osal_thread* osal_thread_t;
typedef void* (*osal_thread_func_t)(void* arg);

typedef struct {
    size_t stack_size;      /* 0 = default */
    bool detached;          /* Start detached */
} osal_thread_attr_t;

#define OSAL_THREAD_ATTR_DEFAULT { 0, false }

OSAL_API osal_result_t osal_thread_create(
    osal_thread_t* thread,
    const osal_thread_attr_t* attr,
    osal_thread_func_t func,
    void* arg
);

OSAL_API osal_result_t osal_thread_join(osal_thread_t thread, void** retval);
OSAL_API osal_result_t osal_thread_detach(osal_thread_t thread);
OSAL_API void          osal_thread_exit(void* retval);
OSAL_API uint64_t      osal_thread_id(void);
OSAL_API void          osal_thread_yield(void);

/*===========================================================================
 * Mutex API
 *===========================================================================*/

typedef struct osal_mutex* osal_mutex_t;

typedef struct {
    bool recursive;         /* Allow recursive locking */
} osal_mutex_attr_t;

#define OSAL_MUTEX_ATTR_DEFAULT { false }

OSAL_API osal_result_t osal_mutex_create(osal_mutex_t* mutex, const osal_mutex_attr_t* attr);
OSAL_API osal_result_t osal_mutex_destroy(osal_mutex_t mutex);
OSAL_API osal_result_t osal_mutex_lock(osal_mutex_t mutex);
OSAL_API osal_result_t osal_mutex_trylock(osal_mutex_t mutex);
OSAL_API osal_result_t osal_mutex_unlock(osal_mutex_t mutex);

/*===========================================================================
 * Read-Write Lock API
 *===========================================================================*/

typedef struct osal_rwlock* osal_rwlock_t;

OSAL_API osal_result_t osal_rwlock_create(osal_rwlock_t* rwlock);
OSAL_API osal_result_t osal_rwlock_destroy(osal_rwlock_t rwlock);
OSAL_API osal_result_t osal_rwlock_rdlock(osal_rwlock_t rwlock);
OSAL_API osal_result_t osal_rwlock_tryrdlock(osal_rwlock_t rwlock);
OSAL_API osal_result_t osal_rwlock_wrlock(osal_rwlock_t rwlock);
OSAL_API osal_result_t osal_rwlock_trywrlock(osal_rwlock_t rwlock);
OSAL_API osal_result_t osal_rwlock_unlock(osal_rwlock_t rwlock);

/*===========================================================================
 * Condition Variable API
 *===========================================================================*/

typedef struct osal_cond* osal_cond_t;

OSAL_API osal_result_t osal_cond_create(osal_cond_t* cond);
OSAL_API osal_result_t osal_cond_destroy(osal_cond_t cond);
OSAL_API osal_result_t osal_cond_wait(osal_cond_t cond, osal_mutex_t mutex);
OSAL_API osal_result_t osal_cond_timedwait(osal_cond_t cond, osal_mutex_t mutex, uint64_t timeout_ms);
OSAL_API osal_result_t osal_cond_signal(osal_cond_t cond);
OSAL_API osal_result_t osal_cond_broadcast(osal_cond_t cond);

/*===========================================================================
 * Time API
 *===========================================================================*/

/* Get monotonic time in nanoseconds (for measuring elapsed time) */
OSAL_API uint64_t osal_time_monotonic_ns(void);

/* Get monotonic time in milliseconds */
OSAL_API uint64_t osal_time_monotonic_ms(void);

/* Get wall-clock time in seconds since epoch */
OSAL_API uint64_t osal_time_epoch_sec(void);

/* Get wall-clock time in milliseconds since epoch */
OSAL_API uint64_t osal_time_epoch_ms(void);

/* Sleep for milliseconds */
OSAL_API void osal_sleep_ms(uint64_t ms);

/* Sleep for microseconds */
OSAL_API void osal_sleep_us(uint64_t us);

/*===========================================================================
 * Process API
 *===========================================================================*/

/* Get current process ID */
OSAL_API uint64_t osal_getpid(void);

/* Get current working directory (caller frees returned string) */
OSAL_API char* osal_getcwd(void);

/* Check if file descriptor is a TTY */
OSAL_API bool osal_isatty(int fd);

/*===========================================================================
 * Dynamic Loading API
 *===========================================================================*/

typedef void* osal_dl_handle_t;

/* Load a shared library */
OSAL_API osal_dl_handle_t osal_dl_open(const char* path);

/* Get symbol address from loaded library */
OSAL_API void* osal_dl_sym(osal_dl_handle_t handle, const char* symbol);

/* Unload a shared library */
OSAL_API osal_result_t osal_dl_close(osal_dl_handle_t handle);

/* Get last dynamic loading error (thread-local) */
OSAL_API const char* osal_dl_error(void);

/*===========================================================================
 * Network Socket Abstraction
 *===========================================================================*/

#if OSAL_PLATFORM_WINDOWS
    typedef uintptr_t osal_socket_t;
    #define OSAL_INVALID_SOCKET ((osal_socket_t)(~0))
#else
    typedef int osal_socket_t;
    #define OSAL_INVALID_SOCKET (-1)
#endif

/* Initialize socket subsystem (no-op on POSIX, required on Windows) */
OSAL_API osal_result_t osal_socket_init(void);

/* Cleanup socket subsystem */
OSAL_API void osal_socket_cleanup(void);

/* Close a socket */
OSAL_API osal_result_t osal_socket_close(osal_socket_t sock);

/* Get last socket error code */
OSAL_API int osal_socket_error(void);

/*===========================================================================
 * Path Utilities
 *===========================================================================*/

/* Get platform path separator ('/' on POSIX, '\\' on Windows) */
OSAL_API char osal_path_separator(void);

/* Normalize path (convert separators, resolve . and ..) 
 * Caller frees returned string */
OSAL_API char* osal_path_normalize(const char* path);

/* Join two path components (caller frees returned string) */
OSAL_API char* osal_path_join(const char* base, const char* component);

/*===========================================================================
 * Atomic Operations
 *===========================================================================*/

/* Atomic increment, returns new value */
OSAL_API int32_t osal_atomic_inc32(volatile int32_t* value);
OSAL_API int64_t osal_atomic_inc64(volatile int64_t* value);

/* Atomic decrement, returns new value */
OSAL_API int32_t osal_atomic_dec32(volatile int32_t* value);
OSAL_API int64_t osal_atomic_dec64(volatile int64_t* value);

/* Atomic compare-and-swap, returns true if exchanged */
OSAL_API bool osal_atomic_cas32(volatile int32_t* value, int32_t expected, int32_t desired);
OSAL_API bool osal_atomic_cas64(volatile int64_t* value, int64_t expected, int64_t desired);
OSAL_API bool osal_atomic_cas_ptr(volatile void** value, void* expected, void* desired);

/* Memory barrier */
OSAL_API void osal_memory_barrier(void);

/*===========================================================================
 * Random Number Generation
 *===========================================================================*/

/* Thread-safe random number generation using internal state */
OSAL_API uint32_t osal_random_u32(uint32_t* state);

/* Get cryptographically secure random bytes */
OSAL_API osal_result_t osal_random_bytes(void* buffer, size_t length);

#ifdef __cplusplus
}
#endif

#endif /* _COGWXP_OSAL_H_ */
