/**
 * @file limbo.h
 * @brief Limbo Language Runtime for CoGWXP-OS9
 * 
 * Limbo is the programming language for Inferno, compiled to Dis bytecode.
 * This header provides the runtime interface for Limbo programs.
 * 
 * @copyright MIT/Lucent Public License
 */

#ifndef _COGWXP_LIMBO_H_
#define _COGWXP_LIMBO_H_

#include "../dis/dis.h"
#include "../../opencog/atomspace/atomspace.h"
#include "../../opencog/pln/pln.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Limbo Types
 *===========================================================================*/

/* Limbo type descriptors */
typedef enum {
    LIMBO_TYPE_BYTE = 0,
    LIMBO_TYPE_INT,
    LIMBO_TYPE_BIG,
    LIMBO_TYPE_REAL,
    LIMBO_TYPE_STRING,
    LIMBO_TYPE_ARRAY,
    LIMBO_TYPE_LIST,
    LIMBO_TYPE_CHAN,
    LIMBO_TYPE_REF,
    LIMBO_TYPE_ADT,
    LIMBO_TYPE_FN,
    LIMBO_TYPE_TUPLE,
    LIMBO_TYPE_MODULE,
    LIMBO_TYPE_POLY,
    
    /* CoGWXP-OS9 extensions */
    LIMBO_TYPE_ATOM,
    LIMBO_TYPE_TRUTHVALUE,
    LIMBO_TYPE_ATOMSPACE
} limbo_type_t;

/*===========================================================================
 * Limbo String
 *===========================================================================*/

typedef struct limbo_string {
    size_t len;
    char* data;
    uint32_t ref_count;
} limbo_string_t;

COGUTIL_API limbo_string_t* limbo_string_new(const char* s);
COGUTIL_API limbo_string_t* limbo_string_new_len(const char* s, size_t len);
COGUTIL_API limbo_string_t* limbo_string_dup(limbo_string_t* s);
COGUTIL_API void            limbo_string_free(limbo_string_t* s);
COGUTIL_API limbo_string_t* limbo_string_concat(limbo_string_t* a, limbo_string_t* b);
COGUTIL_API int             limbo_string_cmp(limbo_string_t* a, limbo_string_t* b);
COGUTIL_API limbo_string_t* limbo_string_slice(limbo_string_t* s, size_t start, size_t end);

/*===========================================================================
 * Limbo Array
 *===========================================================================*/

typedef struct limbo_array {
    limbo_type_t elem_type;
    size_t len;
    void* data;
    uint32_t ref_count;
} limbo_array_t;

COGUTIL_API limbo_array_t* limbo_array_new(limbo_type_t elem_type, size_t len);
COGUTIL_API limbo_array_t* limbo_array_dup(limbo_array_t* a);
COGUTIL_API void           limbo_array_free(limbo_array_t* a);
COGUTIL_API void*          limbo_array_get(limbo_array_t* a, size_t index);
COGUTIL_API cog_result_t   limbo_array_set(limbo_array_t* a, size_t index, void* value);
COGUTIL_API limbo_array_t* limbo_array_slice(limbo_array_t* a, size_t start, size_t end);

/*===========================================================================
 * Limbo List
 *===========================================================================*/

typedef struct limbo_list_node {
    void* value;
    struct limbo_list_node* next;
} limbo_list_node_t;

typedef struct limbo_list {
    limbo_type_t elem_type;
    limbo_list_node_t* head;
    limbo_list_node_t* tail;
    size_t len;
    uint32_t ref_count;
} limbo_list_t;

COGUTIL_API limbo_list_t* limbo_list_new(limbo_type_t elem_type);
COGUTIL_API void          limbo_list_free(limbo_list_t* l);
COGUTIL_API cog_result_t  limbo_list_push(limbo_list_t* l, void* value);
COGUTIL_API void*         limbo_list_pop(limbo_list_t* l);
COGUTIL_API void*         limbo_list_head(limbo_list_t* l);
COGUTIL_API limbo_list_t* limbo_list_tail(limbo_list_t* l);
COGUTIL_API limbo_list_t* limbo_list_cons(void* value, limbo_list_t* l);

/*===========================================================================
 * Limbo Channel (typed wrapper around Dis channel)
 *===========================================================================*/

typedef struct limbo_chan {
    dis_channel_t* dis_chan;
    limbo_type_t elem_type;
    uint32_t ref_count;
} limbo_chan_t;

COGUTIL_API limbo_chan_t* limbo_chan_new(limbo_type_t elem_type, size_t buffer_size);
COGUTIL_API void          limbo_chan_free(limbo_chan_t* c);
COGUTIL_API cog_result_t  limbo_chan_send(limbo_chan_t* c, void* value);
COGUTIL_API cog_result_t  limbo_chan_recv(limbo_chan_t* c, void* value);
COGUTIL_API cog_result_t  limbo_chan_alt(limbo_chan_t** chans, size_t count, size_t* ready_index);

/*===========================================================================
 * Limbo ADT (Abstract Data Type)
 *===========================================================================*/

typedef struct limbo_adt {
    const char* name;
    size_t size;
    size_t field_count;
    struct {
        const char* name;
        limbo_type_t type;
        size_t offset;
    }* fields;
    void* data;
    uint32_t ref_count;
} limbo_adt_t;

COGUTIL_API limbo_adt_t* limbo_adt_new(const char* name, size_t size);
COGUTIL_API void         limbo_adt_free(limbo_adt_t* adt);
COGUTIL_API cog_result_t limbo_adt_add_field(limbo_adt_t* adt, const char* name, limbo_type_t type, size_t offset);
COGUTIL_API void*        limbo_adt_get_field(limbo_adt_t* adt, const char* name);
COGUTIL_API cog_result_t limbo_adt_set_field(limbo_adt_t* adt, const char* name, void* value);

/*===========================================================================
 * Limbo Module Interface
 *===========================================================================*/

typedef struct limbo_module {
    const char* name;
    const char* path;
    dis_module_t* dis_module;
    
    /* Exports */
    struct {
        const char* name;
        limbo_type_t type;
        void* value;
    }* exports;
    size_t export_count;
    
    uint32_t ref_count;
} limbo_module_t;

COGUTIL_API limbo_module_t* limbo_module_load(dis_vm_t* vm, const char* path);
COGUTIL_API void            limbo_module_unload(limbo_module_t* module);
COGUTIL_API void*           limbo_module_get_export(limbo_module_t* module, const char* name);

/*===========================================================================
 * Limbo Built-in Modules
 *===========================================================================*/

/* Sys module */
typedef struct limbo_sys {
    /* File descriptors */
    cog_result_t (*open)(const char* path, int mode, int* fd);
    cog_result_t (*create)(const char* path, int mode, int perm, int* fd);
    cog_result_t (*read)(int fd, void* buf, size_t n, size_t* nread);
    cog_result_t (*write)(int fd, const void* buf, size_t n, size_t* nwritten);
    cog_result_t (*seek)(int fd, int64_t off, int whence, int64_t* newoff);
    cog_result_t (*close)(int fd);
    
    /* Process control */
    cog_result_t (*spawn)(const char* path, limbo_list_t* args, int* pid);
    cog_result_t (*sleep)(int ms);
    void (*exit)(int status);
    
    /* Environment */
    limbo_string_t* (*getenv)(const char* name);
    cog_result_t (*setenv)(const char* name, const char* value);
    
    /* Time */
    int64_t (*millisec)(void);
    
    /* Printing */
    void (*print)(const char* fmt, ...);
    void (*fprint)(int fd, const char* fmt, ...);
} limbo_sys_t;

COGUTIL_API limbo_sys_t* limbo_sys_init(dis_vm_t* vm);

/* Math module */
typedef struct limbo_math {
    double (*sin)(double x);
    double (*cos)(double x);
    double (*tan)(double x);
    double (*asin)(double x);
    double (*acos)(double x);
    double (*atan)(double x);
    double (*atan2)(double y, double x);
    double (*sqrt)(double x);
    double (*pow)(double x, double y);
    double (*exp)(double x);
    double (*log)(double x);
    double (*log10)(double x);
    double (*floor)(double x);
    double (*ceil)(double x);
    double (*fabs)(double x);
    double (*fmod)(double x, double y);
} limbo_math_t;

COGUTIL_API limbo_math_t* limbo_math_init(dis_vm_t* vm);

/*===========================================================================
 * CoGWXP-OS9 Limbo Extensions
 *===========================================================================*/

/* AtomSpace module for Limbo */
typedef struct limbo_atomspace {
    /* AtomSpace handle */
    atomspace_t atomspace;
    
    /* Node operations */
    atom_handle_t (*add_node)(atomspace_t as, atom_type_t type, const char* name);
    atom_handle_t (*get_node)(atomspace_t as, atom_type_t type, const char* name);
    
    /* Link operations */
    atom_handle_t (*add_link)(atomspace_t as, atom_type_t type, atom_handle_t* outgoing, size_t arity);
    atom_handle_t (*get_link)(atomspace_t as, atom_type_t type, atom_handle_t* outgoing, size_t arity);
    
    /* Truth value operations */
    cog_result_t (*set_tv)(atomspace_t as, atom_handle_t atom, double strength, double confidence);
    cog_result_t (*get_tv)(atomspace_t as, atom_handle_t atom, double* strength, double* confidence);
    
    /* Query */
    atom_handle_t* (*query)(atomspace_t as, const char* pattern, size_t* count);
    
    /* Iteration */
    atom_handle_t* (*get_atoms_by_type)(atomspace_t as, atom_type_t type, size_t* count);
} limbo_atomspace_t;

COGUTIL_API limbo_atomspace_t* limbo_atomspace_init(dis_vm_t* vm, atomspace_t atomspace);

/* PLN module for Limbo */
typedef struct limbo_pln {
    pln_engine_t engine;
    
    /* Inference */
    atom_handle_t* (*forward_chain)(pln_engine_t pln, atom_handle_t source, size_t max_steps, size_t* count);
    atom_handle_t* (*backward_chain)(pln_engine_t pln, atom_handle_t target, size_t max_steps, size_t* count);
    
    /* Rule management */
    cog_result_t (*add_rule)(pln_engine_t pln, const char* rule_name);
    cog_result_t (*remove_rule)(pln_engine_t pln, const char* rule_name);
} limbo_pln_t;

COGUTIL_API limbo_pln_t* limbo_pln_init(dis_vm_t* vm, pln_engine_t pln);

/* Agent module for Limbo */
typedef struct limbo_agent {
    void* cogserver;
    
    /* Agent operations */
    uint64_t (*create)(void* cs, const char* name, int type);
    cog_result_t (*destroy)(void* cs, uint64_t agent_id);
    cog_result_t (*start)(void* cs, uint64_t agent_id);
    cog_result_t (*stop)(void* cs, uint64_t agent_id);
    
    /* Messaging */
    cog_result_t (*send)(void* cs, uint64_t from, uint64_t to, atom_handle_t content);
    cog_result_t (*recv)(void* cs, uint64_t agent_id, atom_handle_t* content, int timeout_ms);
    
    /* Goals */
    cog_result_t (*set_goal)(void* cs, uint64_t agent_id, atom_handle_t goal);
} limbo_agent_t;

COGUTIL_API limbo_agent_t* limbo_agent_init(dis_vm_t* vm, void* cogserver);

/*===========================================================================
 * Limbo Runtime Initialization
 *===========================================================================*/

typedef struct limbo_runtime {
    dis_vm_t* vm;
    limbo_sys_t* sys;
    limbo_math_t* math;
    limbo_atomspace_t* atomspace;
    limbo_pln_t* pln;
    limbo_agent_t* agent;
} limbo_runtime_t;

COGUTIL_API limbo_runtime_t* limbo_runtime_create(dis_vm_t* vm);
COGUTIL_API void             limbo_runtime_destroy(limbo_runtime_t* rt);
COGUTIL_API cog_result_t     limbo_runtime_init_cogwxp(limbo_runtime_t* rt, atomspace_t as, pln_engine_t pln, void* cogserver);

#ifdef __cplusplus
}
#endif

#endif /* _COGWXP_LIMBO_H_ */
