/**
 * @file fractal_atoms.h
 * @brief Fractal AtomSpace - Self-Similar Recursive Hypergraph Structures
 * 
 * Extends the AtomSpace with fractal/recursive hypergraph structures that
 * exhibit self-similarity at multiple scales. This enables hierarchical
 * knowledge representation where patterns repeat at different levels.
 * 
 * Key concepts:
 * - Fractal atoms contain references to child atoms with similar structure
 * - Scale-invariant truth value propagation across hierarchical depths
 * - Self-referential atoms for recursive knowledge structures
 * - Fractal dimension metrics for measuring self-similarity
 * 
 * @copyright CoGWXP-OS9 Project - AGI-OS Fractal AI Integration
 */

#ifndef _COGWXP_FRACTAL_ATOMS_H_
#define _COGWXP_FRACTAL_ATOMS_H_

#include "atomspace.h"
#include "../cogutil/cogutil.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Fractal Constants and Limits
 *===========================================================================*/

/** Maximum depth of fractal hierarchy */
#define FRACTAL_MAX_DEPTH           32

/** Default fractal dimension for new atoms */
#define FRACTAL_DEFAULT_DIMENSION   1.5

/** Maximum number of children per fractal node */
#define FRACTAL_MAX_CHILDREN        1024

/** Scale factor decay per level (default) */
#define FRACTAL_DEFAULT_SCALE_DECAY 0.5

/** Truth value inheritance factor */
#define FRACTAL_TV_INHERITANCE      0.9

/** Minimum scale factor before pruning */
#define FRACTAL_MIN_SCALE           0.001

/*===========================================================================
 * Fractal Atom Types
 *===========================================================================*/

/**
 * Extended atom types for fractal structures
 * These extend the base atom_type_t enum
 */
typedef enum {
    /* Fractal node types (starting at 800) */
    FRACTAL_TYPE_NODE = 800,
    FRACTAL_TYPE_RECURSIVE_NODE,      /* Self-referential node */
    FRACTAL_TYPE_SCALE_NODE,          /* Scale-specific node */
    FRACTAL_TYPE_CONTAINER_NODE,      /* Contains sub-atomspace */
    FRACTAL_TYPE_MIRROR_NODE,         /* Reflects parent structure */
    FRACTAL_TYPE_GENERATOR_NODE,      /* Generates children dynamically */
    
    /* Fractal link types */
    FRACTAL_TYPE_LINK = 850,
    FRACTAL_TYPE_RECURSIVE_LINK,      /* Link with recursive structure */
    FRACTAL_TYPE_SCALE_LINK,          /* Cross-scale connection */
    FRACTAL_TYPE_INHERITANCE_LINK,    /* Fractal property inheritance */
    FRACTAL_TYPE_SIMILARITY_LINK,     /* Self-similarity measure */
    FRACTAL_TYPE_PARENT_LINK,         /* Link to parent in hierarchy */
    FRACTAL_TYPE_CHILD_LINK,          /* Link to child in hierarchy */
    FRACTAL_TYPE_SIBLING_LINK,        /* Link to sibling at same scale */
    
    /* Meta-fractal types */
    FRACTAL_TYPE_META = 900,
    FRACTAL_TYPE_DIMENSION_NODE,      /* Stores fractal dimension */
    FRACTAL_TYPE_SCALE_FACTOR_NODE,   /* Stores scale factor */
    FRACTAL_TYPE_DEPTH_NODE,          /* Stores hierarchy depth */
    
    FRACTAL_TYPE_MAX
} fractal_type_t;

/*===========================================================================
 * Fractal Properties
 *===========================================================================*/

/**
 * Scale transformation type
 */
typedef enum {
    FRACTAL_SCALE_LINEAR,        /* Linear scaling s' = k * s */
    FRACTAL_SCALE_LOGARITHMIC,   /* Logarithmic scaling s' = log(k * s) */
    FRACTAL_SCALE_EXPONENTIAL,   /* Exponential scaling s' = exp(k * s) */
    FRACTAL_SCALE_POWER_LAW,     /* Power law scaling s' = s^k */
    FRACTAL_SCALE_CUSTOM         /* Custom scaling function */
} fractal_scale_type_t;

/**
 * Fractal propagation direction
 */
typedef enum {
    FRACTAL_PROPAGATE_UP,        /* Child to parent */
    FRACTAL_PROPAGATE_DOWN,      /* Parent to child */
    FRACTAL_PROPAGATE_BOTH,      /* Bidirectional */
    FRACTAL_PROPAGATE_NONE       /* No propagation */
} fractal_propagation_t;

/**
 * Fractal dimension calculation method
 */
typedef enum {
    FRACTAL_DIM_HAUSDORFF,       /* Hausdorff dimension */
    FRACTAL_DIM_BOX_COUNTING,    /* Box-counting dimension */
    FRACTAL_DIM_CORRELATION,     /* Correlation dimension */
    FRACTAL_DIM_INFORMATION,     /* Information dimension */
    FRACTAL_DIM_ESTIMATED        /* Estimated from structure */
} fractal_dimension_method_t;

/*===========================================================================
 * Core Fractal Structures
 *===========================================================================*/

/**
 * Fractal properties attached to atoms
 */
typedef struct fractal_properties {
    uint32_t depth;                    /* Level in hierarchy (0 = root) */
    float fractal_dimension;           /* Self-similarity measure [1, 3] */
    float scale_factor;                /* Relative scale to parent (0, 1] */
    fractal_scale_type_t scale_type;   /* How scaling is applied */
    fractal_propagation_t propagation; /* TV/AV propagation direction */
    
    /* Self-reference */
    bool is_self_referential;          /* Contains reference to self */
    atom_handle_t self_reference;      /* Handle to self-reference point */
    
    /* Inherited values */
    truth_value_t inherited_tv;        /* TV inherited from parent */
    attention_value_t inherited_av;    /* AV inherited from parent */
    float inheritance_weight;          /* Weight of inherited values */
} fractal_properties_t;

/**
 * Fractal atom structure
 * Extends base atom with fractal hierarchy information
 */
typedef struct fractal_atom {
    /* Base atom handle */
    atom_handle_t handle;
    
    /* Fractal hierarchy */
    atom_handle_t parent;              /* Parent atom in hierarchy */
    atom_handle_t* children;           /* Array of child atoms */
    size_t child_count;                /* Number of children */
    size_t child_capacity;             /* Allocated capacity */
    
    /* Sibling connections */
    atom_handle_t* siblings;           /* Atoms at same scale level */
    size_t sibling_count;
    
    /* Fractal properties */
    fractal_properties_t properties;
    
    /* Containment */
    atomspace_t local_atomspace;       /* Optional contained atomspace */
    bool owns_atomspace;               /* Whether to destroy on cleanup */
    
    /* Generation */
    bool is_generator;                 /* Can generate children dynamically */
    void* generator_context;           /* Context for generation function */
    
    /* Statistics */
    uint64_t access_count;             /* Number of accesses */
    uint64_t last_access_time;         /* Timestamp of last access */
    uint64_t creation_time;            /* Timestamp of creation */
} fractal_atom_t;

/**
 * Fractal traversal callback
 */
typedef bool (*fractal_visitor_fn)(
    fractal_atom_t* atom,
    uint32_t depth,
    void* user_data
);

/**
 * Fractal generator callback
 */
typedef cog_result_t (*fractal_generator_fn)(
    fractal_atom_t* parent,
    uint32_t child_index,
    fractal_atom_t** child,
    void* context
);

/*===========================================================================
 * Fractal AtomSpace Handle
 *===========================================================================*/

typedef struct fractal_atomspace* fractal_atomspace_t;

/**
 * Configuration for fractal atomspace
 */
typedef struct fractal_config {
    /* Base atomspace */
    atomspace_t base_atomspace;
    
    /* Hierarchy limits */
    uint32_t max_depth;                /* Maximum hierarchy depth */
    size_t max_total_atoms;            /* Maximum total fractal atoms */
    
    /* Scaling parameters */
    fractal_scale_type_t default_scale_type;
    float default_scale_decay;         /* Scale factor per level */
    float min_scale_factor;            /* Minimum scale before pruning */
    
    /* Propagation defaults */
    fractal_propagation_t default_propagation;
    float tv_inheritance_weight;       /* Default TV inheritance [0, 1] */
    float av_inheritance_weight;       /* Default AV inheritance [0, 1] */
    
    /* Dimension calculation */
    fractal_dimension_method_t dimension_method;
    
    /* Performance tuning */
    bool enable_lazy_generation;       /* Generate children on access */
    bool enable_caching;               /* Cache traversal results */
    size_t cache_size;                 /* Number of cached traversals */
    
    /* Cycle detection */
    bool detect_cycles;                /* Detect and prevent cycles */
    bool allow_self_reference;         /* Allow controlled self-reference */
    
    /* Persistence */
    bool enable_persistence;
    const char* persistence_path;
} fractal_config_t;

/*===========================================================================
 * Fractal Statistics
 *===========================================================================*/

typedef struct fractal_stats {
    /* Atom counts */
    size_t total_fractal_atoms;
    size_t atoms_by_depth[FRACTAL_MAX_DEPTH];
    size_t generator_atoms;
    size_t self_referential_atoms;
    
    /* Hierarchy metrics */
    uint32_t max_observed_depth;
    float avg_depth;
    float avg_branching_factor;
    float avg_fractal_dimension;
    
    /* Scale metrics */
    float min_scale_factor;
    float max_scale_factor;
    float avg_scale_factor;
    
    /* Performance */
    uint64_t traversals;
    uint64_t generations;
    uint64_t cache_hits;
    uint64_t cache_misses;
} fractal_stats_t;

/*===========================================================================
 * Lifecycle Functions
 *===========================================================================*/

/**
 * Create a fractal atomspace wrapper
 */
COGUTIL_API cog_result_t fractal_atomspace_create(
    const fractal_config_t* config,
    fractal_atomspace_t* fas
);

/**
 * Destroy fractal atomspace (optionally preserving base atomspace)
 */
COGUTIL_API void fractal_atomspace_destroy(
    fractal_atomspace_t fas,
    bool preserve_base
);

/**
 * Get underlying base atomspace
 */
COGUTIL_API atomspace_t fractal_atomspace_get_base(fractal_atomspace_t fas);

/*===========================================================================
 * Fractal Atom Creation
 *===========================================================================*/

/**
 * Create a fractal node
 */
COGUTIL_API cog_result_t fractal_create_node(
    fractal_atomspace_t fas,
    fractal_type_t type,
    const char* name,
    atom_handle_t parent,
    const fractal_properties_t* properties,
    fractal_atom_t** atom
);

/**
 * Create a fractal link
 */
COGUTIL_API cog_result_t fractal_create_link(
    fractal_atomspace_t fas,
    fractal_type_t type,
    const atom_handle_t* outgoing,
    size_t outgoing_count,
    atom_handle_t parent,
    const fractal_properties_t* properties,
    fractal_atom_t** atom
);

/**
 * Create a recursive (self-referential) atom
 */
COGUTIL_API cog_result_t fractal_create_recursive(
    fractal_atomspace_t fas,
    fractal_type_t type,
    const char* name,
    fractal_atom_t** atom
);

/**
 * Create a generator atom (lazy child generation)
 */
COGUTIL_API cog_result_t fractal_create_generator(
    fractal_atomspace_t fas,
    fractal_type_t type,
    const char* name,
    fractal_generator_fn generator,
    void* context,
    fractal_atom_t** atom
);

/*===========================================================================
 * Hierarchy Operations
 *===========================================================================*/

/**
 * Add child atom to parent
 */
COGUTIL_API cog_result_t fractal_add_child(
    fractal_atomspace_t fas,
    atom_handle_t parent,
    atom_handle_t child
);

/**
 * Remove child from parent
 */
COGUTIL_API cog_result_t fractal_remove_child(
    fractal_atomspace_t fas,
    atom_handle_t parent,
    atom_handle_t child
);

/**
 * Move atom to new parent
 */
COGUTIL_API cog_result_t fractal_reparent(
    fractal_atomspace_t fas,
    atom_handle_t atom,
    atom_handle_t new_parent
);

/**
 * Get fractal atom by handle
 */
COGUTIL_API cog_result_t fractal_get_atom(
    fractal_atomspace_t fas,
    atom_handle_t handle,
    fractal_atom_t** atom
);

/**
 * Get parent atom
 */
COGUTIL_API atom_handle_t fractal_get_parent(
    fractal_atomspace_t fas,
    atom_handle_t handle
);

/**
 * Get children atoms
 */
COGUTIL_API cog_result_t fractal_get_children(
    fractal_atomspace_t fas,
    atom_handle_t handle,
    atom_handle_t** children,
    size_t* count
);

/**
 * Get all ancestors (path to root)
 */
COGUTIL_API cog_result_t fractal_get_ancestors(
    fractal_atomspace_t fas,
    atom_handle_t handle,
    atom_handle_t** ancestors,
    size_t* count
);

/**
 * Get all descendants (recursive children)
 */
COGUTIL_API cog_result_t fractal_get_descendants(
    fractal_atomspace_t fas,
    atom_handle_t handle,
    uint32_t max_depth,
    atom_handle_t** descendants,
    size_t* count
);

/**
 * Get siblings (same parent)
 */
COGUTIL_API cog_result_t fractal_get_siblings(
    fractal_atomspace_t fas,
    atom_handle_t handle,
    atom_handle_t** siblings,
    size_t* count
);

/*===========================================================================
 * Scale Operations
 *===========================================================================*/

/**
 * Get depth in hierarchy
 */
COGUTIL_API uint32_t fractal_get_depth(
    fractal_atomspace_t fas,
    atom_handle_t handle
);

/**
 * Get scale factor relative to root
 */
COGUTIL_API float fractal_get_absolute_scale(
    fractal_atomspace_t fas,
    atom_handle_t handle
);

/**
 * Get atoms at specific depth level
 */
COGUTIL_API cog_result_t fractal_get_at_depth(
    fractal_atomspace_t fas,
    uint32_t depth,
    atom_handle_t** atoms,
    size_t* count
);

/**
 * Get atoms within scale range
 */
COGUTIL_API cog_result_t fractal_get_in_scale_range(
    fractal_atomspace_t fas,
    float min_scale,
    float max_scale,
    atom_handle_t** atoms,
    size_t* count
);

/*===========================================================================
 * Fractal Properties
 *===========================================================================*/

/**
 * Set fractal properties
 */
COGUTIL_API cog_result_t fractal_set_properties(
    fractal_atomspace_t fas,
    atom_handle_t handle,
    const fractal_properties_t* properties
);

/**
 * Get fractal properties
 */
COGUTIL_API cog_result_t fractal_get_properties(
    fractal_atomspace_t fas,
    atom_handle_t handle,
    fractal_properties_t* properties
);

/**
 * Calculate fractal dimension
 */
COGUTIL_API cog_result_t fractal_calculate_dimension(
    fractal_atomspace_t fas,
    atom_handle_t root,
    fractal_dimension_method_t method,
    float* dimension
);

/**
 * Check if atoms are self-similar
 */
COGUTIL_API cog_result_t fractal_check_self_similarity(
    fractal_atomspace_t fas,
    atom_handle_t atom1,
    atom_handle_t atom2,
    float* similarity_score
);

/*===========================================================================
 * Truth Value Propagation
 *===========================================================================*/

/**
 * Propagate truth values down hierarchy
 */
COGUTIL_API cog_result_t fractal_propagate_tv_down(
    fractal_atomspace_t fas,
    atom_handle_t root,
    uint32_t max_depth
);

/**
 * Propagate truth values up hierarchy
 */
COGUTIL_API cog_result_t fractal_propagate_tv_up(
    fractal_atomspace_t fas,
    atom_handle_t leaf
);

/**
 * Aggregate truth values from children
 */
COGUTIL_API cog_result_t fractal_aggregate_tv(
    fractal_atomspace_t fas,
    atom_handle_t parent,
    truth_value_t* aggregated
);

/**
 * Set inherited truth value
 */
COGUTIL_API cog_result_t fractal_set_inherited_tv(
    fractal_atomspace_t fas,
    atom_handle_t handle,
    truth_value_t tv,
    float weight
);

/*===========================================================================
 * Attention Propagation
 *===========================================================================*/

/**
 * Propagate attention down hierarchy
 */
COGUTIL_API cog_result_t fractal_propagate_attention_down(
    fractal_atomspace_t fas,
    atom_handle_t root,
    uint32_t max_depth
);

/**
 * Propagate attention up hierarchy
 */
COGUTIL_API cog_result_t fractal_propagate_attention_up(
    fractal_atomspace_t fas,
    atom_handle_t leaf
);

/**
 * Focus attention on subtree
 */
COGUTIL_API cog_result_t fractal_focus_subtree(
    fractal_atomspace_t fas,
    atom_handle_t root,
    int16_t sti_boost
);

/*===========================================================================
 * Traversal
 *===========================================================================*/

/**
 * Traverse fractal hierarchy depth-first
 */
COGUTIL_API cog_result_t fractal_traverse_depth_first(
    fractal_atomspace_t fas,
    atom_handle_t root,
    uint32_t max_depth,
    fractal_visitor_fn visitor,
    void* user_data
);

/**
 * Traverse fractal hierarchy breadth-first
 */
COGUTIL_API cog_result_t fractal_traverse_breadth_first(
    fractal_atomspace_t fas,
    atom_handle_t root,
    uint32_t max_depth,
    fractal_visitor_fn visitor,
    void* user_data
);

/**
 * Traverse by scale (smallest to largest or vice versa)
 */
COGUTIL_API cog_result_t fractal_traverse_by_scale(
    fractal_atomspace_t fas,
    atom_handle_t root,
    bool ascending,
    fractal_visitor_fn visitor,
    void* user_data
);

/*===========================================================================
 * Pattern Matching
 *===========================================================================*/

/**
 * Match pattern at all scales
 */
COGUTIL_API cog_result_t fractal_pattern_match_all_scales(
    fractal_atomspace_t fas,
    atom_handle_t pattern,
    atom_handle_t** matches,
    size_t* count
);

/**
 * Match pattern at specific depth
 */
COGUTIL_API cog_result_t fractal_pattern_match_at_depth(
    fractal_atomspace_t fas,
    atom_handle_t pattern,
    uint32_t depth,
    atom_handle_t** matches,
    size_t* count
);

/**
 * Find self-similar substructures
 */
COGUTIL_API cog_result_t fractal_find_self_similar(
    fractal_atomspace_t fas,
    atom_handle_t root,
    float min_similarity,
    atom_handle_t** matches,
    float** similarities,
    size_t* count
);

/*===========================================================================
 * Generation and Expansion
 *===========================================================================*/

/**
 * Expand generator to specified depth
 */
COGUTIL_API cog_result_t fractal_expand_generator(
    fractal_atomspace_t fas,
    atom_handle_t generator,
    uint32_t target_depth
);

/**
 * Collapse subtree (remove generated children)
 */
COGUTIL_API cog_result_t fractal_collapse_subtree(
    fractal_atomspace_t fas,
    atom_handle_t root,
    uint32_t keep_depth
);

/**
 * Clone subtree at new location
 */
COGUTIL_API cog_result_t fractal_clone_subtree(
    fractal_atomspace_t fas,
    atom_handle_t source_root,
    atom_handle_t dest_parent,
    uint32_t max_depth,
    atom_handle_t* new_root
);

/*===========================================================================
 * Persistence
 *===========================================================================*/

/**
 * Save fractal atomspace to file
 */
COGUTIL_API cog_result_t fractal_save(
    fractal_atomspace_t fas,
    const char* path
);

/**
 * Load fractal atomspace from file
 */
COGUTIL_API cog_result_t fractal_load(
    fractal_atomspace_t fas,
    const char* path
);

/**
 * Export subtree to Scheme expression
 */
COGUTIL_API cog_result_t fractal_export_scheme(
    fractal_atomspace_t fas,
    atom_handle_t root,
    uint32_t max_depth,
    char** scheme_expr
);

/*===========================================================================
 * Statistics
 *===========================================================================*/

/**
 * Get fractal atomspace statistics
 */
COGUTIL_API cog_result_t fractal_get_stats(
    fractal_atomspace_t fas,
    fractal_stats_t* stats
);

/**
 * Reset statistics counters
 */
COGUTIL_API void fractal_reset_stats(fractal_atomspace_t fas);

/*===========================================================================
 * Memory Management
 *===========================================================================*/

/**
 * Free fractal atom (does not remove from atomspace)
 */
COGUTIL_API void fractal_atom_free(fractal_atom_t* atom);

/**
 * Free array of handles
 */
COGUTIL_API void fractal_handles_free(atom_handle_t* handles);

/**
 * Prune atoms below minimum scale
 */
COGUTIL_API cog_result_t fractal_prune_by_scale(
    fractal_atomspace_t fas,
    float min_scale
);

/**
 * Compact fractal atomspace (remove unused atoms)
 */
COGUTIL_API cog_result_t fractal_compact(fractal_atomspace_t fas);

#ifdef __cplusplus
}
#endif

#endif /* _COGWXP_FRACTAL_ATOMS_H_ */
