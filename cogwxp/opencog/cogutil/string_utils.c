/**
 * @file string_utils.c
 * @brief String utility helpers.
 */

#include "cogutil.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

COGUTIL_API char* cog_string_trim(char* str) {
    if (!str) return NULL;

    char* start = str;
    while (*start && isspace((unsigned char)*start)) start++;

    char* end = start + strlen(start);
    while (end > start && isspace((unsigned char)*(end - 1))) end--;
    *end = '\0';

    if (start != str) memmove(str, start, (size_t)(end - start) + 1);
    return str;
}

COGUTIL_API char** cog_string_split(const char* str, char delimiter, size_t* count) {
    if (count) *count = 0;
    if (!str) return NULL;

    if (*str == '\0') {
        return NULL;
    }

    size_t parts_count = 1;
    for (const char* p = str; *p; p++) {
        if (*p == delimiter) parts_count++;
    }

    char** parts = (char**)calloc(parts_count, sizeof(char*));
    if (!parts) return NULL;

    const char* start = str;
    for (size_t i = 0; i < parts_count; i++) {
        const char* end = strchr(start, delimiter);
        size_t len = end ? (size_t)(end - start) : strlen(start);

        parts[i] = (char*)malloc(len + 1);
        if (!parts[i]) {
            cog_string_split_free(parts, i);
            return NULL;
        }
        memcpy(parts[i], start, len);
        parts[i][len] = '\0';

        start = end ? end + 1 : start + len;
    }

    if (count) *count = parts_count;
    return parts;
}

COGUTIL_API void cog_string_split_free(char** parts, size_t count) {
    if (!parts) return;

    for (size_t i = 0; i < count; i++) {
        free(parts[i]);
    }
    free(parts);
}

COGUTIL_API char* cog_string_join(const char** parts, size_t count, const char* delimiter) {
    if (count == 0) return cog_strdup("");
    if (!parts) return NULL;

    size_t delimiter_len = delimiter ? strlen(delimiter) : 0;
    size_t total_len = 1;

    for (size_t i = 0; i < count; i++) {
        if (!parts[i]) return NULL;
        total_len += strlen(parts[i]);
        if (i + 1 < count) total_len += delimiter_len;
    }

    char* joined = (char*)malloc(total_len);
    if (!joined) return NULL;

    char* out = joined;
    for (size_t i = 0; i < count; i++) {
        size_t len = strlen(parts[i]);
        memcpy(out, parts[i], len);
        out += len;
        if (i + 1 < count && delimiter_len > 0) {
            memcpy(out, delimiter, delimiter_len);
            out += delimiter_len;
        }
    }
    *out = '\0';
    return joined;
}

COGUTIL_API bool cog_string_starts_with(const char* str, const char* prefix) {
    if (!str || !prefix) return false;

    size_t prefix_len = strlen(prefix);
    return strncmp(str, prefix, prefix_len) == 0;
}

COGUTIL_API bool cog_string_ends_with(const char* str, const char* suffix) {
    if (!str || !suffix) return false;

    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);
    if (suffix_len > str_len) return false;

    return strcmp(str + str_len - suffix_len, suffix) == 0;
}
