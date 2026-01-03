/**
 * @file toolchains.h
 * @brief AGI Toolchain Integration Layer
 * 
 * Integrates various toolchains and frameworks:
 * - Torch7u: Lua-based neural network framework
 * - DynaV: Dynamic visualization
 * - AzStaHCog: Azure Stack HCI cognitive extensions
 * - PowerShellForGitHub: GitHub automation
 * - HyperMind: Hypergraph-based reasoning
 * 
 * @copyright CoGWXP-OS9 Project
 */

#ifndef _COGWXP_TOOLCHAINS_H_
#define _COGWXP_TOOLCHAINS_H_

#include "../opencog/atomspace/atomspace.h"
#include "../cogpilot/cogpilot.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Torch7u Integration - Lua Neural Networks
 *===========================================================================*/

/**
 * Torch7u provides Lua-based neural network capabilities
 * with tight integration to the AGI OS.
 */

/**
 * Torch7u tensor
 */
typedef struct torch7u_tensor* torch7u_tensor_t;

/**
 * Torch7u module (neural network layer/model)
 */
typedef struct torch7u_module* torch7u_module_t;

/**
 * Torch7u context
 */
typedef struct torch7u_context* torch7u_context_t;

/**
 * Initialize Torch7u
 */
COGUTIL_API cog_result_t torch7u_init(
    atomspace_t atomspace,
    torch7u_context_t* ctx
);

/**
 * Shutdown Torch7u
 */
COGUTIL_API void torch7u_shutdown(torch7u_context_t ctx);

/**
 * Execute Lua script
 */
COGUTIL_API cog_result_t torch7u_exec_lua(
    torch7u_context_t ctx,
    const char* script,
    char** result,
    size_t* result_size
);

/**
 * Load Lua script file
 */
COGUTIL_API cog_result_t torch7u_load_script(
    torch7u_context_t ctx,
    const char* script_path
);

/**
 * Create tensor from data
 */
COGUTIL_API cog_result_t torch7u_tensor_create(
    torch7u_context_t ctx,
    const float* data,
    const size_t* dims,
    size_t ndims,
    torch7u_tensor_t* tensor
);

/**
 * Get tensor data
 */
COGUTIL_API cog_result_t torch7u_tensor_get_data(
    torch7u_tensor_t tensor,
    float** data,
    size_t** dims,
    size_t* ndims
);

/**
 * Free tensor
 */
COGUTIL_API void torch7u_tensor_free(torch7u_tensor_t tensor);

/**
 * Load neural network model
 */
COGUTIL_API cog_result_t torch7u_load_model(
    torch7u_context_t ctx,
    const char* model_path,
    torch7u_module_t* module
);

/**
 * Forward pass
 */
COGUTIL_API cog_result_t torch7u_forward(
    torch7u_module_t module,
    torch7u_tensor_t input,
    torch7u_tensor_t* output
);

/**
 * Store tensor in AtomSpace
 */
COGUTIL_API cog_result_t torch7u_tensor_to_atom(
    torch7u_context_t ctx,
    torch7u_tensor_t tensor,
    atom_handle_t* atom
);

/**
 * Load tensor from AtomSpace
 */
COGUTIL_API cog_result_t torch7u_tensor_from_atom(
    torch7u_context_t ctx,
    atom_handle_t atom,
    torch7u_tensor_t* tensor
);

/*===========================================================================
 * DynaV - Dynamic Visualization
 *===========================================================================*/

/**
 * DynaV provides dynamic visualization capabilities
 * for AtomSpace, neural networks, and agent behavior.
 */

/**
 * Visualization types
 */
typedef enum {
    DYNAV_VIS_GRAPH,            /* Graph/network visualization */
    DYNAV_VIS_TREE,             /* Tree visualization */
    DYNAV_VIS_TIMELINE,         /* Timeline visualization */
    DYNAV_VIS_HEATMAP,          /* Heatmap visualization */
    DYNAV_VIS_3D_SPACE,         /* 3D spatial visualization */
    DYNAV_VIS_ATTENTION,        /* Attention flow visualization */
    DYNAV_VIS_EMBEDDING         /* Embedding space visualization */
} dynav_vis_type_t;

/**
 * DynaV context
 */
typedef struct dynav_context* dynav_context_t;

/**
 * DynaV visualization
 */
typedef struct dynav_visualization* dynav_visualization_t;

/**
 * Initialize DynaV
 */
COGUTIL_API cog_result_t dynav_init(
    atomspace_t atomspace,
    dynav_context_t* ctx
);

/**
 * Shutdown DynaV
 */
COGUTIL_API void dynav_shutdown(dynav_context_t ctx);

/**
 * Create visualization
 */
COGUTIL_API cog_result_t dynav_create_visualization(
    dynav_context_t ctx,
    dynav_vis_type_t type,
    const char* title,
    dynav_visualization_t* vis
);

/**
 * Add atoms to visualization
 */
COGUTIL_API cog_result_t dynav_add_atoms(
    dynav_visualization_t vis,
    atom_handle_t* atoms,
    size_t count
);

/**
 * Add attention flow
 */
COGUTIL_API cog_result_t dynav_add_attention_flow(
    dynav_visualization_t vis,
    atom_handle_t source,
    atom_handle_t target,
    float strength
);

/**
 * Render to image
 */
COGUTIL_API cog_result_t dynav_render_image(
    dynav_visualization_t vis,
    uint32_t width,
    uint32_t height,
    uint8_t** image_data,
    size_t* image_size
);

/**
 * Render to HTML
 */
COGUTIL_API cog_result_t dynav_render_html(
    dynav_visualization_t vis,
    char** html,
    size_t* html_size
);

/**
 * Start interactive server
 */
COGUTIL_API cog_result_t dynav_start_server(
    dynav_context_t ctx,
    uint16_t port
);

/**
 * Stop interactive server
 */
COGUTIL_API cog_result_t dynav_stop_server(dynav_context_t ctx);

/**
 * Free visualization
 */
COGUTIL_API void dynav_visualization_free(dynav_visualization_t vis);

/*===========================================================================
 * AzStaHCog - Azure Stack HCI Cognitive Extensions
 *===========================================================================*/

/**
 * Azure Stack HCI integration for on-premises
 * cognitive computing infrastructure.
 */

/**
 * HCI cluster node
 */
typedef struct {
    const char* name;
    const char* address;
    uint32_t cpu_cores;
    uint64_t ram_gb;
    uint64_t storage_tb;
    bool has_gpu;
    uint32_t gpu_count;
    
    /* Status */
    bool online;
    float cpu_usage;
    float memory_usage;
    float storage_usage;
} azstahcog_node_t;

/**
 * HCI cluster
 */
typedef struct {
    const char* name;
    const char* domain;
    azstahcog_node_t* nodes;
    size_t node_count;
    
    /* Aggregate resources */
    uint32_t total_cpu_cores;
    uint64_t total_ram_gb;
    uint64_t total_storage_tb;
    uint32_t total_gpus;
} azstahcog_cluster_t;

/**
 * AzStaHCog context
 */
typedef struct azstahcog_context* azstahcog_context_t;

/**
 * Initialize AzStaHCog
 */
COGUTIL_API cog_result_t azstahcog_init(
    const char* cluster_address,
    const char* credentials_path,
    azstahcog_context_t* ctx
);

/**
 * Shutdown AzStaHCog
 */
COGUTIL_API void azstahcog_shutdown(azstahcog_context_t ctx);

/**
 * Get cluster info
 */
COGUTIL_API cog_result_t azstahcog_get_cluster(
    azstahcog_context_t ctx,
    azstahcog_cluster_t* cluster
);

/**
 * Deploy cognitive workload
 */
COGUTIL_API cog_result_t azstahcog_deploy_workload(
    azstahcog_context_t ctx,
    const char* workload_name,
    const char* workload_spec_json,
    char** deployment_id
);

/**
 * Scale workload
 */
COGUTIL_API cog_result_t azstahcog_scale_workload(
    azstahcog_context_t ctx,
    const char* deployment_id,
    uint32_t replicas
);

/**
 * Get workload status
 */
COGUTIL_API cog_result_t azstahcog_get_workload_status(
    azstahcog_context_t ctx,
    const char* deployment_id,
    char** status_json
);

/**
 * Migrate AtomSpace to HCI storage
 */
COGUTIL_API cog_result_t azstahcog_migrate_atomspace(
    azstahcog_context_t ctx,
    atomspace_t atomspace,
    const char* storage_path
);

/*===========================================================================
 * PowerShellForGitHub - GitHub Automation
 *===========================================================================*/

/**
 * GitHub automation for managing AGI OS repositories
 */

/**
 * GitHub context
 */
typedef struct psforgithub_context* psforgithub_context_t;

/**
 * Initialize PowerShellForGitHub
 */
COGUTIL_API cog_result_t psforgithub_init(
    const char* pat_token,
    psforgithub_context_t* ctx
);

/**
 * Shutdown PowerShellForGitHub
 */
COGUTIL_API void psforgithub_shutdown(psforgithub_context_t ctx);

/**
 * Execute GitHub PowerShell command
 */
COGUTIL_API cog_result_t psforgithub_execute(
    psforgithub_context_t ctx,
    const char* command,
    char** result,
    size_t* result_size
);

/**
 * Create repository
 */
COGUTIL_API cog_result_t psforgithub_create_repo(
    psforgithub_context_t ctx,
    const char* name,
    const char* description,
    bool is_private,
    char** repo_url
);

/**
 * Create issue
 */
COGUTIL_API cog_result_t psforgithub_create_issue(
    psforgithub_context_t ctx,
    const char* repo,
    const char* title,
    const char* body,
    const char** labels,
    size_t label_count,
    uint64_t* issue_number
);

/**
 * Create pull request
 */
COGUTIL_API cog_result_t psforgithub_create_pr(
    psforgithub_context_t ctx,
    const char* repo,
    const char* title,
    const char* body,
    const char* head_branch,
    const char* base_branch,
    uint64_t* pr_number
);

/**
 * Sync repository with AtomSpace
 * 
 * Creates atoms representing repository structure,
 * issues, PRs, and commits.
 */
COGUTIL_API cog_result_t psforgithub_sync_to_atomspace(
    psforgithub_context_t ctx,
    const char* repo,
    atomspace_t atomspace,
    atom_handle_t* repo_atom
);

/*===========================================================================
 * HyperMind - Hypergraph Reasoning
 *===========================================================================*/

/**
 * HyperMind provides advanced hypergraph-based
 * reasoning capabilities.
 */

/**
 * HyperMind context
 */
typedef struct hypermind_context* hypermind_context_t;

/**
 * Initialize HyperMind
 */
COGUTIL_API cog_result_t hypermind_init(
    atomspace_t atomspace,
    hypermind_context_t* ctx
);

/**
 * Shutdown HyperMind
 */
COGUTIL_API void hypermind_shutdown(hypermind_context_t ctx);

/**
 * Hypergraph pattern matching
 */
COGUTIL_API cog_result_t hypermind_pattern_match(
    hypermind_context_t ctx,
    const char* pattern,
    atom_handle_t** results,
    size_t* count
);

/**
 * Hypergraph transformation
 */
COGUTIL_API cog_result_t hypermind_transform(
    hypermind_context_t ctx,
    const char* rule,
    atom_handle_t* input_atoms,
    size_t input_count,
    atom_handle_t** output_atoms,
    size_t* output_count
);

/**
 * Compute hypergraph centrality
 */
COGUTIL_API cog_result_t hypermind_centrality(
    hypermind_context_t ctx,
    atom_handle_t* atoms,
    size_t count,
    float** centrality_scores
);

/**
 * Detect hypergraph communities
 */
COGUTIL_API cog_result_t hypermind_communities(
    hypermind_context_t ctx,
    uint32_t** community_ids,
    size_t* atom_count
);

/**
 * Hypergraph embedding
 */
COGUTIL_API cog_result_t hypermind_embed(
    hypermind_context_t ctx,
    atom_handle_t* atoms,
    size_t count,
    uint32_t embedding_dim,
    float** embeddings
);

/*===========================================================================
 * Unified Toolchain API
 *===========================================================================*/

/**
 * Toolchain configuration
 */
typedef struct {
    bool enable_torch7u;
    bool enable_dynav;
    bool enable_azstahcog;
    bool enable_psforgithub;
    bool enable_hypermind;
    
    /* Shared resources */
    atomspace_t atomspace;
    
    /* Credentials */
    const char* github_pat;
    const char* azure_credentials_path;
    const char* hci_cluster_address;
} toolchain_config_t;

/**
 * Unified toolchain context
 */
typedef struct toolchain_context* toolchain_context_t;

/**
 * Initialize all toolchains
 */
COGUTIL_API cog_result_t toolchain_init(
    const toolchain_config_t* config,
    toolchain_context_t* ctx
);

/**
 * Shutdown all toolchains
 */
COGUTIL_API void toolchain_shutdown(toolchain_context_t ctx);

/**
 * Get Torch7u context
 */
COGUTIL_API torch7u_context_t toolchain_get_torch7u(toolchain_context_t ctx);

/**
 * Get DynaV context
 */
COGUTIL_API dynav_context_t toolchain_get_dynav(toolchain_context_t ctx);

/**
 * Get AzStaHCog context
 */
COGUTIL_API azstahcog_context_t toolchain_get_azstahcog(toolchain_context_t ctx);

/**
 * Get PowerShellForGitHub context
 */
COGUTIL_API psforgithub_context_t toolchain_get_psforgithub(toolchain_context_t ctx);

/**
 * Get HyperMind context
 */
COGUTIL_API hypermind_context_t toolchain_get_hypermind(toolchain_context_t ctx);

#ifdef __cplusplus
}
#endif

#endif /* _COGWXP_TOOLCHAINS_H_ */
