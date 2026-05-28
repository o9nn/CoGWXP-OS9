/**
 * @file mshypergraph_fs.h
 * @brief MSHyperGraph Core Filesystem with 9P Interface
 * 
 * Implements a Plan 9 style filesystem that exposes MS Graph entities
 * and AtomSpace atoms as a unified namespace. Supports HyperGraphiQL
 * queries through special files.
 * 
 * Filesystem Layout:
 * /mshypergraph/
 * ├── me/                    # Current user context
 * │   ├── profile            # User profile (JSON)
 * │   ├── drive/             # OneDrive items
 * │   ├── mail/              # Mailbox
 * │   ├── calendar/          # Calendar events
 * │   └── teams/             # Teams memberships
 * ├── users/                 # All users
 * │   └── <id>/
 * ├── groups/                # All groups
 * │   └── <id>/
 * ├── sites/                 # SharePoint sites
 * │   └── <id>/
 * ├── atoms/                 # Direct AtomSpace access
 * │   └── <handle>/
 * │       ├── type           # Atom type
 * │       ├── name           # Atom name
 * │       ├── tv             # TruthValue
 * │       ├── av             # AttentionValue
 * │       ├── incoming       # Incoming links
 * │       └── outgoing       # Outgoing links
 * ├── query                  # HyperGraphiQL query file
 * ├── inference              # PLN inference file
 * └── ctl                    # Control file
 * 
 * @copyright CoGWXP-OS9 Project
 */

#ifndef _COGWXP_MSHYPERGRAPH_FS_H_
#define _COGWXP_MSHYPERGRAPH_FS_H_

#include "../../../plan9/9p/9p.h"
#include "../atomspace/ms_hypergraph.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Filesystem Configuration
 *===========================================================================*/

/**
 * MSHyperGraph filesystem configuration
 */
typedef struct {
    /* 9P server settings */
    uint16_t port;
    const char* mount_point;
    const char* announce;       /* Network address to announce */
    
    /* Authentication */
    bool require_auth;
    const char* auth_domain;
    
    /* MS Graph context */
    msgraph_context_t msgraph_ctx;
    
    /* AtomSpace */
    atomspace_t atomspace;
    
    /* Caching */
    bool enable_readahead;
    size_t readahead_size;
    uint32_t cache_timeout_ms;
    
    /* Limits */
    uint32_t max_connections;
    size_t max_message_size;
    uint32_t max_pending_requests;
} mshypergraph_fs_config_t;

/**
 * MSHyperGraph filesystem context
 */
typedef struct mshypergraph_fs* mshypergraph_fs_t;

/*===========================================================================
 * Filesystem Lifecycle
 *===========================================================================*/

/**
 * Create MSHyperGraph filesystem
 */
COGUTIL_API cog_result_t mshypergraph_fs_create(
    const mshypergraph_fs_config_t* config,
    mshypergraph_fs_t* fs
);

/**
 * Start filesystem server
 */
COGUTIL_API cog_result_t mshypergraph_fs_start(mshypergraph_fs_t fs);

/**
 * Stop filesystem server
 */
COGUTIL_API cog_result_t mshypergraph_fs_stop(mshypergraph_fs_t fs);

/**
 * Destroy filesystem
 */
COGUTIL_API void mshypergraph_fs_destroy(mshypergraph_fs_t fs);

/*===========================================================================
 * File Types
 *===========================================================================*/

/**
 * MSHyperGraph file types
 */
typedef enum {
    MSHG_FILE_DIRECTORY = 0,
    MSHG_FILE_ENTITY,           /* MS Graph entity */
    MSHG_FILE_ATOM,             /* AtomSpace atom */
    MSHG_FILE_QUERY,            /* HyperGraphiQL query */
    MSHG_FILE_INFERENCE,        /* PLN inference */
    MSHG_FILE_CONTROL,          /* Control file */
    MSHG_FILE_PROPERTY,         /* Entity/atom property */
    MSHG_FILE_RELATIONSHIP,     /* Relationship/link */
    MSHG_FILE_STREAM,           /* Real-time stream */
    MSHG_FILE_SUBSCRIPTION      /* Change subscription */
} mshypergraph_file_type_t;

/**
 * File metadata
 */
typedef struct {
    mshypergraph_file_type_t type;
    uint64_t qid_path;
    uint32_t qid_version;
    uint8_t qid_type;
    uint32_t mode;
    uint32_t atime;
    uint32_t mtime;
    uint64_t length;
    char* name;
    char* uid;
    char* gid;
    char* muid;
    
    /* Type-specific data */
    union {
        struct {
            char* entity_id;
            msgraph_entity_type_t entity_type;
        } entity;
        struct {
            atom_handle_t handle;
        } atom;
        struct {
            char* query_text;
            char* variables;
        } query;
    } data;
} mshypergraph_file_t;

/*===========================================================================
 * Query File Interface
 *===========================================================================*/

/**
 * Query file format:
 * 
 * Write: HyperGraphiQL query in GraphQL format
 * Read: JSON result
 * 
 * Extended syntax:
 * - @atom(handle) - Reference atom by handle
 * - @entity(id) - Reference MS Graph entity
 * - @infer(rule) - Apply PLN inference rule
 * - @attention(threshold) - Filter by attention
 * - @traverse(depth) - Traverse hyperedges
 */

/**
 * Execute query through filesystem
 */
COGUTIL_API cog_result_t mshypergraph_fs_query(
    mshypergraph_fs_t fs,
    const char* query,
    const char* variables,
    char** result,
    size_t* result_size
);

/*===========================================================================
 * Inference File Interface
 *===========================================================================*/

/**
 * Inference file format:
 * 
 * Write: PLN inference request
 *   - "forward <atom_handle>" - Forward chaining from atom
 *   - "backward <goal>" - Backward chaining to goal
 *   - "rule <rule_name> <args>" - Apply specific rule
 * 
 * Read: Inference results as JSON
 */

/**
 * Execute inference through filesystem
 */
COGUTIL_API cog_result_t mshypergraph_fs_infer(
    mshypergraph_fs_t fs,
    const char* request,
    char** result,
    size_t* result_size
);

/*===========================================================================
 * Control File Interface
 *===========================================================================*/

/**
 * Control file commands:
 * 
 * - "sync" - Force sync with MS Graph
 * - "auth <code>" - Authenticate with auth code
 * - "refresh" - Refresh auth token
 * - "subscribe <type> <filter>" - Subscribe to changes
 * - "unsubscribe <id>" - Unsubscribe
 * - "cache clear" - Clear cache
 * - "stats" - Get statistics
 */

/**
 * Execute control command
 */
COGUTIL_API cog_result_t mshypergraph_fs_control(
    mshypergraph_fs_t fs,
    const char* command,
    char** response,
    size_t* response_size
);

/*===========================================================================
 * Virtual Directory Handlers
 *===========================================================================*/

/**
 * Handler for /me directory
 */
COGUTIL_API cog_result_t mshypergraph_fs_handle_me(
    mshypergraph_fs_t fs,
    const char* path,
    p9_request_t* req,
    p9_response_t* resp
);

/**
 * Handler for /users directory
 */
COGUTIL_API cog_result_t mshypergraph_fs_handle_users(
    mshypergraph_fs_t fs,
    const char* path,
    p9_request_t* req,
    p9_response_t* resp
);

/**
 * Handler for /groups directory
 */
COGUTIL_API cog_result_t mshypergraph_fs_handle_groups(
    mshypergraph_fs_t fs,
    const char* path,
    p9_request_t* req,
    p9_response_t* resp
);

/**
 * Handler for /atoms directory
 */
COGUTIL_API cog_result_t mshypergraph_fs_handle_atoms(
    mshypergraph_fs_t fs,
    const char* path,
    p9_request_t* req,
    p9_response_t* resp
);

/*===========================================================================
 * Atom File Operations
 *===========================================================================*/

/**
 * Read atom as file
 * 
 * Returns JSON representation:
 * {
 *   "handle": <uint64>,
 *   "type": "<type_name>",
 *   "name": "<atom_name>",
 *   "truthValue": { "strength": <float>, "confidence": <float> },
 *   "attentionValue": { "sti": <int>, "lti": <int>, "vlti": <bool> },
 *   "incoming": [<handle>, ...],
 *   "outgoing": [<handle>, ...]
 * }
 */
COGUTIL_API cog_result_t mshypergraph_fs_read_atom(
    mshypergraph_fs_t fs,
    atom_handle_t handle,
    char** json,
    size_t* json_size
);

/**
 * Write atom from JSON
 */
COGUTIL_API cog_result_t mshypergraph_fs_write_atom(
    mshypergraph_fs_t fs,
    const char* json,
    atom_handle_t* handle
);

/**
 * Delete atom
 */
COGUTIL_API cog_result_t mshypergraph_fs_delete_atom(
    mshypergraph_fs_t fs,
    atom_handle_t handle
);

/*===========================================================================
 * Entity File Operations
 *===========================================================================*/

/**
 * Read MS Graph entity as file
 */
COGUTIL_API cog_result_t mshypergraph_fs_read_entity(
    mshypergraph_fs_t fs,
    const char* entity_id,
    msgraph_entity_type_t type,
    char** json,
    size_t* json_size
);

/**
 * Write/update MS Graph entity
 */
COGUTIL_API cog_result_t mshypergraph_fs_write_entity(
    mshypergraph_fs_t fs,
    const char* entity_id,
    msgraph_entity_type_t type,
    const char* json
);

/**
 * Delete MS Graph entity
 */
COGUTIL_API cog_result_t mshypergraph_fs_delete_entity(
    mshypergraph_fs_t fs,
    const char* entity_id,
    msgraph_entity_type_t type
);

/*===========================================================================
 * Streaming Interface
 *===========================================================================*/

/**
 * Stream file for real-time updates
 * 
 * Path: /mshypergraph/stream/<type>/<filter>
 * 
 * Reading from this file blocks until new data is available,
 * then returns JSON-formatted change notifications.
 */

/**
 * Create stream file
 */
COGUTIL_API cog_result_t mshypergraph_fs_create_stream(
    mshypergraph_fs_t fs,
    msgraph_entity_type_t type,
    const char* filter,
    uint64_t* stream_id
);

/**
 * Read from stream (blocking)
 */
COGUTIL_API cog_result_t mshypergraph_fs_read_stream(
    mshypergraph_fs_t fs,
    uint64_t stream_id,
    char** data,
    size_t* data_size,
    uint32_t timeout_ms
);

/**
 * Close stream
 */
COGUTIL_API cog_result_t mshypergraph_fs_close_stream(
    mshypergraph_fs_t fs,
    uint64_t stream_id
);

/*===========================================================================
 * Statistics and Monitoring
 *===========================================================================*/

/**
 * Filesystem statistics
 */
typedef struct {
    uint64_t total_requests;
    uint64_t read_requests;
    uint64_t write_requests;
    uint64_t query_requests;
    uint64_t inference_requests;
    
    uint64_t bytes_read;
    uint64_t bytes_written;
    
    uint64_t cache_hits;
    uint64_t cache_misses;
    
    uint64_t msgraph_api_calls;
    uint64_t atomspace_operations;
    
    uint32_t active_connections;
    uint32_t active_streams;
    uint32_t active_subscriptions;
    
    double avg_query_time_ms;
    double avg_inference_time_ms;
} mshypergraph_fs_stats_t;

/**
 * Get filesystem statistics
 */
COGUTIL_API cog_result_t mshypergraph_fs_get_stats(
    mshypergraph_fs_t fs,
    mshypergraph_fs_stats_t* stats
);

/**
 * Reset statistics
 */
COGUTIL_API void mshypergraph_fs_reset_stats(mshypergraph_fs_t fs);

#ifdef __cplusplus
}
#endif

#endif /* _COGWXP_MSHYPERGRAPH_FS_H_ */
