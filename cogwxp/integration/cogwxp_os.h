/**
 * @file cogwxp_os.h
 * @brief CoGWXP-OS - Main Integration Layer Header
 * 
 * This header provides the unified API for the CoGWXP-OS cognitive operating
 * system, integrating Windows XP NT kernel, OpenCog, Plan 9, and Inferno-OS.
 * 
 * @copyright Mixed Licenses
 */

#ifndef _COGWXP_OS_H_
#define _COGWXP_OS_H_

/* Core components */
#include "../opencog/cogutil/cogutil.h"
#include "../opencog/atomspace/atomspace.h"
#include "../opencog/pln/pln.h"
#include "../opencog/cogserver/cogserver.h"

/* Distributed computing */
#include "../plan9/9p/9p.h"
#include "../inferno/dis/dis.h"

/* Cognitive kernel */
#include "../cogw7os/kernel/cogw7os.h"

/* Integration bridges */
#include "bridges/unified_bridge.h"

/* Advanced cognitive systems */
#include "niche_construction.h"
#include "beast_mode.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * CoGWXP-OS Version and Identity
 *===========================================================================*/

#define COGWXP_OS_VERSION_MAJOR     1
#define COGWXP_OS_VERSION_MINOR     0
#define COGWXP_OS_VERSION_PATCH     0
#define COGWXP_OS_VERSION_STRING    "1.0.0"
#define COGWXP_OS_CODENAME          "Cognitive Synergy"
#define COGWXP_OS_FULL_NAME         "CoGWXP-OS9: Cognitive Windows XP + Plan9 AGI Operating System"

/*===========================================================================
 * System State
 *===========================================================================*/

typedef enum {
    COGWXP_STATE_UNINITIALIZED = 0,
    COGWXP_STATE_INITIALIZING,
    COGWXP_STATE_RUNNING,
    COGWXP_STATE_SHUTTING_DOWN,
    COGWXP_STATE_HALTED,
    COGWXP_STATE_ERROR
} cogwxp_state_t;

/*===========================================================================
 * System Configuration
 *===========================================================================*/

typedef struct cogwxp_config {
    /* System identity */
    const char* system_name;
    
    /* Memory configuration */
    size_t kernel_heap_size;
    size_t atomspace_initial_size;
    size_t dis_heap_size;
    
    /* Threading */
    size_t worker_thread_count;
    size_t agent_thread_pool_size;
    
    /* OpenCog configuration */
    pln_config_t pln_config;
    cogserver_config_t cogserver_config;
    
    /* Dis VM configuration */
    dis_vm_config_t dis_config;
    
    /* 9P configuration */
    bool enable_9p_server;
    const char* p9_listen_address;
    uint16_t p9_listen_port;
    
    /* Networking */
    bool enable_distributed;
    const char** peer_addresses;
    size_t peer_count;
    
    /* Logging */
    cog_log_level_t log_level;
    const char* log_file;
    
    /* b9/p9/j9 configuration */
    size_t max_b9_nodes;
    size_t max_p9_scopes;
    size_t max_j9_gradients;
    
    /* Niche construction configuration */
    bool enable_niche_construction;
    niche_config_t niche_config;
    
    /* Beast mode configuration */
    bool enable_beast_mode;
    beast_config_t beast_config;
} cogwxp_config_t;

COGWXPOS_API cogwxp_config_t cogwxp_config_default(void);

/*===========================================================================
 * System Handle
 *===========================================================================*/

typedef struct cogwxp_system* cogwxp_system_t;

/*===========================================================================
 * System Lifecycle
 *===========================================================================*/

/**
 * Initialize the CoGWXP-OS system
 * 
 * This initializes all subsystems in the correct order:
 * 1. CogUtil (logging, memory, threading)
 * 2. AtomSpace (knowledge representation)
 * 3. PLN (reasoning engine)
 * 4. CogServer (agent orchestration)
 * 5. 9P Server (distributed file system)
 * 6. Dis VM (virtual machine)
 * 7. CogW7OS Kernel (cognitive extensions)
 * 8. Unified Bridge (integration layer)
 */
COGWXPOS_API cogwxp_system_t cogwxp_init(cogwxp_config_t* config);

/**
 * Start the CoGWXP-OS system
 * 
 * Starts all services and begins processing:
 * - CogServer starts accepting tasks
 * - 9P server starts listening
 * - Dis VM begins execution
 * - Agents are activated
 */
COGWXPOS_API cog_result_t cogwxp_start(cogwxp_system_t system);

/**
 * Stop the CoGWXP-OS system
 * 
 * Gracefully stops all services:
 * - Agents are suspended
 * - Pending tasks are completed or cancelled
 * - Network connections are closed
 */
COGWXPOS_API cog_result_t cogwxp_stop(cogwxp_system_t system);

/**
 * Shutdown the CoGWXP-OS system
 * 
 * Releases all resources and terminates:
 * - All subsystems are destroyed
 * - Memory is freed
 * - Logs are flushed
 */
COGWXPOS_API void cogwxp_shutdown(cogwxp_system_t system);

/**
 * Get current system state
 */
COGWXPOS_API cogwxp_state_t cogwxp_get_state(cogwxp_system_t system);

/*===========================================================================
 * Component Access
 *===========================================================================*/

/**
 * Get the global AtomSpace
 */
COGWXPOS_API atomspace_t cogwxp_get_atomspace(cogwxp_system_t system);

/**
 * Get the PLN engine
 */
COGWXPOS_API pln_engine_t cogwxp_get_pln(cogwxp_system_t system);

/**
 * Get the CogServer
 */
COGWXPOS_API cogserver_t cogwxp_get_cogserver(cogwxp_system_t system);

/**
 * Get the Dis VM
 */
COGWXPOS_API dis_vm_t* cogwxp_get_dis_vm(cogwxp_system_t system);

/**
 * Get the 9P server
 */
COGWXPOS_API p9_server_t cogwxp_get_9p_server(cogwxp_system_t system);

/**
 * Get the unified bridge
 */
COGWXPOS_API unified_bridge_t cogwxp_get_bridge(cogwxp_system_t system);

/**
 * Get the niche construction engine
 */
COGWXPOS_API niche_engine_t cogwxp_get_niche_engine(cogwxp_system_t system);

/**
 * Get the beast mode reactor
 */
COGWXPOS_API beast_reactor_t cogwxp_get_beast_reactor(cogwxp_system_t system);

/*===========================================================================
 * High-Level Knowledge Operations
 *===========================================================================*/

/**
 * Add a concept to the knowledge base
 */
COGWXPOS_API atom_handle_t cogwxp_add_concept(
    cogwxp_system_t system,
    const char* name,
    double truth_strength,
    double truth_confidence
);

/**
 * Add a relationship between concepts
 */
COGWXPOS_API atom_handle_t cogwxp_add_relationship(
    cogwxp_system_t system,
    atom_type_t link_type,
    atom_handle_t source,
    atom_handle_t target,
    double truth_strength,
    double truth_confidence
);

/**
 * Query the knowledge base
 */
COGWXPOS_API atom_handle_t* cogwxp_query(
    cogwxp_system_t system,
    const char* query_pattern,
    size_t* result_count
);

/**
 * Perform inference from a source atom
 */
COGWXPOS_API atom_handle_t* cogwxp_infer(
    cogwxp_system_t system,
    atom_handle_t source,
    size_t max_steps,
    size_t* result_count
);

/*===========================================================================
 * High-Level Agent Operations
 *===========================================================================*/

/**
 * Create and start a cognitive agent
 */
COGWXPOS_API uint64_t cogwxp_spawn_agent(
    cogwxp_system_t system,
    const char* name,
    agent_type_t type,
    const char** capabilities,
    size_t capability_count
);

/**
 * Assign a goal to an agent
 */
COGWXPOS_API cog_result_t cogwxp_assign_goal(
    cogwxp_system_t system,
    uint64_t agent_id,
    atom_handle_t goal
);

/**
 * Send a message to an agent
 */
COGWXPOS_API cog_result_t cogwxp_send_to_agent(
    cogwxp_system_t system,
    uint64_t agent_id,
    const char* message_type,
    atom_handle_t content
);

/**
 * Wait for an agent to complete its current goal
 */
COGWXPOS_API cog_result_t cogwxp_wait_agent(
    cogwxp_system_t system,
    uint64_t agent_id,
    uint32_t timeout_ms
);

/*===========================================================================
 * High-Level Task Operations
 *===========================================================================*/

/**
 * Submit a task for execution
 */
COGWXPOS_API uint64_t cogwxp_submit_task(
    cogwxp_system_t system,
    const char* name,
    atom_handle_t goal,
    int16_t priority
);

/**
 * Wait for a task to complete
 */
COGWXPOS_API cog_result_t cogwxp_wait_task(
    cogwxp_system_t system,
    uint64_t task_id,
    uint32_t timeout_ms,
    atom_handle_t** results,
    size_t* result_count
);

/**
 * Submit multiple tasks and wait for all
 */
COGWXPOS_API cog_result_t cogwxp_orchestrate(
    cogwxp_system_t system,
    atom_handle_t* goals,
    size_t goal_count,
    uint32_t timeout_ms
);

/*===========================================================================
 * Distributed Operations
 *===========================================================================*/

/**
 * Connect to a remote CoGWXP-OS peer
 */
COGWXPOS_API uint64_t cogwxp_connect_peer(
    cogwxp_system_t system,
    const char* address,
    uint16_t port
);

/**
 * Disconnect from a peer
 */
COGWXPOS_API cog_result_t cogwxp_disconnect_peer(
    cogwxp_system_t system,
    uint64_t peer_id
);

/**
 * Execute distributed inference across peers
 */
COGWXPOS_API atom_handle_t* cogwxp_distributed_infer(
    cogwxp_system_t system,
    atom_handle_t source,
    uint64_t* peer_ids,
    size_t peer_count,
    size_t* result_count
);

/**
 * Synchronize AtomSpace with peers
 */
COGWXPOS_API cog_result_t cogwxp_sync_peers(
    cogwxp_system_t system
);

/*===========================================================================
 * 9P File System Access
 *===========================================================================*/

/**
 * Open a resource via unified 9P path
 * 
 * Path formats:
 * - /atoms/<type>/<name>     - AtomSpace atoms
 * - /agents/<id>             - Cognitive agents
 * - /tasks/<id>              - Tasks
 * - /dis/<module>/<func>     - Dis VM resources
 * - /b9/<id>                 - b9 nodes
 * - /p9/<name>               - p9 scopes
 * - /j9/<id>                 - j9 gradients
 */
COGWXPOS_API unified_resource_t* cogwxp_open(
    cogwxp_system_t system,
    const char* path,
    uint8_t mode
);

/**
 * Read from a resource
 */
COGWXPOS_API ssize_t cogwxp_read(
    unified_resource_t* resource,
    void* buf,
    size_t count
);

/**
 * Write to a resource
 */
COGWXPOS_API ssize_t cogwxp_write(
    unified_resource_t* resource,
    const void* data,
    size_t count
);

/**
 * Close a resource
 */
COGWXPOS_API void cogwxp_close(unified_resource_t* resource);

/*===========================================================================
 * Limbo/Dis Execution
 *===========================================================================*/

/**
 * Load a Limbo module
 */
COGWXPOS_API cog_result_t cogwxp_load_module(
    cogwxp_system_t system,
    const char* module_path
);

/**
 * Execute a Limbo function
 */
COGWXPOS_API cog_result_t cogwxp_execute(
    cogwxp_system_t system,
    const char* module_name,
    const char* function_name,
    dis_value_t* args,
    size_t arg_count,
    dis_value_t* result
);

/**
 * Spawn a Dis thread
 */
COGWXPOS_API uint64_t cogwxp_spawn_dis_thread(
    cogwxp_system_t system,
    const char* module_name,
    const char* function_name
);

/*===========================================================================
 * b9/p9/j9 Operations
 *===========================================================================*/

/**
 * Create a b9 node from an atom
 */
COGWXPOS_API b9_node_t* cogwxp_b9_from_atom(
    cogwxp_system_t system,
    atom_handle_t atom
);

/**
 * Create a p9 scope from an atomspace
 */
COGWXPOS_API p9_scope_t* cogwxp_p9_from_atomspace(
    cogwxp_system_t system,
    atomspace_t atomspace
);

/**
 * Create a j9 gradient for distributed inference
 */
COGWXPOS_API j9_gradient_t* cogwxp_j9_inference(
    cogwxp_system_t system,
    atom_handle_t premise,
    atom_handle_t conclusion
);

/**
 * Propagate all j9 gradients
 */
COGWXPOS_API cog_result_t cogwxp_j9_propagate(cogwxp_system_t system);

/*===========================================================================
 * Event System
 *===========================================================================*/

typedef void (*cogwxp_event_handler_t)(unified_event_t* event, void* user_data);

/**
 * Subscribe to system events
 */
COGWXPOS_API cog_result_t cogwxp_subscribe(
    cogwxp_system_t system,
    unified_event_type_t event_type,
    cogwxp_event_handler_t handler,
    void* user_data
);

/**
 * Unsubscribe from events
 */
COGWXPOS_API cog_result_t cogwxp_unsubscribe(
    cogwxp_system_t system,
    unified_event_type_t event_type,
    cogwxp_event_handler_t handler
);

/*===========================================================================
 * Statistics and Monitoring
 *===========================================================================*/

typedef struct cogwxp_stats {
    /* System state */
    cogwxp_state_t state;
    uint64_t uptime_ms;
    
    /* Knowledge base */
    size_t total_atoms;
    size_t total_nodes;
    size_t total_links;
    
    /* Inference */
    uint64_t total_inferences;
    double avg_inference_time_ms;
    
    /* Agents */
    size_t total_agents;
    size_t active_agents;
    
    /* Tasks */
    size_t total_tasks;
    size_t completed_tasks;
    size_t failed_tasks;
    
    /* Distributed */
    size_t peer_connections;
    uint64_t messages_exchanged;
    
    /* Resources */
    size_t memory_used;
    size_t atomspace_memory;
    size_t dis_heap_used;
    
    /* b9/p9/j9 */
    size_t b9_nodes;
    size_t p9_scopes;
    size_t j9_gradients;
} cogwxp_stats_t;

COGWXPOS_API void cogwxp_get_stats(cogwxp_system_t system, cogwxp_stats_t* stats);

/*===========================================================================
 * Debugging
 *===========================================================================*/

/**
 * Dump system state to log
 */
COGWXPOS_API void cogwxp_dump_state(cogwxp_system_t system);

/**
 * Dump AtomSpace to file
 */
COGWXPOS_API cog_result_t cogwxp_dump_atomspace(
    cogwxp_system_t system,
    const char* path
);

/**
 * Load AtomSpace from file
 */
COGWXPOS_API cog_result_t cogwxp_load_atomspace(
    cogwxp_system_t system,
    const char* path
);

#ifdef __cplusplus
}
#endif

#endif /* _COGWXP_OS_H_ */
