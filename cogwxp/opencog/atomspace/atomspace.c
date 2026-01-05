/**
 * @file atomspace.c
 * @brief AtomSpace Hypergraph Knowledge Representation Implementation
 * 
 * Core implementation of the typed hypergraph knowledge representation
 * system for OpenCog cognitive architecture.
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

#define ATOM_TABLE_INITIAL_SIZE 4096
#define ATOM_TABLE_LOAD_FACTOR 0.75
#define ATOM_INDEX_BUCKET_COUNT 256

/*===========================================================================
 * Internal Structures
 *===========================================================================*/

/* Atom structure */
struct atom {
    atom_handle_t handle;
    atom_type_t type;
    char* name;                     /* For nodes */
    atom_handle_t* outgoing;        /* For links */
    size_t outgoing_count;
    truth_value_t tv;
    attention_value_t av;
    uint32_t flags;
    size_t incoming_count;
    atom_handle_t* incoming;        /* Back-references */
    size_t incoming_capacity;
    pthread_rwlock_t lock;
};

/* Hash table entry */
typedef struct atom_entry {
    atom_t* atom;
    struct atom_entry* next;
} atom_entry_t;

/* Type hierarchy entry */
typedef struct type_info {
    atom_type_t type;
    const char* name;
    atom_type_t parent;
    bool is_node;
    bool is_link;
} type_info_t;

/* AtomSpace structure */
struct atomspace {
    /* Atom storage */
    atom_entry_t** table;
    size_t table_size;
    size_t atom_count;
    pthread_rwlock_t table_lock;
    
    /* Indexes */
    struct {
        atom_entry_t** by_type;
        atom_entry_t** by_name;
        size_t bucket_count;
    } indexes;
    pthread_rwlock_t index_lock;
    
    /* Type hierarchy */
    type_info_t* types;
    size_t type_count;
    size_t type_capacity;
    pthread_rwlock_t type_lock;
    
    /* Handle generation */
    atom_handle_t next_handle;
    pthread_mutex_t handle_lock;
    
    /* Configuration */
    atomspace_config_t config;
    
    /* Statistics */
    atomspace_stats_t stats;
    pthread_mutex_t stats_lock;
    
    /* Attention bank */
    struct {
        atom_handle_t* attentional_focus;
        size_t focus_size;
        size_t focus_capacity;
        int16_t sti_threshold;
        pthread_mutex_t lock;
    } attention;
};

/*===========================================================================
 * Hash Functions
 *===========================================================================*/

static uint64_t hash_handle(atom_handle_t handle) {
    /* FNV-1a hash */
    uint64_t hash = 14695981039346656037ULL;
    uint8_t* bytes = (uint8_t*)&handle;
    for (size_t i = 0; i < sizeof(handle); i++) {
        hash ^= bytes[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

static uint64_t hash_string(const char* str) {
    if (!str) return 0;
    
    uint64_t hash = 14695981039346656037ULL;
    while (*str) {
        hash ^= (uint8_t)*str++;
        hash *= 1099511628211ULL;
    }
    return hash;
}

static uint64_t hash_outgoing(atom_type_t type, const atom_handle_t* outgoing, size_t count) {
    uint64_t hash = 14695981039346656037ULL;
    
    hash ^= type;
    hash *= 1099511628211ULL;
    
    for (size_t i = 0; i < count; i++) {
        uint8_t* bytes = (uint8_t*)&outgoing[i];
        for (size_t j = 0; j < sizeof(atom_handle_t); j++) {
            hash ^= bytes[j];
            hash *= 1099511628211ULL;
        }
    }
    
    return hash;
}

/*===========================================================================
 * Atom Operations
 *===========================================================================*/

static atom_t* atom_create(atom_type_t type) {
    atom_t* atom = COG_CALLOC(1, sizeof(atom_t));
    if (!atom) return NULL;
    
    atom->type = type;
    atom->tv.type = TV_SIMPLE;
    atom->tv.strength = 0.0;
    atom->tv.confidence = 0.0;
    atom->av.sti = 0;
    atom->av.lti = 0;
    atom->av.vlti = false;
    
    pthread_rwlock_init(&atom->lock, NULL);
    
    return atom;
}

static void atom_destroy(atom_t* atom) {
    if (!atom) return;
    
    COG_FREE(atom->name);
    COG_FREE(atom->outgoing);
    COG_FREE(atom->incoming);
    pthread_rwlock_destroy(&atom->lock);
    COG_FREE(atom);
}

static void atom_add_incoming(atom_t* atom, atom_handle_t link_handle) {
    pthread_rwlock_wrlock(&atom->lock);
    
    if (atom->incoming_count >= atom->incoming_capacity) {
        size_t new_capacity = atom->incoming_capacity == 0 ? 4 : atom->incoming_capacity * 2;
        atom_handle_t* new_incoming = COG_REALLOC(atom->incoming, 
            new_capacity * sizeof(atom_handle_t));
        if (new_incoming) {
            atom->incoming = new_incoming;
            atom->incoming_capacity = new_capacity;
        }
    }
    
    if (atom->incoming_count < atom->incoming_capacity) {
        atom->incoming[atom->incoming_count++] = link_handle;
    }
    
    pthread_rwlock_unlock(&atom->lock);
}

static void atom_remove_incoming(atom_t* atom, atom_handle_t link_handle) {
    pthread_rwlock_wrlock(&atom->lock);
    
    for (size_t i = 0; i < atom->incoming_count; i++) {
        if (atom->incoming[i] == link_handle) {
            memmove(&atom->incoming[i], &atom->incoming[i + 1],
                (atom->incoming_count - i - 1) * sizeof(atom_handle_t));
            atom->incoming_count--;
            break;
        }
    }
    
    pthread_rwlock_unlock(&atom->lock);
}

/*===========================================================================
 * AtomSpace Lifecycle
 *===========================================================================*/

COGUTIL_API cog_result_t atomspace_create(
    const atomspace_config_t* config,
    atomspace_t* as
) {
    if (!as) return COG_ERROR_INVALID_PARAM;
    
    atomspace_t a = COG_CALLOC(1, sizeof(struct atomspace));
    if (!a) return COG_ERROR_MEMORY;
    
    /* Initialize hash table */
    a->table_size = ATOM_TABLE_INITIAL_SIZE;
    a->table = COG_CALLOC(a->table_size, sizeof(atom_entry_t*));
    if (!a->table) {
        COG_FREE(a);
        return COG_ERROR_MEMORY;
    }
    
    /* Initialize indexes */
    a->indexes.bucket_count = ATOM_INDEX_BUCKET_COUNT;
    a->indexes.by_type = COG_CALLOC(a->indexes.bucket_count, sizeof(atom_entry_t*));
    a->indexes.by_name = COG_CALLOC(a->indexes.bucket_count, sizeof(atom_entry_t*));
    
    /* Initialize type hierarchy */
    a->type_capacity = 256;
    a->types = COG_CALLOC(a->type_capacity, sizeof(type_info_t));
    
    /* Register basic types */
    a->types[ATOM_TYPE_NODE] = (type_info_t){ATOM_TYPE_NODE, "Node", 0, true, false};
    a->types[ATOM_TYPE_LINK] = (type_info_t){ATOM_TYPE_LINK, "Link", 0, false, true};
    a->types[ATOM_TYPE_CONCEPT] = (type_info_t){ATOM_TYPE_CONCEPT, "ConceptNode", ATOM_TYPE_NODE, true, false};
    a->types[ATOM_TYPE_PREDICATE] = (type_info_t){ATOM_TYPE_PREDICATE, "PredicateNode", ATOM_TYPE_NODE, true, false};
    a->types[ATOM_TYPE_VARIABLE] = (type_info_t){ATOM_TYPE_VARIABLE, "VariableNode", ATOM_TYPE_NODE, true, false};
    a->types[ATOM_TYPE_NUMBER] = (type_info_t){ATOM_TYPE_NUMBER, "NumberNode", ATOM_TYPE_NODE, true, false};
    a->types[ATOM_TYPE_SCHEMA] = (type_info_t){ATOM_TYPE_SCHEMA, "SchemaNode", ATOM_TYPE_NODE, true, false};
    a->types[ATOM_TYPE_GROUNDED_SCHEMA] = (type_info_t){ATOM_TYPE_GROUNDED_SCHEMA, "GroundedSchemaNode", ATOM_TYPE_SCHEMA, true, false};
    a->types[ATOM_TYPE_INHERITANCE] = (type_info_t){ATOM_TYPE_INHERITANCE, "InheritanceLink", ATOM_TYPE_LINK, false, true};
    a->types[ATOM_TYPE_EVALUATION] = (type_info_t){ATOM_TYPE_EVALUATION, "EvaluationLink", ATOM_TYPE_LINK, false, true};
    a->types[ATOM_TYPE_LIST] = (type_info_t){ATOM_TYPE_LIST, "ListLink", ATOM_TYPE_LINK, false, true};
    a->types[ATOM_TYPE_SET] = (type_info_t){ATOM_TYPE_SET, "SetLink", ATOM_TYPE_LINK, false, true};
    a->types[ATOM_TYPE_AND] = (type_info_t){ATOM_TYPE_AND, "AndLink", ATOM_TYPE_LINK, false, true};
    a->types[ATOM_TYPE_OR] = (type_info_t){ATOM_TYPE_OR, "OrLink", ATOM_TYPE_LINK, false, true};
    a->types[ATOM_TYPE_NOT] = (type_info_t){ATOM_TYPE_NOT, "NotLink", ATOM_TYPE_LINK, false, true};
    a->types[ATOM_TYPE_IMPLICATION] = (type_info_t){ATOM_TYPE_IMPLICATION, "ImplicationLink", ATOM_TYPE_LINK, false, true};
    a->types[ATOM_TYPE_EQUIVALENCE] = (type_info_t){ATOM_TYPE_EQUIVALENCE, "EquivalenceLink", ATOM_TYPE_LINK, false, true};
    a->types[ATOM_TYPE_EXECUTION] = (type_info_t){ATOM_TYPE_EXECUTION, "ExecutionLink", ATOM_TYPE_LINK, false, true};
    a->types[ATOM_TYPE_BIND] = (type_info_t){ATOM_TYPE_BIND, "BindLink", ATOM_TYPE_LINK, false, true};
    a->type_count = 20;
    
    /* Initialize locks */
    pthread_rwlock_init(&a->table_lock, NULL);
    pthread_rwlock_init(&a->index_lock, NULL);
    pthread_rwlock_init(&a->type_lock, NULL);
    pthread_mutex_init(&a->handle_lock, NULL);
    pthread_mutex_init(&a->stats_lock, NULL);
    pthread_mutex_init(&a->attention.lock, NULL);
    
    /* Initialize attention bank */
    a->attention.focus_capacity = 100;
    a->attention.attentional_focus = COG_CALLOC(a->attention.focus_capacity, sizeof(atom_handle_t));
    a->attention.sti_threshold = 0;
    
    /* Copy config */
    if (config) {
        memcpy(&a->config, config, sizeof(atomspace_config_t));
    }
    
    /* Initialize handle counter */
    a->next_handle = 1;
    
    *as = a;
    
    COG_LOG_INFO("AtomSpace created");
    return COG_OK;
}

COGUTIL_API void atomspace_destroy(atomspace_t as) {
    if (!as) return;
    
    /* Destroy all atoms */
    pthread_rwlock_wrlock(&as->table_lock);
    
    for (size_t i = 0; i < as->table_size; i++) {
        atom_entry_t* entry = as->table[i];
        while (entry) {
            atom_entry_t* next = entry->next;
            atom_destroy(entry->atom);
            COG_FREE(entry);
            entry = next;
        }
    }
    
    COG_FREE(as->table);
    pthread_rwlock_unlock(&as->table_lock);
    
    /* Destroy indexes */
    for (size_t i = 0; i < as->indexes.bucket_count; i++) {
        atom_entry_t* entry = as->indexes.by_type[i];
        while (entry) {
            atom_entry_t* next = entry->next;
            COG_FREE(entry);
            entry = next;
        }
        entry = as->indexes.by_name[i];
        while (entry) {
            atom_entry_t* next = entry->next;
            COG_FREE(entry);
            entry = next;
        }
    }
    COG_FREE(as->indexes.by_type);
    COG_FREE(as->indexes.by_name);
    
    /* Destroy type hierarchy */
    COG_FREE(as->types);
    
    /* Destroy attention bank */
    COG_FREE(as->attention.attentional_focus);
    
    /* Destroy locks */
    pthread_rwlock_destroy(&as->table_lock);
    pthread_rwlock_destroy(&as->index_lock);
    pthread_rwlock_destroy(&as->type_lock);
    pthread_mutex_destroy(&as->handle_lock);
    pthread_mutex_destroy(&as->stats_lock);
    pthread_mutex_destroy(&as->attention.lock);
    
    COG_FREE(as);
    
    COG_LOG_INFO("AtomSpace destroyed");
}

/*===========================================================================
 * Atom Addition
 *===========================================================================*/

static atom_handle_t generate_handle(atomspace_t as) {
    pthread_mutex_lock(&as->handle_lock);
    atom_handle_t handle = as->next_handle++;
    pthread_mutex_unlock(&as->handle_lock);
    return handle;
}

static void table_insert(atomspace_t as, atom_t* atom) {
    uint64_t hash = hash_handle(atom->handle);
    size_t index = hash % as->table_size;
    
    atom_entry_t* entry = COG_CALLOC(1, sizeof(atom_entry_t));
    entry->atom = atom;
    entry->next = as->table[index];
    as->table[index] = entry;
    as->atom_count++;
}

static void index_insert(atomspace_t as, atom_t* atom) {
    /* Index by type */
    size_t type_index = atom->type % as->indexes.bucket_count;
    atom_entry_t* entry = COG_CALLOC(1, sizeof(atom_entry_t));
    entry->atom = atom;
    entry->next = as->indexes.by_type[type_index];
    as->indexes.by_type[type_index] = entry;
    
    /* Index by name (for nodes) */
    if (atom->name) {
        size_t name_index = hash_string(atom->name) % as->indexes.bucket_count;
        entry = COG_CALLOC(1, sizeof(atom_entry_t));
        entry->atom = atom;
        entry->next = as->indexes.by_name[name_index];
        as->indexes.by_name[name_index] = entry;
    }
}

COGUTIL_API atom_handle_t atomspace_add_node(
    atomspace_t as,
    atom_type_t type,
    const char* name,
    const truth_value_t* tv
) {
    if (!as || !name) return ATOM_HANDLE_INVALID;
    
    /* Check if node already exists */
    atom_handle_t existing = atomspace_get_node(as, type, name);
    if (existing != ATOM_HANDLE_INVALID) {
        if (tv) {
            atomspace_set_tv(as, existing, tv);
        }
        return existing;
    }
    
    /* Create new node */
    atom_t* atom = atom_create(type);
    if (!atom) return ATOM_HANDLE_INVALID;
    
    atom->handle = generate_handle(as);
    atom->name = COG_STRDUP(name);
    
    if (tv) {
        memcpy(&atom->tv, tv, sizeof(truth_value_t));
    }
    
    /* Insert into table and indexes */
    pthread_rwlock_wrlock(&as->table_lock);
    table_insert(as, atom);
    pthread_rwlock_unlock(&as->table_lock);
    
    pthread_rwlock_wrlock(&as->index_lock);
    index_insert(as, atom);
    pthread_rwlock_unlock(&as->index_lock);
    
    /* Update stats */
    pthread_mutex_lock(&as->stats_lock);
    as->stats.node_count++;
    as->stats.total_atoms++;
    pthread_mutex_unlock(&as->stats_lock);
    
    return atom->handle;
}

COGUTIL_API atom_handle_t atomspace_add_link(
    atomspace_t as,
    atom_type_t type,
    const atom_handle_t* outgoing,
    size_t outgoing_count,
    const truth_value_t* tv
) {
    if (!as || (outgoing_count > 0 && !outgoing)) return ATOM_HANDLE_INVALID;
    
    /* Check if link already exists */
    atom_handle_t existing = atomspace_get_link(as, type, outgoing, outgoing_count);
    if (existing != ATOM_HANDLE_INVALID) {
        if (tv) {
            atomspace_set_tv(as, existing, tv);
        }
        return existing;
    }
    
    /* Verify all outgoing atoms exist */
    for (size_t i = 0; i < outgoing_count; i++) {
        if (!atomspace_get_atom(as, outgoing[i])) {
            COG_LOG_WARN("Cannot create link: outgoing atom %lu not found", outgoing[i]);
            return ATOM_HANDLE_INVALID;
        }
    }
    
    /* Create new link */
    atom_t* atom = atom_create(type);
    if (!atom) return ATOM_HANDLE_INVALID;
    
    atom->handle = generate_handle(as);
    atom->outgoing_count = outgoing_count;
    
    if (outgoing_count > 0) {
        atom->outgoing = COG_CALLOC(outgoing_count, sizeof(atom_handle_t));
        memcpy(atom->outgoing, outgoing, outgoing_count * sizeof(atom_handle_t));
    }
    
    if (tv) {
        memcpy(&atom->tv, tv, sizeof(truth_value_t));
    }
    
    /* Insert into table and indexes */
    pthread_rwlock_wrlock(&as->table_lock);
    table_insert(as, atom);
    pthread_rwlock_unlock(&as->table_lock);
    
    pthread_rwlock_wrlock(&as->index_lock);
    index_insert(as, atom);
    pthread_rwlock_unlock(&as->index_lock);
    
    /* Add incoming references to outgoing atoms */
    for (size_t i = 0; i < outgoing_count; i++) {
        atom_t* target = atomspace_get_atom(as, outgoing[i]);
        if (target) {
            atom_add_incoming(target, atom->handle);
        }
    }
    
    /* Update stats */
    pthread_mutex_lock(&as->stats_lock);
    as->stats.link_count++;
    as->stats.total_atoms++;
    pthread_mutex_unlock(&as->stats_lock);
    
    return atom->handle;
}

/*===========================================================================
 * Atom Retrieval
 *===========================================================================*/

COGUTIL_API atom_t* atomspace_get_atom(atomspace_t as, atom_handle_t handle) {
    if (!as || handle == ATOM_HANDLE_INVALID) return NULL;
    
    pthread_rwlock_rdlock(&as->table_lock);
    
    uint64_t hash = hash_handle(handle);
    size_t index = hash % as->table_size;
    
    atom_entry_t* entry = as->table[index];
    while (entry) {
        if (entry->atom->handle == handle) {
            pthread_rwlock_unlock(&as->table_lock);
            return entry->atom;
        }
        entry = entry->next;
    }
    
    pthread_rwlock_unlock(&as->table_lock);
    return NULL;
}

COGUTIL_API atom_handle_t atomspace_get_node(
    atomspace_t as,
    atom_type_t type,
    const char* name
) {
    if (!as || !name) return ATOM_HANDLE_INVALID;
    
    pthread_rwlock_rdlock(&as->index_lock);
    
    size_t name_index = hash_string(name) % as->indexes.bucket_count;
    atom_entry_t* entry = as->indexes.by_name[name_index];
    
    while (entry) {
        if (entry->atom->type == type && 
            entry->atom->name && 
            strcmp(entry->atom->name, name) == 0) {
            atom_handle_t handle = entry->atom->handle;
            pthread_rwlock_unlock(&as->index_lock);
            return handle;
        }
        entry = entry->next;
    }
    
    pthread_rwlock_unlock(&as->index_lock);
    return ATOM_HANDLE_INVALID;
}

COGUTIL_API atom_handle_t atomspace_get_link(
    atomspace_t as,
    atom_type_t type,
    const atom_handle_t* outgoing,
    size_t outgoing_count
) {
    if (!as) return ATOM_HANDLE_INVALID;
    
    pthread_rwlock_rdlock(&as->index_lock);
    
    size_t type_index = type % as->indexes.bucket_count;
    atom_entry_t* entry = as->indexes.by_type[type_index];
    
    while (entry) {
        if (entry->atom->type == type &&
            entry->atom->outgoing_count == outgoing_count) {
            bool match = true;
            for (size_t i = 0; i < outgoing_count && match; i++) {
                if (entry->atom->outgoing[i] != outgoing[i]) {
                    match = false;
                }
            }
            if (match) {
                atom_handle_t handle = entry->atom->handle;
                pthread_rwlock_unlock(&as->index_lock);
                return handle;
            }
        }
        entry = entry->next;
    }
    
    pthread_rwlock_unlock(&as->index_lock);
    return ATOM_HANDLE_INVALID;
}

/*===========================================================================
 * Atom Properties
 *===========================================================================*/

COGUTIL_API atom_type_t atomspace_get_type(atomspace_t as, atom_handle_t handle) {
    atom_t* atom = atomspace_get_atom(as, handle);
    return atom ? atom->type : 0;
}

COGUTIL_API const char* atomspace_get_name(atomspace_t as, atom_handle_t handle) {
    atom_t* atom = atomspace_get_atom(as, handle);
    return atom ? atom->name : NULL;
}

COGUTIL_API cog_result_t atomspace_get_outgoing(
    atomspace_t as,
    atom_handle_t handle,
    atom_handle_t** outgoing,
    size_t* count
) {
    if (!as || !outgoing || !count) return COG_ERROR_INVALID_PARAM;
    
    atom_t* atom = atomspace_get_atom(as, handle);
    if (!atom) return COG_ERROR_NOT_FOUND;
    
    pthread_rwlock_rdlock(&atom->lock);
    
    *count = atom->outgoing_count;
    if (atom->outgoing_count > 0) {
        *outgoing = COG_CALLOC(atom->outgoing_count, sizeof(atom_handle_t));
        memcpy(*outgoing, atom->outgoing, atom->outgoing_count * sizeof(atom_handle_t));
    } else {
        *outgoing = NULL;
    }
    
    pthread_rwlock_unlock(&atom->lock);
    return COG_OK;
}

COGUTIL_API cog_result_t atomspace_get_incoming(
    atomspace_t as,
    atom_handle_t handle,
    atom_handle_t** incoming,
    size_t* count
) {
    if (!as || !incoming || !count) return COG_ERROR_INVALID_PARAM;
    
    atom_t* atom = atomspace_get_atom(as, handle);
    if (!atom) return COG_ERROR_NOT_FOUND;
    
    pthread_rwlock_rdlock(&atom->lock);
    
    *count = atom->incoming_count;
    if (atom->incoming_count > 0) {
        *incoming = COG_CALLOC(atom->incoming_count, sizeof(atom_handle_t));
        memcpy(*incoming, atom->incoming, atom->incoming_count * sizeof(atom_handle_t));
    } else {
        *incoming = NULL;
    }
    
    pthread_rwlock_unlock(&atom->lock);
    return COG_OK;
}

/*===========================================================================
 * Truth Values
 *===========================================================================*/

COGUTIL_API cog_result_t atomspace_get_tv(
    atomspace_t as,
    atom_handle_t handle,
    truth_value_t* tv
) {
    if (!as || !tv) return COG_ERROR_INVALID_PARAM;
    
    atom_t* atom = atomspace_get_atom(as, handle);
    if (!atom) return COG_ERROR_NOT_FOUND;
    
    pthread_rwlock_rdlock(&atom->lock);
    memcpy(tv, &atom->tv, sizeof(truth_value_t));
    pthread_rwlock_unlock(&atom->lock);
    
    return COG_OK;
}

COGUTIL_API cog_result_t atomspace_set_tv(
    atomspace_t as,
    atom_handle_t handle,
    const truth_value_t* tv
) {
    if (!as || !tv) return COG_ERROR_INVALID_PARAM;
    
    atom_t* atom = atomspace_get_atom(as, handle);
    if (!atom) return COG_ERROR_NOT_FOUND;
    
    pthread_rwlock_wrlock(&atom->lock);
    memcpy(&atom->tv, tv, sizeof(truth_value_t));
    pthread_rwlock_unlock(&atom->lock);
    
    return COG_OK;
}

COGUTIL_API truth_value_t tv_simple(double strength, double confidence) {
    truth_value_t tv = {
        .type = TV_SIMPLE,
        .strength = strength,
        .confidence = confidence,
        .count = 0
    };
    return tv;
}

COGUTIL_API truth_value_t tv_count(double strength, double confidence, uint64_t count) {
    truth_value_t tv = {
        .type = TV_COUNT,
        .strength = strength,
        .confidence = confidence,
        .count = count
    };
    return tv;
}

COGUTIL_API truth_value_t tv_merge(const truth_value_t* a, const truth_value_t* b) {
    truth_value_t result = {0};
    
    if (!a || !b) return result;
    
    /* Revision formula */
    double k = 1.0; /* Confidence discount factor */
    double conf_a = a->confidence;
    double conf_b = b->confidence;
    
    if (conf_a + conf_b > 0) {
        result.strength = (a->strength * conf_a + b->strength * conf_b) / (conf_a + conf_b);
        result.confidence = (conf_a + conf_b - conf_a * conf_b) * k;
    }
    
    result.type = TV_SIMPLE;
    return result;
}

/*===========================================================================
 * Attention Values
 *===========================================================================*/

COGUTIL_API cog_result_t atomspace_get_av(
    atomspace_t as,
    atom_handle_t handle,
    attention_value_t* av
) {
    if (!as || !av) return COG_ERROR_INVALID_PARAM;
    
    atom_t* atom = atomspace_get_atom(as, handle);
    if (!atom) return COG_ERROR_NOT_FOUND;
    
    pthread_rwlock_rdlock(&atom->lock);
    memcpy(av, &atom->av, sizeof(attention_value_t));
    pthread_rwlock_unlock(&atom->lock);
    
    return COG_OK;
}

COGUTIL_API cog_result_t atomspace_set_av(
    atomspace_t as,
    atom_handle_t handle,
    const attention_value_t* av
) {
    if (!as || !av) return COG_ERROR_INVALID_PARAM;
    
    atom_t* atom = atomspace_get_atom(as, handle);
    if (!atom) return COG_ERROR_NOT_FOUND;
    
    pthread_rwlock_wrlock(&atom->lock);
    memcpy(&atom->av, av, sizeof(attention_value_t));
    pthread_rwlock_unlock(&atom->lock);
    
    /* Update attentional focus if needed */
    if (av->sti >= as->attention.sti_threshold) {
        pthread_mutex_lock(&as->attention.lock);
        
        /* Check if already in focus */
        bool in_focus = false;
        for (size_t i = 0; i < as->attention.focus_size; i++) {
            if (as->attention.attentional_focus[i] == handle) {
                in_focus = true;
                break;
            }
        }
        
        if (!in_focus && as->attention.focus_size < as->attention.focus_capacity) {
            as->attention.attentional_focus[as->attention.focus_size++] = handle;
        }
        
        pthread_mutex_unlock(&as->attention.lock);
    }
    
    return COG_OK;
}

COGUTIL_API cog_result_t atomspace_stimulate(
    atomspace_t as,
    atom_handle_t handle,
    int16_t stimulus
) {
    if (!as) return COG_ERROR_INVALID_PARAM;
    
    atom_t* atom = atomspace_get_atom(as, handle);
    if (!atom) return COG_ERROR_NOT_FOUND;
    
    pthread_rwlock_wrlock(&atom->lock);
    atom->av.sti += stimulus;
    pthread_rwlock_unlock(&atom->lock);
    
    return COG_OK;
}

/*===========================================================================
 * Query Operations
 *===========================================================================*/

COGUTIL_API cog_result_t atomspace_get_atoms_by_type(
    atomspace_t as,
    atom_type_t type,
    bool include_subtypes,
    atom_handle_t** handles,
    size_t* count
) {
    if (!as || !handles || !count) return COG_ERROR_INVALID_PARAM;
    
    /* Count matching atoms */
    pthread_rwlock_rdlock(&as->index_lock);
    
    size_t match_count = 0;
    size_t type_index = type % as->indexes.bucket_count;
    atom_entry_t* entry = as->indexes.by_type[type_index];
    
    while (entry) {
        if (entry->atom->type == type) {
            match_count++;
        }
        entry = entry->next;
    }
    
    /* Allocate and fill result */
    *handles = COG_CALLOC(match_count, sizeof(atom_handle_t));
    *count = 0;
    
    entry = as->indexes.by_type[type_index];
    while (entry && *count < match_count) {
        if (entry->atom->type == type) {
            (*handles)[(*count)++] = entry->atom->handle;
        }
        entry = entry->next;
    }
    
    pthread_rwlock_unlock(&as->index_lock);
    return COG_OK;
}

/*===========================================================================
 * Statistics
 *===========================================================================*/

COGUTIL_API cog_result_t atomspace_get_stats(atomspace_t as, atomspace_stats_t* stats) {
    if (!as || !stats) return COG_ERROR_INVALID_PARAM;
    
    pthread_mutex_lock(&as->stats_lock);
    memcpy(stats, &as->stats, sizeof(atomspace_stats_t));
    pthread_mutex_unlock(&as->stats_lock);
    
    return COG_OK;
}

COGUTIL_API size_t atomspace_size(atomspace_t as) {
    if (!as) return 0;
    return as->atom_count;
}
