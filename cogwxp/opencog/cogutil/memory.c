/**
 * @file memory.c
 * @brief Public memory helper API for CogUtil.
 */

#include "cogutil.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

typedef struct cog_mem_record {
    void* ptr;
    size_t size;
    struct cog_mem_record* next;
} cog_mem_record_t;

static pthread_mutex_t g_mem_lock = PTHREAD_MUTEX_INITIALIZER;
static cog_mem_record_t* g_mem_records = NULL;
static cog_allocator_t g_allocator = {0};
static cog_mem_stats_t g_mem_stats = {0};

static void* default_alloc(size_t size) {
    return malloc(size);
}

static void* default_realloc(void* ptr, size_t size) {
    return realloc(ptr, size);
}

static void default_free(void* ptr) {
    free(ptr);
}

static void* allocator_alloc(size_t size) {
    return g_allocator.alloc ? g_allocator.alloc(size, g_allocator.user_data)
                             : default_alloc(size);
}

static void* allocator_realloc(void* ptr, size_t size) {
    return g_allocator.realloc ? g_allocator.realloc(ptr, size, g_allocator.user_data)
                               : default_realloc(ptr, size);
}

static void allocator_free(void* ptr) {
    if (g_allocator.free) {
        g_allocator.free(ptr, g_allocator.user_data);
    } else {
        default_free(ptr);
    }
}

static cog_mem_record_t* find_record(void* ptr, cog_mem_record_t*** link_out) {
    cog_mem_record_t** link = &g_mem_records;
    while (*link) {
        if ((*link)->ptr == ptr) {
            if (link_out) *link_out = link;
            return *link;
        }
        link = &(*link)->next;
    }
    if (link_out) *link_out = NULL;
    return NULL;
}

static bool add_record(void* ptr, size_t size) {
    cog_mem_record_t* record = (cog_mem_record_t*)malloc(sizeof(cog_mem_record_t));
    if (!record) return false;

    record->ptr = ptr;
    record->size = size;

    pthread_mutex_lock(&g_mem_lock);
    record->next = g_mem_records;
    g_mem_records = record;
    g_mem_stats.total_allocated += size;
    g_mem_stats.current_usage += size;
    if (g_mem_stats.current_usage > g_mem_stats.peak_usage) {
        g_mem_stats.peak_usage = g_mem_stats.current_usage;
    }
    g_mem_stats.allocation_count++;
    pthread_mutex_unlock(&g_mem_lock);

    return true;
}

COGUTIL_API void cog_mem_init(cog_allocator_t* allocator) {
    pthread_mutex_lock(&g_mem_lock);
    g_allocator = allocator ? *allocator : (cog_allocator_t){0};
    pthread_mutex_unlock(&g_mem_lock);
}

COGUTIL_API void cog_mem_shutdown(void) {
    pthread_mutex_lock(&g_mem_lock);
    g_allocator = (cog_allocator_t){0};
    pthread_mutex_unlock(&g_mem_lock);
}

COGUTIL_API void* cog_alloc(size_t size) {
    if (size == 0) size = 1;

    void* ptr = allocator_alloc(size);
    if (!ptr) return NULL;

    if (!add_record(ptr, size)) {
        allocator_free(ptr);
        return NULL;
    }
    return ptr;
}

COGUTIL_API void* cog_calloc(size_t count, size_t size) {
    if (count != 0 && size > SIZE_MAX / count) return NULL;
    size_t total = count * size;
    if (total == 0) total = 1;

    void* ptr = cog_alloc(total);
    if (ptr) memset(ptr, 0, total);
    return ptr;
}

COGUTIL_API void* cog_realloc(void* ptr, size_t size) {
    if (!ptr) return cog_alloc(size);
    if (size == 0) {
        cog_free(ptr);
        return NULL;
    }

    pthread_mutex_lock(&g_mem_lock);
    cog_mem_record_t* record = find_record(ptr, NULL);
    if (!record) {
        pthread_mutex_unlock(&g_mem_lock);
        return allocator_realloc(ptr, size);
    }

    size_t old_size = record->size;
    void* new_ptr = allocator_realloc(ptr, size);
    if (!new_ptr) {
        pthread_mutex_unlock(&g_mem_lock);
        return NULL;
    }

    record->ptr = new_ptr;
    record->size = size;
    g_mem_stats.total_allocated += size > old_size ? size - old_size : 0;
    g_mem_stats.total_freed += old_size > size ? old_size - size : 0;
    g_mem_stats.current_usage = g_mem_stats.current_usage - old_size + size;
    if (g_mem_stats.current_usage > g_mem_stats.peak_usage) {
        g_mem_stats.peak_usage = g_mem_stats.current_usage;
    }
    pthread_mutex_unlock(&g_mem_lock);

    return new_ptr;
}

COGUTIL_API void cog_free(void* ptr) {
    if (!ptr) return;

    pthread_mutex_lock(&g_mem_lock);
    cog_mem_record_t** link = NULL;
    cog_mem_record_t* record = find_record(ptr, &link);
    if (record) {
        *link = record->next;
        g_mem_stats.total_freed += record->size;
        g_mem_stats.current_usage -= record->size;
        g_mem_stats.free_count++;
    }
    pthread_mutex_unlock(&g_mem_lock);

    if (record) free(record);
    allocator_free(ptr);
}

COGUTIL_API char* cog_strdup(const char* str) {
    if (!str) return NULL;

    size_t len = strlen(str) + 1;
    char* copy = (char*)malloc(len);
    if (!copy) return NULL;
    memcpy(copy, str, len);
    return copy;
}

COGUTIL_API void cog_mem_get_stats(cog_mem_stats_t* stats) {
    if (!stats) return;

    pthread_mutex_lock(&g_mem_lock);
    *stats = g_mem_stats;
    pthread_mutex_unlock(&g_mem_lock);
}
