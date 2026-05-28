/**
 * @file cogutil.h
 * @brief CogUtil - Common Utilities for OpenCog CoGWXP-OS9 Integration
 * 
 * This header provides the foundational utilities for the OpenCog framework
 * integration with the CoGWXP-OS9 cognitive operating system.
 * 
 * @copyright AGPL-3.0
 */

#ifndef _COGWXP_COGUTIL_H_
#define _COGWXP_COGUTIL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

/*===========================================================================
 * Version Information
 *===========================================================================*/

#define COGUTIL_VERSION_MAJOR   1
#define COGUTIL_VERSION_MINOR   0
#define COGUTIL_VERSION_PATCH   0
#define COGUTIL_VERSION_STRING  "1.0.0-cogwxp"

/*===========================================================================
 * Platform Detection
 *===========================================================================*/

#if defined(_WIN32) || defined(_WIN64) || defined(__NT__)
    #define COGUTIL_PLATFORM_NT     1
    #define COGUTIL_PLATFORM_NAME   "Windows NT"
#elif defined(__plan9__)
    #define COGUTIL_PLATFORM_PLAN9  1
    #define COGUTIL_PLATFORM_NAME   "Plan 9"
#elif defined(__inferno__)
    #define COGUTIL_PLATFORM_INFERNO 1
    #define COGUTIL_PLATFORM_NAME   "Inferno"
#else
    #define COGUTIL_PLATFORM_POSIX  1
    #define COGUTIL_PLATFORM_NAME   "POSIX"
#endif

/*===========================================================================
 * Export/Import Macros
 *===========================================================================*/

#ifdef COGUTIL_PLATFORM_NT
    #ifdef COGUTIL_EXPORTS
        #define COGUTIL_API __declspec(dllexport)
    #else
        #define COGUTIL_API __declspec(dllimport)
    #endif
#else
    #define COGUTIL_API
#endif

/*===========================================================================
 * Basic Types
 *===========================================================================*/

typedef uint64_t    cog_handle_t;
typedef int32_t     cog_result_t;
typedef double      cog_truth_value_t;
typedef double      cog_attention_value_t;

/* Result codes */
#define COG_SUCCESS             0
#define COG_ERROR_GENERAL      -1
#define COG_ERROR_MEMORY       -2
#define COG_ERROR_INVALID_ARG  -3
#define COG_ERROR_NOT_FOUND    -4
#define COG_ERROR_EXISTS       -5
#define COG_ERROR_TIMEOUT      -6
#define COG_ERROR_PERMISSION   -7
#define COG_ERROR_IO           -8
#define COG_ERROR_STATE        -9
#define COG_ERROR_NOT_IMPLEMENTED -10
#define COG_ERROR_INVALID_FORMAT  -11
#define COG_ERROR_DIMENSION_MISMATCH -12
#define COG_ERROR_NETWORK       -13
#define COG_ERROR_AUTH          -14
#define COG_ERROR_PARSE         -15
#define COG_ERROR_CONFIG        -16
#define COG_ERROR_INIT          -17
#define COG_ERROR_EMPTY         -18
#define COG_ERROR_EXEC          -19

/* Aliases used throughout the codebase */
#define COG_OK                      COG_SUCCESS
#define COG_ERROR_INVALID_PARAM     COG_ERROR_INVALID_ARG
#define COG_ERROR_INVALID_ARGUMENT  COG_ERROR_INVALID_ARG
#define COG_ERROR_OUT_OF_MEMORY     COG_ERROR_MEMORY
#define COG_ERROR_INVALID_STATE     COG_ERROR_STATE

/* Convenience memory macros */
#define COG_MALLOC(size)            malloc(size)
#define COG_CALLOC(n, size)         calloc(n, size)
#define COG_REALLOC(ptr, size)      realloc(ptr, size)
#define COG_FREE(ptr)               free(ptr)
#define COG_STRDUP(s)               cog_strdup(s)

/* Version string alias */
#define COGUTIL_VERSION             COGUTIL_VERSION_STRING

/* Logging macros with file/line/func context */
#define COG_LOG_INFO(fmt, ...)  cog_log(COG_LOG_INFO,  __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define COG_LOG_DEBUG(fmt, ...) cog_log(COG_LOG_DEBUG, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define COG_LOG_WARN(fmt, ...)  cog_log(COG_LOG_WARN,  __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define COG_LOG_ERROR(fmt, ...) cog_log(COG_LOG_ERROR, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

/*===========================================================================
 * Logging System
 *===========================================================================*/

typedef enum {
    COG_LOG_TRACE = 0,
    COG_LOG_DEBUG = 1,
    COG_LOG_INFO  = 2,
    COG_LOG_WARN  = 3,
    COG_LOG_ERROR = 4,
    COG_LOG_FATAL = 5
} cog_log_level_t;

typedef void (*cog_log_callback_t)(
    cog_log_level_t level,
    const char* file,
    int line,
    const char* func,
    const char* message,
    void* user_data
);

COGUTIL_API void            cog_log_init(void);
COGUTIL_API void            cog_log_shutdown(void);
COGUTIL_API void            cog_log_set_level(cog_log_level_t level);
COGUTIL_API cog_log_level_t cog_log_get_level(void);
COGUTIL_API void            cog_log_set_callback(cog_log_callback_t callback, void* user_data);
COGUTIL_API void            cog_log_set_file(FILE* file);
COGUTIL_API void            cog_log_set_options(bool timestamps, bool colors);
COGUTIL_API void            cog_log(cog_log_level_t level, const char* file, int line, const char* func, const char* fmt, ...);

/*===========================================================================
 * System Lifecycle
 *===========================================================================*/

COGUTIL_API cog_result_t cog_init(void);
COGUTIL_API void         cog_shutdown(void);
COGUTIL_API const char*  cog_version(void);

/* Convenience macros with module context */
#define COG_TRACE(module, fmt, ...) cog_log(COG_LOG_TRACE, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define COG_DEBUG(module, fmt, ...) cog_log(COG_LOG_DEBUG, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define COG_INFO(module, fmt, ...)  cog_log(COG_LOG_INFO,  __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define COG_WARN(module, fmt, ...)  cog_log(COG_LOG_WARN,  __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define COG_ERROR(module, fmt, ...) cog_log(COG_LOG_ERROR, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define COG_FATAL(module, fmt, ...) cog_log(COG_LOG_FATAL, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

/*===========================================================================
 * Memory Management
 *===========================================================================*/

typedef struct cog_allocator {
    void* (*alloc)(size_t size, void* user_data);
    void* (*realloc)(void* ptr, size_t size, void* user_data);
    void  (*free)(void* ptr, void* user_data);
    void* user_data;
} cog_allocator_t;

COGUTIL_API void  cog_mem_init(cog_allocator_t* allocator);
COGUTIL_API void  cog_mem_shutdown(void);
COGUTIL_API void* cog_alloc(size_t size);
COGUTIL_API void* cog_calloc(size_t count, size_t size);
COGUTIL_API void* cog_realloc(void* ptr, size_t size);
COGUTIL_API void  cog_free(void* ptr);
COGUTIL_API char* cog_strdup(const char* str);

/* Memory statistics */
typedef struct cog_mem_stats {
    size_t total_allocated;
    size_t total_freed;
    size_t current_usage;
    size_t peak_usage;
    size_t allocation_count;
    size_t free_count;
} cog_mem_stats_t;

COGUTIL_API void cog_mem_get_stats(cog_mem_stats_t* stats);

/*===========================================================================
 * Threading Primitives
 *===========================================================================*/

typedef struct cog_mutex*       cog_mutex_t;
typedef struct cog_rwlock*      cog_rwlock_t;
typedef struct cog_cond*        cog_cond_t;
typedef struct cog_thread*      cog_thread_t;
typedef struct cog_threadpool*  cog_threadpool_t;

typedef void* (*cog_thread_func_t)(void* arg);

/* Mutex operations */
COGUTIL_API cog_mutex_t cog_mutex_create(void);
COGUTIL_API void        cog_mutex_destroy(cog_mutex_t mutex);
COGUTIL_API void        cog_mutex_lock(cog_mutex_t mutex);
COGUTIL_API bool        cog_mutex_trylock(cog_mutex_t mutex);
COGUTIL_API void        cog_mutex_unlock(cog_mutex_t mutex);

/* Read-write lock operations */
COGUTIL_API cog_rwlock_t cog_rwlock_create(void);
COGUTIL_API void         cog_rwlock_destroy(cog_rwlock_t rwlock);
COGUTIL_API void         cog_rwlock_read_lock(cog_rwlock_t rwlock);
COGUTIL_API void         cog_rwlock_read_unlock(cog_rwlock_t rwlock);
COGUTIL_API void         cog_rwlock_write_lock(cog_rwlock_t rwlock);
COGUTIL_API void         cog_rwlock_write_unlock(cog_rwlock_t rwlock);

/* Condition variable operations */
COGUTIL_API cog_cond_t cog_cond_create(void);
COGUTIL_API void       cog_cond_destroy(cog_cond_t cond);
COGUTIL_API void       cog_cond_wait(cog_cond_t cond, cog_mutex_t mutex);
COGUTIL_API bool       cog_cond_timedwait(cog_cond_t cond, cog_mutex_t mutex, uint32_t timeout_ms);
COGUTIL_API void       cog_cond_signal(cog_cond_t cond);
COGUTIL_API void       cog_cond_broadcast(cog_cond_t cond);

/* Thread operations */
COGUTIL_API cog_thread_t cog_thread_create(cog_thread_func_t func, void* arg);
COGUTIL_API void         cog_thread_join(cog_thread_t thread);
COGUTIL_API void         cog_thread_detach(cog_thread_t thread);
COGUTIL_API void         cog_thread_yield(void);
COGUTIL_API uint64_t     cog_thread_id(void);

/* Thread pool operations */
COGUTIL_API cog_threadpool_t cog_threadpool_create(size_t num_threads);
COGUTIL_API void             cog_threadpool_destroy(cog_threadpool_t pool);
COGUTIL_API cog_result_t     cog_threadpool_submit(cog_threadpool_t pool, cog_thread_func_t func, void* arg);

/* Task function type (fire-and-forget, returns void) */
typedef void (*cog_task_func_t)(void* arg);

/* Task queue */
struct cog_task_queue;
typedef struct cog_task_queue cog_task_queue_t;

COGUTIL_API cog_result_t cog_task_queue_create(cog_task_queue_t** queue, size_t max_size);
COGUTIL_API void         cog_task_queue_destroy(cog_task_queue_t* queue);
COGUTIL_API cog_result_t cog_task_queue_push(cog_task_queue_t* queue, cog_task_func_t func, void* arg);
COGUTIL_API cog_result_t cog_task_queue_pop(cog_task_queue_t* queue, cog_task_func_t* func, void** arg);
COGUTIL_API size_t       cog_task_queue_size(cog_task_queue_t* queue);
COGUTIL_API void         cog_task_queue_close(cog_task_queue_t* queue);

/* Global thread pool (task-based) */
COGUTIL_API cog_result_t cog_thread_pool_create(size_t thread_count);
COGUTIL_API void         cog_thread_pool_destroy(void);
COGUTIL_API cog_result_t cog_thread_pool_submit(cog_task_func_t func, void* arg);

/*===========================================================================
 * UUID Generation
 *===========================================================================*/

typedef struct {
    uint8_t bytes[16];
} cog_uuid_t;

COGUTIL_API void cog_uuid_generate(cog_uuid_t* uuid);
COGUTIL_API void cog_uuid_to_string(const cog_uuid_t* uuid, char* buffer);
COGUTIL_API bool cog_uuid_from_string(cog_uuid_t* uuid, const char* str);
COGUTIL_API bool cog_uuid_equals(const cog_uuid_t* a, const cog_uuid_t* b);

/*===========================================================================
 * Hash Functions
 *===========================================================================*/

COGUTIL_API uint32_t cog_hash_string(const char* str);
COGUTIL_API uint64_t cog_hash_data(const void* data, size_t size);
COGUTIL_API uint64_t cog_hash_combine(uint64_t h1, uint64_t h2);

/*===========================================================================
 * Configuration Management
 *===========================================================================*/

struct cog_config;
typedef struct cog_config cog_config_t;

COGUTIL_API cog_result_t cog_config_create(cog_config_t** config);
COGUTIL_API void         cog_config_destroy(cog_config_t* config);
COGUTIL_API cog_result_t cog_config_load(cog_config_t* config, const char* path);
COGUTIL_API cog_result_t cog_config_save(cog_config_t* config, const char* path);

COGUTIL_API cog_result_t cog_config_set(cog_config_t* config, const char* key, const char* value);
COGUTIL_API const char*  cog_config_get(cog_config_t* config, const char* key, const char* default_val);
COGUTIL_API int          cog_config_get_int(cog_config_t* config, const char* key, int default_val);
COGUTIL_API double       cog_config_get_double(cog_config_t* config, const char* key, double default_val);
COGUTIL_API bool         cog_config_get_bool(cog_config_t* config, const char* key, bool default_val);

/* Compatibility aliases for old API names */
#define cog_config_load_file(cfg, path)       cog_config_load(cfg, path)
#define cog_config_save_file(cfg, path)       cog_config_save(cfg, path)
#define cog_config_get_string(cfg, key, def)  cog_config_get(cfg, key, def)
#define cog_config_get_float(cfg, key, def)   cog_config_get_double(cfg, key, def)
#define cog_config_set_string(cfg, key, val)  cog_config_set(cfg, key, val)

/*===========================================================================
 * Timer and Time Utilities
 *===========================================================================*/

COGUTIL_API uint64_t cog_time_now_ms(void);
COGUTIL_API uint64_t cog_time_now_us(void);
COGUTIL_API void     cog_time_sleep_ms(uint32_t ms);
#define              cog_sleep_ms(ms) cog_time_sleep_ms(ms)

typedef struct cog_timer* cog_timer_t;
typedef void (*cog_timer_callback_t)(void* user_data);

COGUTIL_API cog_timer_t  cog_timer_create(uint32_t interval_ms, cog_timer_callback_t callback, void* user_data);
COGUTIL_API void         cog_timer_destroy(cog_timer_t timer);
COGUTIL_API void         cog_timer_start(cog_timer_t timer);
COGUTIL_API void         cog_timer_stop(cog_timer_t timer);
COGUTIL_API void         cog_timer_reset(cog_timer_t timer);

/*===========================================================================
 * String Utilities
 *===========================================================================*/

COGUTIL_API char*   cog_string_trim(char* str);
COGUTIL_API char**  cog_string_split(const char* str, char delimiter, size_t* count);
COGUTIL_API void    cog_string_split_free(char** parts, size_t count);
COGUTIL_API char*   cog_string_join(const char** parts, size_t count, const char* delimiter);
COGUTIL_API bool    cog_string_starts_with(const char* str, const char* prefix);
COGUTIL_API bool    cog_string_ends_with(const char* str, const char* suffix);

/*===========================================================================
 * Data Structures
 *===========================================================================*/

/* Dynamic array */
typedef struct cog_array* cog_array_t;

COGUTIL_API cog_array_t cog_array_create(size_t element_size);
COGUTIL_API void        cog_array_destroy(cog_array_t array);
COGUTIL_API size_t      cog_array_size(cog_array_t array);
COGUTIL_API void*       cog_array_get(cog_array_t array, size_t index);
COGUTIL_API void        cog_array_push(cog_array_t array, const void* element);
COGUTIL_API void        cog_array_pop(cog_array_t array);
COGUTIL_API void        cog_array_clear(cog_array_t array);

/* Hash map */
typedef struct cog_hashmap* cog_hashmap_t;

COGUTIL_API cog_hashmap_t cog_hashmap_create(void);
COGUTIL_API void          cog_hashmap_destroy(cog_hashmap_t map);
COGUTIL_API size_t        cog_hashmap_size(cog_hashmap_t map);
COGUTIL_API void*         cog_hashmap_get(cog_hashmap_t map, const char* key);
COGUTIL_API void          cog_hashmap_set(cog_hashmap_t map, const char* key, void* value);
COGUTIL_API bool          cog_hashmap_remove(cog_hashmap_t map, const char* key);
COGUTIL_API bool          cog_hashmap_contains(cog_hashmap_t map, const char* key);

/* Linked list */
typedef struct cog_list* cog_list_t;

COGUTIL_API cog_list_t cog_list_create(void);
COGUTIL_API void       cog_list_destroy(cog_list_t list);
COGUTIL_API size_t     cog_list_size(cog_list_t list);
COGUTIL_API void       cog_list_push_front(cog_list_t list, void* data);
COGUTIL_API void       cog_list_push_back(cog_list_t list, void* data);
COGUTIL_API void*      cog_list_pop_front(cog_list_t list);
COGUTIL_API void*      cog_list_pop_back(cog_list_t list);

/*===========================================================================
 * Platform Abstraction
 *===========================================================================*/

/* File system operations */
COGUTIL_API bool        cog_file_exists(const char* path);
COGUTIL_API bool        cog_dir_exists(const char* path);
COGUTIL_API cog_result_t cog_mkdir(const char* path);
COGUTIL_API cog_result_t cog_rmdir(const char* path);
COGUTIL_API char*       cog_getcwd(char* buffer, size_t size);

/* Environment */
COGUTIL_API const char* cog_getenv(const char* name);
COGUTIL_API void        cog_setenv(const char* name, const char* value);

#ifdef __cplusplus
}
#endif

#endif /* _COGWXP_COGUTIL_H_ */
