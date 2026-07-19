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
    /* Atoms are bucketed by handle (see atom_table_insert), so a name
     * lookup must scan all buckets. */
    for (size_t bucket = 0; bucket < table->bucket_count; bucket++) {
        atom_node_t* node = table->buckets[bucket];
        while (node) {
            if (node->atom.type == type && node->atom.name && strcmp(node->atom.name, name) == 0) {
                return node;
            }
            node = node->next;
        }
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
        if (embedding && embedding_dim > 0 && !existing->atom.embedding) {
            existing->atom.embedding =
                tensor_create(embedding, embedding_dim, ctx->config.default_device);
        }
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
    for (size_t i = 0; res_count > 1 && i < res_count - 1; i++) {
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

/*===========================================================================
 * Atom Removal
 *===========================================================================*/

COGUTIL_API cog_result_t aten_remove_atom(
    aten_context_t ctx,
    aten_handle_t handle,
    bool recursive
) {
    if (!ctx) return COG_ERROR_INVALID_PARAM;

    pthread_rwlock_wrlock(&ctx->atoms.lock);

    size_t bucket = hash_handle(handle, ctx->atoms.bucket_count);
    atom_node_t* prev = NULL;
    atom_node_t* node = ctx->atoms.buckets[bucket];

    while (node) {
        if (node->atom.handle == handle) {
            /* Unlink from bucket chain */
            if (prev) {
                prev->next = node->next;
            } else {
                ctx->atoms.buckets[bucket] = node->next;
            }

            bool is_link = (node->atom.outgoing_count > 0);

            /* Update incoming counts of atoms in the outgoing set */
            if (recursive && is_link) {
                for (size_t i = 0; i < node->atom.outgoing_count; i++) {
                    atom_node_t* target = atom_table_find(&ctx->atoms, node->atom.outgoing[i]);
                    if (target && target->incoming_count > 0) {
                        target->incoming_count--;
                    }
                }
            }

            /* Free resources */
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

            ctx->atoms.atom_count--;
            if (is_link) {
                pthread_mutex_lock(&ctx->stats_lock);
                if (ctx->stats.link_count > 0) ctx->stats.link_count--;
                if (ctx->stats.total_atoms > 0) ctx->stats.total_atoms--;
                pthread_mutex_unlock(&ctx->stats_lock);
            } else {
                pthread_mutex_lock(&ctx->stats_lock);
                if (ctx->stats.node_count > 0) ctx->stats.node_count--;
                if (ctx->stats.total_atoms > 0) ctx->stats.total_atoms--;
                pthread_mutex_unlock(&ctx->stats_lock);
            }

            free(node);
            pthread_rwlock_unlock(&ctx->atoms.lock);
            return COG_OK;
        }
        prev = node;
        node = node->next;
    }

    pthread_rwlock_unlock(&ctx->atoms.lock);
    return COG_ERROR_NOT_FOUND;
}

/*===========================================================================
 * Truth Value Operations
 *===========================================================================*/

COGUTIL_API cog_result_t aten_set_tv(
    aten_context_t ctx,
    aten_handle_t handle,
    const aten_truth_value_t* tv
) {
    if (!ctx || !tv) return COG_ERROR_INVALID_PARAM;

    pthread_rwlock_wrlock(&ctx->atoms.lock);
    atom_node_t* node = atom_table_find(&ctx->atoms, handle);
    if (!node) {
        pthread_rwlock_unlock(&ctx->atoms.lock);
        return COG_ERROR_NOT_FOUND;
    }

    node->atom.tv = *tv;
    pthread_rwlock_unlock(&ctx->atoms.lock);
    return COG_OK;
}

COGUTIL_API cog_result_t aten_get_tv(
    aten_context_t ctx,
    aten_handle_t handle,
    aten_truth_value_t* tv
) {
    if (!ctx || !tv) return COG_ERROR_INVALID_PARAM;

    pthread_rwlock_rdlock(&ctx->atoms.lock);
    atom_node_t* node = atom_table_find(&ctx->atoms, handle);
    if (!node) {
        pthread_rwlock_unlock(&ctx->atoms.lock);
        return COG_ERROR_NOT_FOUND;
    }

    *tv = node->atom.tv;
    pthread_rwlock_unlock(&ctx->atoms.lock);
    return COG_OK;
}

/*===========================================================================
 * Attention Value Operations
 *===========================================================================*/

COGUTIL_API cog_result_t aten_set_av(
    aten_context_t ctx,
    aten_handle_t handle,
    const aten_attention_value_t* av
) {
    if (!ctx || !av) return COG_ERROR_INVALID_PARAM;

    pthread_rwlock_wrlock(&ctx->atoms.lock);
    atom_node_t* node = atom_table_find(&ctx->atoms, handle);
    if (!node) {
        pthread_rwlock_unlock(&ctx->atoms.lock);
        return COG_ERROR_NOT_FOUND;
    }

    node->atom.av = *av;
    pthread_rwlock_unlock(&ctx->atoms.lock);
    return COG_OK;
}

COGUTIL_API cog_result_t aten_get_av(
    aten_context_t ctx,
    aten_handle_t handle,
    aten_attention_value_t* av
) {
    if (!ctx || !av) return COG_ERROR_INVALID_PARAM;

    pthread_rwlock_rdlock(&ctx->atoms.lock);
    atom_node_t* node = atom_table_find(&ctx->atoms, handle);
    if (!node) {
        pthread_rwlock_unlock(&ctx->atoms.lock);
        return COG_ERROR_NOT_FOUND;
    }

    *av = node->atom.av;
    pthread_rwlock_unlock(&ctx->atoms.lock);
    return COG_OK;
}

/*===========================================================================
 * ECAN Step
 *===========================================================================*/

COGUTIL_API cog_result_t aten_ecan_step(aten_context_t ctx) {
    if (!ctx) return COG_ERROR_INVALID_PARAM;

    pthread_rwlock_wrlock(&ctx->atoms.lock);
    pthread_mutex_lock(&ctx->attention.lock);

    float wage = ctx->ecan.wage;
    float rent = ctx->ecan.rent;
    float spread_factor = 0.1f;

    /* Diffuse STI from high-STI atoms to neighbours via links */
    for (size_t b = 0; b < ctx->atoms.bucket_count; b++) {
        atom_node_t* node = ctx->atoms.buckets[b];
        while (node) {
            /* Collect rent: decay STI of every atom */
            float new_sti = (float)node->atom.av.sti - rent;
            if (new_sti < ctx->ecan.min_sti) {
                new_sti = ctx->ecan.min_sti;
            }

            /* Spread activation through outgoing links */
            if (node->atom.av.sti > ctx->attention.focus_boundary &&
                node->atom.outgoing_count > 0) {
                float spread = (float)node->atom.av.sti * spread_factor;
                float per_target = spread / (float)node->atom.outgoing_count;

                for (size_t i = 0; i < node->atom.outgoing_count; i++) {
                    atom_node_t* target = atom_table_find(&ctx->atoms, node->atom.outgoing[i]);
                    if (target) {
                        float new_target_sti = (float)target->atom.av.sti + per_target * wage;
                        if (new_target_sti > ctx->ecan.max_sti) {
                            new_target_sti = ctx->ecan.max_sti;
                        }
                        target->atom.av.sti = (int16_t)new_target_sti;
                    }
                }

                /* Deduct spread amount from source */
                new_sti -= spread;
            }

            node->atom.av.sti = (int16_t)new_sti;

            node = node->next;
        }
    }

    /* Rebuild attentional focus list */
    ctx->attention.focus_count = 0;
    for (size_t b = 0; b < ctx->atoms.bucket_count; b++) {
        atom_node_t* node = ctx->atoms.buckets[b];
        while (node) {
            if (node->atom.av.sti >= ctx->attention.focus_boundary &&
                ctx->attention.focus_count < ctx->attention.focus_capacity) {
                ctx->attention.focus[ctx->attention.focus_count++] = node->atom.handle;
            }
            node = node->next;
        }
    }

    pthread_mutex_unlock(&ctx->attention.lock);
    pthread_rwlock_unlock(&ctx->atoms.lock);

    pthread_mutex_lock(&ctx->stats_lock);
    ctx->stats.atoms_in_focus = ctx->attention.focus_count;
    pthread_mutex_unlock(&ctx->stats_lock);

    return COG_OK;
}

/*===========================================================================
 * Embedding Operations
 *===========================================================================*/

COGUTIL_API cog_result_t aten_set_embedding(
    aten_context_t ctx,
    aten_handle_t handle,
    const float* embedding,
    size_t dim
) {
    if (!ctx || !embedding || dim == 0) return COG_ERROR_INVALID_PARAM;

    pthread_rwlock_wrlock(&ctx->atoms.lock);
    atom_node_t* node = atom_table_find(&ctx->atoms, handle);
    if (!node) {
        pthread_rwlock_unlock(&ctx->atoms.lock);
        return COG_ERROR_NOT_FOUND;
    }

    /* Free existing embedding */
    if (node->atom.embedding) {
        free(node->atom.embedding->data);
        free(node->atom.embedding->shape);
        free(node->atom.embedding->strides);
        free(node->atom.embedding->grad);
        free(node->atom.embedding);
        node->atom.embedding = NULL;
    }

    node->atom.embedding = tensor_create(embedding, dim, ctx->config.default_device);
    pthread_rwlock_unlock(&ctx->atoms.lock);

    if (!node->atom.embedding) return COG_ERROR_MEMORY;

    pthread_mutex_lock(&ctx->stats_lock);
    ctx->stats.tensor_ops++;
    pthread_mutex_unlock(&ctx->stats_lock);

    return COG_OK;
}

COGUTIL_API cog_result_t aten_get_embedding(
    aten_context_t ctx,
    aten_handle_t handle,
    float** embedding,
    size_t* dim
) {
    if (!ctx || !embedding || !dim) return COG_ERROR_INVALID_PARAM;

    pthread_rwlock_rdlock(&ctx->atoms.lock);
    atom_node_t* node = atom_table_find(&ctx->atoms, handle);
    if (!node || !node->atom.embedding) {
        pthread_rwlock_unlock(&ctx->atoms.lock);
        return COG_ERROR_NOT_FOUND;
    }

    size_t sz = node->atom.embedding->size;
    float* out = malloc(sz * sizeof(float));
    if (!out) {
        pthread_rwlock_unlock(&ctx->atoms.lock);
        return COG_ERROR_MEMORY;
    }

    memcpy(out, node->atom.embedding->data, sz * sizeof(float));
    *embedding = out;
    *dim = sz;

    pthread_rwlock_unlock(&ctx->atoms.lock);
    return COG_OK;
}

COGUTIL_API cog_result_t aten_batch_embed(
    aten_context_t ctx,
    const char** texts,
    size_t count,
    aten_handle_t* handles
) {
    if (!ctx || !texts || !handles || count == 0) return COG_ERROR_INVALID_PARAM;

    size_t dim = ctx->config.embedding_dim > 0 ? ctx->config.embedding_dim : 128;

    for (size_t i = 0; i < count; i++) {
        if (!texts[i]) continue;

        /* Generate a deterministic pseudo-embedding from the text via hashing */
        float* emb = calloc(dim, sizeof(float));
        if (!emb) return COG_ERROR_MEMORY;

        /* Simple hash-based pseudo-embedding (simulates a real embedding model) */
        size_t len = strlen(texts[i]);
        for (size_t j = 0; j < dim; j++) {
            float val = 0.0f;
            for (size_t k = 0; k < len; k++) {
                val += (float)((unsigned char)texts[i][k]) *
                       sinf((float)(j + 1) * (float)(k + 1) * 0.01f);
            }
            /* Normalize component */
            emb[j] = tanhf(val / (float)(len + 1));
        }

        /* Create or update node */
        aten_truth_value_t tv = {
            .type = ATEN_TV_SIMPLE,
            .strength = 1.0f,
            .confidence = 0.9f,
            .count = 1.0f,
            .positive = 1.0f
        };
        aten_handle_t h;
        cog_result_t res = aten_create_node(ctx, ATEN_NODE_CONCEPT, texts[i], &tv, emb, dim, &h);
        free(emb);
        if (res != COG_OK) return res;

        handles[i] = h;
    }

    return COG_OK;
}

COGUTIL_API cog_result_t aten_tensor_matmul(
    aten_context_t ctx,
    const aten_tensor_t* a,
    const aten_tensor_t* b,
    aten_tensor_t** result
) {
    if (!ctx || !a || !b || !result) return COG_ERROR_INVALID_PARAM;
    if (a->ndim < 2 || b->ndim < 2)  return COG_ERROR_INVALID_PARAM;

    size_t m  = a->shape[a->ndim - 2];
    size_t k1 = a->shape[a->ndim - 1];
    size_t k2 = b->shape[b->ndim - 2];
    size_t n  = b->shape[b->ndim - 1];

    if (k1 != k2) return COG_ERROR_INVALID_PARAM;

    size_t out_shape[2] = {m, n};
    aten_tensor_t* out = calloc(1, sizeof(aten_tensor_t));
    if (!out) return COG_ERROR_MEMORY;

    out->ndim = 2;
    out->shape = calloc(2, sizeof(size_t));
    out->strides = calloc(2, sizeof(size_t));
    if (!out->shape || !out->strides) {
        free(out->shape); free(out->strides); free(out);
        return COG_ERROR_MEMORY;
    }
    out->shape[0] = m;
    out->shape[1] = n;
    out->strides[0] = n;
    out->strides[1] = 1;
    out->size = m * n;
    out->dtype = a->dtype;
    out->device = a->device;
    out->data = calloc(m * n, sizeof(float));
    if (!out->data) {
        free(out->shape); free(out->strides); free(out);
        return COG_ERROR_MEMORY;
    }
    (void)out_shape;

    float* A = a->data;
    float* B = b->data;
    float* C = out->data;

    for (size_t i = 0; i < m; i++) {
        for (size_t j = 0; j < n; j++) {
            float sum = 0.0f;
            for (size_t p = 0; p < k1; p++) {
                sum += A[i * k1 + p] * B[p * n + j];
            }
            C[i * n + j] = sum;
        }
    }

    *result = out;

    pthread_mutex_lock(&ctx->stats_lock);
    ctx->stats.tensor_ops++;
    pthread_mutex_unlock(&ctx->stats_lock);

    return COG_OK;
}

/*===========================================================================
 * Traversal
 *===========================================================================*/

COGUTIL_API cog_result_t aten_get_incoming(
    aten_context_t ctx,
    aten_handle_t handle,
    aten_atom_type_t filter_type,
    aten_handle_t** links,
    size_t* count
) {
    if (!ctx || !links || !count) return COG_ERROR_INVALID_PARAM;

    pthread_rwlock_rdlock(&ctx->atoms.lock);

    /* Verify the atom exists */
    atom_node_t* target = atom_table_find(&ctx->atoms, handle);
    if (!target) {
        pthread_rwlock_unlock(&ctx->atoms.lock);
        return COG_ERROR_NOT_FOUND;
    }

    /* Scan all link atoms for those containing this handle in outgoing set */
    size_t capacity = 256;
    aten_handle_t* result = calloc(capacity, sizeof(aten_handle_t));
    size_t result_count = 0;

    for (size_t b = 0; b < ctx->atoms.bucket_count; b++) {
        atom_node_t* node = ctx->atoms.buckets[b];
        while (node) {
            if (node->atom.outgoing_count > 0) {
                /* Only apply type filter when filter_type != -1 */
                bool type_ok = ((int)filter_type < 0) || (node->atom.type == filter_type);
                if (type_ok) {
                    for (size_t i = 0; i < node->atom.outgoing_count; i++) {
                        if (node->atom.outgoing[i] == handle) {
                            if (result_count < capacity) {
                                result[result_count++] = node->atom.handle;
                            }
                            break;
                        }
                    }
                }
            }
            node = node->next;
        }
    }

    pthread_rwlock_unlock(&ctx->atoms.lock);

    *links = result;
    *count = result_count;
    return COG_OK;
}

COGUTIL_API cog_result_t aten_get_outgoing(
    aten_context_t ctx,
    aten_handle_t handle,
    aten_handle_t** atoms,
    size_t* count
) {
    if (!ctx || !atoms || !count) return COG_ERROR_INVALID_PARAM;

    pthread_rwlock_rdlock(&ctx->atoms.lock);
    atom_node_t* node = atom_table_find(&ctx->atoms, handle);
    if (!node) {
        pthread_rwlock_unlock(&ctx->atoms.lock);
        return COG_ERROR_NOT_FOUND;
    }

    size_t n = node->atom.outgoing_count;
    aten_handle_t* result = calloc(n + 1, sizeof(aten_handle_t));
    if (!result) {
        pthread_rwlock_unlock(&ctx->atoms.lock);
        return COG_ERROR_MEMORY;
    }

    if (n > 0) {
        memcpy(result, node->atom.outgoing, n * sizeof(aten_handle_t));
    }
    pthread_rwlock_unlock(&ctx->atoms.lock);

    *atoms = result;
    *count = n;
    return COG_OK;
}

COGUTIL_API cog_result_t aten_get_by_type(
    aten_context_t ctx,
    aten_atom_type_t type,
    aten_handle_t** handles,
    size_t* count
) {
    if (!ctx || !handles || !count) return COG_ERROR_INVALID_PARAM;

    pthread_rwlock_rdlock(&ctx->atoms.lock);

    size_t capacity = 1024;
    aten_handle_t* result = calloc(capacity, sizeof(aten_handle_t));
    size_t result_count = 0;

    for (size_t b = 0; b < ctx->atoms.bucket_count; b++) {
        atom_node_t* node = ctx->atoms.buckets[b];
        while (node) {
            if (node->atom.type == type && result_count < capacity) {
                result[result_count++] = node->atom.handle;
            }
            node = node->next;
        }
    }

    pthread_rwlock_unlock(&ctx->atoms.lock);

    *handles = result;
    *count = result_count;
    return COG_OK;
}

/*===========================================================================
 * Pattern Matching (BindLink / GetLink)
 *===========================================================================*/

COGUTIL_API cog_result_t aten_pattern_match(
    aten_context_t ctx,
    const aten_pattern_query_t* query,
    aten_match_result_t** results,
    size_t* count
) {
    if (!ctx || !query || !results || !count) return COG_ERROR_INVALID_PARAM;

    pthread_rwlock_rdlock(&ctx->atoms.lock);

    /* Retrieve pattern atom */
    atom_node_t* pattern = atom_table_find(&ctx->atoms, query->pattern);

    size_t capacity = 64;
    aten_match_result_t* res = calloc(capacity, sizeof(aten_match_result_t));
    size_t res_count = 0;

    if (pattern && pattern->atom.tv.confidence >= query->min_confidence) {
        /* Scan all atoms for ones matching the pattern type */
        for (size_t b = 0; b < ctx->atoms.bucket_count && res_count < query->max_results; b++) {
            atom_node_t* node = ctx->atoms.buckets[b];
            while (node && res_count < query->max_results) {
                /* Match by type: if pattern is a node type, match same-type nodes */
                if (node->atom.type == pattern->atom.type &&
                    node->atom.tv.confidence >= query->min_confidence &&
                    node->atom.handle != pattern->atom.handle) {

                    aten_match_result_t* m = &res[res_count++];
                    m->confidence = node->atom.tv.confidence;

                    /* Create variable bindings for each variable in query */
                    if (query->variable_count > 0 && query->variables) {
                        m->bindings = calloc(query->variable_count, sizeof(aten_binding_t));
                        m->binding_count = query->variable_count;
                        for (size_t v = 0; v < query->variable_count; v++) {
                            atom_node_t* var = atom_table_find(&ctx->atoms, query->variables[v]);
                            m->bindings[v].variable_name = var ? var->atom.name : NULL;
                            m->bindings[v].bound_atom = node->atom.handle;
                        }
                    } else {
                        m->bindings = NULL;
                        m->binding_count = 0;
                    }
                }
                node = node->next;
            }
        }
    }

    pthread_rwlock_unlock(&ctx->atoms.lock);

    *results = res;
    *count = res_count;

    pthread_mutex_lock(&ctx->stats_lock);
    ctx->stats.pattern_matches++;
    pthread_mutex_unlock(&ctx->stats_lock);

    return COG_OK;
}

COGUTIL_API cog_result_t aten_bind(
    aten_context_t ctx,
    aten_handle_t bind_link,
    aten_handle_t** results,
    size_t* count
) {
    if (!ctx || !results || !count) return COG_ERROR_INVALID_PARAM;

    pthread_rwlock_rdlock(&ctx->atoms.lock);
    atom_node_t* bl = atom_table_find(&ctx->atoms, bind_link);
    pthread_rwlock_unlock(&ctx->atoms.lock);

    if (!bl || bl->atom.type != ATEN_LINK_BIND) {
        *results = NULL;
        *count = 0;
        return COG_OK;
    }

    /* BindLink: outgoing[0] = pattern (with variables), outgoing[1] = rewrite
     * Simplified: gather all atoms matching the pattern's type */
    if (bl->atom.outgoing_count < 1) {
        *results = NULL;
        *count = 0;
        return COG_OK;
    }

    aten_pattern_query_t q = {
        .pattern = bl->atom.outgoing[0],
        .variables = NULL,
        .variable_count = 0,
        .min_confidence = 0.0f,
        .max_results = 256
    };

    aten_match_result_t* match_res = NULL;
    size_t match_count = 0;
    aten_pattern_match(ctx, &q, &match_res, &match_count);

    aten_handle_t* out = calloc(match_count, sizeof(aten_handle_t));
    for (size_t i = 0; i < match_count; i++) {
        out[i] = match_res[i].bindings ? match_res[i].bindings[0].bound_atom : 0;
        free(match_res[i].bindings);
    }
    free(match_res);

    *results = out;
    *count = match_count;
    return COG_OK;
}

COGUTIL_API cog_result_t aten_get(
    aten_context_t ctx,
    aten_handle_t get_link,
    aten_handle_t** results,
    size_t* count
) {
    if (!ctx || !results || !count) return COG_ERROR_INVALID_PARAM;

    pthread_rwlock_rdlock(&ctx->atoms.lock);
    atom_node_t* gl = atom_table_find(&ctx->atoms, get_link);
    pthread_rwlock_unlock(&ctx->atoms.lock);

    if (!gl || gl->atom.type != ATEN_LINK_GET || gl->atom.outgoing_count < 1) {
        *results = NULL;
        *count = 0;
        return COG_OK;
    }

    /* GetLink: return atoms matching the clause */
    aten_pattern_query_t q = {
        .pattern = gl->atom.outgoing[0],
        .variables = NULL,
        .variable_count = 0,
        .min_confidence = 0.0f,
        .max_results = 256
    };

    aten_match_result_t* match_res = NULL;
    size_t match_count = 0;
    aten_pattern_match(ctx, &q, &match_res, &match_count);

    aten_handle_t* out = calloc(match_count, sizeof(aten_handle_t));
    for (size_t i = 0; i < match_count; i++) {
        out[i] = match_res[i].bindings ? match_res[i].bindings[0].bound_atom : 0;
        free(match_res[i].bindings);
    }
    free(match_res);

    *results = out;
    *count = match_count;
    return COG_OK;
}

/*===========================================================================
 * Inference Chains
 *===========================================================================*/

COGUTIL_API cog_result_t aten_forward_chain(
    aten_context_t ctx,
    aten_handle_t source,
    const aten_inference_rule_t* rules,
    size_t rule_count,
    uint32_t max_steps,
    aten_inference_result_t* result
) {
    if (!ctx || !result) return COG_ERROR_INVALID_PARAM;
    if (rule_count > 0 && !rules) return COG_ERROR_INVALID_PARAM;

    memset(result, 0, sizeof(aten_inference_result_t));

    size_t step_capacity = (max_steps < 64) ? max_steps : 64;
    result->steps = calloc(step_capacity, sizeof(aten_inference_step_t));
    if (!result->steps) return COG_ERROR_MEMORY;

    uint64_t t_start = (uint64_t)clock();

    /* Frontier: atoms to process */
    aten_handle_t frontier[256];
    size_t frontier_size = 1;
    frontier[0] = source;

    aten_handle_t last_conclusion = source;
    float total_confidence = 1.0f;

    for (uint32_t step = 0; step < max_steps && frontier_size > 0; step++) {
        aten_handle_t current = frontier[--frontier_size];

        pthread_rwlock_rdlock(&ctx->atoms.lock);
        atom_node_t* cur_node = atom_table_find(&ctx->atoms, current);
        if (!cur_node) {
            pthread_rwlock_unlock(&ctx->atoms.lock);
            continue;
        }

        /* For each implication link that has 'current' as the antecedent */
        for (size_t b = 0; b < ctx->atoms.bucket_count; b++) {
            atom_node_t* link = ctx->atoms.buckets[b];
            while (link) {
                if ((link->atom.type == ATEN_LINK_IMPLICATION ||
                     link->atom.type == ATEN_LINK_INHERITANCE) &&
                    link->atom.outgoing_count >= 2 &&
                    link->atom.outgoing[0] == current) {

                    aten_handle_t conclusion = link->atom.outgoing[1];
                    atom_node_t* conc_node = atom_table_find(&ctx->atoms, conclusion);

                    if (conc_node && result->step_count < step_capacity) {
                        aten_inference_step_t* s = &result->steps[result->step_count];
                        s->conclusion = conclusion;
                        s->premises = malloc(sizeof(aten_handle_t));
                        if (s->premises) {
                            s->premises[0] = current;
                            s->premise_count = 1;
                        }
                        s->rule = (rule_count > 0) ? rules[0] : ATEN_RULE_DEDUCTION;
                        /* Derive truth value via modus ponens */
                        s->derived_tv.type = ATEN_TV_SIMPLE;
                        s->derived_tv.strength =
                            cur_node->atom.tv.strength * link->atom.tv.strength;
                        s->derived_tv.confidence =
                            cur_node->atom.tv.confidence * link->atom.tv.confidence;
                        s->inference_cost = 1.0f;

                        result->step_count++;
                        last_conclusion = conclusion;
                        total_confidence *= s->derived_tv.confidence;

                        if (frontier_size < 255) {
                            frontier[frontier_size++] = conclusion;
                        }
                    }
                }
                link = link->next;
            }
        }
        pthread_rwlock_unlock(&ctx->atoms.lock);
    }

    result->final_conclusion = last_conclusion;
    result->total_confidence = total_confidence;
    result->inference_time_us = (uint64_t)clock() - t_start;

    pthread_mutex_lock(&ctx->stats_lock);
    ctx->stats.inferences_run++;
    pthread_mutex_unlock(&ctx->stats_lock);

    return COG_OK;
}

COGUTIL_API cog_result_t aten_backward_chain(
    aten_context_t ctx,
    aten_handle_t target,
    const aten_inference_rule_t* rules,
    size_t rule_count,
    uint32_t max_steps,
    aten_inference_result_t* result
) {
    if (!ctx || !result) return COG_ERROR_INVALID_PARAM;
    if (rule_count > 0 && !rules) return COG_ERROR_INVALID_PARAM;

    memset(result, 0, sizeof(aten_inference_result_t));

    size_t step_capacity = (max_steps < 64) ? max_steps : 64;
    result->steps = calloc(step_capacity, sizeof(aten_inference_step_t));
    if (!result->steps) return COG_ERROR_MEMORY;

    uint64_t t_start = (uint64_t)clock();

    /* Goal stack: atoms we are trying to prove */
    aten_handle_t goals[256];
    size_t goals_size = 1;
    goals[0] = target;

    float total_confidence = 1.0f;

    for (uint32_t step = 0; step < max_steps && goals_size > 0; step++) {
        aten_handle_t goal = goals[--goals_size];

        pthread_rwlock_rdlock(&ctx->atoms.lock);

        /* Find implications that conclude this goal: (A -> goal) */
        for (size_t b = 0; b < ctx->atoms.bucket_count; b++) {
            atom_node_t* link = ctx->atoms.buckets[b];
            while (link) {
                if ((link->atom.type == ATEN_LINK_IMPLICATION ||
                     link->atom.type == ATEN_LINK_INHERITANCE) &&
                    link->atom.outgoing_count >= 2 &&
                    link->atom.outgoing[1] == goal) {

                    aten_handle_t premise = link->atom.outgoing[0];
                    atom_node_t* prem_node = atom_table_find(&ctx->atoms, premise);

                    if (prem_node && result->step_count < step_capacity) {
                        aten_inference_step_t* s = &result->steps[result->step_count];
                        s->conclusion = goal;
                        s->premises = malloc(sizeof(aten_handle_t));
                        if (s->premises) {
                            s->premises[0] = premise;
                            s->premise_count = 1;
                        }
                        s->rule = (rule_count > 0) ? rules[0] : ATEN_RULE_MODUS_PONENS;
                        s->derived_tv.type = ATEN_TV_SIMPLE;
                        s->derived_tv.strength =
                            prem_node->atom.tv.strength * link->atom.tv.strength;
                        s->derived_tv.confidence =
                            prem_node->atom.tv.confidence * link->atom.tv.confidence;
                        s->inference_cost = 1.0f;

                        result->step_count++;
                        total_confidence *= s->derived_tv.confidence;

                        /* Recursively prove the premise */
                        if (goals_size < 255) {
                            goals[goals_size++] = premise;
                        }
                    }
                }
                link = link->next;
            }
        }

        pthread_rwlock_unlock(&ctx->atoms.lock);
    }

    result->final_conclusion = target;
    result->total_confidence = total_confidence;
    result->inference_time_us = (uint64_t)clock() - t_start;

    pthread_mutex_lock(&ctx->stats_lock);
    ctx->stats.inferences_run++;
    pthread_mutex_unlock(&ctx->stats_lock);

    return COG_OK;
}

COGUTIL_API void aten_inference_result_free(aten_inference_result_t* result) {
    if (!result) return;
    for (size_t i = 0; i < result->step_count; i++) {
        free(result->steps[i].premises);
    }
    free(result->steps);
    result->steps = NULL;
    result->step_count = 0;
}

/*===========================================================================
 * Persistence
 *===========================================================================*/

COGUTIL_API cog_result_t aten_save(
    aten_context_t ctx,
    const char* path
) {
    if (!ctx || !path) return COG_ERROR_INVALID_PARAM;

    FILE* f = fopen(path, "wb");
    if (!f) return COG_ERROR_NOT_FOUND;

    /* Magic header */
    fwrite("ATEN", 4, 1, f);
    uint32_t version = 1;
    fwrite(&version, sizeof(uint32_t), 1, f);

    /* Atom count */
    size_t total = ctx->atoms.atom_count;
    fwrite(&total, sizeof(size_t), 1, f);

    /* Write each atom */
    for (size_t b = 0; b < ctx->atoms.bucket_count; b++) {
        atom_node_t* node = ctx->atoms.buckets[b];
        while (node) {
            fwrite(&node->atom.handle, sizeof(aten_handle_t), 1, f);
            fwrite(&node->atom.type, sizeof(aten_atom_type_t), 1, f);
            fwrite(&node->atom.tv, sizeof(aten_truth_value_t), 1, f);
            fwrite(&node->atom.av, sizeof(aten_attention_value_t), 1, f);

            /* Name */
            size_t name_len = node->atom.name ? strlen(node->atom.name) : 0;
            fwrite(&name_len, sizeof(size_t), 1, f);
            if (name_len > 0) fwrite(node->atom.name, 1, name_len, f);

            /* Outgoing */
            fwrite(&node->atom.outgoing_count, sizeof(size_t), 1, f);
            if (node->atom.outgoing_count > 0) {
                fwrite(node->atom.outgoing, sizeof(aten_handle_t),
                       node->atom.outgoing_count, f);
            }

            /* Embedding */
            size_t emb_dim = (node->atom.embedding) ? node->atom.embedding->size : 0;
            fwrite(&emb_dim, sizeof(size_t), 1, f);
            if (emb_dim > 0) {
                fwrite(node->atom.embedding->data, sizeof(float), emb_dim, f);
            }

            node = node->next;
        }
    }

    fclose(f);
    return COG_OK;
}

COGUTIL_API cog_result_t aten_load(
    aten_context_t ctx,
    const char* path
) {
    if (!ctx || !path) return COG_ERROR_INVALID_PARAM;

    FILE* f = fopen(path, "rb");
    if (!f) return COG_ERROR_NOT_FOUND;

    /* Verify magic */
    char magic[4];
    fread(magic, 4, 1, f);
    if (memcmp(magic, "ATEN", 4) != 0) {
        fclose(f);
        return COG_ERROR_INVALID_PARAM;
    }

    uint32_t version;
    fread(&version, sizeof(uint32_t), 1, f);

    size_t total;
    fread(&total, sizeof(size_t), 1, f);

    for (size_t i = 0; i < total; i++) {
        aten_handle_t handle;
        aten_atom_type_t type;
        aten_truth_value_t tv;
        aten_attention_value_t av;

        fread(&handle, sizeof(aten_handle_t), 1, f);
        fread(&type, sizeof(aten_atom_type_t), 1, f);
        fread(&tv, sizeof(aten_truth_value_t), 1, f);
        fread(&av, sizeof(aten_attention_value_t), 1, f);

        size_t name_len;
        fread(&name_len, sizeof(size_t), 1, f);
        char* name = NULL;
        if (name_len > 0) {
            name = malloc(name_len + 1);
            fread(name, 1, name_len, f);
            name[name_len] = '\0';
        }

        size_t outgoing_count;
        fread(&outgoing_count, sizeof(size_t), 1, f);
        aten_handle_t* outgoing = NULL;
        if (outgoing_count > 0) {
            outgoing = malloc(outgoing_count * sizeof(aten_handle_t));
            fread(outgoing, sizeof(aten_handle_t), outgoing_count, f);
        }

        size_t emb_dim;
        fread(&emb_dim, sizeof(size_t), 1, f);
        float* emb = NULL;
        if (emb_dim > 0) {
            emb = malloc(emb_dim * sizeof(float));
            fread(emb, sizeof(float), emb_dim, f);
        }

        /* Recreate the atom */
        aten_handle_t new_handle;
        if (name) {
            aten_create_node(ctx, type, name, &tv, emb, emb_dim, &new_handle);
        } else if (outgoing_count > 0) {
            aten_create_link(ctx, type, outgoing, outgoing_count, &tv, &new_handle);
        }

        free(name);
        free(outgoing);
        free(emb);
    }

    fclose(f);
    return COG_OK;
}

/*===========================================================================
 * Atomese Serialization
 *===========================================================================*/

static void aten_write_atomese_node(
    aten_context_t ctx,
    atom_node_t* node,
    char* buf,
    size_t buf_size,
    size_t* pos
) {
    static const char* type_names[] = {
        "ConceptNode", "PredicateNode", "VariableNode", "NumberNode",
        "SchemaNode", "GroundedSchemaNode", "DefinedSchemaNode",
        "TypeNode", "AnchorNode"
    };
    const char* type_str = "Node";
    if (node->atom.type < 9) {
        type_str = type_names[node->atom.type];
    }

    int n = snprintf(buf + *pos, buf_size - *pos,
                     "(%s \"%s\" (stv %.4f %.4f))",
                     type_str,
                     node->atom.name ? node->atom.name : "",
                     node->atom.tv.strength,
                     node->atom.tv.confidence);
    if (n > 0) *pos += (size_t)n;
}

COGUTIL_API cog_result_t aten_export_atomese(
    aten_context_t ctx,
    aten_handle_t root,
    char** atomese
) {
    if (!ctx || !atomese) return COG_ERROR_INVALID_PARAM;

    pthread_rwlock_rdlock(&ctx->atoms.lock);
    atom_node_t* node = atom_table_find(&ctx->atoms, root);
    pthread_rwlock_unlock(&ctx->atoms.lock);

    if (!node) return COG_ERROR_NOT_FOUND;

    size_t buf_size = 4096;
    char* buf = calloc(buf_size, 1);
    if (!buf) return COG_ERROR_MEMORY;

    size_t pos = 0;
    aten_write_atomese_node(ctx, node, buf, buf_size, &pos);
    *atomese = buf;
    return COG_OK;
}

COGUTIL_API cog_result_t aten_import_atomese(
    aten_context_t ctx,
    const char* atomese,
    aten_handle_t* root
) {
    if (!ctx || !atomese || !root) return COG_ERROR_INVALID_PARAM;

    /* Minimal Atomese parser: handles (ConceptNode "name" (stv s c)) */
    const char* p = atomese;
    while (*p && *p != '(') p++;
    if (!*p) return COG_ERROR_INVALID_PARAM;
    p++; /* skip '(' */

    /* Read type */
    char type_buf[128] = {0};
    size_t ti = 0;
    while (*p && *p != ' ' && *p != '"' && ti < sizeof(type_buf) - 1) {
        type_buf[ti++] = *p++;
    }
    while (*p == ' ') p++;

    /* Read name */
    char name_buf[512] = {0};
    if (*p == '"') {
        p++; /* skip opening quote */
        size_t ni = 0;
        while (*p && *p != '"' && ni < sizeof(name_buf) - 1) {
            name_buf[ni++] = *p++;
        }
        if (*p == '"') p++;
    }

    /* Read truth value if present */
    float strength = 1.0f, confidence = 0.9f;
    const char* stv_pos = strstr(p, "stv");
    if (stv_pos) {
        sscanf(stv_pos, "stv %f %f", &strength, &confidence);
    }

    /* Map type string to enum */
    aten_atom_type_t type = ATEN_NODE_CONCEPT;
    if (strncmp(type_buf, "Predicate", 9) == 0) type = ATEN_NODE_PREDICATE;
    else if (strncmp(type_buf, "Variable", 8) == 0) type = ATEN_NODE_VARIABLE;
    else if (strncmp(type_buf, "Number", 6) == 0) type = ATEN_NODE_NUMBER;

    aten_truth_value_t tv = {
        .type = ATEN_TV_SIMPLE,
        .strength = strength,
        .confidence = confidence,
        .count = 1.0f,
        .positive = strength
    };

    return aten_create_node(ctx, type, name_buf, &tv, NULL, 0, root);
}
