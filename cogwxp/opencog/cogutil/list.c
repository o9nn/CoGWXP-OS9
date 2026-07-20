/**
 * @file list.c
 * @brief Intrusive-free doubly-linked list for void pointers.
 */

#include "cogutil.h"

#include <stdlib.h>

typedef struct cog_list_node {
    void* data;
    struct cog_list_node* prev;
    struct cog_list_node* next;
} cog_list_node_t;

struct cog_list {
    cog_list_node_t* head;
    cog_list_node_t* tail;
    size_t size;
};

COGUTIL_API cog_list_t cog_list_create(void) {
    return (cog_list_t)calloc(1, sizeof(struct cog_list));
}

COGUTIL_API void cog_list_destroy(cog_list_t list) {
    if (!list) return;

    cog_list_node_t* node = list->head;
    while (node) {
        cog_list_node_t* next = node->next;
        free(node);
        node = next;
    }
    free(list);
}

COGUTIL_API size_t cog_list_size(cog_list_t list) {
    return list ? list->size : 0;
}

COGUTIL_API void cog_list_push_front(cog_list_t list, void* data) {
    if (!list) return;

    cog_list_node_t* node = (cog_list_node_t*)calloc(1, sizeof(cog_list_node_t));
    if (!node) return;

    node->data = data;
    node->next = list->head;
    if (list->head) list->head->prev = node;
    list->head = node;
    if (!list->tail) list->tail = node;
    list->size++;
}

COGUTIL_API void cog_list_push_back(cog_list_t list, void* data) {
    if (!list) return;

    cog_list_node_t* node = (cog_list_node_t*)calloc(1, sizeof(cog_list_node_t));
    if (!node) return;

    node->data = data;
    node->prev = list->tail;
    if (list->tail) list->tail->next = node;
    list->tail = node;
    if (!list->head) list->head = node;
    list->size++;
}

COGUTIL_API void* cog_list_pop_front(cog_list_t list) {
    if (!list || !list->head) return NULL;

    cog_list_node_t* node = list->head;
    void* data = node->data;
    list->head = node->next;
    if (list->head) list->head->prev = NULL;
    else list->tail = NULL;
    list->size--;
    free(node);
    return data;
}

COGUTIL_API void* cog_list_pop_back(cog_list_t list) {
    if (!list || !list->tail) return NULL;

    cog_list_node_t* node = list->tail;
    void* data = node->data;
    list->tail = node->prev;
    if (list->tail) list->tail->next = NULL;
    else list->head = NULL;
    list->size--;
    free(node);
    return data;
}
