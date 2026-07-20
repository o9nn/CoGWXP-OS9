/**
 * @file hashmap.c
 * @brief String-keyed hashmap implementation.
 */

#include "cogutil.h"

#include <stdlib.h>
#include <string.h>

typedef struct cog_hashmap_entry {
    char* key;
    void* value;
    struct cog_hashmap_entry* next;
} cog_hashmap_entry_t;

struct cog_hashmap {
    cog_hashmap_entry_t** buckets;
    size_t bucket_count;
    size_t size;
};

static const size_t INITIAL_BUCKETS = 64;

static cog_hashmap_entry_t* find_entry(cog_hashmap_t map,
                                       const char* key,
                                       size_t* bucket_out,
                                       cog_hashmap_entry_t*** link_out) {
    if (!map || !key || map->bucket_count == 0) return NULL;

    size_t bucket = cog_hash_string(key) % map->bucket_count;
    cog_hashmap_entry_t** link = &map->buckets[bucket];
    while (*link) {
        if (strcmp((*link)->key, key) == 0) {
            if (bucket_out) *bucket_out = bucket;
            if (link_out) *link_out = link;
            return *link;
        }
        link = &(*link)->next;
    }

    if (bucket_out) *bucket_out = bucket;
    if (link_out) *link_out = link;
    return NULL;
}

static bool hashmap_resize(cog_hashmap_t map, size_t new_bucket_count) {
    cog_hashmap_entry_t** new_buckets =
        (cog_hashmap_entry_t**)calloc(new_bucket_count, sizeof(cog_hashmap_entry_t*));
    if (!new_buckets) return false;

    for (size_t i = 0; i < map->bucket_count; i++) {
        cog_hashmap_entry_t* entry = map->buckets[i];
        while (entry) {
            cog_hashmap_entry_t* next = entry->next;
            size_t bucket = cog_hash_string(entry->key) % new_bucket_count;
            entry->next = new_buckets[bucket];
            new_buckets[bucket] = entry;
            entry = next;
        }
    }

    free(map->buckets);
    map->buckets = new_buckets;
    map->bucket_count = new_bucket_count;
    return true;
}

COGUTIL_API cog_hashmap_t cog_hashmap_create(void) {
    cog_hashmap_t map = (cog_hashmap_t)calloc(1, sizeof(struct cog_hashmap));
    if (!map) return NULL;

    map->buckets =
        (cog_hashmap_entry_t**)calloc(INITIAL_BUCKETS, sizeof(cog_hashmap_entry_t*));
    if (!map->buckets) {
        free(map);
        return NULL;
    }

    map->bucket_count = INITIAL_BUCKETS;
    return map;
}

COGUTIL_API void cog_hashmap_destroy(cog_hashmap_t map) {
    if (!map) return;

    for (size_t i = 0; i < map->bucket_count; i++) {
        cog_hashmap_entry_t* entry = map->buckets[i];
        while (entry) {
            cog_hashmap_entry_t* next = entry->next;
            free(entry->key);
            free(entry);
            entry = next;
        }
    }

    free(map->buckets);
    free(map);
}

COGUTIL_API size_t cog_hashmap_size(cog_hashmap_t map) {
    return map ? map->size : 0;
}

COGUTIL_API void* cog_hashmap_get(cog_hashmap_t map, const char* key) {
    cog_hashmap_entry_t* entry = find_entry(map, key, NULL, NULL);
    return entry ? entry->value : NULL;
}

COGUTIL_API void cog_hashmap_set(cog_hashmap_t map, const char* key, void* value) {
    if (!map || !key) return;

    cog_hashmap_entry_t* entry = find_entry(map, key, NULL, NULL);
    if (entry) {
        entry->value = value;
        return;
    }

    if ((map->size + 1) * 4 > map->bucket_count * 3) {
        (void)hashmap_resize(map, map->bucket_count * 2);
    }

    size_t bucket = cog_hash_string(key) % map->bucket_count;
    entry = (cog_hashmap_entry_t*)calloc(1, sizeof(cog_hashmap_entry_t));
    if (!entry) return;

    entry->key = cog_strdup(key);
    if (!entry->key) {
        free(entry);
        return;
    }

    entry->value = value;
    entry->next = map->buckets[bucket];
    map->buckets[bucket] = entry;
    map->size++;
}

COGUTIL_API bool cog_hashmap_remove(cog_hashmap_t map, const char* key) {
    cog_hashmap_entry_t** link = NULL;
    cog_hashmap_entry_t* entry = find_entry(map, key, NULL, &link);
    if (!entry || !link) return false;

    *link = entry->next;
    free(entry->key);
    free(entry);
    map->size--;
    return true;
}

COGUTIL_API bool cog_hashmap_contains(cog_hashmap_t map, const char* key) {
    return find_entry(map, key, NULL, NULL) != NULL;
}
