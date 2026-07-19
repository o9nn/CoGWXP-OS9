/**
 * @file cogutil.c
 * @brief CogUtil Foundation Library Implementation
 * 
 * Core utilities for the OpenCog framework including logging, configuration,
 * memory management, threading, and common data structures.
 * 
 * @copyright CoGWXP-OS9 Project
 */

#include "cogutil.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>

#if defined(_WIN32)
#include <io.h>
#ifndef isatty
#define isatty _isatty
#endif
#ifndef fileno
#define fileno _fileno
#endif
#else
#include <unistd.h>
#endif

/* Forward declarations for memory-tracking helpers used before their
 * definitions further down in this file. */
COGUTIL_API void* cog_malloc_debug(size_t size, const char* file, int line);
COGUTIL_API void* cog_calloc_debug(size_t count, size_t size, const char* file, int line);
COGUTIL_API void* cog_realloc_debug(void* ptr, size_t size, const char* file, int line);
COGUTIL_API void  cog_free_debug(void* ptr, const char* file, int line);
COGUTIL_API char* cog_strdup_debug(const char* str, const char* file, int line);

/*===========================================================================
 * Global State
 *===========================================================================*/

static struct {
    /* Logging */
    cog_log_level_t log_level;
    FILE* log_file;
    cog_log_callback_t log_callback;
    void* log_callback_data;
    pthread_mutex_t log_lock;
    bool log_timestamps;
    bool log_colors;
    
    /* Memory tracking */
    size_t allocated_bytes;
    size_t allocation_count;
    size_t peak_allocated;
    pthread_mutex_t mem_lock;
    
    /* Configuration */
    cog_config_t* config;
    pthread_rwlock_t config_lock;
    
    /* Thread pool */
    struct {
        pthread_t* threads;
        size_t thread_count;
        cog_task_queue_t* queue;
        bool running;
    } thread_pool;
    
    /* Initialization state */
    bool initialized;
    pthread_mutex_t init_lock;
} g_cogutil = {
    .log_level = COG_LOG_INFO,
    .log_file = NULL,
    .log_callback = NULL,
    .log_timestamps = true,
    .log_colors = true,
    .initialized = false
};

/*===========================================================================
 * Logging Implementation
 *===========================================================================*/

static const char* log_level_names[] = {
    "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"
};

static const char* log_level_colors[] = {
    "\033[90m",  /* TRACE - gray */
    "\033[36m",  /* DEBUG - cyan */
    "\033[32m",  /* INFO - green */
    "\033[33m",  /* WARN - yellow */
    "\033[31m",  /* ERROR - red */
    "\033[35m"   /* FATAL - magenta */
};

COGUTIL_API void cog_log_set_level(cog_log_level_t level) {
    pthread_mutex_lock(&g_cogutil.log_lock);
    g_cogutil.log_level = level;
    pthread_mutex_unlock(&g_cogutil.log_lock);
}

COGUTIL_API cog_log_level_t cog_log_get_level(void) {
    return g_cogutil.log_level;
}

COGUTIL_API void cog_log_set_file(FILE* file) {
    pthread_mutex_lock(&g_cogutil.log_lock);
    g_cogutil.log_file = file;
    pthread_mutex_unlock(&g_cogutil.log_lock);
}

COGUTIL_API void cog_log_set_callback(cog_log_callback_t callback, void* data) {
    pthread_mutex_lock(&g_cogutil.log_lock);
    g_cogutil.log_callback = callback;
    g_cogutil.log_callback_data = data;
    pthread_mutex_unlock(&g_cogutil.log_lock);
}

COGUTIL_API void cog_log_set_options(bool timestamps, bool colors) {
    pthread_mutex_lock(&g_cogutil.log_lock);
    g_cogutil.log_timestamps = timestamps;
    g_cogutil.log_colors = colors;
    pthread_mutex_unlock(&g_cogutil.log_lock);
}

COGUTIL_API void cog_log(
    cog_log_level_t level,
    const char* file,
    int line,
    const char* func,
    const char* format,
    ...
) {
    if (level < g_cogutil.log_level) return;
    
    pthread_mutex_lock(&g_cogutil.log_lock);
    
    FILE* output = g_cogutil.log_file ? g_cogutil.log_file : stderr;
    
    /* Build timestamp */
    char timestamp[32] = "";
    if (g_cogutil.log_timestamps) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        struct tm* tm_info = localtime(&tv.tv_sec);
        snprintf(timestamp, sizeof(timestamp), "%02d:%02d:%02d.%03ld ",
            tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec,
            tv.tv_usec / 1000);
    }
    
    /* Build message */
    char message[4096];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    
    /* Output with colors if enabled */
    if (g_cogutil.log_colors && isatty(fileno(output))) {
        fprintf(output, "%s%s[%-5s]\033[0m %s:%d (%s): %s\n",
            timestamp,
            log_level_colors[level],
            log_level_names[level],
            file, line, func,
            message);
    } else {
        fprintf(output, "%s[%-5s] %s:%d (%s): %s\n",
            timestamp,
            log_level_names[level],
            file, line, func,
            message);
    }
    
    fflush(output);
    
    /* Call callback if set */
    if (g_cogutil.log_callback) {
        g_cogutil.log_callback(level, file, line, func, message, 
            g_cogutil.log_callback_data);
    }
    
    pthread_mutex_unlock(&g_cogutil.log_lock);
}

/*===========================================================================
 * Memory Management Implementation
 *===========================================================================*/

typedef struct mem_header {
    size_t size;
    const char* file;
    int line;
    uint32_t magic;
} mem_header_t;

#define MEM_MAGIC 0xDEADBEEF

COGUTIL_API void* cog_malloc_debug(size_t size, const char* file, int line) {
    if (size == 0) return NULL;
    
    mem_header_t* header = malloc(sizeof(mem_header_t) + size);
    if (!header) {
        COG_LOG_ERROR("Memory allocation failed: %zu bytes at %s:%d", size, file, line);
        return NULL;
    }
    
    header->size = size;
    header->file = file;
    header->line = line;
    header->magic = MEM_MAGIC;
    
    pthread_mutex_lock(&g_cogutil.mem_lock);
    g_cogutil.allocated_bytes += size;
    g_cogutil.allocation_count++;
    if (g_cogutil.allocated_bytes > g_cogutil.peak_allocated) {
        g_cogutil.peak_allocated = g_cogutil.allocated_bytes;
    }
    pthread_mutex_unlock(&g_cogutil.mem_lock);
    
    return (void*)(header + 1);
}

COGUTIL_API void* cog_calloc_debug(size_t count, size_t size, const char* file, int line) {
    size_t total = count * size;
    void* ptr = cog_malloc_debug(total, file, line);
    if (ptr) {
        memset(ptr, 0, total);
    }
    return ptr;
}

COGUTIL_API void* cog_realloc_debug(void* ptr, size_t size, const char* file, int line) {
    if (!ptr) return cog_malloc_debug(size, file, line);
    if (size == 0) {
        cog_free_debug(ptr, file, line);
        return NULL;
    }
    
    mem_header_t* old_header = ((mem_header_t*)ptr) - 1;
    if (old_header->magic != MEM_MAGIC) {
        COG_LOG_ERROR("Invalid memory block in realloc at %s:%d", file, line);
        return NULL;
    }
    
    size_t old_size = old_header->size;
    
    mem_header_t* new_header = realloc(old_header, sizeof(mem_header_t) + size);
    if (!new_header) {
        COG_LOG_ERROR("Memory reallocation failed: %zu bytes at %s:%d", size, file, line);
        return NULL;
    }
    
    new_header->size = size;
    new_header->file = file;
    new_header->line = line;
    
    pthread_mutex_lock(&g_cogutil.mem_lock);
    g_cogutil.allocated_bytes = g_cogutil.allocated_bytes - old_size + size;
    if (g_cogutil.allocated_bytes > g_cogutil.peak_allocated) {
        g_cogutil.peak_allocated = g_cogutil.allocated_bytes;
    }
    pthread_mutex_unlock(&g_cogutil.mem_lock);
    
    return (void*)(new_header + 1);
}

COGUTIL_API void cog_free_debug(void* ptr, const char* file, int line) {
    if (!ptr) return;
    
    mem_header_t* header = ((mem_header_t*)ptr) - 1;
    if (header->magic != MEM_MAGIC) {
        COG_LOG_ERROR("Invalid memory block in free at %s:%d", file, line);
        return;
    }
    
    pthread_mutex_lock(&g_cogutil.mem_lock);
    g_cogutil.allocated_bytes -= header->size;
    g_cogutil.allocation_count--;
    pthread_mutex_unlock(&g_cogutil.mem_lock);
    
    header->magic = 0; /* Invalidate */
    free(header);
}

COGUTIL_API char* cog_strdup_debug(const char* str, const char* file, int line) {
    if (!str) return NULL;
    size_t len = strlen(str) + 1;
    char* dup = cog_malloc_debug(len, file, line);
    if (dup) {
        memcpy(dup, str, len);
    }
    return dup;
}

COGUTIL_API void cog_mem_stats(size_t* allocated, size_t* count, size_t* peak) {
    pthread_mutex_lock(&g_cogutil.mem_lock);
    if (allocated) *allocated = g_cogutil.allocated_bytes;
    if (count) *count = g_cogutil.allocation_count;
    if (peak) *peak = g_cogutil.peak_allocated;
    pthread_mutex_unlock(&g_cogutil.mem_lock);
}

/*===========================================================================
 * Configuration Implementation
 *===========================================================================*/

struct cog_config {
    struct config_entry {
        char* key;
        char* value;
        struct config_entry* next;
    }* entries;
    size_t count;
};

COGUTIL_API cog_result_t cog_config_create(cog_config_t** config) {
    if (!config) return COG_ERROR_INVALID_PARAM;
    
    cog_config_t* c = COG_CALLOC(1, sizeof(cog_config_t));
    if (!c) return COG_ERROR_MEMORY;
    
    *config = c;
    return COG_OK;
}

COGUTIL_API void cog_config_destroy(cog_config_t* config) {
    if (!config) return;
    
    struct config_entry* entry = config->entries;
    while (entry) {
        struct config_entry* next = entry->next;
        COG_FREE(entry->key);
        COG_FREE(entry->value);
        COG_FREE(entry);
        entry = next;
    }
    
    COG_FREE(config);
}

COGUTIL_API cog_result_t cog_config_load(cog_config_t* config, const char* filename) {
    if (!config || !filename) return COG_ERROR_INVALID_PARAM;
    
    FILE* f = fopen(filename, "r");
    if (!f) return COG_ERROR_IO;
    
    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        /* Skip comments and empty lines */
        char* p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\n' || *p == '\0') continue;
        
        /* Parse key=value */
        char* eq = strchr(p, '=');
        if (!eq) continue;
        
        *eq = '\0';
        char* key = p;
        char* value = eq + 1;
        
        /* Trim whitespace */
        while (*key && (key[strlen(key)-1] == ' ' || key[strlen(key)-1] == '\t')) {
            key[strlen(key)-1] = '\0';
        }
        while (*value == ' ' || *value == '\t') value++;
        while (*value && (value[strlen(value)-1] == '\n' || 
               value[strlen(value)-1] == ' ' || value[strlen(value)-1] == '\t')) {
            value[strlen(value)-1] = '\0';
        }
        
        cog_config_set(config, key, value);
    }
    
    fclose(f);
    return COG_OK;
}

COGUTIL_API cog_result_t cog_config_save(cog_config_t* config, const char* filename) {
    if (!config || !filename) return COG_ERROR_INVALID_PARAM;
    
    FILE* f = fopen(filename, "w");
    if (!f) return COG_ERROR_IO;
    
    fprintf(f, "# CoGWXP-OS9 Configuration\n");
    fprintf(f, "# Generated: %s\n\n", __DATE__);
    
    struct config_entry* entry = config->entries;
    while (entry) {
        fprintf(f, "%s=%s\n", entry->key, entry->value);
        entry = entry->next;
    }
    
    fclose(f);
    return COG_OK;
}

COGUTIL_API cog_result_t cog_config_set(cog_config_t* config, const char* key, const char* value) {
    if (!config || !key) return COG_ERROR_INVALID_PARAM;
    
    /* Check if key exists */
    struct config_entry* entry = config->entries;
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            COG_FREE(entry->value);
            entry->value = value ? COG_STRDUP(value) : NULL;
            return COG_OK;
        }
        entry = entry->next;
    }
    
    /* Create new entry */
    entry = COG_CALLOC(1, sizeof(struct config_entry));
    if (!entry) return COG_ERROR_MEMORY;
    
    entry->key = COG_STRDUP(key);
    entry->value = value ? COG_STRDUP(value) : NULL;
    entry->next = config->entries;
    config->entries = entry;
    config->count++;
    
    return COG_OK;
}

COGUTIL_API const char* cog_config_get(cog_config_t* config, const char* key, const char* default_value) {
    if (!config || !key) return default_value;
    
    struct config_entry* entry = config->entries;
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            return entry->value ? entry->value : default_value;
        }
        entry = entry->next;
    }
    
    return default_value;
}

COGUTIL_API int cog_config_get_int(cog_config_t* config, const char* key, int default_value) {
    const char* value = cog_config_get(config, key, NULL);
    if (!value) return default_value;
    return atoi(value);
}

COGUTIL_API double cog_config_get_double(cog_config_t* config, const char* key, double default_value) {
    const char* value = cog_config_get(config, key, NULL);
    if (!value) return default_value;
    return atof(value);
}

COGUTIL_API bool cog_config_get_bool(cog_config_t* config, const char* key, bool default_value) {
    const char* value = cog_config_get(config, key, NULL);
    if (!value) return default_value;
    return (strcmp(value, "true") == 0 || strcmp(value, "1") == 0 ||
            strcmp(value, "yes") == 0 || strcmp(value, "on") == 0);
}

/*===========================================================================
 * Task Queue Implementation
 *===========================================================================*/

typedef struct task_node {
    cog_task_func_t func;
    void* arg;
    struct task_node* next;
} task_node_t;

struct cog_task_queue {
    task_node_t* head;
    task_node_t* tail;
    size_t count;
    size_t max_size;
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
    bool closed;
};

COGUTIL_API cog_result_t cog_task_queue_create(cog_task_queue_t** queue, size_t max_size) {
    if (!queue) return COG_ERROR_INVALID_PARAM;
    
    cog_task_queue_t* q = COG_CALLOC(1, sizeof(cog_task_queue_t));
    if (!q) return COG_ERROR_MEMORY;
    
    q->max_size = max_size > 0 ? max_size : SIZE_MAX;
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    pthread_cond_init(&q->not_full, NULL);
    
    *queue = q;
    return COG_OK;
}

COGUTIL_API void cog_task_queue_destroy(cog_task_queue_t* queue) {
    if (!queue) return;
    
    pthread_mutex_lock(&queue->lock);
    queue->closed = true;
    pthread_cond_broadcast(&queue->not_empty);
    pthread_cond_broadcast(&queue->not_full);
    
    task_node_t* node = queue->head;
    while (node) {
        task_node_t* next = node->next;
        COG_FREE(node);
        node = next;
    }
    
    pthread_mutex_unlock(&queue->lock);
    
    pthread_mutex_destroy(&queue->lock);
    pthread_cond_destroy(&queue->not_empty);
    pthread_cond_destroy(&queue->not_full);
    
    COG_FREE(queue);
}

COGUTIL_API cog_result_t cog_task_queue_push(cog_task_queue_t* queue, cog_task_func_t func, void* arg) {
    if (!queue || !func) return COG_ERROR_INVALID_PARAM;
    
    task_node_t* node = COG_CALLOC(1, sizeof(task_node_t));
    if (!node) return COG_ERROR_MEMORY;
    
    node->func = func;
    node->arg = arg;
    
    pthread_mutex_lock(&queue->lock);
    
    while (queue->count >= queue->max_size && !queue->closed) {
        pthread_cond_wait(&queue->not_full, &queue->lock);
    }
    
    if (queue->closed) {
        pthread_mutex_unlock(&queue->lock);
        COG_FREE(node);
        return COG_ERROR_STATE;
    }
    
    if (queue->tail) {
        queue->tail->next = node;
        queue->tail = node;
    } else {
        queue->head = queue->tail = node;
    }
    queue->count++;
    
    pthread_cond_signal(&queue->not_empty);
    pthread_mutex_unlock(&queue->lock);
    
    return COG_OK;
}

COGUTIL_API cog_result_t cog_task_queue_pop(cog_task_queue_t* queue, cog_task_func_t* func, void** arg) {
    if (!queue || !func || !arg) return COG_ERROR_INVALID_PARAM;
    
    pthread_mutex_lock(&queue->lock);
    
    while (queue->count == 0 && !queue->closed) {
        pthread_cond_wait(&queue->not_empty, &queue->lock);
    }
    
    if (queue->count == 0) {
        pthread_mutex_unlock(&queue->lock);
        return COG_ERROR_EMPTY;
    }
    
    task_node_t* node = queue->head;
    queue->head = node->next;
    if (!queue->head) {
        queue->tail = NULL;
    }
    queue->count--;
    
    *func = node->func;
    *arg = node->arg;
    
    pthread_cond_signal(&queue->not_full);
    pthread_mutex_unlock(&queue->lock);
    
    COG_FREE(node);
    return COG_OK;
}

COGUTIL_API size_t cog_task_queue_size(cog_task_queue_t* queue) {
    if (!queue) return 0;
    
    pthread_mutex_lock(&queue->lock);
    size_t size = queue->count;
    pthread_mutex_unlock(&queue->lock);
    
    return size;
}

COGUTIL_API void cog_task_queue_close(cog_task_queue_t* queue) {
    if (!queue) return;
    
    pthread_mutex_lock(&queue->lock);
    queue->closed = true;
    pthread_cond_broadcast(&queue->not_empty);
    pthread_cond_broadcast(&queue->not_full);
    pthread_mutex_unlock(&queue->lock);
}

/*===========================================================================
 * Thread Pool Implementation
 *===========================================================================*/

static void* thread_pool_worker(void* arg) {
    cog_task_queue_t* queue = (cog_task_queue_t*)arg;
    
    while (1) {
        cog_task_func_t func;
        void* task_arg;
        
        cog_result_t result = cog_task_queue_pop(queue, &func, &task_arg);
        if (result != COG_OK) break;
        
        func(task_arg);
    }
    
    return NULL;
}

COGUTIL_API cog_result_t cog_thread_pool_create(size_t thread_count) {
    if (g_cogutil.thread_pool.threads) return COG_ERROR_STATE;
    
    cog_result_t result = cog_task_queue_create(&g_cogutil.thread_pool.queue, 1024);
    if (result != COG_OK) return result;
    
    g_cogutil.thread_pool.threads = COG_CALLOC(thread_count, sizeof(pthread_t));
    if (!g_cogutil.thread_pool.threads) {
        cog_task_queue_destroy(g_cogutil.thread_pool.queue);
        return COG_ERROR_MEMORY;
    }
    
    g_cogutil.thread_pool.thread_count = thread_count;
    g_cogutil.thread_pool.running = true;
    
    for (size_t i = 0; i < thread_count; i++) {
        pthread_create(&g_cogutil.thread_pool.threads[i], NULL, 
            thread_pool_worker, g_cogutil.thread_pool.queue);
    }
    
    COG_LOG_INFO("Thread pool created with %zu threads", thread_count);
    return COG_OK;
}

COGUTIL_API void cog_thread_pool_destroy(void) {
    if (!g_cogutil.thread_pool.threads) return;
    
    cog_task_queue_close(g_cogutil.thread_pool.queue);
    
    for (size_t i = 0; i < g_cogutil.thread_pool.thread_count; i++) {
        pthread_join(g_cogutil.thread_pool.threads[i], NULL);
    }
    
    cog_task_queue_destroy(g_cogutil.thread_pool.queue);
    COG_FREE(g_cogutil.thread_pool.threads);
    
    g_cogutil.thread_pool.threads = NULL;
    g_cogutil.thread_pool.queue = NULL;
    g_cogutil.thread_pool.thread_count = 0;
    g_cogutil.thread_pool.running = false;
    
    COG_LOG_INFO("Thread pool destroyed");
}

COGUTIL_API cog_result_t cog_thread_pool_submit(cog_task_func_t func, void* arg) {
    if (!g_cogutil.thread_pool.queue) return COG_ERROR_STATE;
    return cog_task_queue_push(g_cogutil.thread_pool.queue, func, arg);
}

/*===========================================================================
 * UUID Implementation
 *===========================================================================*/

COGUTIL_API void cog_uuid_generate(cog_uuid_t* uuid) {
    if (!uuid) return;
    
    /* Generate random UUID (version 4) */
    for (int i = 0; i < 16; i++) {
        uuid->bytes[i] = (uint8_t)(rand() & 0xFF);
    }
    
    /* Set version (4) and variant (RFC 4122) */
    uuid->bytes[6] = (uuid->bytes[6] & 0x0F) | 0x40;
    uuid->bytes[8] = (uuid->bytes[8] & 0x3F) | 0x80;
}

COGUTIL_API void cog_uuid_to_string(const cog_uuid_t* uuid, char* str) {
    if (!uuid || !str) return;
    
    snprintf(str, 37, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        uuid->bytes[0], uuid->bytes[1], uuid->bytes[2], uuid->bytes[3],
        uuid->bytes[4], uuid->bytes[5], uuid->bytes[6], uuid->bytes[7],
        uuid->bytes[8], uuid->bytes[9], uuid->bytes[10], uuid->bytes[11],
        uuid->bytes[12], uuid->bytes[13], uuid->bytes[14], uuid->bytes[15]);
}

COGUTIL_API bool cog_uuid_from_string(cog_uuid_t* uuid, const char* str) {
    if (!uuid || !str) return false;
    
    unsigned int bytes[16];
    int n = sscanf(str, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        &bytes[0], &bytes[1], &bytes[2], &bytes[3],
        &bytes[4], &bytes[5], &bytes[6], &bytes[7],
        &bytes[8], &bytes[9], &bytes[10], &bytes[11],
        &bytes[12], &bytes[13], &bytes[14], &bytes[15]);
    
    if (n != 16) return false;
    
    for (int i = 0; i < 16; i++) {
        uuid->bytes[i] = (uint8_t)bytes[i];
    }
    
    return true;
}

COGUTIL_API bool cog_uuid_equals(const cog_uuid_t* a, const cog_uuid_t* b) {
    if (!a || !b) return false;
    return memcmp(a->bytes, b->bytes, 16) == 0;
}

/*===========================================================================
 * Time Utilities
 *===========================================================================*/

COGUTIL_API uint64_t cog_time_now_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

COGUTIL_API uint64_t cog_time_now_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

COGUTIL_API void cog_time_sleep_ms(uint32_t ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

/*===========================================================================
 * Initialization
 *===========================================================================*/

COGUTIL_API cog_result_t cog_init(void) {
    pthread_mutex_lock(&g_cogutil.init_lock);
    
    if (g_cogutil.initialized) {
        pthread_mutex_unlock(&g_cogutil.init_lock);
        return COG_OK;
    }
    
    /* Initialize mutexes */
    pthread_mutex_init(&g_cogutil.log_lock, NULL);
    pthread_mutex_init(&g_cogutil.mem_lock, NULL);
    pthread_rwlock_init(&g_cogutil.config_lock, NULL);
    
    /* Seed random number generator */
    srand((unsigned int)time(NULL));
    
    g_cogutil.initialized = true;
    
    pthread_mutex_unlock(&g_cogutil.init_lock);
    
    COG_LOG_INFO("CogUtil initialized");
    return COG_OK;
}

COGUTIL_API void cog_shutdown(void) {
    pthread_mutex_lock(&g_cogutil.init_lock);
    
    if (!g_cogutil.initialized) {
        pthread_mutex_unlock(&g_cogutil.init_lock);
        return;
    }
    
    /* Destroy thread pool */
    cog_thread_pool_destroy();
    
    /* Destroy global config */
    if (g_cogutil.config) {
        cog_config_destroy(g_cogutil.config);
        g_cogutil.config = NULL;
    }
    
    /* Log memory stats */
    size_t allocated, count, peak;
    cog_mem_stats(&allocated, &count, &peak);
    if (allocated > 0 || count > 0) {
        COG_LOG_WARN("Memory leak detected: %zu bytes in %zu allocations", allocated, count);
    }
    COG_LOG_INFO("Peak memory usage: %zu bytes", peak);
    
    /* Destroy mutexes */
    pthread_mutex_destroy(&g_cogutil.log_lock);
    pthread_mutex_destroy(&g_cogutil.mem_lock);
    pthread_rwlock_destroy(&g_cogutil.config_lock);
    
    g_cogutil.initialized = false;
    
    pthread_mutex_unlock(&g_cogutil.init_lock);
}

COGUTIL_API const char* cog_version(void) {
    return COGUTIL_VERSION;
}

COGUTIL_API const char* cog_result_string(cog_result_t result) {
    switch (result) {
        case COG_OK: return "Success";
        case COG_ERROR_INVALID_PARAM: return "Invalid parameter";
        case COG_ERROR_MEMORY: return "Memory allocation failed";
        case COG_ERROR_IO: return "I/O error";
        case COG_ERROR_NOT_FOUND: return "Not found";
        case COG_ERROR_EXISTS: return "Already exists";
        case COG_ERROR_TIMEOUT: return "Timeout";
        case COG_ERROR_STATE: return "Invalid state";
        case COG_ERROR_NETWORK: return "Network error";
        case COG_ERROR_AUTH: return "Authentication error";
        case COG_ERROR_PARSE: return "Parse error";
        case COG_ERROR_CONFIG: return "Configuration error";
        case COG_ERROR_INIT: return "Initialization error";
        case COG_ERROR_EMPTY: return "Empty";
        default: return "Unknown error";
    }
}
