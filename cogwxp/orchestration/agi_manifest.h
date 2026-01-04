/**
 * @file agi_manifest.h
 * @brief Unified AGI Orchestration Manifest
 *
 * Master orchestration layer that integrates all cognitive components:
 * - OpenCog AtomSpace & PLN
 * - OSDeploy for deployment
 * - CogPilot (HyperAgentQL, AzureCogML, PNN)
 * - MSHyperGraph (MS Graph + AtomSpace + HyperGraphiQL)
 * - Azurite Cognitive Architecture
 * - GraphRAG Knowledge Extraction
 * - Claude/LLM Orchestration
 * - Hyperd Container Orchestration
 * - PowerShell GitHub Automation
 * - Azure Stack HCI Cognitive
 * - Toolchains (Torch7u, DynaV, HyperMind)
 *
 * @copyright CoGWXP-OS9 Project
 */

#ifndef COGWXP_AGI_MANIFEST_H
#define COGWXP_AGI_MANIFEST_H

#include "orchestration.h"
#include "osdeploy/osdeploy.h"
#include "cogpilot/cogpilot.h"
#include "msgraph/atomspace/ms_hypergraph.h"
#include "msgraph/queries/hypergraphiql.h"
#include "msgraph/filesystem/mshypergraph_fs.h"
#include "azurite/azurite_cognitive.h"
#include "graphrag/graphrag.h"
#include "claude/claude_orchestration.h"
#include "hyperd/hyperd_orchestration.h"
#include "automation/powershell_github.h"
#include "azstahcog/azstahcog.h"
#include "toolchains/toolchains.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * AGI System State
 *===========================================================================*/

typedef enum {
    AGI_STATE_UNINITIALIZED,
    AGI_STATE_INITIALIZING,
    AGI_STATE_RUNNING,
    AGI_STATE_PAUSED,
    AGI_STATE_SHUTTING_DOWN,
    AGI_STATE_STOPPED,
    AGI_STATE_ERROR
} agi_state_t;

typedef enum {
    AGI_CAPABILITY_PERCEPTION,
    AGI_CAPABILITY_REASONING,
    AGI_CAPABILITY_LEARNING,
    AGI_CAPABILITY_PLANNING,
    AGI_CAPABILITY_LANGUAGE,
    AGI_CAPABILITY_MEMORY,
    AGI_CAPABILITY_ATTENTION,
    AGI_CAPABILITY_EMOTION,
    AGI_CAPABILITY_SOCIAL,
    AGI_CAPABILITY_MOTOR,
    AGI_CAPABILITY_COUNT
} agi_capability_t;

/*===========================================================================
 * Component Registry
 *===========================================================================*/

typedef struct {
    const char* name;
    const char* version;
    bool enabled;
    bool initialized;
    void* context;
} agi_component_t;

typedef struct {
    /* Core AtomSpace */
    atomspace_t atomspace;

    /* Deployment */
    osdeploy_context_t osdeploy;

    /* Agent Systems */
    cogpilot_context_t cogpilot;
    azurite_context_t azurite;

    /* MS Graph Integration */
    msgraph_context_t msgraph;
    mshypergraph_fs_t mshypergraph_fs;

    /* Knowledge Systems */
    graphrag_context_t graphrag;
    hgql_context_t hypergraphiql;

    /* LLM & Containers */
    claude_context_t claude;
    hyperd_context_t hyperd;

    /* Automation & Infrastructure */
    psgithub_context_t psgithub;
    azstahcog_context_t azstahcog;

    /* Neural & Visualization */
    torch7u_context_t torch7u;
    dynav_context_t dynav;
    hypermind_context_t hypermind;

} agi_components_t;

/*===========================================================================
 * Master Configuration
 *===========================================================================*/

typedef struct {
    /* System identity */
    const char* system_name;
    const char* system_id;

    /* AtomSpace */
    const char* atomspace_uri;
    bool enable_persistence;
    const char* persistence_path;

    /* LLM Configuration */
    claude_config_t claude_config;

    /* MS Graph */
    msgraph_config_t msgraph_config;

    /* Deployment */
    osdeploy_config_t osdeploy_config;

    /* Agents */
    cogpilot_config_t cogpilot_config;
    azurite_config_t azurite_config;

    /* Knowledge */
    graphrag_config_t graphrag_config;

    /* Containers */
    hyperd_config_t hyperd_config;

    /* Infrastructure */
    azstahcog_config_t azstahcog_config;
    psgithub_config_t psgithub_config;

    /* Toolchains */
    torch7u_config_t torch7u_config;
    dynav_config_t dynav_config;
    hypermind_config_t hypermind_config;

    /* Capabilities to enable */
    bool capabilities[AGI_CAPABILITY_COUNT];

    /* Performance */
    uint32_t worker_threads;
    size_t memory_limit_mb;
    bool enable_gpu;

} agi_config_t;

/*===========================================================================
 * Unified Statistics
 *===========================================================================*/

typedef struct {
    /* System */
    agi_state_t state;
    int64_t uptime_seconds;
    uint64_t memory_used_mb;
    float cpu_usage_percent;

    /* AtomSpace */
    uint64_t atom_count;
    uint64_t link_count;

    /* Agents */
    uint32_t active_agents;
    uint64_t agent_actions;

    /* LLM */
    uint64_t llm_requests;
    uint64_t llm_tokens;

    /* Containers */
    uint32_t running_containers;

    /* Knowledge */
    uint64_t entities_extracted;
    uint64_t queries_processed;

    /* Infrastructure */
    uint32_t deployed_services;

} agi_stats_t;

/*===========================================================================
 * AGI Context Handle
 *===========================================================================*/

typedef struct agi_context* agi_context_t;

/*===========================================================================
 * Lifecycle API
 *===========================================================================*/

/**
 * Initialize the AGI system with all components
 */
COGUTIL_API cog_result_t agi_init(
    const agi_config_t* config,
    agi_context_t* ctx
);

/**
 * Start the AGI system
 */
COGUTIL_API cog_result_t agi_start(agi_context_t ctx);

/**
 * Pause the AGI system
 */
COGUTIL_API cog_result_t agi_pause(agi_context_t ctx);

/**
 * Resume the AGI system
 */
COGUTIL_API cog_result_t agi_resume(agi_context_t ctx);

/**
 * Shutdown the AGI system
 */
COGUTIL_API void agi_shutdown(agi_context_t ctx);

/**
 * Get current state
 */
COGUTIL_API agi_state_t agi_get_state(agi_context_t ctx);

/*===========================================================================
 * Component Access
 *===========================================================================*/

/**
 * Get components registry
 */
COGUTIL_API cog_result_t agi_get_components(
    agi_context_t ctx,
    agi_components_t* components
);

/**
 * Get AtomSpace
 */
COGUTIL_API atomspace_t agi_get_atomspace(agi_context_t ctx);

/**
 * Get Claude context
 */
COGUTIL_API claude_context_t agi_get_claude(agi_context_t ctx);

/**
 * Get GraphRAG context
 */
COGUTIL_API graphrag_context_t agi_get_graphrag(agi_context_t ctx);

/*===========================================================================
 * Unified Query API
 *===========================================================================*/

/**
 * Natural language query across all systems
 */
COGUTIL_API cog_result_t agi_query(
    agi_context_t ctx,
    const char* query,
    char** response
);

/**
 * Execute HyperGraphiQL query
 */
COGUTIL_API cog_result_t agi_graphql_query(
    agi_context_t ctx,
    const char* query,
    const char* variables,
    char** result
);

/**
 * Run PLN inference
 */
COGUTIL_API cog_result_t agi_infer(
    agi_context_t ctx,
    const char* query,
    uint32_t max_steps,
    atom_handle_t** conclusions,
    size_t* count
);

/*===========================================================================
 * Agent API
 *===========================================================================*/

/**
 * Create a new cognitive agent
 */
COGUTIL_API cog_result_t agi_create_agent(
    agi_context_t ctx,
    const char* name,
    const char* personality,
    const char* goals,
    uint64_t* agent_id
);

/**
 * Send stimulus to agent
 */
COGUTIL_API cog_result_t agi_agent_perceive(
    agi_context_t ctx,
    uint64_t agent_id,
    const char* stimulus,
    char** response
);

/**
 * Have agent reflect
 */
COGUTIL_API cog_result_t agi_agent_reflect(
    agi_context_t ctx,
    uint64_t agent_id,
    char** reflection
);

/**
 * Have agent plan
 */
COGUTIL_API cog_result_t agi_agent_plan(
    agi_context_t ctx,
    uint64_t agent_id,
    const char* goal,
    char** plan
);

/*===========================================================================
 * Knowledge Management
 *===========================================================================*/

/**
 * Ingest document into knowledge graph
 */
COGUTIL_API cog_result_t agi_ingest_document(
    agi_context_t ctx,
    const char* content,
    const char* source,
    size_t* entities_extracted
);

/**
 * Ingest from MS Graph
 */
COGUTIL_API cog_result_t agi_sync_msgraph(
    agi_context_t ctx,
    size_t* entities_synced
);

/**
 * Search knowledge
 */
COGUTIL_API cog_result_t agi_search_knowledge(
    agi_context_t ctx,
    const char* query,
    size_t max_results,
    char** results_json
);

/*===========================================================================
 * Deployment & Infrastructure
 *===========================================================================*/

/**
 * Deploy cognitive workload
 */
COGUTIL_API cog_result_t agi_deploy_workload(
    agi_context_t ctx,
    const char* workload_name,
    const char* workload_type,
    uint32_t replicas,
    char** deployment_id
);

/**
 * Scale workload
 */
COGUTIL_API cog_result_t agi_scale_workload(
    agi_context_t ctx,
    const char* deployment_id,
    uint32_t replicas
);

/**
 * Create GitHub issue for AGI task
 */
COGUTIL_API cog_result_t agi_create_task_issue(
    agi_context_t ctx,
    const char* title,
    const char* description,
    uint32_t* issue_number
);

/*===========================================================================
 * Statistics
 *===========================================================================*/

/**
 * Get unified statistics
 */
COGUTIL_API cog_result_t agi_get_stats(
    agi_context_t ctx,
    agi_stats_t* stats
);

/**
 * Reset statistics
 */
COGUTIL_API void agi_reset_stats(agi_context_t ctx);

/**
 * Get health check
 */
COGUTIL_API cog_result_t agi_health_check(
    agi_context_t ctx,
    char** health_json
);

/*===========================================================================
 * Visualization
 *===========================================================================*/

/**
 * Export knowledge graph visualization
 */
COGUTIL_API cog_result_t agi_export_graph_svg(
    agi_context_t ctx,
    const char* filename
);

/**
 * Get neural network visualization
 */
COGUTIL_API cog_result_t agi_visualize_network(
    agi_context_t ctx,
    const char* network_id,
    const char* filename
);

#ifdef __cplusplus
}
#endif

#endif /* COGWXP_AGI_MANIFEST_H */
