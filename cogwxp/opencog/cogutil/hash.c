/**
 * @file hash.c
 * @brief Stable non-cryptographic hash helpers.
 */

#include "cogutil.h"

#include <stdint.h>

COGUTIL_API uint32_t cog_hash_string(const char* str) {
    const unsigned char* p = (const unsigned char*)str;
    uint32_t hash = 2166136261u;

    if (!p) return 0;
    while (*p) {
        hash ^= (uint32_t)*p++;
        hash *= 16777619u;
    }
    return hash;
}

COGUTIL_API uint64_t cog_hash_data(const void* data, size_t size) {
    const unsigned char* p = (const unsigned char*)data;
    uint64_t hash = 14695981039346656037ull;

    if (!p && size > 0) return 0;
    for (size_t i = 0; i < size; i++) {
        hash ^= (uint64_t)p[i];
        hash *= 1099511628211ull;
    }
    return hash;
}

COGUTIL_API uint64_t cog_hash_combine(uint64_t h1, uint64_t h2) {
    return h1 ^ (h2 + 0x9e3779b97f4a7c15ull + (h1 << 6) + (h1 >> 2));
}
