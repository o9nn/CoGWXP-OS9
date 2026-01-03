/**
 * @file ms_hypergraph.h
 * @brief Microsoft Graph Integration with OpenCog AtomSpace Metagraph
 * 
 * This module provides deep integration between Microsoft Graph API
 * and OpenCog's AtomSpace typed hypergraph, enabling:
 * - Graph entities as Atoms (Users, Groups, Files, Sites, etc.)
 * - Graph relationships as Links
 * - Real-time synchronization with MS Graph
 * - HyperGraphiQL query interface
 * - GraphRAG-style retrieval augmented generation
 * 
 * @copyright CoGWXP-OS9 Project
 */

#ifndef _COGWXP_MS_HYPERGRAPH_H_
#define _COGWXP_MS_HYPERGRAPH_H_

#include "../../opencog/atomspace/atomspace.h"
#include "../../opencog/cogutil/cogutil.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * MS Graph Entity Types (mapped to AtomSpace Types)
 *===========================================================================*/

/**
 * MS Graph entity types that map to AtomSpace node types
 */
typedef enum {
    /* Core directory entities */
    MSGRAPH_TYPE_USER = 0x1000,
    MSGRAPH_TYPE_GROUP,
    MSGRAPH_TYPE_APPLICATION,
    MSGRAPH_TYPE_SERVICE_PRINCIPAL,
    MSGRAPH_TYPE_DEVICE,
    MSGRAPH_TYPE_ORGANIZATION,
    
    /* Drive and files */
    MSGRAPH_TYPE_DRIVE,
    MSGRAPH_TYPE_DRIVE_ITEM,
    MSGRAPH_TYPE_FOLDER,
    MSGRAPH_TYPE_FILE,
    MSGRAPH_TYPE_WORKBOOK,
    MSGRAPH_TYPE_WORKSHEET,
    
    /* SharePoint */
    MSGRAPH_TYPE_SITE,
    MSGRAPH_TYPE_LIST,
    MSGRAPH_TYPE_LIST_ITEM,
    MSGRAPH_TYPE_PAGE,
    
    /* Teams */
    MSGRAPH_TYPE_TEAM,
    MSGRAPH_TYPE_CHANNEL,
    MSGRAPH_TYPE_CHAT,
    MSGRAPH_TYPE_MESSAGE,
    
    /* Mail */
    MSGRAPH_TYPE_MAILBOX,
    MSGRAPH_TYPE_MAIL_FOLDER,
    MSGRAPH_TYPE_MAIL_MESSAGE,
    MSGRAPH_TYPE_CALENDAR,
    MSGRAPH_TYPE_EVENT,
    MSGRAPH_TYPE_CONTACT,
    
    /* Planner */
    MSGRAPH_TYPE_PLANNER_PLAN,
    MSGRAPH_TYPE_PLANNER_BUCKET,
    MSGRAPH_TYPE_PLANNER_TASK,
    
    /* Security */
    MSGRAPH_TYPE_SECURITY_ALERT,
    MSGRAPH_TYPE_THREAT_INDICATOR,
    MSGRAPH_TYPE_SECURE_SCORE,
    
    /* Cognitive extensions */
    MSGRAPH_TYPE_COGNITIVE_ENTITY,
    MSGRAPH_TYPE_INFERENCE_RESULT,
    MSGRAPH_TYPE_ATTENTION_FOCUS,
    
    MSGRAPH_TYPE_MAX
} msgraph_entity_type_t;

/**
 * MS Graph relationship types that map to AtomSpace link types
 */
typedef enum {
    /* Membership relationships */
    MSGRAPH_REL_MEMBER_OF = 0x2000,
    MSGRAPH_REL_OWNER_OF,
    MSGRAPH_REL_CREATED_BY,
    MSGRAPH_REL_MODIFIED_BY,
    
    /* Hierarchical relationships */
    MSGRAPH_REL_PARENT_OF,
    MSGRAPH_REL_CHILD_OF,
    MSGRAPH_REL_CONTAINS,
    MSGRAPH_REL_CONTAINED_BY,
    
    /* Access relationships */
    MSGRAPH_REL_HAS_ACCESS_TO,
    MSGRAPH_REL_SHARED_WITH,
    MSGRAPH_REL_PERMISSION,
    
    /* Communication relationships */
    MSGRAPH_REL_SENT_TO,
    MSGRAPH_REL_RECEIVED_FROM,
    MSGRAPH_REL_REPLIED_TO,
    MSGRAPH_REL_MENTIONED,
    
    /* Cognitive relationships */
    MSGRAPH_REL_INFERRED_FROM,
    MSGRAPH_REL_SIMILAR_TO,
    MSGRAPH_REL_RELATED_TO,
    MSGRAPH_REL_ATTENTION_LINK,
    
    MSGRAPH_REL_MAX
} msgraph_relationship_type_t;

/*===========================================================================
 * MSHyperGraph Configuration
 *===========================================================================*/

/**
 * Configuration for MS Graph connection
 */
typedef struct {
    const char* tenant_id;
    const char* client_id;
    const char* client_secret;
    const char* redirect_uri;
    const char* scope;
    
    /* Sync options */
    bool auto_sync;
    uint32_t sync_interval_ms;
    bool sync_on_change;
    
    /* Cache options */
    bool enable_cache;
    uint32_t cache_ttl_seconds;
    size_t max_cache_size;
    
    /* AtomSpace options */
    atomspace_t target_atomspace;
    bool create_inverse_links;
    bool track_attention;
} msgraph_config_t;

/**
 * MSHyperGraph context
 */
typedef struct msgraph_context* msgraph_context_t;

/*===========================================================================
 * MSHyperGraph Core API
 *===========================================================================*/

/**
 * Initialize MSHyperGraph with configuration
 */
COGUTIL_API cog_result_t msgraph_init(
    const msgraph_config_t* config,
    msgraph_context_t* ctx
);

/**
 * Shutdown MSHyperGraph context
 */
COGUTIL_API void msgraph_shutdown(msgraph_context_t ctx);

/**
 * Authenticate with MS Graph
 */
COGUTIL_API cog_result_t msgraph_authenticate(
    msgraph_context_t ctx,
    const char* auth_code
);

/**
 * Authenticate with device code flow
 */
COGUTIL_API cog_result_t msgraph_authenticate_device_code(
    msgraph_context_t ctx,
    void (*device_code_callback)(const char* user_code, const char* verification_uri)
);

/**
 * Refresh authentication token
 */
COGUTIL_API cog_result_t msgraph_refresh_token(msgraph_context_t ctx);

/*===========================================================================
 * Entity Import/Export
 *===========================================================================*/

/**
 * Import MS Graph entity as AtomSpace atom
 */
COGUTIL_API atom_handle_t msgraph_import_entity(
    msgraph_context_t ctx,
    const char* entity_id,
    msgraph_entity_type_t type
);

/**
 * Import MS Graph entity with all relationships
 */
COGUTIL_API atom_handle_t msgraph_import_entity_deep(
    msgraph_context_t ctx,
    const char* entity_id,
    msgraph_entity_type_t type,
    uint32_t depth
);

/**
 * Export AtomSpace atom to MS Graph entity
 */
COGUTIL_API cog_result_t msgraph_export_entity(
    msgraph_context_t ctx,
    atom_handle_t atom,
    char* entity_id_out,
    size_t entity_id_size
);

/**
 * Sync entity between MS Graph and AtomSpace
 */
COGUTIL_API cog_result_t msgraph_sync_entity(
    msgraph_context_t ctx,
    const char* entity_id,
    atom_handle_t atom
);

/*===========================================================================
 * Bulk Operations
 *===========================================================================*/

/**
 * Import all entities of a type
 */
COGUTIL_API cog_result_t msgraph_import_all(
    msgraph_context_t ctx,
    msgraph_entity_type_t type,
    const char* filter,
    atom_handle_t** atoms_out,
    size_t* count_out
);

/**
 * Import user's entire graph
 */
COGUTIL_API cog_result_t msgraph_import_user_graph(
    msgraph_context_t ctx,
    const char* user_id,
    uint32_t depth
);

/**
 * Import organization graph
 */
COGUTIL_API cog_result_t msgraph_import_org_graph(
    msgraph_context_t ctx,
    uint32_t depth
);

/*===========================================================================
 * HyperGraphiQL Query Interface
 *===========================================================================*/

/**
 * HyperGraphiQL query result
 */
typedef struct {
    atom_handle_t* atoms;
    size_t atom_count;
    char* json_result;
    size_t json_size;
    double execution_time_ms;
    bool has_more;
    char* continuation_token;
} hypergraphiql_result_t;

/**
 * Execute HyperGraphiQL query
 * 
 * Supports GraphQL-like syntax extended for hypergraph operations:
 * - Standard GraphQL queries
 * - Hyperedge traversal
 * - Pattern matching
 * - Attention-based filtering
 * - PLN inference integration
 */
COGUTIL_API cog_result_t hypergraphiql_query(
    msgraph_context_t ctx,
    const char* query,
    const char* variables_json,
    hypergraphiql_result_t* result
);

/**
 * Execute HyperGraphiQL mutation
 */
COGUTIL_API cog_result_t hypergraphiql_mutate(
    msgraph_context_t ctx,
    const char* mutation,
    const char* variables_json,
    hypergraphiql_result_t* result
);

/**
 * Subscribe to HyperGraphiQL subscription
 */
COGUTIL_API cog_result_t hypergraphiql_subscribe(
    msgraph_context_t ctx,
    const char* subscription,
    const char* variables_json,
    void (*callback)(const hypergraphiql_result_t* result, void* user_data),
    void* user_data,
    uint64_t* subscription_id
);

/**
 * Unsubscribe from HyperGraphiQL subscription
 */
COGUTIL_API cog_result_t hypergraphiql_unsubscribe(
    msgraph_context_t ctx,
    uint64_t subscription_id
);

/**
 * Free HyperGraphiQL result
 */
COGUTIL_API void hypergraphiql_result_free(hypergraphiql_result_t* result);

/*===========================================================================
 * GraphRAG Integration
 *===========================================================================*/

/**
 * GraphRAG configuration
 */
typedef struct {
    const char* embedding_model;
    const char* llm_model;
    uint32_t chunk_size;
    uint32_t chunk_overlap;
    float similarity_threshold;
    uint32_t max_results;
    bool use_community_detection;
    bool use_entity_extraction;
} graphrag_config_t;

/**
 * GraphRAG query result
 */
typedef struct {
    char* answer;
    atom_handle_t* source_atoms;
    size_t source_count;
    float* relevance_scores;
    char* reasoning_trace;
} graphrag_result_t;

/**
 * Initialize GraphRAG on MSHyperGraph
 */
COGUTIL_API cog_result_t graphrag_init(
    msgraph_context_t ctx,
    const graphrag_config_t* config
);

/**
 * Index content for GraphRAG
 */
COGUTIL_API cog_result_t graphrag_index(
    msgraph_context_t ctx,
    atom_handle_t* atoms,
    size_t count
);

/**
 * Query using GraphRAG
 */
COGUTIL_API cog_result_t graphrag_query(
    msgraph_context_t ctx,
    const char* question,
    graphrag_result_t* result
);

/**
 * Free GraphRAG result
 */
COGUTIL_API void graphrag_result_free(graphrag_result_t* result);

/*===========================================================================
 * Real-time Sync and Change Notifications
 *===========================================================================*/

/**
 * Change notification callback
 */
typedef void (*msgraph_change_callback_t)(
    msgraph_context_t ctx,
    const char* entity_id,
    msgraph_entity_type_t type,
    const char* change_type,  /* "created", "updated", "deleted" */
    atom_handle_t atom,
    void* user_data
);

/**
 * Subscribe to MS Graph change notifications
 */
COGUTIL_API cog_result_t msgraph_subscribe_changes(
    msgraph_context_t ctx,
    msgraph_entity_type_t type,
    const char* filter,
    msgraph_change_callback_t callback,
    void* user_data,
    uint64_t* subscription_id
);

/**
 * Unsubscribe from change notifications
 */
COGUTIL_API cog_result_t msgraph_unsubscribe_changes(
    msgraph_context_t ctx,
    uint64_t subscription_id
);

/**
 * Start real-time sync
 */
COGUTIL_API cog_result_t msgraph_start_sync(msgraph_context_t ctx);

/**
 * Stop real-time sync
 */
COGUTIL_API cog_result_t msgraph_stop_sync(msgraph_context_t ctx);

/*===========================================================================
 * Cognitive Extensions
 *===========================================================================*/

/**
 * Run PLN inference on MS Graph data
 */
COGUTIL_API cog_result_t msgraph_infer(
    msgraph_context_t ctx,
    const char* inference_query,
    atom_handle_t* results,
    size_t* result_count
);

/**
 * Find similar entities using attention
 */
COGUTIL_API cog_result_t msgraph_find_similar(
    msgraph_context_t ctx,
    atom_handle_t entity,
    float threshold,
    atom_handle_t* similar,
    size_t* count
);

/**
 * Propagate attention through MS Graph relationships
 */
COGUTIL_API cog_result_t msgraph_propagate_attention(
    msgraph_context_t ctx,
    atom_handle_t focus,
    uint32_t depth,
    float decay_factor
);

/**
 * Get cognitive summary of entity
 */
COGUTIL_API cog_result_t msgraph_cognitive_summary(
    msgraph_context_t ctx,
    atom_handle_t entity,
    char* summary_out,
    size_t summary_size
);

/*===========================================================================
 * Filesystem Interface
 *===========================================================================*/

/**
 * Mount MSHyperGraph as 9P filesystem
 * 
 * This exposes the MS Graph + AtomSpace as a Plan 9 style filesystem:
 * - /users/<id>/ - User entities
 * - /groups/<id>/ - Group entities
 * - /drives/<id>/ - Drive items
 * - /sites/<id>/ - SharePoint sites
 * - /atoms/<handle>/ - Direct atom access
 * - /query - HyperGraphiQL query interface
 * - /inference - PLN inference interface
 */
COGUTIL_API cog_result_t msgraph_mount_9p(
    msgraph_context_t ctx,
    const char* mount_point,
    uint16_t port
);

/**
 * Unmount MSHyperGraph filesystem
 */
COGUTIL_API cog_result_t msgraph_unmount_9p(msgraph_context_t ctx);

#ifdef __cplusplus
}
#endif

#endif /* _COGWXP_MS_HYPERGRAPH_H_ */
