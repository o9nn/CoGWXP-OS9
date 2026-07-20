/**
 * @file atomspace.c
 * @brief AtomSpace Hypergraph Knowledge Representation Implementation
 *
 * Implementation of the public API declared in atomspace.h (the header is
 * the canonical interface contract consumed by the integration and
 * orchestration layers).
 *
 * @copyright CoGWXP-OS9 Project
 */

#include "atomspace.h"
#include "../cogutil/cogutil.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <math.h>

/*===========================================================================
 * Constants
 *===========================================================================*/

#define ATOM_BUCKET_COUNT 1024
#define DEFAULT_AFB_THRESHOLD 100

/*===========================================================================
 * Internal Structures
 *===========================================================================*/

typedef struct atom_entry {
    atom_t* atom;
    struct atom_entry* next;
} atom_entry_t;

struct attention_bank {
    struct atomspace* as;
    int16_t afb_threshold;
};

struct atomspace {
    atom_entry_t* buckets[ATOM_BUCKET_COUNT];   /* keyed by handle   */
    size_t atom_count;

    atom_handle_t next_handle;

    struct atomspace* parent;

    struct attention_bank bank;

    pthread_rwlock_t lock;
};

struct atom_query {
    struct atomspace* as;

    bool filter_type;
    atom_type_t type;

    char* name_pattern;            /* prefix match */

    bool filter_tv;
    double min_strength;
    double min_confidence;

    bool filter_av;
    int16_t min_sti;
};

/*===========================================================================
 * Internal Helpers
 *===========================================================================*/

static size_t bucket_of(atom_handle_t handle) {
    /* FNV-1a hash of the handle */
    uint64_t hash = 14695981039346656037ULL;
    const uint8_t* bytes = (const uint8_t*)&handle;
    for (size_t i = 0; i < sizeof(handle); i++) {
        hash ^= bytes[i];
        hash *= 1099511628211ULL;
    }
    return (size_t)(hash % ATOM_BUCKET_COUNT);
}

static atom_t* find_atom(atomspace_t as, atom_handle_t handle) {
    if (!as || handle == ATOM_HANDLE_INVALID) return NULL;
    for (atom_entry_t* e = as->buckets[bucket_of(handle)]; e; e = e->next) {
        if (e->atom->handle == handle) return e->atom;
    }
    return NULL;
}

static void atom_free(atom_t* atom) {
    if (!atom) return;
    free(atom->name);
    free(atom->outgoing);
    free(atom->incoming);
    free(atom);
}

static void incoming_add(atom_t* target, atom_handle_t link) {
    if (target->incoming_count >= target->incoming_capacity) {
        size_t cap = target->incoming_capacity ? target->incoming_capacity * 2 : 4;
        atom_handle_t* grown =
            realloc(target->incoming, cap * sizeof(atom_handle_t));
        if (!grown) return;
        target->incoming = grown;
        target->incoming_capacity = cap;
    }
    target->incoming[target->incoming_count++] = link;
}

static void incoming_remove(atom_t* target, atom_handle_t link) {
    for (size_t i = 0; i < target->incoming_count; i++) {
        if (target->incoming[i] == link) {
            target->incoming[i] = target->incoming[target->incoming_count - 1];
            target->incoming_count--;
            return;
        }
    }
}

static atom_t* atom_new(atomspace_t as, atom_type_t type) {
    atom_t* atom = calloc(1, sizeof(atom_t));
    if (!atom) return NULL;
    atom->handle = as->next_handle++;
    atom->type = type;
    truth_value_t default_tv = TRUTH_VALUE_DEFAULT;
    attention_value_t default_av = ATTENTION_VALUE_DEFAULT;
    atom->truth_value = default_tv;
    atom->attention_value = default_av;
    return atom;
}

static void table_insert(atomspace_t as, atom_t* atom) {
    atom_entry_t* entry = calloc(1, sizeof(atom_entry_t));
    if (!entry) {
        atom_free(atom);
        return;
    }
    size_t b = bucket_of(atom->handle);
    entry->atom = atom;
    entry->next = as->buckets[b];
    as->buckets[b] = entry;
    as->atom_count++;
}

/* Iterate over every atom; callback returns false to stop iteration. */
typedef bool (*atom_visit_fn)(atom_t* atom, void* user);

static void for_each_atom(atomspace_t as, atom_visit_fn fn, void* user) {
    for (size_t b = 0; b < ATOM_BUCKET_COUNT; b++) {
        for (atom_entry_t* e = as->buckets[b]; e; e = e->next) {
            if (!fn(e->atom, user)) return;
        }
    }
}

static bool is_node_type(atom_type_t type) {
    if (type >= ATOM_TYPE_NODE && type < ATOM_TYPE_LINK) return true;
    if (type >= ATOM_TYPE_9P_RESOURCE_NODE && type <= ATOM_TYPE_LIMBO_PROC_NODE) return true;
    if (type >= ATOM_TYPE_KERNEL_PROCESS_NODE && type <= ATOM_TYPE_KERNEL_MEMORY_NODE) return true;
    return false;
}

static bool is_link_type(atom_type_t type) {
    if (type >= ATOM_TYPE_LINK && type < ATOM_TYPE_9P_RESOURCE_NODE) return true;
    if (type == ATOM_TYPE_STYX_CHANNEL_LINK) return true;
    if (type == ATOM_TYPE_KERNEL_IPC_LINK) return true;
    return false;
}

static void unlink_and_free(atomspace_t as, atom_t* atom) {
    /* Remove back-references from outgoing targets */
    for (size_t i = 0; i < atom->outgoing_count; i++) {
        atom_t* target = find_atom(as, atom->outgoing[i]);
        if (target) incoming_remove(target, atom->handle);
    }

    size_t b = bucket_of(atom->handle);
    atom_entry_t** link = &as->buckets[b];
    while (*link) {
        if ((*link)->atom == atom) {
            atom_entry_t* dead = *link;
            *link = dead->next;
            free(dead);
            as->atom_count--;
            atom_free(atom);
            return;
        }
        link = &(*link)->next;
    }
}

/*===========================================================================
 * Lifecycle
 *===========================================================================*/

ATOMSPACE_API atomspace_t atomspace_create(void) {
    struct atomspace* as = calloc(1, sizeof(struct atomspace));
    if (!as) return NULL;
    as->next_handle = 1;
    as->bank.as = as;
    as->bank.afb_threshold = DEFAULT_AFB_THRESHOLD;
    pthread_rwlock_init(&as->lock, NULL);
    return as;
}

ATOMSPACE_API void atomspace_destroy(atomspace_t as) {
    if (!as) return;
    atomspace_clear(as);
    pthread_rwlock_destroy(&as->lock);
    free(as);
}

ATOMSPACE_API atomspace_t atomspace_create_child(atomspace_t parent) {
    atomspace_t child = atomspace_create();
    if (child) child->parent = parent;
    return child;
}

ATOMSPACE_API atomspace_t atomspace_get_parent(atomspace_t as) {
    return as ? as->parent : NULL;
}

ATOMSPACE_API void atomspace_clear(atomspace_t as) {
    if (!as) return;
    pthread_rwlock_wrlock(&as->lock);
    for (size_t b = 0; b < ATOM_BUCKET_COUNT; b++) {
        atom_entry_t* e = as->buckets[b];
        while (e) {
            atom_entry_t* next = e->next;
            atom_free(e->atom);
            free(e);
            e = next;
        }
        as->buckets[b] = NULL;
    }
    as->atom_count = 0;
    pthread_rwlock_unlock(&as->lock);
}

/*===========================================================================
 * Atom Creation and Retrieval
 *===========================================================================*/

ATOMSPACE_API atom_handle_t atomspace_add_node(
    atomspace_t as,
    atom_type_t type,
    const char* name
) {
    truth_value_t tv = TRUTH_VALUE_DEFAULT;
    return atomspace_add_node_tv(as, type, name, tv);
}

ATOMSPACE_API atom_handle_t atomspace_add_node_tv(
    atomspace_t as,
    atom_type_t type,
    const char* name,
    truth_value_t tv
) {
    if (!as || !name) return ATOM_HANDLE_INVALID;

    /* Idempotency: adding the same node twice returns the same handle */
    atom_handle_t existing = atomspace_get_node(as, type, name);
    if (existing != ATOM_HANDLE_INVALID) {
        atomspace_merge_tv(as, existing, tv);
        return existing;
    }

    pthread_rwlock_wrlock(&as->lock);
    atom_t* atom = atom_new(as, type);
    if (!atom) {
        pthread_rwlock_unlock(&as->lock);
        return ATOM_HANDLE_INVALID;
    }
    atom->name = strdup(name);
    if (!atom->name) {
        atom_free(atom);
        pthread_rwlock_unlock(&as->lock);
        return ATOM_HANDLE_INVALID;
    }
    atom->truth_value = tv;
    atom_handle_t handle = atom->handle;
    table_insert(as, atom);
    pthread_rwlock_unlock(&as->lock);
    return handle;
}

ATOMSPACE_API atom_handle_t atomspace_add_link(
    atomspace_t as,
    atom_type_t type,
    const atom_handle_t* outgoing,
    size_t outgoing_count
) {
    truth_value_t tv = TRUTH_VALUE_DEFAULT;
    return atomspace_add_link_tv(as, type, outgoing, outgoing_count, tv);
}

ATOMSPACE_API atom_handle_t atomspace_add_link_tv(
    atomspace_t as,
    atom_type_t type,
    const atom_handle_t* outgoing,
    size_t outgoing_count,
    truth_value_t tv
) {
    if (!as) return ATOM_HANDLE_INVALID;
    if (outgoing_count > 0 && !outgoing) return ATOM_HANDLE_INVALID;

    atom_handle_t existing = atomspace_get_link(as, type, outgoing, outgoing_count);
    if (existing != ATOM_HANDLE_INVALID) {
        atomspace_merge_tv(as, existing, tv);
        return existing;
    }

    pthread_rwlock_wrlock(&as->lock);
    atom_t* atom = atom_new(as, type);
    if (!atom) {
        pthread_rwlock_unlock(&as->lock);
        return ATOM_HANDLE_INVALID;
    }
    if (outgoing_count > 0) {
        atom->outgoing = malloc(outgoing_count * sizeof(atom_handle_t));
        if (!atom->outgoing) {
            atom_free(atom);
            pthread_rwlock_unlock(&as->lock);
            return ATOM_HANDLE_INVALID;
        }
        memcpy(atom->outgoing, outgoing, outgoing_count * sizeof(atom_handle_t));
        atom->outgoing_count = outgoing_count;
    }
    atom->truth_value = tv;
    atom_handle_t handle = atom->handle;
    table_insert(as, atom);

    /* Maintain incoming sets of targets */
    for (size_t i = 0; i < outgoing_count; i++) {
        atom_t* target = find_atom(as, outgoing[i]);
        if (target) incoming_add(target, handle);
    }
    pthread_rwlock_unlock(&as->lock);
    return handle;
}

typedef struct node_lookup {
    atom_type_t type;
    const char* name;
    atom_handle_t result;
} node_lookup_t;

static bool node_lookup_visit(atom_t* atom, void* user) {
    node_lookup_t* q = user;
    if (atom->type == q->type && atom->name && strcmp(atom->name, q->name) == 0) {
        q->result = atom->handle;
        return false;
    }
    return true;
}

ATOMSPACE_API atom_handle_t atomspace_get_node(
    atomspace_t as,
    atom_type_t type,
    const char* name
) {
    if (!as || !name) return ATOM_HANDLE_INVALID;
    node_lookup_t q = { type, name, ATOM_HANDLE_INVALID };
    pthread_rwlock_rdlock(&as->lock);
    for_each_atom(as, node_lookup_visit, &q);
    pthread_rwlock_unlock(&as->lock);
    return q.result;
}

typedef struct link_lookup {
    atom_type_t type;
    const atom_handle_t* outgoing;
    size_t outgoing_count;
    atom_handle_t result;
} link_lookup_t;

static bool link_lookup_visit(atom_t* atom, void* user) {
    link_lookup_t* q = user;
    if (atom->type == q->type &&
        atom->outgoing_count == q->outgoing_count &&
        (q->outgoing_count == 0 ||
         memcmp(atom->outgoing, q->outgoing,
                q->outgoing_count * sizeof(atom_handle_t)) == 0) &&
        atom->name == NULL) {
        q->result = atom->handle;
        return false;
    }
    return true;
}

ATOMSPACE_API atom_handle_t atomspace_get_link(
    atomspace_t as,
    atom_type_t type,
    const atom_handle_t* outgoing,
    size_t outgoing_count
) {
    if (!as) return ATOM_HANDLE_INVALID;
    if (outgoing_count > 0 && !outgoing) return ATOM_HANDLE_INVALID;
    link_lookup_t q = { type, outgoing, outgoing_count, ATOM_HANDLE_INVALID };
    pthread_rwlock_rdlock(&as->lock);
    for_each_atom(as, link_lookup_visit, &q);
    pthread_rwlock_unlock(&as->lock);
    return q.result;
}

ATOMSPACE_API const atom_t* atomspace_get_atom(atomspace_t as, atom_handle_t handle) {
    if (!as) return NULL;
    pthread_rwlock_rdlock(&as->lock);
    const atom_t* atom = find_atom(as, handle);
    pthread_rwlock_unlock(&as->lock);
    return atom;
}

ATOMSPACE_API bool atomspace_remove_atom(atomspace_t as, atom_handle_t handle) {
    if (!as) return false;
    pthread_rwlock_wrlock(&as->lock);
    atom_t* atom = find_atom(as, handle);
    if (!atom || atom->incoming_count > 0) {
        pthread_rwlock_unlock(&as->lock);
        return false;
    }
    unlink_and_free(as, atom);
    pthread_rwlock_unlock(&as->lock);
    return true;
}

static void remove_recursive_locked(atomspace_t as, atom_handle_t handle) {
    atom_t* atom = find_atom(as, handle);
    if (!atom) return;
    /* First remove every link referencing this atom */
    while (atom->incoming_count > 0) {
        atom_handle_t link = atom->incoming[0];
        remove_recursive_locked(as, link);
        /* Re-find in case atom was invalidated (it cannot be: links only
         * reference it, they don't own it), but stay safe: */
        atom = find_atom(as, handle);
        if (!atom) return;
    }
    unlink_and_free(as, atom);
}

ATOMSPACE_API bool atomspace_remove_atom_recursive(atomspace_t as, atom_handle_t handle) {
    if (!as) return false;
    pthread_rwlock_wrlock(&as->lock);
    atom_t* atom = find_atom(as, handle);
    if (!atom) {
        pthread_rwlock_unlock(&as->lock);
        return false;
    }
    remove_recursive_locked(as, handle);
    pthread_rwlock_unlock(&as->lock);
    return true;
}

/*===========================================================================
 * Truth Values
 *===========================================================================*/

ATOMSPACE_API truth_value_t atomspace_get_tv(atomspace_t as, atom_handle_t handle) {
    truth_value_t tv = TRUTH_VALUE_DEFAULT;
    if (!as) return tv;
    pthread_rwlock_rdlock(&as->lock);
    atom_t* atom = find_atom(as, handle);
    if (atom) tv = atom->truth_value;
    pthread_rwlock_unlock(&as->lock);
    return tv;
}

ATOMSPACE_API void atomspace_set_tv(atomspace_t as, atom_handle_t handle, truth_value_t tv) {
    if (!as) return;
    pthread_rwlock_wrlock(&as->lock);
    atom_t* atom = find_atom(as, handle);
    if (atom) atom->truth_value = tv;
    pthread_rwlock_unlock(&as->lock);
}

ATOMSPACE_API truth_value_t tv_simple(double strength, double confidence) {
    truth_value_t tv;
    tv.type = TRUTH_VALUE_SIMPLE;
    tv.strength = strength;
    tv.confidence = confidence;
    tv.count = 1.0;
    return tv;
}

ATOMSPACE_API truth_value_t tv_count(double strength, double confidence, uint64_t count) {
    truth_value_t tv;
    tv.type = TRUTH_VALUE_COUNT;
    tv.strength = strength;
    tv.confidence = confidence;
    tv.count = (double)count;
    return tv;
}

ATOMSPACE_API truth_value_t tv_merge(const truth_value_t* a, const truth_value_t* b) {
    truth_value_t tv = TRUTH_VALUE_DEFAULT;
    if (!a && !b) return tv;
    if (!a) return *b;
    if (!b) return *a;

    /* Confidence-weighted revision */
    double ca = a->confidence;
    double cb = b->confidence;
    double total = ca + cb;
    tv.type = a->type;
    if (total > 0.0) {
        tv.strength = (a->strength * ca + b->strength * cb) / total;
    } else {
        tv.strength = (a->strength + b->strength) / 2.0;
    }
    tv.confidence = ca + cb - ca * cb;   /* evidence accumulation */
    if (tv.confidence > 1.0) tv.confidence = 1.0;
    tv.count = a->count + b->count;
    return tv;
}

ATOMSPACE_API void atomspace_merge_tv(atomspace_t as, atom_handle_t handle, truth_value_t tv) {
    if (!as) return;
    pthread_rwlock_wrlock(&as->lock);
    atom_t* atom = find_atom(as, handle);
    if (atom) atom->truth_value = tv_merge(&atom->truth_value, &tv);
    pthread_rwlock_unlock(&as->lock);
}

/*===========================================================================
 * Attention Values
 *===========================================================================*/

ATOMSPACE_API attention_value_t atomspace_get_av(atomspace_t as, atom_handle_t handle) {
    attention_value_t av = ATTENTION_VALUE_DEFAULT;
    if (!as) return av;
    pthread_rwlock_rdlock(&as->lock);
    atom_t* atom = find_atom(as, handle);
    if (atom) av = atom->attention_value;
    pthread_rwlock_unlock(&as->lock);
    return av;
}

ATOMSPACE_API void atomspace_set_av(atomspace_t as, atom_handle_t handle, attention_value_t av) {
    if (!as) return;
    pthread_rwlock_wrlock(&as->lock);
    atom_t* atom = find_atom(as, handle);
    if (atom) atom->attention_value = av;
    pthread_rwlock_unlock(&as->lock);
}

static int16_t sti_clamp(int32_t value) {
    if (value > INT16_MAX) return INT16_MAX;
    if (value < INT16_MIN) return INT16_MIN;
    return (int16_t)value;
}

ATOMSPACE_API void atomspace_stimulate(atomspace_t as, atom_handle_t handle, int16_t stimulus) {
    if (!as) return;
    pthread_rwlock_wrlock(&as->lock);
    atom_t* atom = find_atom(as, handle);
    if (atom) {
        atom->attention_value.sti =
            sti_clamp((int32_t)atom->attention_value.sti + stimulus);
    }
    pthread_rwlock_unlock(&as->lock);
}

/*===========================================================================
 * Type and Name
 *===========================================================================*/

ATOMSPACE_API atom_type_t atomspace_get_type(atomspace_t as, atom_handle_t handle) {
    atom_type_t type = ATOM_TYPE_MAX;
    if (!as) return type;
    pthread_rwlock_rdlock(&as->lock);
    atom_t* atom = find_atom(as, handle);
    if (atom) type = atom->type;
    pthread_rwlock_unlock(&as->lock);
    return type;
}

ATOMSPACE_API const char* atomspace_get_name(atomspace_t as, atom_handle_t handle) {
    const char* name = NULL;
    if (!as) return NULL;
    pthread_rwlock_rdlock(&as->lock);
    atom_t* atom = find_atom(as, handle);
    if (atom) name = atom->name;
    pthread_rwlock_unlock(&as->lock);
    return name;
}

ATOMSPACE_API bool atomspace_is_node(atomspace_t as, atom_handle_t handle) {
    if (!as) return false;
    pthread_rwlock_rdlock(&as->lock);
    atom_t* atom = find_atom(as, handle);
    bool result = atom && (atom->name != NULL || is_node_type(atom->type));
    pthread_rwlock_unlock(&as->lock);
    return result;
}

ATOMSPACE_API bool atomspace_is_link(atomspace_t as, atom_handle_t handle) {
    if (!as) return false;
    pthread_rwlock_rdlock(&as->lock);
    atom_t* atom = find_atom(as, handle);
    bool result = atom && atom->name == NULL &&
                  (atom->outgoing_count > 0 || is_link_type(atom->type));
    pthread_rwlock_unlock(&as->lock);
    return result;
}

/*===========================================================================
 * Outgoing / Incoming Sets
 *===========================================================================*/

ATOMSPACE_API size_t atomspace_get_arity(atomspace_t as, atom_handle_t handle) {
    size_t arity = 0;
    if (!as) return 0;
    pthread_rwlock_rdlock(&as->lock);
    atom_t* atom = find_atom(as, handle);
    if (atom) arity = atom->outgoing_count;
    pthread_rwlock_unlock(&as->lock);
    return arity;
}

ATOMSPACE_API const atom_handle_t* atomspace_get_outgoing(atomspace_t as, atom_handle_t handle) {
    const atom_handle_t* outgoing = NULL;
    if (!as) return NULL;
    pthread_rwlock_rdlock(&as->lock);
    atom_t* atom = find_atom(as, handle);
    if (atom) outgoing = atom->outgoing;
    pthread_rwlock_unlock(&as->lock);
    return outgoing;
}

ATOMSPACE_API atom_handle_t atomspace_get_outgoing_at(atomspace_t as, atom_handle_t handle, size_t index) {
    atom_handle_t result = ATOM_HANDLE_INVALID;
    if (!as) return result;
    pthread_rwlock_rdlock(&as->lock);
    atom_t* atom = find_atom(as, handle);
    if (atom && index < atom->outgoing_count) result = atom->outgoing[index];
    pthread_rwlock_unlock(&as->lock);
    return result;
}

ATOMSPACE_API size_t atomspace_get_incoming_size(atomspace_t as, atom_handle_t handle) {
    size_t count = 0;
    if (!as) return 0;
    pthread_rwlock_rdlock(&as->lock);
    atom_t* atom = find_atom(as, handle);
    if (atom) count = atom->incoming_count;
    pthread_rwlock_unlock(&as->lock);
    return count;
}

ATOMSPACE_API atom_handle_t* atomspace_get_incoming(atomspace_t as, atom_handle_t handle, size_t* count) {
    if (count) *count = 0;
    if (!as || !count) return NULL;
    pthread_rwlock_rdlock(&as->lock);
    atom_t* atom = find_atom(as, handle);
    atom_handle_t* result = NULL;
    if (atom && atom->incoming_count > 0) {
        result = malloc(atom->incoming_count * sizeof(atom_handle_t));
        if (result) {
            memcpy(result, atom->incoming,
                   atom->incoming_count * sizeof(atom_handle_t));
            *count = atom->incoming_count;
        }
    }
    pthread_rwlock_unlock(&as->lock);
    return result;
}

/*===========================================================================
 * Query
 *===========================================================================*/

ATOMSPACE_API atom_query_t atomspace_query_create(atomspace_t as) {
    if (!as) return NULL;
    struct atom_query* q = calloc(1, sizeof(struct atom_query));
    if (q) q->as = as;
    return q;
}

ATOMSPACE_API void atomspace_query_destroy(atom_query_t query) {
    if (!query) return;
    free(query->name_pattern);
    free(query);
}

ATOMSPACE_API void atomspace_query_type(atom_query_t query, atom_type_t type) {
    if (!query) return;
    query->filter_type = true;
    query->type = type;
}

ATOMSPACE_API void atomspace_query_name(atom_query_t query, const char* pattern) {
    if (!query) return;
    free(query->name_pattern);
    query->name_pattern = pattern ? strdup(pattern) : NULL;
}

ATOMSPACE_API void atomspace_query_tv_min(atom_query_t query, double min_strength, double min_confidence) {
    if (!query) return;
    query->filter_tv = true;
    query->min_strength = min_strength;
    query->min_confidence = min_confidence;
}

ATOMSPACE_API void atomspace_query_av_min(atom_query_t query, int16_t min_sti) {
    if (!query) return;
    query->filter_av = true;
    query->min_sti = min_sti;
}

typedef struct query_exec {
    struct atom_query* q;
    atom_handle_t* results;
    size_t count;
    size_t capacity;
} query_exec_t;

static bool query_matches(const struct atom_query* q, const atom_t* atom) {
    if (q->filter_type && atom->type != q->type) return false;
    if (q->name_pattern) {
        if (!atom->name) return false;
        if (strncmp(atom->name, q->name_pattern, strlen(q->name_pattern)) != 0)
            return false;
    }
    if (q->filter_tv) {
        if (atom->truth_value.strength < q->min_strength) return false;
        if (atom->truth_value.confidence < q->min_confidence) return false;
    }
    if (q->filter_av && atom->attention_value.sti < q->min_sti) return false;
    return true;
}

static bool query_exec_visit(atom_t* atom, void* user) {
    query_exec_t* ctx = user;
    if (!query_matches(ctx->q, atom)) return true;
    if (ctx->count >= ctx->capacity) {
        size_t cap = ctx->capacity ? ctx->capacity * 2 : 16;
        atom_handle_t* grown = realloc(ctx->results, cap * sizeof(atom_handle_t));
        if (!grown) return false;
        ctx->results = grown;
        ctx->capacity = cap;
    }
    ctx->results[ctx->count++] = atom->handle;
    return true;
}

ATOMSPACE_API atom_handle_t* atomspace_query_execute(atom_query_t query, size_t* count) {
    if (count) *count = 0;
    if (!query || !query->as || !count) return NULL;
    query_exec_t ctx = { query, NULL, 0, 0 };
    pthread_rwlock_rdlock(&query->as->lock);
    for_each_atom(query->as, query_exec_visit, &ctx);
    pthread_rwlock_unlock(&query->as->lock);
    *count = ctx.count;
    return ctx.results;
}

ATOMSPACE_API void atomspace_query_results_free(atom_handle_t* results) {
    free(results);
}

ATOMSPACE_API cog_result_t atomspace_get_atoms_by_type(
    atomspace_t as,
    atom_type_t type,
    bool include_subtypes,
    atom_handle_t** atoms,
    size_t* count
) {
    (void)include_subtypes;
    if (!as || !atoms || !count) return COG_ERROR_INVALID_ARG;
    atom_query_t q = atomspace_query_create(as);
    if (!q) return COG_ERROR_MEMORY;
    atomspace_query_type(q, type);
    *atoms = atomspace_query_execute(q, count);
    atomspace_query_destroy(q);
    return COG_SUCCESS;
}

/*===========================================================================
 * Pattern Matching
 *===========================================================================*/

ATOMSPACE_API atom_handle_t* atomspace_pattern_match(
    atomspace_t as,
    atom_handle_t pattern,
    size_t* count
) {
    if (count) *count = 0;
    if (!as || !count) return NULL;

    pthread_rwlock_rdlock(&as->lock);
    atom_t* pat = find_atom(as, pattern);
    if (!pat) {
        pthread_rwlock_unlock(&as->lock);
        return NULL;
    }
    atom_type_t type = pat->type;
    pthread_rwlock_unlock(&as->lock);

    /* Simple structural match: atoms of the same type */
    atom_handle_t* results = NULL;
    atomspace_get_atoms_by_type(as, type, false, &results, count);
    return results;
}

/*===========================================================================
 * Attention Bank
 *===========================================================================*/

ATOMSPACE_API attention_bank_t atomspace_get_attention_bank(atomspace_t as) {
    return as ? &as->bank : NULL;
}

ATOMSPACE_API void attention_bank_update_sti(attention_bank_t bank, atom_handle_t handle, int16_t delta) {
    if (!bank || !bank->as) return;
    atomspace_stimulate(bank->as, handle, delta);
}

ATOMSPACE_API void attention_bank_update_lti(attention_bank_t bank, atom_handle_t handle, int16_t delta) {
    if (!bank || !bank->as) return;
    atomspace_t as = bank->as;
    pthread_rwlock_wrlock(&as->lock);
    atom_t* atom = find_atom(as, handle);
    if (atom) {
        atom->attention_value.lti =
            sti_clamp((int32_t)atom->attention_value.lti + delta);
    }
    pthread_rwlock_unlock(&as->lock);
}

typedef struct top_sti_collect {
    atom_handle_t* handles;
    int16_t* stis;
    size_t count;
    size_t capacity;
} top_sti_collect_t;

static bool top_sti_visit(atom_t* atom, void* user) {
    top_sti_collect_t* ctx = user;
    if (ctx->count >= ctx->capacity) {
        size_t cap = ctx->capacity ? ctx->capacity * 2 : 32;
        atom_handle_t* h = realloc(ctx->handles, cap * sizeof(atom_handle_t));
        if (!h) return false;
        ctx->handles = h;
        int16_t* s = realloc(ctx->stis, cap * sizeof(int16_t));
        if (!s) return false;
        ctx->stis = s;
        ctx->capacity = cap;
    }
    ctx->handles[ctx->count] = atom->handle;
    ctx->stis[ctx->count] = atom->attention_value.sti;
    ctx->count++;
    return true;
}

ATOMSPACE_API atom_handle_t* attention_bank_get_top_sti(attention_bank_t bank, size_t count, size_t* actual_count) {
    if (actual_count) *actual_count = 0;
    if (!bank || !bank->as || !actual_count || count == 0) return NULL;

    atomspace_t as = bank->as;
    top_sti_collect_t ctx = { NULL, NULL, 0, 0 };
    pthread_rwlock_rdlock(&as->lock);
    for_each_atom(as, top_sti_visit, &ctx);
    pthread_rwlock_unlock(&as->lock);

    if (ctx.count == 0) {
        free(ctx.handles);
        free(ctx.stis);
        return NULL;
    }

    /* Selection sort of the top `count` entries (descending STI) */
    size_t want = count < ctx.count ? count : ctx.count;
    for (size_t i = 0; i < want; i++) {
        size_t best = i;
        for (size_t j = i + 1; j < ctx.count; j++) {
            if (ctx.stis[j] > ctx.stis[best]) best = j;
        }
        if (best != i) {
            int16_t ts = ctx.stis[i]; ctx.stis[i] = ctx.stis[best]; ctx.stis[best] = ts;
            atom_handle_t th = ctx.handles[i]; ctx.handles[i] = ctx.handles[best]; ctx.handles[best] = th;
        }
    }

    atom_handle_t* result = malloc(want * sizeof(atom_handle_t));
    if (result) {
        memcpy(result, ctx.handles, want * sizeof(atom_handle_t));
        *actual_count = want;
    }
    free(ctx.handles);
    free(ctx.stis);
    return result;
}

ATOMSPACE_API int16_t attention_bank_get_attentional_focus_boundary(attention_bank_t bank) {
    return bank ? bank->afb_threshold : 0;
}

/*===========================================================================
 * Activation Spreading
 *===========================================================================*/

ATOMSPACE_API void atomspace_spread_activation(
    atomspace_t as,
    atom_handle_t source,
    spreading_config_t* config
) {
    if (!as || !config) return;

    pthread_rwlock_wrlock(&as->lock);
    atom_t* frontier_atom = find_atom(as, source);
    if (!frontier_atom) {
        pthread_rwlock_unlock(&as->lock);
        return;
    }

    atom_handle_t current = source;
    for (size_t step = 0; step < config->max_steps; step++) {
        atom_t* src = find_atom(as, current);
        if (!src) break;

        double spread = (double)src->attention_value.sti * config->decay_rate;
        if (spread < config->spread_threshold) break;

        atom_handle_t next = ATOM_HANDLE_INVALID;
        /* Spread along links referencing this atom */
        for (size_t i = 0; i < src->incoming_count; i++) {
            atom_t* link = find_atom(as, src->incoming[i]);
            if (!link) continue;
            if (!config->include_hebbian &&
                (link->type == ATOM_TYPE_HEBBIAN_LINK ||
                 link->type == ATOM_TYPE_ASYMMETRIC_HEBBIAN_LINK)) {
                continue;
            }
            for (size_t j = 0; j < link->outgoing_count; j++) {
                if (link->outgoing[j] == current) continue;
                atom_t* neighbor = find_atom(as, link->outgoing[j]);
                if (!neighbor) continue;
                neighbor->attention_value.sti = sti_clamp(
                    (int32_t)neighbor->attention_value.sti + (int32_t)spread);
                if (next == ATOM_HANDLE_INVALID) next = neighbor->handle;
            }
        }
        if (next == ATOM_HANDLE_INVALID) break;
        current = next;
    }
    pthread_rwlock_unlock(&as->lock);
}

/*===========================================================================
 * Statistics
 *===========================================================================*/

typedef struct stats_collect {
    atomspace_stats_t* stats;
    double strength_sum;
    double confidence_sum;
    int64_t sti_sum;
} stats_collect_t;

static bool stats_visit(atom_t* atom, void* user) {
    stats_collect_t* ctx = user;
    atomspace_stats_t* s = ctx->stats;
    s->total_atoms++;
    if (atom->name != NULL || is_node_type(atom->type)) {
        s->total_nodes++;
    } else {
        s->total_links++;
    }
    if (atom->type >= 0 && atom->type < ATOM_TYPE_MAX) {
        s->atoms_by_type[atom->type]++;
    }
    ctx->strength_sum += atom->truth_value.strength;
    ctx->confidence_sum += atom->truth_value.confidence;
    ctx->sti_sum += atom->attention_value.sti;
    return true;
}

ATOMSPACE_API void atomspace_get_stats(atomspace_t as, atomspace_stats_t* stats) {
    if (!stats) return;
    memset(stats, 0, sizeof(*stats));
    if (!as) return;

    stats_collect_t ctx = { stats, 0.0, 0.0, 0 };
    pthread_rwlock_rdlock(&as->lock);
    for_each_atom(as, stats_visit, &ctx);
    pthread_rwlock_unlock(&as->lock);

    if (stats->total_atoms > 0) {
        stats->avg_truth_strength = ctx.strength_sum / (double)stats->total_atoms;
        stats->avg_truth_confidence = ctx.confidence_sum / (double)stats->total_atoms;
        stats->avg_sti = (int16_t)(ctx.sti_sum / (int64_t)stats->total_atoms);
    }
}

ATOMSPACE_API size_t atomspace_size(atomspace_t as) {
    if (!as) return 0;
    pthread_rwlock_rdlock(&as->lock);
    size_t n = as->atom_count;
    pthread_rwlock_unlock(&as->lock);
    return n;
}

/*===========================================================================
 * Serialization
 *
 * Simple line-oriented text format:
 *   N <handle> <type> <tv-type> <strength> <confidence> <count> <sti> <lti> <vlti> <name>
 *   L <handle> <type> <tv-type> <strength> <confidence> <count> <sti> <lti> <vlti> <arity> <h1> ... <hn>
 *===========================================================================*/

typedef struct save_ctx {
    FILE* f;
    bool links_pass;
    bool ok;
} save_ctx_t;

static bool save_visit(atom_t* atom, void* user) {
    save_ctx_t* ctx = user;
    bool is_link = (atom->name == NULL);
    if (is_link != ctx->links_pass) return true;

    if (!is_link) {
        if (fprintf(ctx->f, "N %llu %d %d %.17g %.17g %.17g %d %d %d %s\n",
                (unsigned long long)atom->handle, (int)atom->type,
                (int)atom->truth_value.type,
                atom->truth_value.strength, atom->truth_value.confidence,
                atom->truth_value.count,
                (int)atom->attention_value.sti, (int)atom->attention_value.lti,
                (int)atom->attention_value.vlti,
                atom->name) < 0) {
            ctx->ok = false;
            return false;
        }
    } else {
        if (fprintf(ctx->f, "L %llu %d %d %.17g %.17g %.17g %d %d %d %zu",
                (unsigned long long)atom->handle, (int)atom->type,
                (int)atom->truth_value.type,
                atom->truth_value.strength, atom->truth_value.confidence,
                atom->truth_value.count,
                (int)atom->attention_value.sti, (int)atom->attention_value.lti,
                (int)atom->attention_value.vlti,
                atom->outgoing_count) < 0) {
            ctx->ok = false;
            return false;
        }
        for (size_t i = 0; i < atom->outgoing_count; i++) {
            if (fprintf(ctx->f, " %llu",
                        (unsigned long long)atom->outgoing[i]) < 0) {
                ctx->ok = false;
                return false;
            }
        }
        if (fprintf(ctx->f, "\n") < 0) {
            ctx->ok = false;
            return false;
        }
    }
    return true;
}

ATOMSPACE_API cog_result_t atomspace_save(atomspace_t as, const char* path) {
    if (!as || !path) return COG_ERROR_INVALID_ARG;
    FILE* f = fopen(path, "w");
    if (!f) return COG_ERROR_IO;

    save_ctx_t ctx = { f, false, true };
    pthread_rwlock_rdlock(&as->lock);
    for_each_atom(as, save_visit, &ctx);          /* nodes first */
    if (ctx.ok) {
        ctx.links_pass = true;
        for_each_atom(as, save_visit, &ctx);      /* then links */
    }
    pthread_rwlock_unlock(&as->lock);

    if (fclose(f) != 0) ctx.ok = false;
    return ctx.ok ? COG_SUCCESS : COG_ERROR_IO;
}

typedef struct handle_map_entry {
    atom_handle_t old_handle;
    atom_handle_t new_handle;
    struct handle_map_entry* next;
} handle_map_entry_t;

static atom_handle_t handle_map_get(handle_map_entry_t* map, atom_handle_t old_handle) {
    for (handle_map_entry_t* e = map; e; e = e->next) {
        if (e->old_handle == old_handle) return e->new_handle;
    }
    return ATOM_HANDLE_INVALID;
}

ATOMSPACE_API cog_result_t atomspace_load(atomspace_t as, const char* path) {
    if (!as || !path) return COG_ERROR_INVALID_ARG;
    FILE* f = fopen(path, "r");
    if (!f) return COG_ERROR_IO;

    handle_map_entry_t* map = NULL;
    cog_result_t result = COG_SUCCESS;
    char line[4096];

    while (fgets(line, sizeof(line), f)) {
        char kind = line[0];
        if (kind != 'N' && kind != 'L') continue;

        unsigned long long old_handle;
        int type, tv_type, sti, lti, vlti;
        double strength, confidence, cnt;
        int consumed = 0;

        if (sscanf(line + 1, " %llu %d %d %lg %lg %lg %d %d %d%n",
                   &old_handle, &type, &tv_type,
                   &strength, &confidence, &cnt,
                   &sti, &lti, &vlti, &consumed) != 9) {
            result = COG_ERROR_INVALID_FORMAT;
            break;
        }

        truth_value_t tv;
        tv.type = (truth_value_type_t)tv_type;
        tv.strength = strength;
        tv.confidence = confidence;
        tv.count = cnt;

        atom_handle_t new_handle = ATOM_HANDLE_INVALID;
        const char* rest = line + 1 + consumed;

        if (kind == 'N') {
            while (*rest == ' ') rest++;
            char name[2048];
            size_t len = strcspn(rest, "\n");
            if (len >= sizeof(name)) len = sizeof(name) - 1;
            memcpy(name, rest, len);
            name[len] = '\0';
            new_handle = atomspace_add_node_tv(as, (atom_type_t)type, name, tv);
        } else {
            size_t arity = 0;
            int arity_consumed = 0;
            if (sscanf(rest, " %zu%n", &arity, &arity_consumed) != 1) {
                result = COG_ERROR_INVALID_FORMAT;
                break;
            }
            rest += arity_consumed;
            atom_handle_t* outgoing = NULL;
            if (arity > 0) {
                outgoing = malloc(arity * sizeof(atom_handle_t));
                if (!outgoing) {
                    result = COG_ERROR_MEMORY;
                    break;
                }
                bool bad = false;
                for (size_t i = 0; i < arity; i++) {
                    unsigned long long h;
                    int c = 0;
                    if (sscanf(rest, " %llu%n", &h, &c) != 1) {
                        bad = true;
                        break;
                    }
                    rest += c;
                    outgoing[i] = handle_map_get(map, (atom_handle_t)h);
                }
                if (bad) {
                    free(outgoing);
                    result = COG_ERROR_INVALID_FORMAT;
                    break;
                }
            }
            new_handle = atomspace_add_link_tv(as, (atom_type_t)type,
                                               outgoing, arity, tv);
            free(outgoing);
        }

        if (new_handle != ATOM_HANDLE_INVALID) {
            attention_value_t av;
            av.sti = (int16_t)sti;
            av.lti = (int16_t)lti;
            av.vlti = (int16_t)vlti;
            atomspace_set_av(as, new_handle, av);

            handle_map_entry_t* e = malloc(sizeof(handle_map_entry_t));
            if (e) {
                e->old_handle = (atom_handle_t)old_handle;
                e->new_handle = new_handle;
                e->next = map;
                map = e;
            }
        }
    }

    while (map) {
        handle_map_entry_t* next = map->next;
        free(map);
        map = next;
    }
    fclose(f);
    return result;
}

/*===========================================================================
 * Scheme Serialization
 *===========================================================================*/

static const char* type_to_scheme_name(atom_type_t type) {
    switch (type) {
        case ATOM_TYPE_CONCEPT_NODE:        return "ConceptNode";
        case ATOM_TYPE_PREDICATE_NODE:      return "PredicateNode";
        case ATOM_TYPE_SCHEMA_NODE:         return "SchemaNode";
        case ATOM_TYPE_VARIABLE_NODE:       return "VariableNode";
        case ATOM_TYPE_NUMBER_NODE:         return "NumberNode";
        case ATOM_TYPE_LIST_LINK:           return "ListLink";
        case ATOM_TYPE_SET_LINK:            return "SetLink";
        case ATOM_TYPE_AND_LINK:            return "AndLink";
        case ATOM_TYPE_OR_LINK:             return "OrLink";
        case ATOM_TYPE_NOT_LINK:            return "NotLink";
        case ATOM_TYPE_IMPLICATION_LINK:    return "ImplicationLink";
        case ATOM_TYPE_EQUIVALENCE_LINK:    return "EquivalenceLink";
        case ATOM_TYPE_INHERITANCE_LINK:    return "InheritanceLink";
        case ATOM_TYPE_SIMILARITY_LINK:     return "SimilarityLink";
        case ATOM_TYPE_MEMBER_LINK:         return "MemberLink";
        case ATOM_TYPE_EVALUATION_LINK:     return "EvaluationLink";
        default:                            return "Atom";
    }
}

static atom_type_t scheme_name_to_type(const char* name) {
    if (strcmp(name, "ConceptNode") == 0)     return ATOM_TYPE_CONCEPT_NODE;
    if (strcmp(name, "PredicateNode") == 0)   return ATOM_TYPE_PREDICATE_NODE;
    if (strcmp(name, "SchemaNode") == 0)      return ATOM_TYPE_SCHEMA_NODE;
    if (strcmp(name, "VariableNode") == 0)    return ATOM_TYPE_VARIABLE_NODE;
    if (strcmp(name, "NumberNode") == 0)      return ATOM_TYPE_NUMBER_NODE;
    return ATOM_TYPE_CONCEPT_NODE;
}

ATOMSPACE_API char* atomspace_to_scheme(atomspace_t as, atom_handle_t handle) {
    if (!as) return NULL;

    pthread_rwlock_rdlock(&as->lock);
    atom_t* atom = find_atom(as, handle);
    if (!atom) {
        pthread_rwlock_unlock(&as->lock);
        return NULL;
    }

    char* result = NULL;
    if (atom->name) {
        size_t len = strlen(atom->name) + 64;
        result = malloc(len);
        if (result) {
            snprintf(result, len, "(%s \"%s\")",
                     type_to_scheme_name(atom->type), atom->name);
        }
    } else {
        /* Render link with handles of its outgoing set */
        size_t len = 64 + atom->outgoing_count * 24;
        result = malloc(len);
        if (result) {
            size_t off = (size_t)snprintf(result, len, "(%s",
                                          type_to_scheme_name(atom->type));
            for (size_t i = 0; i < atom->outgoing_count && off < len; i++) {
                off += (size_t)snprintf(result + off, len - off, " #%llu",
                        (unsigned long long)atom->outgoing[i]);
            }
            if (off < len) snprintf(result + off, len - off, ")");
        }
    }
    pthread_rwlock_unlock(&as->lock);
    return result;
}

ATOMSPACE_API atom_handle_t atomspace_from_scheme(atomspace_t as, const char* scheme_expr) {
    if (!as || !scheme_expr) return ATOM_HANDLE_INVALID;

    /* Minimal parser: (TypeName "name") */
    char type_name[64];
    char atom_name[1024];
    if (sscanf(scheme_expr, " ( %63s \"%1023[^\"]\" )", type_name, atom_name) == 2) {
        return atomspace_add_node(as, scheme_name_to_type(type_name), atom_name);
    }
    return ATOM_HANDLE_INVALID;
}

/*===========================================================================
 * 9P/Styx Integration
 *===========================================================================*/

ATOMSPACE_API cog_result_t atomspace_export_9p(atomspace_t as, const char* mount_point) {
    if (!as || !mount_point) return COG_ERROR_INVALID_ARG;
    /* Full 9P export is provided by the plan9 integration layer; the core
     * library records the request as a success no-op so callers can probe
     * for availability. */
    return COG_ERROR_NOT_IMPLEMENTED;
}

ATOMSPACE_API cog_result_t atomspace_unexport_9p(atomspace_t as) {
    if (!as) return COG_ERROR_INVALID_ARG;
    return COG_ERROR_NOT_IMPLEMENTED;
}

ATOMSPACE_API atom_handle_t atomspace_get_by_path(atomspace_t as, const char* path) {
    if (!as || !path || path[0] != '/') return ATOM_HANDLE_INVALID;

    /* Path format: /<type-number>/<name> */
    const char* type_start = path + 1;
    const char* sep = strchr(type_start, '/');
    if (!sep) return ATOM_HANDLE_INVALID;

    char type_buf[32];
    size_t type_len = (size_t)(sep - type_start);
    if (type_len == 0 || type_len >= sizeof(type_buf)) return ATOM_HANDLE_INVALID;
    memcpy(type_buf, type_start, type_len);
    type_buf[type_len] = '\0';

    char* end = NULL;
    long type_num = strtol(type_buf, &end, 10);
    if (!end || *end != '\0') return ATOM_HANDLE_INVALID;

    const char* name = sep + 1;
    if (*name == '\0') return ATOM_HANDLE_INVALID;

    return atomspace_get_node(as, (atom_type_t)type_num, name);
}

ATOMSPACE_API char* atomspace_get_path(atomspace_t as, atom_handle_t handle) {
    if (!as) return NULL;
    pthread_rwlock_rdlock(&as->lock);
    atom_t* atom = find_atom(as, handle);
    char* path = NULL;
    if (atom && atom->name) {
        size_t len = strlen(atom->name) + 32;
        path = malloc(len);
        if (path) snprintf(path, len, "/%d/%s", (int)atom->type, atom->name);
    }
    pthread_rwlock_unlock(&as->lock);
    return path;
}

/*===========================================================================
 * Dis VM Integration
 *===========================================================================*/

ATOMSPACE_API atom_handle_t atomspace_register_dis_module(
    atomspace_t as,
    const char* module_name,
    void* dis_module
) {
    if (!as || !module_name) return ATOM_HANDLE_INVALID;
    atom_handle_t handle =
        atomspace_add_node(as, ATOM_TYPE_DIS_MODULE_NODE, module_name);
    if (handle != ATOM_HANDLE_INVALID) {
        pthread_rwlock_wrlock(&as->lock);
        atom_t* atom = find_atom(as, handle);
        if (atom) atom->user_data = dis_module;
        pthread_rwlock_unlock(&as->lock);
    }
    return handle;
}

ATOMSPACE_API cog_result_t atomspace_execute_limbo(
    atomspace_t as,
    atom_handle_t proc_node,
    atom_handle_t* args,
    size_t arg_count,
    atom_handle_t* result
) {
    (void)args;
    (void)arg_count;
    if (!as || !result) return COG_ERROR_INVALID_ARG;
    pthread_rwlock_rdlock(&as->lock);
    atom_t* atom = find_atom(as, proc_node);
    pthread_rwlock_unlock(&as->lock);
    if (!atom) return COG_ERROR_NOT_FOUND;
    /* Actual Limbo execution requires the Dis integration layer */
    return COG_ERROR_NOT_IMPLEMENTED;
}

/*===========================================================================
 * Kernel Integration
 *===========================================================================*/

ATOMSPACE_API atom_handle_t atomspace_register_kernel_object(
    atomspace_t as,
    atom_type_t type,
    const char* name,
    void* kernel_handle
) {
    if (!as || !name) return ATOM_HANDLE_INVALID;
    atom_handle_t handle = atomspace_add_node(as, type, name);
    if (handle != ATOM_HANDLE_INVALID) {
        pthread_rwlock_wrlock(&as->lock);
        atom_t* atom = find_atom(as, handle);
        if (atom) atom->user_data = kernel_handle;
        pthread_rwlock_unlock(&as->lock);
    }
    return handle;
}

ATOMSPACE_API void* atomspace_get_kernel_handle(atomspace_t as, atom_handle_t handle) {
    if (!as) return NULL;
    pthread_rwlock_rdlock(&as->lock);
    atom_t* atom = find_atom(as, handle);
    void* result = atom ? atom->user_data : NULL;
    pthread_rwlock_unlock(&as->lock);
    return result;
}
