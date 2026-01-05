/**
 * @file atenspace.c
 * @brief ATenSpace Implementation
 *
 * @copyright CoGWXP-OS9 Project
 */

#include "atenspace.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <math.h>
#include <time.h>

/*===========================================================================
 * Internal Structures
 *===========================================================================*/

typedef struct atom_node {
    aten_atom_t atom;
    struct atom_node* next;
    struct atom_node* incoming_head;  /* Links pointing to this atom */
    size_t incoming_count;
} atom_node_t;

typedef struct {
    atom_node_t** buckets;
    size_t bucket_count;
    size_t atom_count;
    pthread_rwlock_t lock;
} atom_table_t;

struct aten_context {
    aten_config_t config;

    /* Atom storage */
    atom_table_t atoms;

    /* Attention bank */
    struct {
        aten_handle_t* focus;
        size_t focus_count;
        size_t focus_capacity;
        int16_t focus_boundary;
        pthread_mutex_t lock;
    } attention;

    /* ECAN parameters */
    aten_ecan_params_t ecan;

    /* Statistics */
    aten_stats_t stats;
    pthread_mutex_t stats_lock;

    /* ID generator */
    uint64_t next_handle;
    pthread_mutex_t handle_lock;
};

/*===========================================================================
 * Hash Table Operations
 *===========================================================================*/

static size_t hash_handle(aten_handle_t handle, size_t bucket_count) {
    return handle % bucket_count;
}

static size_t hash_name(const char* name, aten_atom_type_t type, size_t bucket_count) {
    size_t hash = type;
    for (const char* p = name; *p; p++) {
        hash = hash * 31 + *p;
    }
    return hash % bucket_count;
}

static cog_result_t atom_table_init(atom_table_t* table, size_t initial_size) {
    table->bucket_count = initial_size > 0 ? initial_size : 1024;
    table->buckets = calloc(table->bucket_count, sizeof(atom_node_t*));
    table->atom_count = 0;
    pthread_rwlock_init(&table->lock, NULL);
    return table->buckets ? COG_OK : COG_ERROR_MEMORY;
}

static void atom_table_destroy(atom_table_t* table) {
    for (size_t i = 0; i < table->bucket_count; i++) {
        atom_node_t* node = table->buckets[i];
        while (node) {
            atom_node_t* next = node->next;
            free((void*)node->atom.name);
            free(node->atom.outgoing);
            free((void*)node->atom.value_json);
            if (node->atom.embedding) {
                free(node->atom.embedding->data);
                free(node->atom.embedding->shape);
                free(node->atom.embedding->strides);
                free(node->atom.embedding->grad);
                free(node->atom.embedding);
            }
            free(node);
            node = next;
        }
    }
    free(table->buckets);
    pthread_rwlock_destroy(&table->lock);
}

static atom_node_t* atom_table_find(atom_table_t* table, aten_handle_t handle) {
    size_t bucket = hash_handle(handle, table->bucket_count);
    atom_node_t* node = table->buckets[bucket];
    while (node) {
        if (node->atom.handle == handle) return node;
        node = node->next;
    }
    return NULL;
}

static atom_node_t* atom_table_find_node(atom_table_t* table, aten_atom_type_t type, const char* name) {
    size_t bucket = hash_name(name, type, table->bucket_count);
    atom_node_t* node = table->buckets[bucket];
    while (node) {
        if (node->atom.type == type && node->atom.name && strcmp(node->atom.name, name) == 0) {
            return node;
        }
        node = node->next;
    }
    return NULL;
}

static cog_result_t atom_table_insert(atom_table_t* table, atom_node_t* node) {
    size_t bucket = hash_handle(node->atom.handle, table->bucket_count);
    node->next = table->buckets[bucket];
    table->buckets[bucket] = node;
    table->atom_count++;
    return COG_OK;
}

/*===========================================================================
 * Tensor Operations
 *===========================================================================*/

static aten_tensor_t* tensor_create(const float* data, size_t dim, aten_device_t device) {
    aten_tensor_t* t = calloc(1, sizeof(aten_tensor_t));
    if (!t) return NULL;

    t->ndim = 1;
    t->shape = calloc(1, sizeof(size_t));
    t->strides = calloc(1, sizeof(size_t));
    t->shape[0] = dim;
    t->strides[0] = 1;
    t->size = dim;
    t->dtype = ATEN_DTYPE_FLOAT32;
    t->device = device;
    t->data = calloc(dim, sizeof(float));

    if (data) {
        memcpy(t->data, data, dim * sizeof(float));
    }

    return t;
}

static float tensor_cosine_similarity(const aten_tensor_t* a, const aten_tensor_t* b) {
    if (!a || !b || a->size != b->size) return 0.0f;

    float dot = 0.0f, norm_a = 0.0f, norm_b = 0.0f;
    for (size_t i = 0; i < a->size; i++) {
        dot += a->data[i] * b->data[i];
        norm_a += a->data[i] * a->data[i];
        norm_b += b->data[i] * b->data[i];
    }

    if (norm_a == 0.0f || norm_b == 0.0f) return 0.0f;
    return dot / (sqrtf(norm_a) * sqrtf(norm_b));
}

/*===========================================================================
 * Truth Value Operations
 *===========================================================================*/

static aten_truth_value_t default_tv(void) {
    return (aten_truth_value_t){
        .type = ATEN_TV_SIMPLE,
        .strength = 1.0f,
        .confidence = 0.9f,
        .count = 1.0f,
        .positive = 1.0f
    };
}

COGUTIL_API cog_result_t aten_merge_tv(
    const aten_truth_value_t* tv1,
    const aten_truth_value_t* tv2,
    aten_truth_value_t* result
) {
    if (!tv1 || !tv2 || !result) return COG_ERROR_INVALID_PARAM;

    /* Simple revision formula */
    float w1 = tv1->confidence;
    float w2 = tv2->confidence;
    float total = w1 + w2;

    if (total == 0.0f) {
        *result = default_tv();
        return COG_OK;
    }

    result->type = ATEN_TV_SIMPLE;
    result->strength = (tv1->strength * w1 + tv2->strength * w2) / total;
    result->confidence = total / (total + 1.0f);
    result->count = tv1->count + tv2->count;
    result->positive = tv1->positive + tv2->positive;

    return COG_OK;
}

/*===========================================================================
 * Lifecycle
 *===========================================================================*/

COGUTIL_API cog_result_t aten_init(
    const aten_config_t* config,
    aten_context_t* ctx
) {
    if (!config || !ctx) return COG_ERROR_INVALID_PARAM;

    aten_context_t c = calloc(1, sizeof(struct aten_context));
    if (!c) return COG_ERROR_MEMORY;

    memcpy(&c->config, config, sizeof(aten_config_t));
    if (config->persist_path) {
        c->config.persist_path = strdup(config->persist_path);
    }

    /* Initialize atom table */
    atom_table_init(&c->atoms, 4096);

    /* Initialize attention bank */
    c->attention.focus_capacity = 1024;
    c->attention.focus = calloc(c->attention.focus_capacity, sizeof(aten_handle_t));
    c->attention.focus_boundary = 100;
    pthread_mutex_init(&c->attention.lock, NULL);

    /* Copy ECAN params */
    if (config->enable_ecan) {
        memcpy(&c->ecan, &config->ecan_params, sizeof(aten_ecan_params_t));
    } else {
        c->ecan.stimulus_amount = 10.0f;
        c->ecan.wage = 0.1f;
        c->ecan.rent = 0.05f;
        c->ecan.max_sti = 1000;
        c->ecan.min_sti = -1000;
        c->ecan.attention_focus_boundary = 100;
    }

    pthread_mutex_init(&c->stats_lock, NULL);
    pthread_mutex_init(&c->handle_lock, NULL);
    c->next_handle = 1;

    *ctx = c;
    return COG_OK;
}

COGUTIL_API void aten_shutdown(aten_context_t ctx) {
    if (!ctx) return;

    atom_table_destroy(&ctx->atoms);
    free(ctx->attention.focus);
    free((void*)ctx->config.persist_path);

    pthread_mutex_destroy(&ctx->attention.lock);
    pthread_mutex_destroy(&ctx->stats_lock);
    pthread_mutex_destroy(&ctx->handle_lock);

    free(ctx);
}

/*===========================================================================
 * Atom Creation
 *===========================================================================*/

COGUTIL_API cog_result_t aten_create_node(
    aten_context_t ctx,
    aten_atom_type_t type,
    const char* name,
    const aten_truth_value_t* tv,
    const float* embedding,
    size_t embedding_dim,
    aten_handle_t* handle
) {
    if (!ctx || !name || !handle) return COG_ERROR_INVALID_PARAM;

    pthread_rwlock_wrlock(&ctx->atoms.lock);

    /* Check if node already exists */
    atom_node_t* existing = atom_table_find_node(&ctx->atoms, type, name);
    if (existing) {
        *handle = existing->atom.handle;
        pthread_rwlock_unlock(&ctx->atoms.lock);
        return COG_OK;
    }

    /* Create new node */
    atom_node_t* node = calloc(1, sizeof(atom_node_t));
    if (!node) {
        pthread_rwlock_unlock(&ctx->atoms.lock);
        return COG_ERROR_MEMORY;
    }

    pthread_mutex_lock(&ctx->handle_lock);
    node->atom.handle = ctx->next_handle++;
    pthread_mutex_unlock(&ctx->handle_lock);

    node->atom.type = type;
    node->atom.name = strdup(name);
    node->atom.tv = tv ? *tv : default_tv();
    node->atom.av = (aten_attention_value_t){0, 0, false};
    node->atom.timestamp = time(NULL);

    if (embedding && embedding_dim > 0) {
        node->atom.embedding = tensor_create(embedding, embedding_dim, ctx->config.default_device);
    }

    atom_table_insert(&ctx->atoms, node);

    pthread_rwlock_unlock(&ctx->atoms.lock);

    pthread_mutex_lock(&ctx->stats_lock);
    ctx->stats.node_count++;
    ctx->stats.total_atoms++;
    pthread_mutex_unlock(&ctx->stats_lock);

    *handle = node->atom.handle;
    return COG_OK;
}

COGUTIL_API cog_result_t aten_create_link(
    aten_context_t ctx,
    aten_atom_type_t type,
    const aten_handle_t* outgoing,
    size_t outgoing_count,
    const aten_truth_value_t* tv,
    aten_handle_t* handle
) {
    if (!ctx || !outgoing || outgoing_count == 0 || !handle) {
        return COG_ERROR_INVALID_PARAM;
    }

    pthread_rwlock_wrlock(&ctx->atoms.lock);

    /* Create link */
    atom_node_t* node = calloc(1, sizeof(atom_node_t));
    if (!node) {
        pthread_rwlock_unlock(&ctx->atoms.lock);
        return COG_ERROR_MEMORY;
    }

    pthread_mutex_lock(&ctx->handle_lock);
    node->atom.handle = ctx->next_handle++;
    pthread_mutex_unlock(&ctx->handle_lock);

    node->atom.type = type;
    node->atom.outgoing = calloc(outgoing_count, sizeof(aten_handle_t));
    memcpy(node->atom.outgoing, outgoing, outgoing_count * sizeof(aten_handle_t));
    node->atom.outgoing_count = outgoing_count;
    node->atom.tv = tv ? *tv : default_tv();
    node->atom.av = (aten_attention_value_t){0, 0, false};
    node->atom.timestamp = time(NULL);

    /* Add to incoming sets of outgoing atoms */
    for (size_t i = 0; i < outgoing_count; i++) {
        atom_node_t* target = atom_table_find(&ctx->atoms, outgoing[i]);
        if (target) {
            /* Would add to incoming list - simplified */
            target->incoming_count++;
        }
    }

    atom_table_insert(&ctx->atoms, node);

    pthread_rwlock_unlock(&ctx->atoms.lock);

    pthread_mutex_lock(&ctx->stats_lock);
    ctx->stats.link_count++;
    ctx->stats.total_atoms++;
    pthread_mutex_unlock(&ctx->stats_lock);

    *handle = node->atom.handle;
    return COG_OK;
}

COGUTIL_API cog_result_t aten_get_atom(
    aten_context_t ctx,
    aten_handle_t handle,
    aten_atom_t* atom
) {
    if (!ctx || !atom) return COG_ERROR_INVALID_PARAM;

    pthread_rwlock_rdlock(&ctx->atoms.lock);
    atom_node_t* node = atom_table_find(&ctx->atoms, handle);
    pthread_rwlock_unlock(&ctx->atoms.lock);

    if (!node) return COG_ERROR_NOT_FOUND;

    memcpy(atom, &node->atom, sizeof(aten_atom_t));
    return COG_OK;
}

COGUTIL_API cog_result_t aten_get_node(
    aten_context_t ctx,
    aten_atom_type_t type,
    const char* name,
    aten_handle_t* handle
) {
    if (!ctx || !name || !handle) return COG_ERROR_INVALID_PARAM;

    pthread_rwlock_rdlock(&ctx->atoms.lock);
    atom_node_t* node = atom_table_find_node(&ctx->atoms, type, name);
    pthread_rwlock_unlock(&ctx->atoms.lock);

    if (!node) return COG_ERROR_NOT_FOUND;

    *handle = node->atom.handle;
    return COG_OK;
}

/*===========================================================================
 * Attention Operations
 *===========================================================================*/

COGUTIL_API cog_result_t aten_stimulate(
    aten_context_t ctx,
    aten_handle_t handle,
    float amount
) {
    if (!ctx) return COG_ERROR_INVALID_PARAM;

    pthread_rwlock_wrlock(&ctx->atoms.lock);
    atom_node_t* node = atom_table_find(&ctx->atoms, handle);
    if (!node) {
        pthread_rwlock_unlock(&ctx->atoms.lock);
        return COG_ERROR_NOT_FOUND;
    }

    node->atom.av.sti += (int16_t)amount;
    if (node->atom.av.sti > ctx->ecan.max_sti) {
        node->atom.av.sti = (int16_t)ctx->ecan.max_sti;
    }

    /* Update attention focus */
    pthread_mutex_lock(&ctx->attention.lock);
    if (node->atom.av.sti >= ctx->attention.focus_boundary) {
        /* Add to focus if not already there */
        bool in_focus = false;
        for (size_t i = 0; i < ctx->attention.focus_count; i++) {
            if (ctx->attention.focus[i] == handle) {
                in_focus = true;
                break;
            }
        }
        if (!in_focus && ctx->attention.focus_count < ctx->attention.focus_capacity) {
            ctx->attention.focus[ctx->attention.focus_count++] = handle;
        }
    }
    pthread_mutex_unlock(&ctx->attention.lock);

    pthread_rwlock_unlock(&ctx->atoms.lock);
    return COG_OK;
}

COGUTIL_API cog_result_t aten_get_attention_focus(
    aten_context_t ctx,
    aten_handle_t** handles,
    size_t* count
) {
    if (!ctx || !handles || !count) return COG_ERROR_INVALID_PARAM;

    pthread_mutex_lock(&ctx->attention.lock);

    *handles = calloc(ctx->attention.focus_count, sizeof(aten_handle_t));
    memcpy(*handles, ctx->attention.focus, ctx->attention.focus_count * sizeof(aten_handle_t));
    *count = ctx->attention.focus_count;

    pthread_mutex_unlock(&ctx->attention.lock);

    pthread_mutex_lock(&ctx->stats_lock);
    ctx->stats.atoms_in_focus = ctx->attention.focus_count;
    pthread_mutex_unlock(&ctx->stats_lock);

    return COG_OK;
}

/*===========================================================================
 * Tensor/Embedding Operations
 *===========================================================================*/

COGUTIL_API cog_result_t aten_tensor_similarity(
    aten_context_t ctx,
    aten_handle_t a,
    aten_handle_t b,
    float* similarity
) {
    if (!ctx || !similarity) return COG_ERROR_INVALID_PARAM;

    pthread_rwlock_rdlock(&ctx->atoms.lock);

    atom_node_t* node_a = atom_table_find(&ctx->atoms, a);
    atom_node_t* node_b = atom_table_find(&ctx->atoms, b);

    if (!node_a || !node_b) {
        pthread_rwlock_unlock(&ctx->atoms.lock);
        return COG_ERROR_NOT_FOUND;
    }

    if (!node_a->atom.embedding || !node_b->atom.embedding) {
        pthread_rwlock_unlock(&ctx->atoms.lock);
        *similarity = 0.0f;
        return COG_OK;
    }

    *similarity = tensor_cosine_similarity(node_a->atom.embedding, node_b->atom.embedding);

    pthread_rwlock_unlock(&ctx->atoms.lock);

    pthread_mutex_lock(&ctx->stats_lock);
    ctx->stats.tensor_ops++;
    pthread_mutex_unlock(&ctx->stats_lock);

    return COG_OK;
}

COGUTIL_API cog_result_t aten_find_similar(
    aten_context_t ctx,
    aten_handle_t reference,
    float threshold,
    size_t max_results,
    aten_handle_t** results,
    float** similarities,
    size_t* count
) {
    if (!ctx || !results || !similarities || !count) return COG_ERROR_INVALID_PARAM;

    pthread_rwlock_rdlock(&ctx->atoms.lock);

    atom_node_t* ref_node = atom_table_find(&ctx->atoms, reference);
    if (!ref_node || !ref_node->atom.embedding) {
        pthread_rwlock_unlock(&ctx->atoms.lock);
        return COG_ERROR_NOT_FOUND;
    }

    /* Collect similar atoms */
    size_t capacity = 256;
    aten_handle_t* res_handles = calloc(capacity, sizeof(aten_handle_t));
    float* res_sims = calloc(capacity, sizeof(float));
    size_t res_count = 0;

    for (size_t i = 0; i < ctx->atoms.bucket_count && res_count < max_results; i++) {
        atom_node_t* node = ctx->atoms.buckets[i];
        while (node && res_count < max_results) {
            if (node->atom.handle != reference && node->atom.embedding) {
                float sim = tensor_cosine_similarity(ref_node->atom.embedding, node->atom.embedding);
                if (sim >= threshold) {
                    res_handles[res_count] = node->atom.handle;
                    res_sims[res_count] = sim;
                    res_count++;
                }
            }
            node = node->next;
        }
    }

    pthread_rwlock_unlock(&ctx->atoms.lock);

    /* Sort by similarity */
    for (size_t i = 0; i < res_count - 1; i++) {
        for (size_t j = 0; j < res_count - i - 1; j++) {
            if (res_sims[j] < res_sims[j + 1]) {
                float tmp_sim = res_sims[j];
                res_sims[j] = res_sims[j + 1];
                res_sims[j + 1] = tmp_sim;
                aten_handle_t tmp_h = res_handles[j];
                res_handles[j] = res_handles[j + 1];
                res_handles[j + 1] = tmp_h;
            }
        }
    }

    *results = res_handles;
    *similarities = res_sims;
    *count = res_count;

    return COG_OK;
}

/*===========================================================================
 * Pattern Matching
 *===========================================================================*/

COGUTIL_API cog_result_t aten_pattern_match(
    aten_context_t ctx,
    const aten_pattern_query_t* query,
    aten_match_result_t** results,
    size_t* count
) {
    if (!ctx || !query || !results || !count) return COG_ERROR_INVALID_PARAM;

    /* Simplified pattern matching - would implement full PM */
    *results = NULL;
    *count = 0;

    pthread_mutex_lock(&ctx->stats_lock);
    ctx->stats.pattern_matches++;
    pthread_mutex_unlock(&ctx->stats_lock);

    return COG_OK;
}

/*===========================================================================
 * Inference
 *===========================================================================*/

COGUTIL_API cog_result_t aten_pln_deduction(
    aten_context_t ctx,
    aten_handle_t a_implies_b,
    aten_handle_t b_implies_c,
    aten_handle_t* a_implies_c,
    aten_truth_value_t* derived_tv
) {
    if (!ctx || !a_implies_c || !derived_tv) return COG_ERROR_INVALID_PARAM;

    pthread_rwlock_rdlock(&ctx->atoms.lock);

    atom_node_t* ab = atom_table_find(&ctx->atoms, a_implies_b);
    atom_node_t* bc = atom_table_find(&ctx->atoms, b_implies_c);

    if (!ab || !bc) {
        pthread_rwlock_unlock(&ctx->atoms.lock);
        return COG_ERROR_NOT_FOUND;
    }

    /* PLN deduction formula */
    float sAB = ab->atom.tv.strength;
    float sBC = bc->atom.tv.strength;
    float cAB = ab->atom.tv.confidence;
    float cBC = bc->atom.tv.confidence;

    /* Simplified deduction: P(A->C) = P(A->B) * P(B->C) */
    derived_tv->type = ATEN_TV_SIMPLE;
    derived_tv->strength = sAB * sBC;
    derived_tv->confidence = cAB * cBC;
    derived_tv->count = fminf(ab->atom.tv.count, bc->atom.tv.count);

    /* Get A and C from the implications */
    aten_handle_t a_handle = ab->atom.outgoing[0];
    aten_handle_t c_handle = bc->atom.outgoing[1];

    pthread_rwlock_unlock(&ctx->atoms.lock);

    /* Create A->C implication */
    aten_handle_t outgoing[2] = {a_handle, c_handle};
    aten_create_link(ctx, ATEN_LINK_IMPLICATION, outgoing, 2, derived_tv, a_implies_c);

    pthread_mutex_lock(&ctx->stats_lock);
    ctx->stats.inferences_run++;
    pthread_mutex_unlock(&ctx->stats_lock);

    return COG_OK;
}

/*===========================================================================
 * Statistics
 *===========================================================================*/

COGUTIL_API cog_result_t aten_get_stats(
    aten_context_t ctx,
    aten_stats_t* stats
) {
    if (!ctx || !stats) return COG_ERROR_INVALID_PARAM;

    pthread_mutex_lock(&ctx->stats_lock);
    memcpy(stats, &ctx->stats, sizeof(aten_stats_t));
    pthread_mutex_unlock(&ctx->stats_lock);

    return COG_OK;
}

COGUTIL_API void aten_reset_stats(aten_context_t ctx) {
    if (!ctx) return;

    pthread_mutex_lock(&ctx->stats_lock);
    memset(&ctx->stats, 0, sizeof(aten_stats_t));
    pthread_mutex_unlock(&ctx->stats_lock);
}
