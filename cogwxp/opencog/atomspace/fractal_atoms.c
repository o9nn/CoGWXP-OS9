/**
 * @file fractal_atoms.c
 * @brief Fractal AtomSpace Implementation - Self-similar recursive hypergraph structures
 */

#include "fractal_atoms.h"
#include "atomspace.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

/* Internal fractal atomspace structure */
struct fractal_atomspace {
    atomspace_t* base;
    fractal_config_t config;
    fractal_stats_t stats;
    fractal_atom_t** atoms;
    size_t atom_count;
    size_t atom_capacity;
    bool owns_base;
};

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

COGUTIL_API fractal_atomspace_t* fractal_atomspace_create(const fractal_config_t* config) {
    fractal_atomspace_t* fas = calloc(1, sizeof(fractal_atomspace_t));
    if (!fas) return NULL;
    
    if (config) {
        fas->config = *config;
    } else {
        fas->config.max_depth = FRACTAL_MAX_DEPTH;
        fas->config.default_dimension = FRACTAL_DEFAULT_DIMENSION;
        fas->config.scale_decay = FRACTAL_DEFAULT_SCALE_DECAY;
        fas->config.tv_inheritance = FRACTAL_TV_INHERITANCE;
        fas->config.propagation_mode = FRACTAL_PROPAGATION_INHERIT;
        fas->config.enable_self_reference = true;
        fas->config.enable_generators = true;
    }
    
    fas->base = atomspace_create();
    fas->owns_base = true;
    fas->atom_capacity = 1024;
    fas->atoms = calloc(fas->atom_capacity, sizeof(fractal_atom_t*));
    
    return fas;
}

COGUTIL_API fractal_atomspace_t* fractal_atomspace_create_with_base(atomspace_t* base, const fractal_config_t* config) {
    if (!base) return NULL;
    fractal_atomspace_t* fas = fractal_atomspace_create(config);
    if (!fas) return NULL;
    
    if (fas->owns_base && fas->base) {
        atomspace_destroy(fas->base);
    }
    fas->base = base;
    fas->owns_base = false;
    return fas;
}

COGUTIL_API void fractal_atomspace_destroy(fractal_atomspace_t* fas) {
    if (!fas) return;
    
    for (size_t i = 0; i < fas->atom_count; i++) {
        if (fas->atoms[i]) {
            free(fas->atoms[i]->children);
            free(fas->atoms[i]->siblings);
            free(fas->atoms[i]);
        }
    }
    free(fas->atoms);
    
    if (fas->owns_base && fas->base) {
        atomspace_destroy(fas->base);
    }
    free(fas);
}

COGUTIL_API atomspace_t* fractal_atomspace_get_base(fractal_atomspace_t* fas) {
    return fas ? fas->base : NULL;
}

/* ============================================================================
 * Atom Creation Functions
 * ============================================================================ */

static fractal_atom_t* alloc_fractal_atom(fractal_atomspace_t* fas) {
    if (fas->atom_count >= fas->atom_capacity) {
        size_t new_cap = fas->atom_capacity * 2;
        fractal_atom_t** new_atoms = realloc(fas->atoms, new_cap * sizeof(fractal_atom_t*));
        if (!new_atoms) return NULL;
        fas->atoms = new_atoms;
        fas->atom_capacity = new_cap;
    }
    
    fractal_atom_t* atom = calloc(1, sizeof(fractal_atom_t));
    if (!atom) return NULL;
    
    fas->atoms[fas->atom_count++] = atom;
    fas->stats.total_atoms++;
    return atom;
}

COGUTIL_API atom_handle_t fractal_create_node(fractal_atomspace_t* fas, fractal_type_t type,
                                              const char* name, const fractal_properties_t* props) {
    if (!fas || type < FRACTAL_TYPE_NODE || type >= FRACTAL_TYPE_LINK) return ATOM_HANDLE_INVALID;
    
    fractal_atom_t* atom = alloc_fractal_atom(fas);
    if (!atom) return ATOM_HANDLE_INVALID;
    
    atom->handle = atomspace_add_node(fas->base, (atom_type_t)type, name);
    atom->parent = ATOM_HANDLE_INVALID;
    
    if (props) {
        atom->properties = *props;
    } else {
        atom->properties.depth = 0;
        atom->properties.fractal_dimension = fas->config.default_dimension;
        atom->properties.scale_factor = 1.0f;
        atom->properties.scale_type = FRACTAL_SCALE_LINEAR;
        atom->properties.propagation = fas->config.propagation_mode;
    }
    
    if (atom->properties.depth == 0) fas->stats.root_count++;
    fas->stats.max_depth = (atom->properties.depth > fas->stats.max_depth) ? 
                           atom->properties.depth : fas->stats.max_depth;
    
    return atom->handle;
}

COGUTIL_API atom_handle_t fractal_create_link(fractal_atomspace_t* fas, fractal_type_t type,
                                               const atom_handle_t* outgoing, size_t arity,
                                               const fractal_properties_t* props) {
    if (!fas || !outgoing || arity == 0) return ATOM_HANDLE_INVALID;
    if (type < FRACTAL_TYPE_LINK || type >= FRACTAL_TYPE_META) return ATOM_HANDLE_INVALID;
    
    fractal_atom_t* atom = alloc_fractal_atom(fas);
    if (!atom) return ATOM_HANDLE_INVALID;
    
    atom->handle = atomspace_add_link(fas->base, (atom_type_t)type, outgoing, arity);
    atom->parent = ATOM_HANDLE_INVALID;
    
    if (props) {
        atom->properties = *props;
    } else {
        atom->properties.depth = 0;
        atom->properties.fractal_dimension = fas->config.default_dimension;
        atom->properties.scale_factor = 1.0f;
    }
    
    return atom->handle;
}

COGUTIL_API atom_handle_t fractal_create_recursive(fractal_atomspace_t* fas, fractal_type_t type,
                                                    atom_handle_t self_ref, const fractal_properties_t* props) {
    if (!fas) return ATOM_HANDLE_INVALID;
    if (!fas->config.enable_self_reference) return ATOM_HANDLE_INVALID;
    
    fractal_atom_t* atom = alloc_fractal_atom(fas);
    if (!atom) return ATOM_HANDLE_INVALID;
    
    atom->handle = atomspace_add_node(fas->base, (atom_type_t)FRACTAL_TYPE_RECURSIVE_NODE, "recursive");
    atom->properties.is_self_referential = true;
    atom->properties.self_reference_handle = self_ref;
    
    if (props) {
        atom->properties.depth = props->depth;
        atom->properties.fractal_dimension = props->fractal_dimension;
        atom->properties.scale_factor = props->scale_factor;
    }
    
    return atom->handle;
}

/* ============================================================================
 * Hierarchy Operations
 * ============================================================================ */

static fractal_atom_t* find_atom(fractal_atomspace_t* fas, atom_handle_t handle) {
    for (size_t i = 0; i < fas->atom_count; i++) {
        if (fas->atoms[i] && fas->atoms[i]->handle == handle) {
            return fas->atoms[i];
        }
    }
    return NULL;
}

COGUTIL_API cog_result_t fractal_add_child(fractal_atomspace_t* fas, atom_handle_t parent,
                                           atom_handle_t child, float scale_factor) {
    if (!fas) return COG_ERROR_INVALID_PARAM;
    
    fractal_atom_t* p = find_atom(fas, parent);
    fractal_atom_t* c = find_atom(fas, child);
    if (!p || !c) return COG_ERROR_NOT_FOUND;
    
    if (c->properties.depth + 1 >= fas->config.max_depth) return COG_ERROR_DEPTH_EXCEEDED;
    
    /* Check for duplicate - prevent adding same child twice */
    for (size_t i = 0; i < p->child_count; i++) {
        if (p->children[i] == child) {
            return COG_SUCCESS;  /* Child already exists, idempotent success */
        }
    }
    
    /* Expand children array */
    atom_handle_t* new_children = realloc(p->children, (p->child_count + 1) * sizeof(atom_handle_t));
    if (!new_children) return COG_ERROR_MEMORY;
    p->children = new_children;
    p->children[p->child_count++] = child;
    
    c->parent = parent;
    c->properties.depth = p->properties.depth + 1;
    c->properties.scale_factor = scale_factor > 0 ? scale_factor : fas->config.scale_decay;
    
    fas->stats.max_depth = (c->properties.depth > fas->stats.max_depth) ?
                           c->properties.depth : fas->stats.max_depth;
    
    return COG_SUCCESS;
}

COGUTIL_API cog_result_t fractal_remove_child(fractal_atomspace_t* fas, atom_handle_t parent, atom_handle_t child) {
    if (!fas) return COG_ERROR_INVALID_PARAM;
    
    fractal_atom_t* p = find_atom(fas, parent);
    if (!p) return COG_ERROR_NOT_FOUND;
    
    for (size_t i = 0; i < p->child_count; i++) {
        if (p->children[i] == child) {
            memmove(&p->children[i], &p->children[i+1], (p->child_count - i - 1) * sizeof(atom_handle_t));
            p->child_count--;
            
            fractal_atom_t* c = find_atom(fas, child);
            if (c) c->parent = ATOM_HANDLE_INVALID;
            return COG_SUCCESS;
        }
    }
    return COG_ERROR_NOT_FOUND;
}

COGUTIL_API atom_handle_t fractal_get_parent(fractal_atomspace_t* fas, atom_handle_t handle) {
    if (!fas) return ATOM_HANDLE_INVALID;
    fractal_atom_t* atom = find_atom(fas, handle);
    return atom ? atom->parent : ATOM_HANDLE_INVALID;
}

COGUTIL_API size_t fractal_get_children(fractal_atomspace_t* fas, atom_handle_t handle,
                                        atom_handle_t* buffer, size_t buffer_size) {
    if (!fas || !buffer) return 0;
    fractal_atom_t* atom = find_atom(fas, handle);
    if (!atom) return 0;
    
    size_t count = (atom->child_count < buffer_size) ? atom->child_count : buffer_size;
    memcpy(buffer, atom->children, count * sizeof(atom_handle_t));
    return count;
}

COGUTIL_API size_t fractal_get_child_count(fractal_atomspace_t* fas, atom_handle_t handle) {
    if (!fas) return 0;
    fractal_atom_t* atom = find_atom(fas, handle);
    return atom ? atom->child_count : 0;
}

COGUTIL_API uint32_t fractal_get_depth(fractal_atomspace_t* fas, atom_handle_t handle) {
    if (!fas) return 0;
    fractal_atom_t* atom = find_atom(fas, handle);
    return atom ? atom->properties.depth : 0;
}

/* ============================================================================
 * Scale Operations
 * ============================================================================ */

COGUTIL_API float fractal_get_scale(fractal_atomspace_t* fas, atom_handle_t handle) {
    if (!fas) return 1.0f;
    fractal_atom_t* atom = find_atom(fas, handle);
    return atom ? atom->properties.scale_factor : 1.0f;
}

COGUTIL_API float fractal_get_absolute_scale(fractal_atomspace_t* fas, atom_handle_t handle) {
    if (!fas) return 1.0f;
    
    float scale = 1.0f;
    atom_handle_t current = handle;
    
    while (current != ATOM_HANDLE_INVALID) {
        fractal_atom_t* atom = find_atom(fas, current);
        if (!atom) break;
        scale *= atom->properties.scale_factor;
        current = atom->parent;
    }
    return scale;
}

COGUTIL_API cog_result_t fractal_set_scale(fractal_atomspace_t* fas, atom_handle_t handle, float scale) {
    if (!fas || scale < FRACTAL_MIN_SCALE) return COG_ERROR_INVALID_PARAM;
    fractal_atom_t* atom = find_atom(fas, handle);
    if (!atom) return COG_ERROR_NOT_FOUND;
    atom->properties.scale_factor = scale;
    return COG_SUCCESS;
}

/* ============================================================================
 * Truth Value Propagation
 * ============================================================================ */

COGUTIL_API cog_result_t fractal_propagate_tv_down(fractal_atomspace_t* fas, atom_handle_t root) {
    if (!fas) return COG_ERROR_INVALID_PARAM;
    
    fractal_atom_t* atom = find_atom(fas, root);
    if (!atom) return COG_ERROR_NOT_FOUND;
    
    truth_value_t tv = atomspace_get_tv(fas->base, root);
    float inheritance = fas->config.tv_inheritance;
    
    for (size_t i = 0; i < atom->child_count; i++) {
        fractal_atom_t* child = find_atom(fas, atom->children[i]);
        if (child) {
            child->properties.inherited_tv.strength = tv.strength * inheritance;
            child->properties.inherited_tv.confidence = tv.confidence * inheritance;
            fractal_propagate_tv_down(fas, atom->children[i]);
        }
    }
    return COG_SUCCESS;
}

COGUTIL_API cog_result_t fractal_propagate_tv_up(fractal_atomspace_t* fas, atom_handle_t leaf) {
    if (!fas) return COG_ERROR_INVALID_PARAM;
    
    fractal_atom_t* atom = find_atom(fas, leaf);
    if (!atom || atom->parent == ATOM_HANDLE_INVALID) return COG_SUCCESS;
    
    fractal_atom_t* parent = find_atom(fas, atom->parent);
    if (!parent) return COG_ERROR_NOT_FOUND;
    
    /* Aggregate children TVs */
    float total_strength = 0, total_confidence = 0;
    for (size_t i = 0; i < parent->child_count; i++) {
        truth_value_t child_tv = atomspace_get_tv(fas->base, parent->children[i]);
        total_strength += child_tv.strength;
        total_confidence += child_tv.confidence;
    }
    
    if (parent->child_count > 0) {
        truth_value_t new_tv = {
            .strength = total_strength / parent->child_count,
            .confidence = total_confidence / parent->child_count
        };
        atomspace_set_tv(fas->base, atom->parent, new_tv);
    }
    
    return fractal_propagate_tv_up(fas, atom->parent);
}

/* ============================================================================
 * Traversal Functions
 * ============================================================================ */

COGUTIL_API cog_result_t fractal_traverse_depth_first(fractal_atomspace_t* fas, atom_handle_t root,
                                                       fractal_visitor_fn visitor, void* user_data) {
    if (!fas || !visitor) return COG_ERROR_INVALID_PARAM;
    
    fractal_atom_t* atom = find_atom(fas, root);
    if (!atom) return COG_ERROR_NOT_FOUND;
    
    if (!visitor(fas, root, user_data)) return COG_SUCCESS;
    
    for (size_t i = 0; i < atom->child_count; i++) {
        fractal_traverse_depth_first(fas, atom->children[i], visitor, user_data);
    }
    return COG_SUCCESS;
}

COGUTIL_API cog_result_t fractal_traverse_by_scale(fractal_atomspace_t* fas, atom_handle_t root,
                                                    float min_scale, float max_scale,
                                                    fractal_visitor_fn visitor, void* user_data) {
    if (!fas || !visitor) return COG_ERROR_INVALID_PARAM;
    
    fractal_atom_t* atom = find_atom(fas, root);
    if (!atom) return COG_ERROR_NOT_FOUND;
    
    float abs_scale = fractal_get_absolute_scale(fas, root);
    if (abs_scale >= min_scale && abs_scale <= max_scale) {
        if (!visitor(fas, root, user_data)) return COG_SUCCESS;
    }
    
    for (size_t i = 0; i < atom->child_count; i++) {
        fractal_traverse_by_scale(fas, atom->children[i], min_scale, max_scale, visitor, user_data);
    }
    return COG_SUCCESS;
}

/* ============================================================================
 * Statistics
 * ============================================================================ */

COGUTIL_API cog_result_t fractal_get_stats(fractal_atomspace_t* fas, fractal_stats_t* stats) {
    if (!fas || !stats) return COG_ERROR_INVALID_PARAM;
    *stats = fas->stats;
    return COG_SUCCESS;
}

COGUTIL_API float fractal_compute_dimension(fractal_atomspace_t* fas, atom_handle_t handle) {
    if (!fas) return 0.0f;
    fractal_atom_t* atom = find_atom(fas, handle);
    if (!atom || atom->child_count == 0) return 0.0f;
    
    /* Simplified box-counting dimension estimate */
    return log((float)atom->child_count) / log(1.0f / atom->properties.scale_factor);
}

/* ============================================================================
 * PHASE 2: HIERARCHY NAVIGATION FUNCTIONS
 * ============================================================================ */

COGUTIL_API atom_handle_t* fractal_get_ancestors(fractal_atomspace_t* fas, atom_handle_t handle, size_t* count) {
    if (!fas || !count) return NULL;
    *count = 0;
    
    fractal_atom_t* atom = find_atom(fas, handle);
    if (!atom) return NULL;
    
    /* Count ancestors first */
    size_t ancestor_count = 0;
    fractal_atom_t* current = atom;
    while (current->parent != ATOM_HANDLE_INVALID) {
        ancestor_count++;
        current = find_atom(fas, current->parent);
        if (!current) break;
    }
    
    if (ancestor_count == 0) return NULL;
    
    atom_handle_t* ancestors = (atom_handle_t*)malloc(ancestor_count * sizeof(atom_handle_t));
    if (!ancestors) return NULL;
    
    /* Populate ancestors array (from immediate parent to root) */
    current = atom;
    for (size_t i = 0; i < ancestor_count; i++) {
        ancestors[i] = current->parent;
        current = find_atom(fas, current->parent);
        if (!current) break;
    }
    
    *count = ancestor_count;
    return ancestors;
}

/* Helper for recursive descendant collection */
static void collect_descendants(fractal_atomspace_t* fas, fractal_atom_t* atom, 
                                 atom_handle_t** handles, size_t* count, size_t* capacity) {
    for (size_t i = 0; i < atom->child_count; i++) {
        fractal_atom_t* child = find_atom(fas, atom->children[i]);
        if (!child) continue;
        
        /* Expand array if needed */
        if (*count >= *capacity) {
            *capacity *= 2;
            atom_handle_t* new_handles = (atom_handle_t*)realloc(*handles, *capacity * sizeof(atom_handle_t));
            if (!new_handles) return;
            *handles = new_handles;
        }
        
        (*handles)[(*count)++] = atom->children[i];
        collect_descendants(fas, child, handles, count, capacity);
    }
}

COGUTIL_API atom_handle_t* fractal_get_descendants(fractal_atomspace_t* fas, atom_handle_t handle, size_t* count) {
    if (!fas || !count) return NULL;
    *count = 0;
    
    fractal_atom_t* atom = find_atom(fas, handle);
    if (!atom || atom->child_count == 0) return NULL;
    
    size_t capacity = 64;
    atom_handle_t* handles = (atom_handle_t*)malloc(capacity * sizeof(atom_handle_t));
    if (!handles) return NULL;
    
    collect_descendants(fas, atom, &handles, count, &capacity);
    
    if (*count == 0) {
        free(handles);
        return NULL;
    }
    
    return handles;
}

COGUTIL_API atom_handle_t* fractal_get_siblings(fractal_atomspace_t* fas, atom_handle_t handle, size_t* count) {
    if (!fas || !count) return NULL;
    *count = 0;
    
    fractal_atom_t* atom = find_atom(fas, handle);
    if (!atom || atom->parent == ATOM_HANDLE_INVALID) return NULL;
    
    fractal_atom_t* parent = find_atom(fas, atom->parent);
    if (!parent || parent->child_count <= 1) return NULL;
    
    /* Allocate for siblings (excluding self) */
    atom_handle_t* siblings = (atom_handle_t*)malloc((parent->child_count - 1) * sizeof(atom_handle_t));
    if (!siblings) return NULL;
    
    size_t sibling_count = 0;
    for (size_t i = 0; i < parent->child_count; i++) {
        if (parent->children[i] != handle) {
            siblings[sibling_count++] = parent->children[i];
        }
    }
    
    *count = sibling_count;
    return siblings;
}

COGUTIL_API cog_result_t fractal_reparent(fractal_atomspace_t* fas, atom_handle_t handle, atom_handle_t new_parent) {
    if (!fas) return COG_ERROR_INVALID_PARAM;
    
    fractal_atom_t* atom = find_atom(fas, handle);
    if (!atom) return COG_ERROR_NOT_FOUND;
    
    /* Remove from old parent */
    if (atom->parent != ATOM_HANDLE_INVALID) {
        fractal_remove_child(fas, atom->parent, handle);
    }
    
    /* Add to new parent */
    if (new_parent != ATOM_HANDLE_INVALID) {
        cog_result_t result = fractal_add_child(fas, new_parent, handle);
        if (result != COG_SUCCESS) return result;
    }
    
    atom->parent = new_parent;
    
    /* Recalculate depth and absolute scale */
    atom->depth = (new_parent == ATOM_HANDLE_INVALID) ? 0 : fractal_get_depth(fas, new_parent) + 1;
    atom->absolute_scale = fractal_get_absolute_scale(fas, handle);
    
    return COG_SUCCESS;
}

/* ============================================================================
 * PHASE 2: PROPERTY MANAGEMENT FUNCTIONS
 * ============================================================================ */

COGUTIL_API cog_result_t fractal_get_properties(fractal_atomspace_t* fas, atom_handle_t handle, 
                                                  fractal_properties_t* props) {
    if (!fas || !props) return COG_ERROR_INVALID_PARAM;
    
    fractal_atom_t* atom = find_atom(fas, handle);
    if (!atom) return COG_ERROR_NOT_FOUND;
    
    *props = atom->properties;
    return COG_SUCCESS;
}

COGUTIL_API cog_result_t fractal_set_properties(fractal_atomspace_t* fas, atom_handle_t handle, 
                                                  const fractal_properties_t* props) {
    if (!fas || !props) return COG_ERROR_INVALID_PARAM;
    
    fractal_atom_t* atom = find_atom(fas, handle);
    if (!atom) return COG_ERROR_NOT_FOUND;
    
    atom->properties = *props;
    return COG_SUCCESS;
}

COGUTIL_API float fractal_calculate_dimension(fractal_atomspace_t* fas, atom_handle_t handle) {
    if (!fas) return 0.0f;
    
    fractal_atom_t* atom = find_atom(fas, handle);
    if (!atom) return 0.0f;
    
    /* Use box-counting method: D = log(N) / log(1/r) */
    if (atom->child_count == 0) return 0.0f;
    
    float scale_factor = atom->properties.scale_factor;
    if (scale_factor <= 0.0f || scale_factor >= 1.0f) {
        scale_factor = FRACTAL_DEFAULT_SCALE_DECAY;
    }
    
    return log((float)atom->child_count) / log(1.0f / scale_factor);
}

COGUTIL_API bool fractal_check_self_similarity(fractal_atomspace_t* fas, atom_handle_t handle, float tolerance) {
    if (!fas) return false;
    
    fractal_atom_t* atom = find_atom(fas, handle);
    if (!atom || atom->child_count == 0) return false;
    
    /* Check if structure is self-similar by comparing dimension ratios across levels */
    float parent_dim = fractal_calculate_dimension(fas, handle);
    
    for (size_t i = 0; i < atom->child_count; i++) {
        fractal_atom_t* child = find_atom(fas, atom->children[i]);
        if (!child || child->child_count == 0) continue;
        
        float child_dim = fractal_calculate_dimension(fas, atom->children[i]);
        float ratio = (parent_dim != 0.0f) ? fabs(child_dim - parent_dim) / parent_dim : fabs(child_dim);
        
        if (ratio > tolerance) return false;
    }
    
    return true;
}

/* ============================================================================
 * PHASE 2: ATTENTION PROPAGATION FUNCTIONS
 * ============================================================================ */

COGUTIL_API cog_result_t fractal_propagate_attention_down(fractal_atomspace_t* fas, atom_handle_t handle, 
                                                            float sti_decay, float lti_decay) {
    if (!fas) return COG_ERROR_INVALID_PARAM;
    
    fractal_atom_t* atom = find_atom(fas, handle);
    if (!atom) return COG_ERROR_NOT_FOUND;
    
    /* Propagate attention values to children with decay */
    for (size_t i = 0; i < atom->child_count; i++) {
        fractal_atom_t* child = find_atom(fas, atom->children[i]);
        if (!child) continue;
        
        child->properties.sti = atom->properties.sti * sti_decay;
        child->properties.lti = atom->properties.lti * lti_decay;
        
        /* Recursive propagation */
        fractal_propagate_attention_down(fas, atom->children[i], sti_decay, lti_decay);
    }
    
    return COG_SUCCESS;
}

COGUTIL_API cog_result_t fractal_propagate_attention_up(fractal_atomspace_t* fas, atom_handle_t handle) {
    if (!fas) return COG_ERROR_INVALID_PARAM;
    
    fractal_atom_t* atom = find_atom(fas, handle);
    if (!atom || atom->parent == ATOM_HANDLE_INVALID) return COG_ERROR_NOT_FOUND;
    
    fractal_atom_t* parent = find_atom(fas, atom->parent);
    if (!parent) return COG_ERROR_NOT_FOUND;
    
    /* Aggregate STI/LTI from all children */
    float total_sti = 0.0f;
    float total_lti = 0.0f;
    
    for (size_t i = 0; i < parent->child_count; i++) {
        fractal_atom_t* sibling = find_atom(fas, parent->children[i]);
        if (!sibling) continue;
        total_sti += sibling->properties.sti;
        total_lti += sibling->properties.lti;
    }
    
    parent->properties.sti = total_sti / (float)parent->child_count;
    parent->properties.lti = total_lti / (float)parent->child_count;
    
    /* Continue propagation upward */
    if (parent->parent != ATOM_HANDLE_INVALID) {
        return fractal_propagate_attention_up(fas, atom->parent);
    }
    
    return COG_SUCCESS;
}

COGUTIL_API cog_result_t fractal_focus_subtree(fractal_atomspace_t* fas, atom_handle_t handle, float boost) {
    if (!fas) return COG_ERROR_INVALID_PARAM;
    
    fractal_atom_t* atom = find_atom(fas, handle);
    if (!atom) return COG_ERROR_NOT_FOUND;
    
    /* Boost attention for this atom and all descendants */
    atom->properties.sti *= boost;
    atom->properties.lti *= boost;
    
    for (size_t i = 0; i < atom->child_count; i++) {
        fractal_focus_subtree(fas, atom->children[i], boost);
    }
    
    return COG_SUCCESS;
}

/* ============================================================================
 * PHASE 2: BREADTH-FIRST TRAVERSAL
 * ============================================================================ */

COGUTIL_API cog_result_t fractal_traverse_breadth_first(fractal_atomspace_t* fas, atom_handle_t root,
                                                          fractal_visitor_fn visitor, void* user_data) {
    if (!fas || !visitor) return COG_ERROR_INVALID_PARAM;
    
    fractal_atom_t* root_atom = find_atom(fas, root);
    if (!root_atom) return COG_ERROR_NOT_FOUND;
    
    /* Simple queue implementation using dynamic array */
    size_t queue_capacity = 64;
    size_t queue_head = 0;
    size_t queue_tail = 0;
    atom_handle_t* queue = (atom_handle_t*)malloc(queue_capacity * sizeof(atom_handle_t));
    if (!queue) return COG_ERROR_OUT_OF_MEMORY;
    
    queue[queue_tail++] = root;
    
    while (queue_head < queue_tail) {
        atom_handle_t current_handle = queue[queue_head++];
        fractal_atom_t* current = find_atom(fas, current_handle);
        if (!current) continue;
        
        /* Visit current node */
        if (!visitor(fas, current_handle, current->depth, user_data)) {
            free(queue);
            return COG_SUCCESS; /* Early termination requested */
        }
        
        /* Enqueue children */
        for (size_t i = 0; i < current->child_count; i++) {
            /* Expand queue if needed */
            if (queue_tail >= queue_capacity) {
                queue_capacity *= 2;
                atom_handle_t* new_queue = (atom_handle_t*)realloc(queue, queue_capacity * sizeof(atom_handle_t));
                if (!new_queue) {
                    free(queue);
                    return COG_ERROR_OUT_OF_MEMORY;
                }
                queue = new_queue;
            }
            queue[queue_tail++] = current->children[i];
        }
    }
    
    free(queue);
    return COG_SUCCESS;
}

/* ============================================================================
 * PHASE 2: PATTERN MATCHING FUNCTIONS
 * ============================================================================ */

/* Helper to check if atom matches pattern based on type and TV */
static bool atom_matches_pattern(fractal_atom_t* atom, atom_type_t type, const truth_value_t* min_tv) {
    if (atom->atom_type != type) return false;
    if (min_tv) {
        if (atom->properties.base_tv.strength < min_tv->strength) return false;
        if (atom->properties.base_tv.confidence < min_tv->confidence) return false;
    }
    return true;
}

COGUTIL_API atom_handle_t* fractal_pattern_match_all_scales(fractal_atomspace_t* fas, atom_type_t type,
                                                              const truth_value_t* min_tv, size_t* count) {
    if (!fas || !count) return NULL;
    *count = 0;
    
    size_t capacity = 64;
    atom_handle_t* matches = (atom_handle_t*)malloc(capacity * sizeof(atom_handle_t));
    if (!matches) return NULL;
    
    /* Scan all atoms for matches */
    for (size_t i = 0; i < fas->atom_count; i++) {
        if (!fas->atoms[i]) continue;
        if (!atom_matches_pattern(fas->atoms[i], type, min_tv)) continue;
        
        if (*count >= capacity) {
            capacity *= 2;
            atom_handle_t* new_matches = (atom_handle_t*)realloc(matches, capacity * sizeof(atom_handle_t));
            if (!new_matches) {
                free(matches);
                return NULL;
            }
            matches = new_matches;
        }
        matches[(*count)++] = fas->atoms[i]->handle;
    }
    
    if (*count == 0) {
        free(matches);
        return NULL;
    }
    
    return matches;
}

COGUTIL_API atom_handle_t* fractal_pattern_match_at_depth(fractal_atomspace_t* fas, atom_type_t type,
                                                            uint32_t depth, const truth_value_t* min_tv, 
                                                            size_t* count) {
    if (!fas || !count) return NULL;
    *count = 0;
    
    size_t capacity = 64;
    atom_handle_t* matches = (atom_handle_t*)malloc(capacity * sizeof(atom_handle_t));
    if (!matches) return NULL;
    
    /* Scan all atoms at specific depth */
    for (size_t i = 0; i < fas->atom_count; i++) {
        if (!fas->atoms[i]) continue;
        if (fas->atoms[i]->depth != depth) continue;
        if (!atom_matches_pattern(fas->atoms[i], type, min_tv)) continue;
        
        if (*count >= capacity) {
            capacity *= 2;
            atom_handle_t* new_matches = (atom_handle_t*)realloc(matches, capacity * sizeof(atom_handle_t));
            if (!new_matches) {
                free(matches);
                return NULL;
            }
            matches = new_matches;
        }
        matches[(*count)++] = fas->atoms[i]->handle;
    }
    
    if (*count == 0) {
        free(matches);
        return NULL;
    }
    
    return matches;
}

COGUTIL_API atom_handle_t* fractal_find_self_similar(fractal_atomspace_t* fas, atom_handle_t pattern,
                                                       float tolerance, size_t* count) {
    if (!fas || !count) return NULL;
    *count = 0;
    
    fractal_atom_t* pattern_atom = find_atom(fas, pattern);
    if (!pattern_atom) return NULL;
    
    float pattern_dim = fractal_calculate_dimension(fas, pattern);
    
    size_t capacity = 32;
    atom_handle_t* matches = (atom_handle_t*)malloc(capacity * sizeof(atom_handle_t));
    if (!matches) return NULL;
    
    for (size_t i = 0; i < fas->atom_count; i++) {
        if (!fas->atoms[i]) continue;
        if (fas->atoms[i]->handle == pattern) continue;
        if (fas->atoms[i]->atom_type != pattern_atom->atom_type) continue;
        
        float dim = fractal_calculate_dimension(fas, fas->atoms[i]->handle);
        float diff = fabs(dim - pattern_dim);
        
        if (diff <= tolerance * fmax(pattern_dim, 0.001f)) {
            if (*count >= capacity) {
                capacity *= 2;
                atom_handle_t* new_matches = (atom_handle_t*)realloc(matches, capacity * sizeof(atom_handle_t));
                if (!new_matches) {
                    free(matches);
                    return NULL;
                }
                matches = new_matches;
            }
            matches[(*count)++] = fas->atoms[i]->handle;
        }
    }
    
    if (*count == 0) {
        free(matches);
        return NULL;
    }
    
    return matches;
}

/* ============================================================================
 * PHASE 2: GENERATION AND EXPANSION FUNCTIONS
 * ============================================================================ */

COGUTIL_API atom_handle_t fractal_create_generator(fractal_atomspace_t* fas, atom_handle_t template_atom,
                                                     uint32_t iterations) {
    if (!fas || iterations == 0) return ATOM_HANDLE_INVALID;
    
    fractal_atom_t* template = find_atom(fas, template_atom);
    if (!template) return ATOM_HANDLE_INVALID;
    
    /* Create a generator node that will produce fractal structure */
    atom_handle_t gen = fractal_create_node(fas, template->atom_type, "generator");
    if (gen == ATOM_HANDLE_INVALID) return ATOM_HANDLE_INVALID;
    
    fractal_atom_t* gen_atom = find_atom(fas, gen);
    if (!gen_atom) return ATOM_HANDLE_INVALID;
    
    gen_atom->properties = template->properties;
    gen_atom->properties.iteration_count = iterations;
    gen_atom->properties.flags |= FRACTAL_FLAG_GENERATOR;
    
    return gen;
}

COGUTIL_API cog_result_t fractal_expand_generator(fractal_atomspace_t* fas, atom_handle_t generator) {
    if (!fas) return COG_ERROR_INVALID_PARAM;
    
    fractal_atom_t* gen = find_atom(fas, generator);
    if (!gen || !(gen->properties.flags & FRACTAL_FLAG_GENERATOR)) {
        return COG_ERROR_INVALID_PARAM;
    }
    
    if (gen->properties.iteration_count == 0) return COG_SUCCESS;
    
    /* Create children based on self-similar pattern */
    uint32_t num_children = (uint32_t)(1.0f / gen->properties.scale_factor);
    if (num_children < 2) num_children = 2;
    if (num_children > FRACTAL_MAX_CHILDREN) num_children = FRACTAL_MAX_CHILDREN;
    
    char child_name[64];
    for (uint32_t i = 0; i < num_children; i++) {
        snprintf(child_name, sizeof(child_name), "gen_child_%u", i);
        atom_handle_t child = fractal_create_node(fas, gen->atom_type, child_name);
        if (child == ATOM_HANDLE_INVALID) continue;
        
        fractal_atom_t* child_atom = find_atom(fas, child);
        if (child_atom) {
            child_atom->properties = gen->properties;
            child_atom->properties.scale_factor *= gen->properties.scale_factor;
            child_atom->properties.iteration_count--;
            if (child_atom->properties.iteration_count > 0) {
                child_atom->properties.flags |= FRACTAL_FLAG_GENERATOR;
            } else {
                child_atom->properties.flags &= ~FRACTAL_FLAG_GENERATOR;
            }
        }
        
        fractal_add_child(fas, generator, child);
    }
    
    gen->properties.flags &= ~FRACTAL_FLAG_GENERATOR;
    gen->properties.iteration_count = 0;
    
    return COG_SUCCESS;
}

COGUTIL_API cog_result_t fractal_collapse_subtree(fractal_atomspace_t* fas, atom_handle_t root) {
    if (!fas) return COG_ERROR_INVALID_PARAM;
    
    fractal_atom_t* atom = find_atom(fas, root);
    if (!atom) return COG_ERROR_NOT_FOUND;
    
    /* Aggregate TV from all descendants before collapsing */
    truth_value_t agg_tv;
    if (fractal_aggregate_tv(fas, root, &agg_tv) == COG_SUCCESS) {
        atom->properties.base_tv = agg_tv;
    }
    
    /* Remove all children recursively */
    while (atom->child_count > 0) {
        atom_handle_t child = atom->children[atom->child_count - 1];
        fractal_collapse_subtree(fas, child);
        fractal_remove_child(fas, root, child);
    }
    
    /* Mark as collapsed */
    atom->properties.flags |= FRACTAL_FLAG_COLLAPSED;
    
    return COG_SUCCESS;
}

COGUTIL_API atom_handle_t fractal_clone_subtree(fractal_atomspace_t* fas, atom_handle_t root,
                                                  atom_handle_t new_parent) {
    if (!fas) return ATOM_HANDLE_INVALID;
    
    fractal_atom_t* src = find_atom(fas, root);
    if (!src) return ATOM_HANDLE_INVALID;
    
    /* Create clone of root node */
    char clone_name[128];
    snprintf(clone_name, sizeof(clone_name), "%s_clone", src->name);
    
    atom_handle_t clone = fractal_create_node(fas, src->atom_type, clone_name);
    if (clone == ATOM_HANDLE_INVALID) return ATOM_HANDLE_INVALID;
    
    fractal_atom_t* clone_atom = find_atom(fas, clone);
    if (clone_atom) {
        clone_atom->properties = src->properties;
    }
    
    /* Attach to new parent if specified */
    if (new_parent != ATOM_HANDLE_INVALID) {
        fractal_add_child(fas, new_parent, clone);
    }
    
    /* Clone children recursively */
    for (size_t i = 0; i < src->child_count; i++) {
        fractal_clone_subtree(fas, src->children[i], clone);
    }
    
    return clone;
}

/* ============================================================================
 * PHASE 2: TRUTH VALUE FUNCTIONS
 * ============================================================================ */

COGUTIL_API cog_result_t fractal_aggregate_tv(fractal_atomspace_t* fas, atom_handle_t root, truth_value_t* result) {
    if (!fas || !result) return COG_ERROR_INVALID_PARAM;
    
    fractal_atom_t* atom = find_atom(fas, root);
    if (!atom) return COG_ERROR_NOT_FOUND;
    
    if (atom->child_count == 0) {
        *result = atom->properties.base_tv;
        return COG_SUCCESS;
    }
    
    /* Aggregate from children using weighted average */
    float total_strength = 0.0f;
    float total_confidence = 0.0f;
    float total_weight = 0.0f;
    
    for (size_t i = 0; i < atom->child_count; i++) {
        truth_value_t child_tv;
        if (fractal_aggregate_tv(fas, atom->children[i], &child_tv) == COG_SUCCESS) {
            float weight = child_tv.confidence;
            total_strength += child_tv.strength * weight;
            total_confidence += child_tv.confidence * weight;
            total_weight += weight;
        }
    }
    
    if (total_weight > 0.0f) {
        result->strength = total_strength / total_weight;
        result->confidence = total_confidence / total_weight;
    } else {
        result->strength = atom->properties.base_tv.strength;
        result->confidence = atom->properties.base_tv.confidence;
    }
    
    return COG_SUCCESS;
}

COGUTIL_API cog_result_t fractal_set_inherited_tv(fractal_atomspace_t* fas, atom_handle_t handle,
                                                    const truth_value_t* tv) {
    if (!fas || !tv) return COG_ERROR_INVALID_PARAM;
    
    fractal_atom_t* atom = find_atom(fas, handle);
    if (!atom) return COG_ERROR_NOT_FOUND;
    
    atom->properties.inherited_tv = *tv;
    atom->properties.flags |= FRACTAL_FLAG_TV_INHERITED;
    
    return COG_SUCCESS;
}

/* ============================================================================
 * PHASE 2: PERSISTENCE FUNCTIONS
 * ============================================================================ */

COGUTIL_API cog_result_t fractal_save(fractal_atomspace_t* fas, const char* filename) {
    if (!fas || !filename) return COG_ERROR_INVALID_PARAM;
    
    FILE* fp = fopen(filename, "wb");
    if (!fp) return COG_ERROR_IO;
    
    /* Write header */
    const char magic[] = "FATM";
    uint32_t version = 1;
    fwrite(magic, 1, 4, fp);
    fwrite(&version, sizeof(uint32_t), 1, fp);
    fwrite(&fas->atom_count, sizeof(size_t), 1, fp);
    fwrite(&fas->stats, sizeof(fractal_stats_t), 1, fp);
    
    /* Write atoms */
    for (size_t i = 0; i < fas->atom_count; i++) {
        fractal_atom_t* atom = fas->atoms[i];
        if (!atom) continue;
        fwrite(&atom->handle, sizeof(atom_handle_t), 1, fp);
        fwrite(&atom->atom_type, sizeof(atom_type_t), 1, fp);
        fwrite(&atom->depth, sizeof(uint32_t), 1, fp);
        fwrite(&atom->absolute_scale, sizeof(float), 1, fp);
        fwrite(&atom->parent, sizeof(atom_handle_t), 1, fp);
        fwrite(&atom->child_count, sizeof(size_t), 1, fp);
        fwrite(&atom->properties, sizeof(fractal_properties_t), 1, fp);
        
        /* Write name with length prefix */
        size_t name_len = strlen(atom->name);
        fwrite(&name_len, sizeof(size_t), 1, fp);
        fwrite(atom->name, 1, name_len, fp);
        
        /* Write children handles */
        if (atom->child_count > 0 && atom->children) {
            fwrite(atom->children, sizeof(atom_handle_t), atom->child_count, fp);
        }
    }
    
    fclose(fp);
    return COG_SUCCESS;
}

COGUTIL_API cog_result_t fractal_load(fractal_atomspace_t* fas, const char* filename) {
    if (!fas || !filename) return COG_ERROR_INVALID_PARAM;
    
    FILE* fp = fopen(filename, "rb");
    if (!fp) return COG_ERROR_IO;
    
    /* Read and verify header */
    char magic[4];
    uint32_t version;
    size_t atom_count;
    
    fread(magic, 1, 4, fp);
    if (memcmp(magic, "FATM", 4) != 0) {
        fclose(fp);
        return COG_ERROR_INVALID_PARAM;
    }
    
    fread(&version, sizeof(uint32_t), 1, fp);
    if (version != 1) {
        fclose(fp);
        return COG_ERROR_INVALID_PARAM;
    }
    
    fread(&atom_count, sizeof(size_t), 1, fp);
    fread(&fas->stats, sizeof(fractal_stats_t), 1, fp);
    
    /* Read atoms */
    for (size_t i = 0; i < atom_count; i++) {
        if (fas->atom_count >= fas->atom_capacity) {
            size_t new_capacity = fas->atom_capacity * 2;
            fractal_atom_t** new_atoms = (fractal_atom_t**)realloc(fas->atoms, 
                new_capacity * sizeof(fractal_atom_t*));
            if (!new_atoms) {
                fclose(fp);
                return COG_ERROR_OUT_OF_MEMORY;
            }
            fas->atoms = new_atoms;
            fas->atom_capacity = new_capacity;
        }
        
        fractal_atom_t* atom = (fractal_atom_t*)calloc(1, sizeof(fractal_atom_t));
        if (!atom) {
            fclose(fp);
            return COG_ERROR_OUT_OF_MEMORY;
        }
        
        fread(&atom->handle, sizeof(atom_handle_t), 1, fp);
        fread(&atom->atom_type, sizeof(atom_type_t), 1, fp);
        fread(&atom->depth, sizeof(uint32_t), 1, fp);
        fread(&atom->absolute_scale, sizeof(float), 1, fp);
        fread(&atom->parent, sizeof(atom_handle_t), 1, fp);
        fread(&atom->child_count, sizeof(size_t), 1, fp);
        fread(&atom->properties, sizeof(fractal_properties_t), 1, fp);
        
        /* Read name */
        size_t name_len;
        fread(&name_len, sizeof(size_t), 1, fp);
        if (name_len < sizeof(atom->name)) {
            fread(atom->name, 1, name_len, fp);
            atom->name[name_len] = '\0';
        } else {
            fread(atom->name, 1, sizeof(atom->name) - 1, fp);
            atom->name[sizeof(atom->name) - 1] = '\0';
            fseek(fp, (long)(name_len - sizeof(atom->name) + 1), SEEK_CUR);
        }
        
        /* Read children handles */
        if (atom->child_count > 0) {
            atom->children = (atom_handle_t*)malloc(atom->child_count * sizeof(atom_handle_t));
            if (atom->children) {
                fread(atom->children, sizeof(atom_handle_t), atom->child_count, fp);
            }
        }
        
        fas->atoms[fas->atom_count++] = atom;
    }
    
    fclose(fp);
    return COG_SUCCESS;
}

COGUTIL_API cog_result_t fractal_export_scheme(fractal_atomspace_t* fas, const char* filename) {
    if (!fas || !filename) return COG_ERROR_INVALID_PARAM;
    
    FILE* fp = fopen(filename, "w");
    if (!fp) return COG_ERROR_IO;
    
    fprintf(fp, ";; Fractal AtomSpace Export (Scheme format)\n");
    fprintf(fp, ";; Total atoms: %zu\n\n", fas->atom_count);
    
    for (size_t i = 0; i < fas->atom_count; i++) {
        fractal_atom_t* atom = fas->atoms[i];
        if (!atom) continue;
        
        fprintf(fp, "(FractalAtom\n");
        fprintf(fp, "  (handle %lu)\n", (unsigned long)atom->handle);
        fprintf(fp, "  (type %u)\n", atom->atom_type);
        fprintf(fp, "  (name \"%s\")\n", atom->name);
        fprintf(fp, "  (depth %u)\n", atom->depth);
        fprintf(fp, "  (scale %f)\n", atom->absolute_scale);
        fprintf(fp, "  (stv %f %f)\n", atom->properties.base_tv.strength, 
                atom->properties.base_tv.confidence);
        
        if (atom->child_count > 0) {
            fprintf(fp, "  (children");
            for (size_t j = 0; j < atom->child_count; j++) {
                fprintf(fp, " %lu", (unsigned long)atom->children[j]);
            }
            fprintf(fp, ")\n");
        }
        
        fprintf(fp, ")\n\n");
    }
    
    fclose(fp);
    return COG_SUCCESS;
}

/* ============================================================================
 * PHASE 2: MEMORY MANAGEMENT FUNCTIONS
 * ============================================================================ */

COGUTIL_API void fractal_atom_free(fractal_atomspace_t* fas, atom_handle_t handle) {
    if (!fas) return;
    
    fractal_atom_t* atom = find_atom(fas, handle);
    if (!atom) return;
    
    /* Free children array */
    if (atom->children) {
        free(atom->children);
        atom->children = NULL;
    }
    
    /* Mark slot as free by invalidating handle */
    atom->handle = ATOM_HANDLE_INVALID;
    atom->child_count = 0;
    
    fas->stats.total_atoms--;
}

COGUTIL_API void fractal_handles_free(atom_handle_t* handles) {
    if (handles) {
        free(handles);
    }
}

COGUTIL_API cog_result_t fractal_prune_by_scale(fractal_atomspace_t* fas, float min_scale) {
    if (!fas) return COG_ERROR_INVALID_PARAM;
    
    /* Prune atoms below minimum scale threshold */
    for (size_t i = 0; i < fas->atom_count; i++) {
        if (!fas->atoms[i]) continue;
        if (fas->atoms[i]->handle == ATOM_HANDLE_INVALID) continue;
        if (fas->atoms[i]->absolute_scale < min_scale) {
            /* Remove from parent first */
            if (fas->atoms[i]->parent != ATOM_HANDLE_INVALID) {
                fractal_remove_child(fas, fas->atoms[i]->parent, fas->atoms[i]->handle);
            }
            fractal_atom_free(fas, fas->atoms[i]->handle);
        }
    }
    
    return COG_SUCCESS;
}

COGUTIL_API cog_result_t fractal_compact(fractal_atomspace_t* fas) {
    if (!fas) return COG_ERROR_INVALID_PARAM;
    
    /* Compact atom array by removing freed slots */
    size_t write_idx = 0;
    for (size_t read_idx = 0; read_idx < fas->atom_count; read_idx++) {
        if (fas->atoms[read_idx] && fas->atoms[read_idx]->handle != ATOM_HANDLE_INVALID) {
            if (write_idx != read_idx) {
                fas->atoms[write_idx] = fas->atoms[read_idx];
                fas->atoms[read_idx] = NULL;
            }
            write_idx++;
        }
    }
    
    fas->atom_count = write_idx;
    
    return COG_SUCCESS;
}

COGUTIL_API void fractal_reset_stats(fractal_atomspace_t* fas) {
    if (!fas) return;
    
    memset(&fas->stats, 0, sizeof(fractal_stats_t));
    
    /* Recalculate basic stats */
    fas->stats.total_atoms = fas->atom_count;
    
    uint32_t max_depth = 0;
    for (size_t i = 0; i < fas->atom_count; i++) {
        if (!fas->atoms[i]) continue;
        if (fas->atoms[i]->handle == ATOM_HANDLE_INVALID) continue;
        if (fas->atoms[i]->depth > max_depth) {
            max_depth = fas->atoms[i]->depth;
        }
    }
    fas->stats.max_depth = max_depth;
}

COGUTIL_API fractal_atom_t* fractal_get_atom(fractal_atomspace_t* fas, atom_handle_t handle) {
    return find_atom(fas, handle);
}

COGUTIL_API atom_handle_t* fractal_get_at_depth(fractal_atomspace_t* fas, uint32_t depth, size_t* count) {
    if (!fas || !count) return NULL;
    *count = 0;
    
    /* First pass: count atoms at depth */
    size_t n = 0;
    for (size_t i = 0; i < fas->atom_count; i++) {
        if (!fas->atoms[i]) continue;
        if (fas->atoms[i]->handle != ATOM_HANDLE_INVALID && fas->atoms[i]->depth == depth) {
            n++;
        }
    }
    
    if (n == 0) return NULL;
    
    atom_handle_t* handles = (atom_handle_t*)malloc(n * sizeof(atom_handle_t));
    if (!handles) return NULL;
    
    /* Second pass: collect handles */
    size_t idx = 0;
    for (size_t i = 0; i < fas->atom_count && idx < n; i++) {
        if (!fas->atoms[i]) continue;
        if (fas->atoms[i]->handle != ATOM_HANDLE_INVALID && fas->atoms[i]->depth == depth) {
            handles[idx++] = fas->atoms[i]->handle;
        }
    }
    
    *count = n;
    return handles;
}

COGUTIL_API atom_handle_t* fractal_get_in_scale_range(fractal_atomspace_t* fas, float min_scale, 
                                                        float max_scale, size_t* count) {
    if (!fas || !count) return NULL;
    *count = 0;
    
    size_t capacity = 64;
    atom_handle_t* handles = (atom_handle_t*)malloc(capacity * sizeof(atom_handle_t));
    if (!handles) return NULL;
    
    for (size_t i = 0; i < fas->atom_count; i++) {
        if (fas->atoms[i].handle == ATOM_HANDLE_INVALID) continue;
        
        float scale = fas->atoms[i].absolute_scale;
        if (scale >= min_scale && scale <= max_scale) {
            if (*count >= capacity) {
                capacity *= 2;
                atom_handle_t* new_handles = (atom_handle_t*)realloc(handles, capacity * sizeof(atom_handle_t));
                if (!new_handles) {
                    free(handles);
                    return NULL;
                }
                handles = new_handles;
            }
            handles[(*count)++] = fas->atoms[i].handle;
        }
    }
    
    if (*count == 0) {
        free(handles);
        return NULL;
    }
    
    return handles;
}

/* ============================================================================
 * REPAIR AND INTEGRITY FUNCTIONS
 * ============================================================================ */

/**
 * Check if an atom has duplicate children in its child array
 */
COGUTIL_API bool fractal_has_duplicate_children(fractal_atomspace_t* fas, atom_handle_t handle) {
    if (!fas) return false;
    
    fractal_atom_t* atom = find_atom(fas, handle);
    if (!atom || atom->child_count < 2) return false;
    
    /* Check for duplicates using O(n^2) comparison */
    for (size_t i = 0; i < atom->child_count; i++) {
        for (size_t j = i + 1; j < atom->child_count; j++) {
            if (atom->children[i] == atom->children[j]) {
                return true;
            }
        }
    }
    
    return false;
}

/**
 * Count the number of duplicate children for an atom
 */
COGUTIL_API size_t fractal_count_duplicate_children(fractal_atomspace_t* fas, atom_handle_t handle) {
    if (!fas) return 0;
    
    fractal_atom_t* atom = find_atom(fas, handle);
    if (!atom || atom->child_count < 2) return 0;
    
    size_t duplicate_count = 0;
    
    for (size_t i = 0; i < atom->child_count; i++) {
        for (size_t j = i + 1; j < atom->child_count; j++) {
            if (atom->children[i] == atom->children[j]) {
                duplicate_count++;
            }
        }
    }
    
    return duplicate_count;
}

/**
 * Repair duplicate children in an atom's child array
 * Returns the number of duplicates removed
 */
COGUTIL_API size_t fractal_repair_duplicates(fractal_atomspace_t* fas, atom_handle_t handle) {
    if (!fas) return 0;
    
    fractal_atom_t* atom = find_atom(fas, handle);
    if (!atom || atom->child_count < 2) return 0;
    
    size_t duplicates_removed = 0;
    size_t write_idx = 0;
    
    /* Create a temporary array to track unique children */
    for (size_t i = 0; i < atom->child_count; i++) {
        bool is_duplicate = false;
        
        /* Check if this child already exists in the compacted portion */
        for (size_t j = 0; j < write_idx; j++) {
            if (atom->children[i] == atom->children[j]) {
                is_duplicate = true;
                duplicates_removed++;
                break;
            }
        }
        
        if (!is_duplicate) {
            atom->children[write_idx++] = atom->children[i];
        }
    }
    
    atom->child_count = write_idx;
    
    return duplicates_removed;
}

/**
 * Repair all duplicate children in the entire fractal atomspace
 * Returns total number of duplicates removed
 */
COGUTIL_API size_t fractal_repair_all_duplicates(fractal_atomspace_t* fas) {
    if (!fas) return 0;
    
    size_t total_removed = 0;
    
    for (size_t i = 0; i < fas->atom_count; i++) {
        if (!fas->atoms[i]) continue;
        if (fas->atoms[i]->handle == ATOM_HANDLE_INVALID) continue;
        
        total_removed += fractal_repair_duplicates(fas, fas->atoms[i]->handle);
    }
    
    return total_removed;
}

/**
 * Hierarchy integrity issue flags
 */
#define FRACTAL_ISSUE_NONE            0x0000
#define FRACTAL_ISSUE_DUPLICATE_CHILD 0x0001
#define FRACTAL_ISSUE_ORPHAN_ATOM     0x0002
#define FRACTAL_ISSUE_INVALID_PARENT  0x0004
#define FRACTAL_ISSUE_DEPTH_MISMATCH  0x0008
#define FRACTAL_ISSUE_CYCLE_DETECTED  0x0010

/**
 * Validate hierarchy integrity for a single atom
 * Returns a bitmask of issues found
 */
COGUTIL_API uint32_t fractal_validate_atom(fractal_atomspace_t* fas, atom_handle_t handle) {
    if (!fas) return FRACTAL_ISSUE_NONE;
    
    fractal_atom_t* atom = find_atom(fas, handle);
    if (!atom) return FRACTAL_ISSUE_NONE;
    
    uint32_t issues = FRACTAL_ISSUE_NONE;
    
    /* Check for duplicate children */
    if (fractal_has_duplicate_children(fas, handle)) {
        issues |= FRACTAL_ISSUE_DUPLICATE_CHILD;
    }
    
    /* Check parent validity */
    if (atom->parent != ATOM_HANDLE_INVALID) {
        fractal_atom_t* parent = find_atom(fas, atom->parent);
        if (!parent) {
            issues |= FRACTAL_ISSUE_INVALID_PARENT;
        } else {
            /* Check depth consistency */
            if (atom->properties.depth != parent->properties.depth + 1) {
                issues |= FRACTAL_ISSUE_DEPTH_MISMATCH;
            }
            
            /* Check if atom is actually in parent's child list */
            bool found_in_parent = false;
            for (size_t i = 0; i < parent->child_count; i++) {
                if (parent->children[i] == handle) {
                    found_in_parent = true;
                    break;
                }
            }
            if (!found_in_parent) {
                issues |= FRACTAL_ISSUE_ORPHAN_ATOM;
            }
        }
    }
    
    /* Check for cycles by following parent chain */
    atom_handle_t current = atom->parent;
    size_t max_depth = fas->config.max_depth;
    size_t depth_count = 0;
    
    while (current != ATOM_HANDLE_INVALID && depth_count < max_depth) {
        if (current == handle) {
            issues |= FRACTAL_ISSUE_CYCLE_DETECTED;
            break;
        }
        fractal_atom_t* parent_atom = find_atom(fas, current);
        if (!parent_atom) break;
        current = parent_atom->parent;
        depth_count++;
    }
    
    return issues;
}

COGUTIL_API cog_result_t fractal_validate_hierarchy(fractal_atomspace_t* fas, 
                                                      fractal_validation_result_t* result) {
    if (!fas || !result) return COG_ERROR_INVALID_PARAM;
    
    memset(result, 0, sizeof(fractal_validation_result_t));
    
    for (size_t i = 0; i < fas->atom_count; i++) {
        if (!fas->atoms[i]) continue;
        if (fas->atoms[i]->handle == ATOM_HANDLE_INVALID) continue;
        
        result->atoms_checked++;
        
        uint32_t issues = fractal_validate_atom(fas, fas->atoms[i]->handle);
        
        if (issues != FRACTAL_ISSUE_NONE) {
            result->atoms_with_issues++;
            
            if (issues & FRACTAL_ISSUE_DUPLICATE_CHILD) result->duplicate_child_issues++;
            if (issues & FRACTAL_ISSUE_ORPHAN_ATOM) result->orphan_issues++;
            if (issues & FRACTAL_ISSUE_INVALID_PARENT) result->invalid_parent_issues++;
            if (issues & FRACTAL_ISSUE_DEPTH_MISMATCH) result->depth_mismatch_issues++;
            if (issues & FRACTAL_ISSUE_CYCLE_DETECTED) result->cycle_issues++;
        }
    }
    
    return COG_SUCCESS;
}

/**
 * Comprehensive repair function that fixes all detectable issues
 * Returns COG_SUCCESS if all repairs succeeded
 */
COGUTIL_API cog_result_t fractal_repair_hierarchy(fractal_atomspace_t* fas) {
    if (!fas) return COG_ERROR_INVALID_PARAM;
    
    /* Step 1: Repair duplicate children */
    fractal_repair_all_duplicates(fas);
    
    /* Step 2: Fix depth mismatches by recalculating from roots */
    for (size_t i = 0; i < fas->atom_count; i++) {
        if (!fas->atoms[i]) continue;
        if (fas->atoms[i]->handle == ATOM_HANDLE_INVALID) continue;
        
        /* Find root atoms and recalculate depth for their subtrees */
        if (fas->atoms[i]->parent == ATOM_HANDLE_INVALID) {
            fas->atoms[i]->properties.depth = 0;
        } else {
            fractal_atom_t* parent = find_atom(fas, fas->atoms[i]->parent);
            if (parent) {
                fas->atoms[i]->properties.depth = parent->properties.depth + 1;
            } else {
                /* Invalid parent reference - make this a root */
                fas->atoms[i]->parent = ATOM_HANDLE_INVALID;
                fas->atoms[i]->properties.depth = 0;
            }
        }
    }
    
    /* Step 3: Fix orphan atoms by removing from parent reference */
    for (size_t i = 0; i < fas->atom_count; i++) {
        if (!fas->atoms[i]) continue;
        if (fas->atoms[i]->handle == ATOM_HANDLE_INVALID) continue;
        if (fas->atoms[i]->parent == ATOM_HANDLE_INVALID) continue;
        
        fractal_atom_t* parent = find_atom(fas, fas->atoms[i]->parent);
        if (parent) {
            bool found = false;
            for (size_t j = 0; j < parent->child_count; j++) {
                if (parent->children[j] == fas->atoms[i]->handle) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                /* Re-add to parent or make orphan a root */
                cog_result_t result = fractal_add_child(fas, fas->atoms[i]->parent, 
                                                         fas->atoms[i]->handle, 
                                                         fas->atoms[i]->properties.scale_factor);
                if (result != COG_SUCCESS) {
                    /* If we can't add back, make it a root */
                    fas->atoms[i]->parent = ATOM_HANDLE_INVALID;
                    fas->atoms[i]->properties.depth = 0;
                }
            }
        }
    }
    
    /* Step 4: Update statistics */
    fractal_reset_stats(fas);
    
    return COG_SUCCESS;
}
