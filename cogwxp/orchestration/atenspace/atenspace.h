/**
 * @file atenspace.h
 * @brief ATenSpace - Symbolic-Neural Hybrid Knowledge Representation
 *
 * ATenSpace merges OpenCog's AtomSpace hypergraph with PyTorch's ATen
 * tensor library, enabling GPU-accelerated symbolic reasoning with
 * neural embeddings.
 *
 * Based on: https://github.com/o9nn/ATenSpace
 *
 * @copyright CoGWXP-OS9 Project
 */

#ifndef COGWXP_ATENSPACE_H
#define COGWXP_ATENSPACE_H

#include "../orchestration.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef COGUTIL_PLATFORM_NT
    #ifdef ATENSPACE_EXPORTS
        #define ATENSPACE_API __declspec(dllexport)
    #else
        #define ATENSPACE_API __declspec(dllimport)
    #endif
#else
    #define ATENSPACE_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Atom Types
 *===========================================================================*/

typedef enum {
    /* Node types */
    ATEN_NODE_CONCEPT,
    ATEN_NODE_PREDICATE,
    ATEN_NODE_VARIABLE,
    ATEN_NODE_NUMBER,
    ATEN_NODE_SCHEMA,
    ATEN_NODE_GROUNDED_SCHEMA,
    ATEN_NODE_DEFINED_SCHEMA,
    ATEN_NODE_TYPE,
    ATEN_NODE_ANCHOR,

    /* Link types */
    ATEN_LINK_ORDERED,
    ATEN_LINK_UNORDERED,
    ATEN_LINK_LIST,
    ATEN_LINK_SET,
    ATEN_LINK_INHERITANCE,
    ATEN_LINK_SIMILARITY,
    ATEN_LINK_IMPLICATION,
    ATEN_LINK_EQUIVALENCE,
    ATEN_LINK_EVALUATION,
    ATEN_LINK_EXECUTION,
    ATEN_LINK_MEMBER,
    ATEN_LINK_BIND,
    ATEN_LINK_GET,
    ATEN_LINK_PUT,
    ATEN_LINK_LAMBDA,
    ATEN_LINK_DEFINE,

    /* Logical links */
    ATEN_LINK_AND,
    ATEN_LINK_OR,
    ATEN_LINK_NOT,
    ATEN_LINK_SEQUENTIAL_AND,
    ATEN_LINK_SEQUENTIAL_OR,

    /* Attention links */
    ATEN_LINK_ATTENTION_FOCUS,
    ATEN_LINK_HEBBIAN,

    ATEN_TYPE_COUNT
} aten_atom_type_t;

/*===========================================================================
 * Truth Values
 *===========================================================================*/

typedef enum {
    ATEN_TV_SIMPLE,
    ATEN_TV_COUNT,
    ATEN_TV_INDEFINITE,
    ATEN_TV_FUZZY,
    ATEN_TV_PROBABILISTIC,
    ATEN_TV_EVIDENCE_COUNT
} aten_tv_type_t;

typedef struct {
    aten_tv_type_t type;
    float strength;      /* Mean/probability [0,1] */
    float confidence;    /* Certainty [0,1] */
    float count;         /* Evidence count */
    float positive;      /* Positive evidence */
} aten_truth_value_t;

/*===========================================================================
 * Attention Values
 *===========================================================================*/

typedef struct {
    int16_t sti;         /* Short-term importance */
    int16_t lti;         /* Long-term importance */
    bool vlti;           /* Very long-term importance flag */
} aten_attention_value_t;

/*===========================================================================
 * Tensor Embeddings
 *===========================================================================*/

typedef enum {
    ATEN_DTYPE_FLOAT32,
    ATEN_DTYPE_FLOAT64,
    ATEN_DTYPE_FLOAT16,
    ATEN_DTYPE_BFLOAT16,
    ATEN_DTYPE_INT32,
    ATEN_DTYPE_INT64
} aten_dtype_t;

typedef enum {
    ATEN_DEVICE_CPU,
    ATEN_DEVICE_CUDA,
    ATEN_DEVICE_METAL,
    ATEN_DEVICE_VULKAN
} aten_device_t;

typedef struct {
    float* data;
    size_t* shape;
    size_t* strides;
    size_t ndim;
    size_t size;
    aten_dtype_t dtype;
    aten_device_t device;
    int device_index;
    bool requires_grad;
    float* grad;
} aten_tensor_t;

/*===========================================================================
 * Atom Structure
 *===========================================================================*/

typedef uint64_t aten_handle_t;

typedef struct {
    aten_handle_t handle;
    aten_atom_type_t type;
    const char* name;            /* For nodes */
    aten_handle_t* outgoing;     /* For links */
    size_t outgoing_count;
    aten_truth_value_t tv;
    aten_attention_value_t av;
    aten_tensor_t* embedding;    /* Neural embedding */
    int64_t timestamp;
    const char* value_json;      /* For grounded atoms */
} aten_atom_t;

/*===========================================================================
 * Pattern Matching
 *===========================================================================*/

typedef struct {
    const char* variable_name;
    aten_handle_t bound_atom;
} aten_binding_t;

typedef struct {
    aten_binding_t* bindings;
    size_t binding_count;
    float confidence;
} aten_match_result_t;

typedef struct {
    aten_handle_t pattern;
    aten_handle_t* variables;
    size_t variable_count;
    float min_confidence;
    size_t max_results;
} aten_pattern_query_t;

/*===========================================================================
 * Inference
 *===========================================================================*/

typedef enum {
    ATEN_RULE_DEDUCTION,
    ATEN_RULE_INDUCTION,
    ATEN_RULE_ABDUCTION,
    ATEN_RULE_MODUS_PONENS,
    ATEN_RULE_MODUS_TOLLENS,
    ATEN_RULE_SIMILARITY_SUBSTITUTION,
    ATEN_RULE_INHERITANCE_TRANSITIVITY,
    ATEN_RULE_FUZZY_CONJUNCTION,
    ATEN_RULE_FUZZY_DISJUNCTION,
    ATEN_RULE_CUSTOM
} aten_inference_rule_t;

typedef struct {
    aten_handle_t conclusion;
    aten_handle_t* premises;
    size_t premise_count;
    aten_inference_rule_t rule;
    aten_truth_value_t derived_tv;
    float inference_cost;
} aten_inference_step_t;

typedef struct {
    aten_inference_step_t* steps;
    size_t step_count;
    aten_handle_t final_conclusion;
    float total_confidence;
    uint64_t inference_time_us;
} aten_inference_result_t;

/*===========================================================================
 * ECAN - Economic Attention Networks
 *===========================================================================*/

typedef struct {
    float stimulus_amount;
    float wage;
    float rent;
    float max_sti;
    float min_sti;
    float attention_focus_boundary;
} aten_ecan_params_t;

/*===========================================================================
 * Configuration
 *===========================================================================*/

typedef struct {
    /* Storage */
    const char* persist_path;
    bool enable_persistence;
    size_t cache_size_mb;

    /* Tensor settings */
    aten_device_t default_device;
    int cuda_device_id;
    size_t embedding_dim;
    aten_dtype_t embedding_dtype;

    /* Attention */
    bool enable_ecan;
    aten_ecan_params_t ecan_params;

    /* Inference */
    uint32_t max_inference_steps;
    float min_tv_confidence;

    /* Performance */
    uint32_t worker_threads;
    bool enable_gpu_acceleration;
} aten_config_t;

/*===========================================================================
 * Statistics
 *===========================================================================*/

typedef struct {
    uint64_t node_count;
    uint64_t link_count;
    uint64_t total_atoms;
    uint64_t atoms_in_focus;
    uint64_t pattern_matches;
    uint64_t inferences_run;
    uint64_t tensor_ops;
    size_t memory_used_mb;
    size_t gpu_memory_used_mb;
    double avg_query_time_us;
} aten_stats_t;

/*===========================================================================
 * Context Handle
 *===========================================================================*/

typedef struct aten_context* aten_context_t;

/*===========================================================================
 * Lifecycle
 *===========================================================================*/

ATENSPACE_API cog_result_t aten_init(
    const aten_config_t* config,
    aten_context_t* ctx
);

ATENSPACE_API void aten_shutdown(aten_context_t ctx);

/*===========================================================================
 * Atom Creation
 *===========================================================================*/

/**
 * Create a node with optional embedding
 */
ATENSPACE_API cog_result_t aten_create_node(
    aten_context_t ctx,
    aten_atom_type_t type,
    const char* name,
    const aten_truth_value_t* tv,
    const float* embedding,
    size_t embedding_dim,
    aten_handle_t* handle
);

/**
 * Create a link between atoms
 */
ATENSPACE_API cog_result_t aten_create_link(
    aten_context_t ctx,
    aten_atom_type_t type,
    const aten_handle_t* outgoing,
    size_t outgoing_count,
    const aten_truth_value_t* tv,
    aten_handle_t* handle
);

/**
 * Get atom by handle
 */
ATENSPACE_API cog_result_t aten_get_atom(
    aten_context_t ctx,
    aten_handle_t handle,
    aten_atom_t* atom
);

/**
 * Get atom by name (for nodes)
 */
ATENSPACE_API cog_result_t aten_get_node(
    aten_context_t ctx,
    aten_atom_type_t type,
    const char* name,
    aten_handle_t* handle
);

/**
 * Remove atom
 */
ATENSPACE_API cog_result_t aten_remove_atom(
    aten_context_t ctx,
    aten_handle_t handle,
    bool recursive
);

/*===========================================================================
 * Truth Value Operations
 *===========================================================================*/

ATENSPACE_API cog_result_t aten_set_tv(
    aten_context_t ctx,
    aten_handle_t handle,
    const aten_truth_value_t* tv
);

ATENSPACE_API cog_result_t aten_get_tv(
    aten_context_t ctx,
    aten_handle_t handle,
    aten_truth_value_t* tv
);

/**
 * Merge truth values (revision)
 */
ATENSPACE_API cog_result_t aten_merge_tv(
    const aten_truth_value_t* tv1,
    const aten_truth_value_t* tv2,
    aten_truth_value_t* result
);

/*===========================================================================
 * Attention Operations
 *===========================================================================*/

ATENSPACE_API cog_result_t aten_set_av(
    aten_context_t ctx,
    aten_handle_t handle,
    const aten_attention_value_t* av
);

ATENSPACE_API cog_result_t aten_get_av(
    aten_context_t ctx,
    aten_handle_t handle,
    aten_attention_value_t* av
);

/**
 * Stimulate atom (increase STI)
 */
ATENSPACE_API cog_result_t aten_stimulate(
    aten_context_t ctx,
    aten_handle_t handle,
    float amount
);

/**
 * Get atoms in attentional focus
 */
ATENSPACE_API cog_result_t aten_get_attention_focus(
    aten_context_t ctx,
    aten_handle_t** handles,
    size_t* count
);

/**
 * Run ECAN diffusion step
 */
ATENSPACE_API cog_result_t aten_ecan_step(aten_context_t ctx);

/*===========================================================================
 * Tensor/Embedding Operations
 *===========================================================================*/

/**
 * Set embedding for atom
 */
ATENSPACE_API cog_result_t aten_set_embedding(
    aten_context_t ctx,
    aten_handle_t handle,
    const float* embedding,
    size_t dim
);

/**
 * Get embedding
 */
ATENSPACE_API cog_result_t aten_get_embedding(
    aten_context_t ctx,
    aten_handle_t handle,
    float** embedding,
    size_t* dim
);

/**
 * Compute similarity between atoms via embeddings
 */
ATENSPACE_API cog_result_t aten_tensor_similarity(
    aten_context_t ctx,
    aten_handle_t a,
    aten_handle_t b,
    float* similarity
);

/**
 * Find similar atoms by embedding
 */
ATENSPACE_API cog_result_t aten_find_similar(
    aten_context_t ctx,
    aten_handle_t reference,
    float threshold,
    size_t max_results,
    aten_handle_t** results,
    float** similarities,
    size_t* count
);

/**
 * Batch embed text to atoms
 */
ATENSPACE_API cog_result_t aten_batch_embed(
    aten_context_t ctx,
    const char** texts,
    size_t count,
    aten_handle_t* handles
);

/**
 * GPU tensor operation
 */
ATENSPACE_API cog_result_t aten_tensor_matmul(
    aten_context_t ctx,
    const aten_tensor_t* a,
    const aten_tensor_t* b,
    aten_tensor_t** result
);

/*===========================================================================
 * Pattern Matching
 *===========================================================================*/

/**
 * Run pattern match query
 */
ATENSPACE_API cog_result_t aten_pattern_match(
    aten_context_t ctx,
    const aten_pattern_query_t* query,
    aten_match_result_t** results,
    size_t* count
);

/**
 * Bind pattern (BindLink execution)
 */
ATENSPACE_API cog_result_t aten_bind(
    aten_context_t ctx,
    aten_handle_t bind_link,
    aten_handle_t** results,
    size_t* count
);

/**
 * Get pattern (GetLink execution)
 */
ATENSPACE_API cog_result_t aten_get(
    aten_context_t ctx,
    aten_handle_t get_link,
    aten_handle_t** results,
    size_t* count
);

/*===========================================================================
 * Inference
 *===========================================================================*/

/**
 * Forward chaining inference
 */
ATENSPACE_API cog_result_t aten_forward_chain(
    aten_context_t ctx,
    aten_handle_t source,
    const aten_inference_rule_t* rules,
    size_t rule_count,
    uint32_t max_steps,
    aten_inference_result_t* result
);

/**
 * Backward chaining inference
 */
ATENSPACE_API cog_result_t aten_backward_chain(
    aten_context_t ctx,
    aten_handle_t target,
    const aten_inference_rule_t* rules,
    size_t rule_count,
    uint32_t max_steps,
    aten_inference_result_t* result
);

/**
 * PLN deduction
 */
ATENSPACE_API cog_result_t aten_pln_deduction(
    aten_context_t ctx,
    aten_handle_t a_implies_b,
    aten_handle_t b_implies_c,
    aten_handle_t* a_implies_c,
    aten_truth_value_t* derived_tv
);

/**
 * Free inference result
 */
ATENSPACE_API void aten_inference_result_free(aten_inference_result_t* result);

/*===========================================================================
 * Traversal
 *===========================================================================*/

/**
 * Get incoming links
 */
ATENSPACE_API cog_result_t aten_get_incoming(
    aten_context_t ctx,
    aten_handle_t handle,
    aten_atom_type_t filter_type,
    aten_handle_t** links,
    size_t* count
);

/**
 * Get outgoing atoms (for links)
 */
ATENSPACE_API cog_result_t aten_get_outgoing(
    aten_context_t ctx,
    aten_handle_t handle,
    aten_handle_t** atoms,
    size_t* count
);

/**
 * Get atoms by type
 */
ATENSPACE_API cog_result_t aten_get_by_type(
    aten_context_t ctx,
    aten_atom_type_t type,
    aten_handle_t** handles,
    size_t* count
);

/*===========================================================================
 * Persistence
 *===========================================================================*/

ATENSPACE_API cog_result_t aten_save(
    aten_context_t ctx,
    const char* path
);

ATENSPACE_API cog_result_t aten_load(
    aten_context_t ctx,
    const char* path
);

ATENSPACE_API cog_result_t aten_export_atomese(
    aten_context_t ctx,
    aten_handle_t root,
    char** atomese
);

ATENSPACE_API cog_result_t aten_import_atomese(
    aten_context_t ctx,
    const char* atomese,
    aten_handle_t* root
);

/*===========================================================================
 * Statistics
 *===========================================================================*/

ATENSPACE_API cog_result_t aten_get_stats(
    aten_context_t ctx,
    aten_stats_t* stats
);

ATENSPACE_API void aten_reset_stats(aten_context_t ctx);

#ifdef __cplusplus
}
#endif

#endif /* COGWXP_ATENSPACE_H */
