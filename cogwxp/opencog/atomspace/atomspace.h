/**
 * @file atomspace.h
 * @brief AtomSpace - Hypergraph Knowledge Representation for CoGWXP-OS9
 * 
 * The AtomSpace is the core knowledge representation system for OpenCog,
 * providing a hypergraph database with truth values and attention allocation.
 * 
 * @copyright AGPL-3.0
 */

#ifndef _COGWXP_ATOMSPACE_H_
#define _COGWXP_ATOMSPACE_H_

#include "../cogutil/cogutil.h"

#ifdef COGUTIL_PLATFORM_NT
    #ifdef ATOMSPACE_EXPORTS
        #define ATOMSPACE_API __declspec(dllexport)
    #else
        #define ATOMSPACE_API __declspec(dllimport)
    #endif
#else
    #define ATOMSPACE_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Atom Types
 *===========================================================================*/

typedef enum {
    /* Node types */
    ATOM_TYPE_NODE = 0,
    ATOM_TYPE_CONCEPT_NODE,
    ATOM_TYPE_PREDICATE_NODE,
    ATOM_TYPE_SCHEMA_NODE,
    ATOM_TYPE_GROUNDED_SCHEMA_NODE,
    ATOM_TYPE_VARIABLE_NODE,
    ATOM_TYPE_NUMBER_NODE,
    ATOM_TYPE_TYPE_NODE,
    
    /* Link types */
    ATOM_TYPE_LINK = 100,
    ATOM_TYPE_ORDERED_LINK,
    ATOM_TYPE_UNORDERED_LINK,
    ATOM_TYPE_LIST_LINK,
    ATOM_TYPE_SET_LINK,
    
    /* Logical links */
    ATOM_TYPE_AND_LINK = 200,
    ATOM_TYPE_OR_LINK,
    ATOM_TYPE_NOT_LINK,
    ATOM_TYPE_IMPLICATION_LINK,
    ATOM_TYPE_EQUIVALENCE_LINK,
    
    /* Relational links */
    ATOM_TYPE_INHERITANCE_LINK = 300,
    ATOM_TYPE_SIMILARITY_LINK,
    ATOM_TYPE_MEMBER_LINK,
    ATOM_TYPE_SUBSET_LINK,
    
    /* Evaluation links */
    ATOM_TYPE_EVALUATION_LINK = 400,
    ATOM_TYPE_EXECUTION_LINK,
    ATOM_TYPE_EXECUTION_OUTPUT_LINK,
    
    /* Cognitive links */
    ATOM_TYPE_ATTENTION_LINK = 500,
    ATOM_TYPE_HEBBIAN_LINK,
    ATOM_TYPE_ASYMMETRIC_HEBBIAN_LINK,
    
    /* Plan 9 / Inferno integration types */
    ATOM_TYPE_9P_RESOURCE_NODE = 600,
    ATOM_TYPE_DIS_MODULE_NODE,
    ATOM_TYPE_LIMBO_PROC_NODE,
    ATOM_TYPE_STYX_CHANNEL_LINK,
    
    /* CogW7OS kernel types */
    ATOM_TYPE_KERNEL_PROCESS_NODE = 700,
    ATOM_TYPE_KERNEL_THREAD_NODE,
    ATOM_TYPE_KERNEL_MEMORY_NODE,
    ATOM_TYPE_KERNEL_IPC_LINK,
    
    ATOM_TYPE_MAX
} atom_type_t;

/*===========================================================================
 * Truth Value Types
 *===========================================================================*/

typedef enum {
    TRUTH_VALUE_SIMPLE = 0,
    TRUTH_VALUE_COUNT,
    TRUTH_VALUE_INDEFINITE,
    TRUTH_VALUE_FUZZY,
    TRUTH_VALUE_PROBABILISTIC
} truth_value_type_t;

typedef struct truth_value {
    truth_value_type_t type;
    double strength;        /* Primary truth value [0, 1] */
    double confidence;      /* Confidence in the truth value [0, 1] */
    double count;           /* Evidence count (for CountTruthValue) */
} truth_value_t;

/* Default truth values */
#define TRUTH_VALUE_DEFAULT     { TRUTH_VALUE_SIMPLE, 1.0, 0.0, 0.0 }
#define TRUTH_VALUE_TRUE        { TRUTH_VALUE_SIMPLE, 1.0, 1.0, 1.0 }
#define TRUTH_VALUE_FALSE       { TRUTH_VALUE_SIMPLE, 0.0, 1.0, 1.0 }
#define TRUTH_VALUE_UNKNOWN     { TRUTH_VALUE_SIMPLE, 0.5, 0.0, 0.0 }

/*===========================================================================
 * Attention Value
 *===========================================================================*/

typedef struct attention_value {
    int16_t sti;            /* Short-Term Importance */
    int16_t lti;            /* Long-Term Importance */
    int16_t vlti;           /* Very Long-Term Importance (boolean flag) */
} attention_value_t;

#define ATTENTION_VALUE_DEFAULT { 0, 0, 0 }

/*===========================================================================
 * Atom Handle and Structure
 *===========================================================================*/

typedef uint64_t atom_handle_t;
#define ATOM_HANDLE_INVALID ((atom_handle_t)0)

typedef struct atom {
    atom_handle_t handle;
    atom_type_t type;
    char* name;                     /* For nodes */
    atom_handle_t* outgoing;        /* For links */
    size_t outgoing_count;
    truth_value_t truth_value;
    attention_value_t attention_value;
    uint64_t flags;
    void* user_data;

    /* Internal bookkeeping (managed by the AtomSpace — do not modify) */
    atom_handle_t* incoming;        /* Links referencing this atom */
    size_t incoming_count;
    size_t incoming_capacity;
} atom_t;

/*===========================================================================
 * AtomSpace Handle
 *===========================================================================*/

typedef struct atomspace* atomspace_t;

/*===========================================================================
 * AtomSpace Lifecycle
 *===========================================================================*/

ATOMSPACE_API atomspace_t atomspace_create(void);
ATOMSPACE_API void        atomspace_destroy(atomspace_t as);
ATOMSPACE_API atomspace_t atomspace_create_child(atomspace_t parent);
ATOMSPACE_API atomspace_t atomspace_get_parent(atomspace_t as);
ATOMSPACE_API void        atomspace_clear(atomspace_t as);

/*===========================================================================
 * Atom Creation and Retrieval
 *===========================================================================*/

/* Node operations */
ATOMSPACE_API atom_handle_t atomspace_add_node(
    atomspace_t as,
    atom_type_t type,
    const char* name
);

ATOMSPACE_API atom_handle_t atomspace_add_node_tv(
    atomspace_t as,
    atom_type_t type,
    const char* name,
    truth_value_t tv
);

/* Link operations */
ATOMSPACE_API atom_handle_t atomspace_add_link(
    atomspace_t as,
    atom_type_t type,
    const atom_handle_t* outgoing,
    size_t outgoing_count
);

ATOMSPACE_API atom_handle_t atomspace_add_link_tv(
    atomspace_t as,
    atom_type_t type,
    const atom_handle_t* outgoing,
    size_t outgoing_count,
    truth_value_t tv
);

/* Retrieval */
ATOMSPACE_API atom_handle_t atomspace_get_node(
    atomspace_t as,
    atom_type_t type,
    const char* name
);

ATOMSPACE_API atom_handle_t atomspace_get_link(
    atomspace_t as,
    atom_type_t type,
    const atom_handle_t* outgoing,
    size_t outgoing_count
);

ATOMSPACE_API const atom_t* atomspace_get_atom(atomspace_t as, atom_handle_t handle);

/* Removal */
ATOMSPACE_API bool atomspace_remove_atom(atomspace_t as, atom_handle_t handle);
ATOMSPACE_API bool atomspace_remove_atom_recursive(atomspace_t as, atom_handle_t handle);

/*===========================================================================
 * Atom Properties
 *===========================================================================*/

/* Truth value operations */
ATOMSPACE_API truth_value_t atomspace_get_tv(atomspace_t as, atom_handle_t handle);
ATOMSPACE_API void          atomspace_set_tv(atomspace_t as, atom_handle_t handle, truth_value_t tv);
ATOMSPACE_API void          atomspace_merge_tv(atomspace_t as, atom_handle_t handle, truth_value_t tv);

/* Truth value helpers */
ATOMSPACE_API truth_value_t tv_simple(double strength, double confidence);
ATOMSPACE_API truth_value_t tv_count(double strength, double confidence, uint64_t count);
ATOMSPACE_API truth_value_t tv_merge(const truth_value_t* a, const truth_value_t* b);

/* Attention value operations */
ATOMSPACE_API attention_value_t atomspace_get_av(atomspace_t as, atom_handle_t handle);
ATOMSPACE_API void              atomspace_set_av(atomspace_t as, atom_handle_t handle, attention_value_t av);
ATOMSPACE_API void              atomspace_stimulate(atomspace_t as, atom_handle_t handle, int16_t stimulus);

/* Type and name */
ATOMSPACE_API atom_type_t   atomspace_get_type(atomspace_t as, atom_handle_t handle);
ATOMSPACE_API const char*   atomspace_get_name(atomspace_t as, atom_handle_t handle);
ATOMSPACE_API bool          atomspace_is_node(atomspace_t as, atom_handle_t handle);
ATOMSPACE_API bool          atomspace_is_link(atomspace_t as, atom_handle_t handle);

/* Outgoing set (for links) */
ATOMSPACE_API size_t              atomspace_get_arity(atomspace_t as, atom_handle_t handle);
ATOMSPACE_API const atom_handle_t* atomspace_get_outgoing(atomspace_t as, atom_handle_t handle);
ATOMSPACE_API atom_handle_t       atomspace_get_outgoing_at(atomspace_t as, atom_handle_t handle, size_t index);

/* Incoming set */
ATOMSPACE_API size_t              atomspace_get_incoming_size(atomspace_t as, atom_handle_t handle);
ATOMSPACE_API atom_handle_t*      atomspace_get_incoming(atomspace_t as, atom_handle_t handle, size_t* count);

/*===========================================================================
 * Query and Pattern Matching
 *===========================================================================*/

typedef struct atom_query* atom_query_t;

/* Query builder */
ATOMSPACE_API atom_query_t atomspace_query_create(atomspace_t as);
ATOMSPACE_API void         atomspace_query_destroy(atom_query_t query);
ATOMSPACE_API void         atomspace_query_type(atom_query_t query, atom_type_t type);
ATOMSPACE_API void         atomspace_query_name(atom_query_t query, const char* pattern);
ATOMSPACE_API void         atomspace_query_tv_min(atom_query_t query, double min_strength, double min_confidence);
ATOMSPACE_API void         atomspace_query_av_min(atom_query_t query, int16_t min_sti);

/* Query execution */
ATOMSPACE_API atom_handle_t* atomspace_query_execute(atom_query_t query, size_t* count);
ATOMSPACE_API void           atomspace_query_results_free(atom_handle_t* results);

/* Pattern matching */
ATOMSPACE_API atom_handle_t* atomspace_pattern_match(
    atomspace_t as,
    atom_handle_t pattern,
    size_t* count
);

/*===========================================================================
 * Attention Allocation
 *===========================================================================*/

typedef struct attention_bank* attention_bank_t;

ATOMSPACE_API attention_bank_t atomspace_get_attention_bank(atomspace_t as);
ATOMSPACE_API void             attention_bank_update_sti(attention_bank_t bank, atom_handle_t handle, int16_t delta);
ATOMSPACE_API void             attention_bank_update_lti(attention_bank_t bank, atom_handle_t handle, int16_t delta);
ATOMSPACE_API atom_handle_t*   attention_bank_get_top_sti(attention_bank_t bank, size_t count, size_t* actual_count);
ATOMSPACE_API int16_t          attention_bank_get_attentional_focus_boundary(attention_bank_t bank);

/*===========================================================================
 * Activation Spreading
 *===========================================================================*/

typedef struct spreading_config {
    double decay_rate;
    double spread_threshold;
    size_t max_steps;
    bool include_hebbian;
} spreading_config_t;

ATOMSPACE_API void atomspace_spread_activation(
    atomspace_t as,
    atom_handle_t source,
    spreading_config_t* config
);

/*===========================================================================
 * Statistics
 *===========================================================================*/

typedef struct atomspace_stats {
    size_t total_atoms;
    size_t total_nodes;
    size_t total_links;
    size_t atoms_by_type[ATOM_TYPE_MAX];
    double avg_truth_strength;
    double avg_truth_confidence;
    int16_t avg_sti;
} atomspace_stats_t;

ATOMSPACE_API void atomspace_get_stats(atomspace_t as, atomspace_stats_t* stats);
ATOMSPACE_API size_t atomspace_size(atomspace_t as);

/* Retrieve all atoms of a given type (optionally including subtypes).
 * On success *atoms is a malloc'd array the caller frees with
 * atomspace_query_results_free(). */
ATOMSPACE_API cog_result_t atomspace_get_atoms_by_type(
    atomspace_t as,
    atom_type_t type,
    bool include_subtypes,
    atom_handle_t** atoms,
    size_t* count
);

/*===========================================================================
 * Serialization
 *===========================================================================*/

ATOMSPACE_API cog_result_t atomspace_save(atomspace_t as, const char* path);
ATOMSPACE_API cog_result_t atomspace_load(atomspace_t as, const char* path);
ATOMSPACE_API char*        atomspace_to_scheme(atomspace_t as, atom_handle_t handle);
ATOMSPACE_API atom_handle_t atomspace_from_scheme(atomspace_t as, const char* scheme_expr);

/*===========================================================================
 * 9P/Styx Integration
 *===========================================================================*/

/* Export AtomSpace as 9P file system */
ATOMSPACE_API cog_result_t atomspace_export_9p(atomspace_t as, const char* mount_point);
ATOMSPACE_API cog_result_t atomspace_unexport_9p(atomspace_t as);

/* Access atoms via 9P paths */
ATOMSPACE_API atom_handle_t atomspace_get_by_path(atomspace_t as, const char* path);
ATOMSPACE_API char*         atomspace_get_path(atomspace_t as, atom_handle_t handle);

/*===========================================================================
 * Dis VM Integration
 *===========================================================================*/

/* Register Dis module as atom */
ATOMSPACE_API atom_handle_t atomspace_register_dis_module(
    atomspace_t as,
    const char* module_name,
    void* dis_module
);

/* Execute Limbo procedure on atom */
ATOMSPACE_API cog_result_t atomspace_execute_limbo(
    atomspace_t as,
    atom_handle_t proc_node,
    atom_handle_t* args,
    size_t arg_count,
    atom_handle_t* result
);

/*===========================================================================
 * Kernel Integration
 *===========================================================================*/

/* Register kernel object as atom */
ATOMSPACE_API atom_handle_t atomspace_register_kernel_object(
    atomspace_t as,
    atom_type_t type,
    const char* name,
    void* kernel_handle
);

/* Get kernel handle from atom */
ATOMSPACE_API void* atomspace_get_kernel_handle(atomspace_t as, atom_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* _COGWXP_ATOMSPACE_H_ */
