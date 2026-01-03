/**
 * @file unified_bridge.h
 * @brief Unified Integration Bridge for CoGWXP-OS9
 * 
 * This bridge provides seamless integration between all major subsystems:
 * - Windows NT Kernel (NTOS)
 * - OpenCog (AtomSpace, PLN, CogServer)
 * - Plan 9 (9P Protocol)
 * - Inferno (Dis VM, Limbo)
 * - CogW7OS Cognitive Extensions
 * 
 * The bridge implements the b9/p9/j9 architectural model for cognitive synergy.
 * 
 * @copyright Mixed Licenses (see individual components)
 */

#ifndef _COGWXP_UNIFIED_BRIDGE_H_
#define _COGWXP_UNIFIED_BRIDGE_H_

#include "../../../cogwxp/opencog/cogutil/cogutil.h"
#include "../../../cogwxp/opencog/atomspace/atomspace.h"
#include "../../../cogwxp/opencog/pln/pln.h"
#include "../../../cogwxp/plan9/9p/9p.h"
#include "../../../cogwxp/inferno/dis/dis.h"
#include "../../../cogwxp/cogw7os/kernel/cogw7os.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Unified Bridge Version
 *===========================================================================*/

#define UNIFIED_BRIDGE_VERSION  "1.0.0"

/*===========================================================================
 * b9/p9/j9 Architectural Model
 *===========================================================================*/

/**
 * b9 - Binary/Base Files (Rooted Trees)
 * 
 * Represents connection edge patterns to localhost terminal nodes.
 * Maps to b-files (binary or base files).
 * 
 * In the unified bridge:
 * - NT kernel objects
 * - AtomSpace nodes (leaf atoms)
 * - 9P file descriptors
 * - Dis value primitives
 */
typedef struct b9_node {
    uint64_t id;
    enum {
        B9_TYPE_NT_OBJECT,
        B9_TYPE_ATOM_NODE,
        B9_TYPE_9P_FID,
        B9_TYPE_DIS_VALUE
    } type;
    union {
        cogw7_handle_t nt_handle;
        atom_handle_t atom;
        uint32_t fid;
        dis_value_t dis_val;
    } data;
    struct b9_node* parent;
    struct b9_node** children;
    size_t child_count;
} b9_node_t;

/**
 * p9 - Process/Module Files (Nested Scopes)
 * 
 * Represents execution context membranes for globalhost thread pools.
 * Maps to m-files (membrane or module files).
 * 
 * In the unified bridge:
 * - NT processes and threads
 * - AtomSpace contexts (sub-atomspaces)
 * - 9P namespaces
 * - Dis modules and frames
 */
typedef struct p9_scope {
    uint64_t id;
    char* name;
    enum {
        P9_SCOPE_NT_PROCESS,
        P9_SCOPE_ATOMSPACE,
        P9_SCOPE_9P_NAMESPACE,
        P9_SCOPE_DIS_MODULE
    } type;
    union {
        cogw7_process_t* process;
        atomspace_t atomspace;
        p9_namespace_t namespace;
        dis_module_t* module;
    } context;
    struct p9_scope* parent;
    struct p9_scope** children;
    size_t child_count;
    
    /* Thread pool for this scope */
    cog_threadpool_t thread_pool;
} p9_scope_t;

/**
 * j9 - Distributed/Dis Files (Elementary Differentials)
 * 
 * Represents distribution compute gradients for orgalhost topology net.
 * Maps to dis-files (distributed or virtual machine files).
 * 
 * In the unified bridge:
 * - Distributed inference results
 * - 9P network connections
 * - Dis channels
 * - Inter-agent messages
 */
typedef struct j9_gradient {
    uint64_t id;
    enum {
        J9_TYPE_INFERENCE,
        J9_TYPE_9P_CONNECTION,
        J9_TYPE_DIS_CHANNEL,
        J9_TYPE_AGENT_MESSAGE
    } type;
    
    /* Source and destination */
    p9_scope_t* source_scope;
    p9_scope_t* dest_scope;
    
    /* Gradient data */
    union {
        struct {
            atom_handle_t premise;
            atom_handle_t conclusion;
            truth_value_t delta_tv;
        } inference;
        struct {
            p9_conn_t connection;
            char* remote_address;
        } p9_conn;
        dis_channel_t* channel;
        struct {
            cogw7_agent_t* sender;
            cogw7_agent_t* receiver;
            dis_value_t message;
        } agent_msg;
    } data;
    
    /* Compute weight */
    double weight;
} j9_gradient_t;

/*===========================================================================
 * Unified Bridge Handle
 *===========================================================================*/

typedef struct unified_bridge* unified_bridge_t;

/*===========================================================================
 * Bridge Lifecycle
 *===========================================================================*/

typedef struct unified_bridge_config {
    /* Component references */
    atomspace_t global_atomspace;
    pln_engine_t global_pln;
    dis_vm_t* global_dis_vm;
    p9_server_t p9_server;
    
    /* b9 configuration */
    size_t max_b9_nodes;
    
    /* p9 configuration */
    size_t max_p9_scopes;
    size_t default_thread_pool_size;
    
    /* j9 configuration */
    size_t max_j9_gradients;
    double default_gradient_weight;
    
    /* Integration options */
    bool enable_auto_sync;
    uint32_t sync_interval_ms;
    bool enable_distributed_inference;
} unified_bridge_config_t;

COGUTIL_API unified_bridge_config_t unified_bridge_config_default(void);
COGUTIL_API unified_bridge_t unified_bridge_create(unified_bridge_config_t* config);
COGUTIL_API void             unified_bridge_destroy(unified_bridge_t bridge);

/*===========================================================================
 * b9 Operations (Rooted Tree / Binary Files)
 *===========================================================================*/

COGUTIL_API b9_node_t* b9_create_from_nt(unified_bridge_t bridge, cogw7_handle_t handle);
COGUTIL_API b9_node_t* b9_create_from_atom(unified_bridge_t bridge, atom_handle_t atom);
COGUTIL_API b9_node_t* b9_create_from_9p(unified_bridge_t bridge, uint32_t fid);
COGUTIL_API b9_node_t* b9_create_from_dis(unified_bridge_t bridge, dis_value_t* value);

COGUTIL_API void       b9_destroy(unified_bridge_t bridge, b9_node_t* node);
COGUTIL_API b9_node_t* b9_get_root(unified_bridge_t bridge);
COGUTIL_API b9_node_t* b9_find_by_id(unified_bridge_t bridge, uint64_t id);

/* Tree operations */
COGUTIL_API cog_result_t b9_attach(b9_node_t* parent, b9_node_t* child);
COGUTIL_API cog_result_t b9_detach(b9_node_t* node);
COGUTIL_API b9_node_t**  b9_get_path(b9_node_t* node, size_t* path_length);

/* Conversion */
COGUTIL_API atom_handle_t b9_to_atom(unified_bridge_t bridge, b9_node_t* node);
COGUTIL_API uint32_t      b9_to_9p_fid(unified_bridge_t bridge, b9_node_t* node);

/*===========================================================================
 * p9 Operations (Nested Scopes / Module Files)
 *===========================================================================*/

COGUTIL_API p9_scope_t* p9_create_from_process(unified_bridge_t bridge, cogw7_process_t* process);
COGUTIL_API p9_scope_t* p9_create_from_atomspace(unified_bridge_t bridge, atomspace_t atomspace);
COGUTIL_API p9_scope_t* p9_create_from_namespace(unified_bridge_t bridge, p9_namespace_t namespace);
COGUTIL_API p9_scope_t* p9_create_from_module(unified_bridge_t bridge, dis_module_t* module);

COGUTIL_API void        p9_destroy(unified_bridge_t bridge, p9_scope_t* scope);
COGUTIL_API p9_scope_t* p9_get_global_scope(unified_bridge_t bridge);
COGUTIL_API p9_scope_t* p9_find_by_name(unified_bridge_t bridge, const char* name);

/* Scope hierarchy */
COGUTIL_API cog_result_t p9_nest(p9_scope_t* parent, p9_scope_t* child);
COGUTIL_API cog_result_t p9_unnest(p9_scope_t* scope);
COGUTIL_API p9_scope_t*  p9_get_parent(p9_scope_t* scope);

/* Thread pool management */
COGUTIL_API cog_result_t p9_submit_task(p9_scope_t* scope, cog_thread_func_t func, void* arg);
COGUTIL_API size_t       p9_get_active_threads(p9_scope_t* scope);

/* Cross-scope operations */
COGUTIL_API cog_result_t p9_sync_atomspaces(p9_scope_t* source, p9_scope_t* dest);
COGUTIL_API cog_result_t p9_merge_namespaces(p9_scope_t* source, p9_scope_t* dest);

/*===========================================================================
 * j9 Operations (Distributed Gradients / Dis Files)
 *===========================================================================*/

COGUTIL_API j9_gradient_t* j9_create_inference(unified_bridge_t bridge, 
                                                atom_handle_t premise, 
                                                atom_handle_t conclusion,
                                                truth_value_t delta_tv);

COGUTIL_API j9_gradient_t* j9_create_connection(unified_bridge_t bridge,
                                                 p9_conn_t connection,
                                                 const char* remote_address);

COGUTIL_API j9_gradient_t* j9_create_channel(unified_bridge_t bridge,
                                              dis_channel_t* channel);

COGUTIL_API j9_gradient_t* j9_create_message(unified_bridge_t bridge,
                                              cogw7_agent_t* sender,
                                              cogw7_agent_t* receiver,
                                              dis_value_t* message);

COGUTIL_API void j9_destroy(unified_bridge_t bridge, j9_gradient_t* gradient);

/* Gradient propagation */
COGUTIL_API cog_result_t j9_propagate(j9_gradient_t* gradient);
COGUTIL_API cog_result_t j9_propagate_all(unified_bridge_t bridge);

/* Distributed inference */
COGUTIL_API cog_result_t j9_distributed_infer(unified_bridge_t bridge,
                                               atom_handle_t source,
                                               p9_scope_t** target_scopes,
                                               size_t scope_count,
                                               atom_handle_t** results,
                                               size_t* result_count);

/*===========================================================================
 * Unified Resource Access
 *===========================================================================*/

/**
 * Unified resource path format:
 * 
 * /nt/<object_type>/<handle>           - NT kernel objects
 * /atoms/<type>/<name>                 - AtomSpace atoms
 * /9p/<server>/<path>                  - 9P resources
 * /dis/<module>/<function>             - Dis VM resources
 * /agents/<name>                       - Cognitive agents
 * /b9/<id>                             - b9 nodes
 * /p9/<name>                           - p9 scopes
 * /j9/<id>                             - j9 gradients
 */

typedef enum {
    UNIFIED_RESOURCE_NT,
    UNIFIED_RESOURCE_ATOM,
    UNIFIED_RESOURCE_9P,
    UNIFIED_RESOURCE_DIS,
    UNIFIED_RESOURCE_AGENT,
    UNIFIED_RESOURCE_B9,
    UNIFIED_RESOURCE_P9,
    UNIFIED_RESOURCE_J9
} unified_resource_type_t;

typedef struct unified_resource {
    unified_resource_type_t type;
    char* path;
    union {
        cogw7_handle_t nt_handle;
        atom_handle_t atom;
        uint32_t fid;
        dis_value_t dis_val;
        cogw7_agent_t* agent;
        b9_node_t* b9_node;
        p9_scope_t* p9_scope;
        j9_gradient_t* j9_gradient;
    } resource;
} unified_resource_t;

COGUTIL_API unified_resource_t* unified_open(unified_bridge_t bridge, const char* path);
COGUTIL_API void                unified_close(unified_resource_t* resource);
COGUTIL_API cog_result_t        unified_read(unified_resource_t* resource, void* buf, size_t size, size_t* nread);
COGUTIL_API cog_result_t        unified_write(unified_resource_t* resource, const void* data, size_t size, size_t* nwritten);
COGUTIL_API cog_result_t        unified_stat(unified_resource_t* resource, p9_stat_t* stat);

/*===========================================================================
 * Event System
 *===========================================================================*/

typedef enum {
    UNIFIED_EVENT_ATOM_CREATED,
    UNIFIED_EVENT_ATOM_DELETED,
    UNIFIED_EVENT_ATOM_TV_CHANGED,
    UNIFIED_EVENT_INFERENCE_COMPLETED,
    UNIFIED_EVENT_AGENT_STATE_CHANGED,
    UNIFIED_EVENT_SCOPE_CREATED,
    UNIFIED_EVENT_SCOPE_DESTROYED,
    UNIFIED_EVENT_GRADIENT_PROPAGATED,
    UNIFIED_EVENT_9P_MESSAGE,
    UNIFIED_EVENT_DIS_THREAD_SPAWNED,
    UNIFIED_EVENT_DIS_THREAD_TERMINATED
} unified_event_type_t;

typedef struct unified_event {
    unified_event_type_t type;
    uint64_t timestamp;
    void* source;
    void* data;
} unified_event_t;

typedef void (*unified_event_handler_t)(unified_event_t* event, void* user_data);

COGUTIL_API cog_result_t unified_subscribe(unified_bridge_t bridge, 
                                            unified_event_type_t type,
                                            unified_event_handler_t handler,
                                            void* user_data);

COGUTIL_API cog_result_t unified_unsubscribe(unified_bridge_t bridge,
                                              unified_event_type_t type,
                                              unified_event_handler_t handler);

COGUTIL_API cog_result_t unified_emit(unified_bridge_t bridge, unified_event_t* event);

/*===========================================================================
 * Synchronization
 *===========================================================================*/

COGUTIL_API cog_result_t unified_sync_all(unified_bridge_t bridge);
COGUTIL_API cog_result_t unified_sync_atomspace_to_9p(unified_bridge_t bridge);
COGUTIL_API cog_result_t unified_sync_9p_to_atomspace(unified_bridge_t bridge);
COGUTIL_API cog_result_t unified_sync_agents_to_dis(unified_bridge_t bridge);

/*===========================================================================
 * Statistics
 *===========================================================================*/

typedef struct unified_bridge_stats {
    /* Node counts */
    size_t b9_node_count;
    size_t p9_scope_count;
    size_t j9_gradient_count;
    
    /* Operations */
    uint64_t total_syncs;
    uint64_t total_events;
    uint64_t total_gradients_propagated;
    
    /* Performance */
    double avg_sync_time_ms;
    double avg_propagation_time_ms;
    
    /* Memory */
    size_t bridge_memory_usage;
} unified_bridge_stats_t;

COGUTIL_API void unified_get_stats(unified_bridge_t bridge, unified_bridge_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* _COGWXP_UNIFIED_BRIDGE_H_ */
