/**
 * @file array.c
 * @brief Simple dynamic array implementation.
 */

#include "cogutil.h"

#include <stdlib.h>
#include <string.h>

struct cog_array {
    unsigned char* data;
    size_t element_size;
    size_t size;
    size_t capacity;
};

COGUTIL_API cog_array_t cog_array_create(size_t element_size) {
    if (element_size == 0) return NULL;

    cog_array_t array = (cog_array_t)calloc(1, sizeof(struct cog_array));
    if (!array) return NULL;

    array->element_size = element_size;
    return array;
}

COGUTIL_API void cog_array_destroy(cog_array_t array) {
    if (!array) return;
    free(array->data);
    free(array);
}

COGUTIL_API size_t cog_array_size(cog_array_t array) {
    return array ? array->size : 0;
}

COGUTIL_API void* cog_array_get(cog_array_t array, size_t index) {
    if (!array || index >= array->size) return NULL;
    return array->data + index * array->element_size;
}

COGUTIL_API void cog_array_push(cog_array_t array, const void* element) {
    if (!array || !element) return;

    if (array->size == array->capacity) {
        size_t new_capacity = array->capacity ? array->capacity * 2 : 8;
        if (new_capacity < array->capacity ||
            new_capacity > SIZE_MAX / array->element_size) {
            return;
        }

        unsigned char* data = (unsigned char*)realloc(
            array->data, new_capacity * array->element_size);
        if (!data) return;

        array->data = data;
        array->capacity = new_capacity;
    }

    memcpy(array->data + array->size * array->element_size,
           element,
           array->element_size);
    array->size++;
}

COGUTIL_API void cog_array_pop(cog_array_t array) {
    if (!array || array->size == 0) return;
    array->size--;
}

COGUTIL_API void cog_array_clear(cog_array_t array) {
    if (!array) return;
    array->size = 0;
}
