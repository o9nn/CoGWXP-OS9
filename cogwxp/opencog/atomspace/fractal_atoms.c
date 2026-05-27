/**
 * @file fractal_atoms.c
 * @brief Fractal AtomSpace Implementation - Self-similar recursive hypergraph structures
 */

#include "fractal_atoms.h"
#include "atomspace.h"
#include <stdlib.h>
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
