/**
 * @file orchestration.h
 * @brief Unified AGI Toolchain Orchestration Layer
 * 
 * Master header for the CoGWXP-OS9 orchestration system.
 * Provides unified access to all integrated toolchains:
 * 
 * - MSHyperGraph: MS Graph + AtomSpace integration
 * - HyperGraphiQL: Query interface for hypergraph operations
 * - CogPilot: Agent orchestration, Azure CogML, Planetary Neural Net
 * - OSDeploy: OS deployment and provisioning
 * - Azurite: Cognitive architecture for generative agents
 * - Toolchains: Torch7u, DynaV, AzStaHCog, PowerShellForGitHub, HyperMind
 * 
 * @copyright CoGWXP-OS9 Project
 */

#ifndef _COGWXP_ORCHESTRATION_H_
#define _COGWXP_ORCHESTRATION_H_

/* Core OpenCog components */
#include "../opencog/cogutil/cogutil.h"
#include "../opencog/atomspace/atomspace.h"
#include "../opencog/pln/pln.h"
#include "../opencog/cogserver/cogserver.h"

/* Plan 9 / Inferno components */
#include "../plan9/9p/9p.h"
#include "../inferno/dis/dis.h"

/* Orchestration components */
#ifdef ENABLE_MSGRAPH
#include "msgraph/atomspace/ms_hypergraph.h"
#include "msgraph/filesystem/mshypergraph_fs.h"
#include "msgraph/queries/hypergraphiql.h"
#endif

#ifdef ENABLE_COGPILOT
#include "cogpilot/cogpilot.h"
#endif

#ifdef ENABLE_OSDEPLOY
#include "osdeploy/osdeploy.h"
#endif

#ifdef ENABLE_AZURITE
#include "azurite/azurite_cognitive.h"
#endif

#ifdef ENABLE_TOOLCHAINS
#include "toolchains/toolchains.h"
#endif

/* Advanced cognitive systems */
#include "../integration/niche_construction.h"
#include "../integration/beast_mode.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Orchestration Configuration
 *===========================================================================*/

/**
 * Master orchestration configuration
 */
typedef struct {
    /* Core components */
    atomspace_t atomspace;
    cogserver_t cogserver;
    
    /* MS Graph */
#ifdef ENABLE_MSGRAPH
    bool enable_msgraph;
    msgraph_config_t msgraph_config;
#endif
    
    /* CogPilot */
#ifdef ENABLE_COGPILOT
    bool enable_cogpilot;
    cogpilot_config_t cogpilot_config;
#endif
    
    /* OSDeploy */
#ifdef ENABLE_OSDEPLOY
    bool enable_osdeploy;
    osdeploy_config_t osdeploy_config;
#endif
    
    /* Azurite */
#ifdef ENABLE_AZURITE
    bool enable_azurite;
    /* Azurite agents created dynamically */
#endif
    
    /* Toolchains */
#ifdef ENABLE_TOOLCHAINS
    bool enable_toolchains;
    toolchain_config_t toolchain_config;
#endif
    
    /* Global settings */
    const char* data_directory;
    const char* log_directory;
    uint32_t log_level;
    bool enable_telemetry;
} orchestration_config_t;

/**
 * Orchestration context
 */
typedef struct orchestration_context* orchestration_context_t;

/*===========================================================================
 * Orchestration Lifecycle
 *===========================================================================*/

/**
 * Initialize orchestration system
 */
COGUTIL_API cog_result_t orchestration_init(
    const orchestration_config_t* config,
    orchestration_context_t* ctx
);

/**
 * Shutdown orchestration system
 */
COGUTIL_API void orchestration_shutdown(orchestration_context_t ctx);

/**
 * Get orchestration status
 */
COGUTIL_API cog_result_t orchestration_status(
    orchestration_context_t ctx,
    char** status_json,
    size_t* status_size
);

/*===========================================================================
 * Component Access
 *===========================================================================*/

/**
 * Get AtomSpace
 */
COGUTIL_API atomspace_t orchestration_get_atomspace(orchestration_context_t ctx);

/**
 * Get CogServer
 */
COGUTIL_API cogserver_t orchestration_get_cogserver(orchestration_context_t ctx);

#ifdef ENABLE_MSGRAPH
/**
 * Get MS Graph context
 */
COGUTIL_API msgraph_context_t orchestration_get_msgraph(orchestration_context_t ctx);

/**
 * Get MSHyperGraph filesystem
 */
COGUTIL_API mshypergraph_fs_t orchestration_get_mshypergraph_fs(orchestration_context_t ctx);
#endif

#ifdef ENABLE_COGPILOT
/**
 * Get CogPilot context
 */
COGUTIL_API cogpilot_context_t orchestration_get_cogpilot(orchestration_context_t ctx);

/**
 * Get HyperAgentQL context
 */
COGUTIL_API hagent_context_t orchestration_get_hagent(orchestration_context_t ctx);

/**
 * Get Azure CogML context
 */
COGUTIL_API azure_cogml_context_t orchestration_get_azure_cogml(orchestration_context_t ctx);

/**
 * Get Planetary Neural Net context
 */
COGUTIL_API pnn_context_t orchestration_get_pnn(orchestration_context_t ctx);
#endif

#ifdef ENABLE_OSDEPLOY
/**
 * Get OSDeploy context
 */
COGUTIL_API osdeploy_context_t orchestration_get_osdeploy(orchestration_context_t ctx);
#endif

#ifdef ENABLE_TOOLCHAINS
/**
 * Get Toolchains context
 */
COGUTIL_API toolchain_context_t orchestration_get_toolchains(orchestration_context_t ctx);
#endif

/*===========================================================================
 * Unified Task Execution
 *===========================================================================*/

/**
 * Task types
 */
typedef enum {
    ORCH_TASK_QUERY,            /* HyperGraphiQL query */
    ORCH_TASK_INFERENCE,        /* PLN inference */
    ORCH_TASK_AGENT,            /* Agent task */
    ORCH_TASK_DEPLOY,           /* Deployment task */
    ORCH_TASK_COGNITIVE,        /* Azurite cognitive task */
    ORCH_TASK_NEURAL,           /* Neural network task */
    ORCH_TASK_VISUALIZATION,    /* DynaV visualization */
    ORCH_TASK_GITHUB,           /* GitHub automation */
    ORCH_TASK_CUSTOM            /* Custom task */
} orchestration_task_type_t;

/**
 * Task result
 */
typedef struct {
    bool success;
    char* result_json;
    size_t result_size;
    char* error_message;
    
    /* Atoms produced */
    atom_handle_t* result_atoms;
    size_t atom_count;
    
    /* Performance */
    double execution_time_ms;
} orchestration_result_t;

/**
 * Execute orchestration task
 */
COGUTIL_API cog_result_t orchestration_execute(
    orchestration_context_t ctx,
    orchestration_task_type_t type,
    const char* task_json,
    orchestration_result_t* result
);

/**
 * Execute task asynchronously
 */
COGUTIL_API cog_result_t orchestration_execute_async(
    orchestration_context_t ctx,
    orchestration_task_type_t type,
    const char* task_json,
    void (*callback)(const orchestration_result_t* result, void* user_data),
    void* user_data,
    uint64_t* task_id
);

/**
 * Cancel async task
 */
COGUTIL_API cog_result_t orchestration_cancel(
    orchestration_context_t ctx,
    uint64_t task_id
);

/**
 * Free result
 */
COGUTIL_API void orchestration_result_free(orchestration_result_t* result);

/*===========================================================================
 * Workflow Orchestration
 *===========================================================================*/

/**
 * Workflow step
 */
typedef struct {
    const char* name;
    orchestration_task_type_t type;
    const char* task_json;
    const char** depends_on;
    size_t dependency_count;
} orchestration_step_t;

/**
 * Workflow definition
 */
typedef struct {
    const char* name;
    const char* description;
    orchestration_step_t* steps;
    size_t step_count;
    bool parallel_execution;
} orchestration_workflow_t;

/**
 * Workflow execution result
 */
typedef struct {
    bool success;
    orchestration_result_t* step_results;
    size_t step_count;
    double total_execution_time_ms;
} orchestration_workflow_result_t;

/**
 * Execute workflow
 */
COGUTIL_API cog_result_t orchestration_execute_workflow(
    orchestration_context_t ctx,
    const orchestration_workflow_t* workflow,
    orchestration_workflow_result_t* result
);

/**
 * Load workflow from JSON
 */
COGUTIL_API cog_result_t orchestration_load_workflow(
    const char* workflow_json,
    orchestration_workflow_t* workflow
);

/**
 * Free workflow
 */
COGUTIL_API void orchestration_workflow_free(orchestration_workflow_t* workflow);

/**
 * Free workflow result
 */
COGUTIL_API void orchestration_workflow_result_free(orchestration_workflow_result_t* result);

/*===========================================================================
 * Event System
 *===========================================================================*/

/**
 * Event types
 */
typedef enum {
    ORCH_EVENT_ATOM_CREATED,
    ORCH_EVENT_ATOM_MODIFIED,
    ORCH_EVENT_ATOM_DELETED,
    ORCH_EVENT_AGENT_REGISTERED,
    ORCH_EVENT_AGENT_UNREGISTERED,
    ORCH_EVENT_TASK_STARTED,
    ORCH_EVENT_TASK_COMPLETED,
    ORCH_EVENT_TASK_FAILED,
    ORCH_EVENT_MSGRAPH_CHANGE,
    ORCH_EVENT_INFERENCE_RESULT,
    ORCH_EVENT_DEPLOYMENT_STATUS,
    ORCH_EVENT_CUSTOM
} orchestration_event_type_t;

/**
 * Event
 */
typedef struct {
    orchestration_event_type_t type;
    uint64_t timestamp;
    const char* source;
    const char* data_json;
    atom_handle_t related_atom;
} orchestration_event_t;

/**
 * Event callback
 */
typedef void (*orchestration_event_callback_t)(
    const orchestration_event_t* event,
    void* user_data
);

/**
 * Subscribe to events
 */
COGUTIL_API cog_result_t orchestration_subscribe(
    orchestration_context_t ctx,
    orchestration_event_type_t type,
    orchestration_event_callback_t callback,
    void* user_data,
    uint64_t* subscription_id
);

/**
 * Unsubscribe from events
 */
COGUTIL_API cog_result_t orchestration_unsubscribe(
    orchestration_context_t ctx,
    uint64_t subscription_id
);

/**
 * Emit custom event
 */
COGUTIL_API cog_result_t orchestration_emit_event(
    orchestration_context_t ctx,
    const char* source,
    const char* data_json,
    atom_handle_t related_atom
);

/*===========================================================================
 * Telemetry and Monitoring
 *===========================================================================*/

/**
 * Telemetry metrics
 */
typedef struct {
    /* Task metrics */
    uint64_t total_tasks;
    uint64_t successful_tasks;
    uint64_t failed_tasks;
    double avg_task_time_ms;
    
    /* AtomSpace metrics */
    uint64_t atom_count;
    uint64_t link_count;
    uint64_t atomspace_size_bytes;
    
    /* Agent metrics */
    uint32_t registered_agents;
    uint32_t active_agents;
    uint64_t agent_tasks_completed;
    
    /* MS Graph metrics */
    uint64_t msgraph_api_calls;
    uint64_t msgraph_entities_synced;
    
    /* Neural network metrics */
    uint64_t inference_count;
    double avg_inference_time_ms;
    
    /* Resource usage */
    float cpu_usage_percent;
    uint64_t memory_usage_bytes;
    uint64_t disk_usage_bytes;
} orchestration_metrics_t;

/**
 * Get telemetry metrics
 */
COGUTIL_API cog_result_t orchestration_get_metrics(
    orchestration_context_t ctx,
    orchestration_metrics_t* metrics
);

/**
 * Reset telemetry metrics
 */
COGUTIL_API void orchestration_reset_metrics(orchestration_context_t ctx);

/**
 * Export metrics as JSON
 */
COGUTIL_API cog_result_t orchestration_export_metrics(
    orchestration_context_t ctx,
    char** metrics_json,
    size_t* json_size
);

/*===========================================================================
 * Configuration Management
 *===========================================================================*/

/**
 * Save configuration
 */
COGUTIL_API cog_result_t orchestration_save_config(
    orchestration_context_t ctx,
    const char* path
);

/**
 * Load configuration
 */
COGUTIL_API cog_result_t orchestration_load_config(
    const char* path,
    orchestration_config_t* config
);

/**
 * Update configuration at runtime
 */
COGUTIL_API cog_result_t orchestration_update_config(
    orchestration_context_t ctx,
    const char* config_json
);

#ifdef __cplusplus
}
#endif

#endif /* _COGWXP_ORCHESTRATION_H_ */
