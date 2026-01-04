/**
 * @file graphrag.h
 * @brief GraphRAG - Graph-based Retrieval Augmented Generation
 *
 * Implements knowledge graph extraction, entity resolution, and
 * graph-based retrieval for enhanced LLM context.
 *
 * Based on: https://github.com/cogpy/graphrag
 *
 * @copyright CoGWXP-OS9 Project
 */

#ifndef COGWXP_GRAPHRAG_H
#define COGWXP_GRAPHRAG_H

#include "../orchestration.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Types and Structures
 *===========================================================================*/

/* Entity types in knowledge graph */
typedef enum {
    GRAPHRAG_ENTITY_PERSON,
    GRAPHRAG_ENTITY_ORGANIZATION,
    GRAPHRAG_ENTITY_LOCATION,
    GRAPHRAG_ENTITY_EVENT,
    GRAPHRAG_ENTITY_CONCEPT,
    GRAPHRAG_ENTITY_DOCUMENT,
    GRAPHRAG_ENTITY_CODE,
    GRAPHRAG_ENTITY_CUSTOM
} graphrag_entity_type_t;

/* Relationship types */
typedef enum {
    GRAPHRAG_REL_IS_A,
    GRAPHRAG_REL_PART_OF,
    GRAPHRAG_REL_RELATED_TO,
    GRAPHRAG_REL_MENTIONS,
    GRAPHRAG_REL_AUTHORED_BY,
    GRAPHRAG_REL_LOCATED_IN,
    GRAPHRAG_REL_OCCURRED_AT,
    GRAPHRAG_REL_DEPENDS_ON,
    GRAPHRAG_REL_IMPLEMENTS,
    GRAPHRAG_REL_CUSTOM
} graphrag_rel_type_t;

/* Entity structure */
typedef struct {
    uint64_t id;
    graphrag_entity_type_t type;
    const char* name;
    const char* description;
    float* embedding;
    size_t embedding_dim;
    const char* metadata_json;
    atom_handle_t atom_handle;  /* Link to AtomSpace */
} graphrag_entity_t;

/* Relationship structure */
typedef struct {
    uint64_t id;
    uint64_t source_id;
    uint64_t target_id;
    graphrag_rel_type_t type;
    float weight;
    const char* description;
} graphrag_relationship_t;

/* Community structure (for hierarchical summarization) */
typedef struct {
    uint64_t id;
    const char* name;
    const char* summary;
    uint64_t* entity_ids;
    size_t entity_count;
    uint32_t level;
    uint64_t parent_id;
} graphrag_community_t;

/* Document chunk */
typedef struct {
    uint64_t id;
    const char* content;
    const char* source_file;
    uint32_t start_offset;
    uint32_t end_offset;
    float* embedding;
    size_t embedding_dim;
    uint64_t* entity_ids;
    size_t entity_count;
} graphrag_chunk_t;

/* Search result */
typedef struct {
    uint64_t entity_id;
    const char* name;
    const char* context;
    float relevance_score;
    uint64_t* related_chunk_ids;
    size_t chunk_count;
} graphrag_search_result_t;

/* Query response with sources */
typedef struct {
    const char* answer;
    graphrag_search_result_t* sources;
    size_t source_count;
    const char* community_context;
    float confidence;
    uint64_t tokens_used;
} graphrag_response_t;

/* Configuration */
typedef struct {
    /* LLM settings */
    const char* llm_endpoint;
    const char* llm_api_key;
    const char* llm_model;

    /* Embedding settings */
    const char* embedding_endpoint;
    const char* embedding_model;
    size_t embedding_dim;

    /* Chunking settings */
    size_t chunk_size;
    size_t chunk_overlap;

    /* Graph settings */
    size_t max_entities_per_chunk;
    float entity_resolution_threshold;

    /* Community detection */
    bool enable_community_detection;
    uint32_t max_community_levels;
    size_t min_community_size;

    /* AtomSpace integration */
    atomspace_t atomspace;
    bool sync_to_atomspace;
} graphrag_config_t;

/* Statistics */
typedef struct {
    uint64_t entities_extracted;
    uint64_t relationships_created;
    uint64_t chunks_processed;
    uint64_t communities_detected;
    uint64_t queries_processed;
    uint64_t llm_calls;
    double avg_query_time_ms;
} graphrag_stats_t;

/* Context handle */
typedef struct graphrag_context* graphrag_context_t;

/*===========================================================================
 * Lifecycle
 *===========================================================================*/

/**
 * Initialize GraphRAG context
 */
COGUTIL_API cog_result_t graphrag_init(
    const graphrag_config_t* config,
    graphrag_context_t* ctx
);

/**
 * Shutdown GraphRAG context
 */
COGUTIL_API void graphrag_shutdown(graphrag_context_t ctx);

/*===========================================================================
 * Document Processing
 *===========================================================================*/

/**
 * Ingest a document and extract knowledge graph
 */
COGUTIL_API cog_result_t graphrag_ingest_document(
    graphrag_context_t ctx,
    const char* content,
    const char* source_name,
    size_t* entities_extracted,
    size_t* relationships_created
);

/**
 * Ingest a file
 */
COGUTIL_API cog_result_t graphrag_ingest_file(
    graphrag_context_t ctx,
    const char* file_path,
    size_t* entities_extracted,
    size_t* relationships_created
);

/**
 * Ingest a directory recursively
 */
COGUTIL_API cog_result_t graphrag_ingest_directory(
    graphrag_context_t ctx,
    const char* dir_path,
    const char* pattern,
    size_t* files_processed,
    size_t* entities_extracted
);

/*===========================================================================
 * Entity Operations
 *===========================================================================*/

/**
 * Get entity by ID
 */
COGUTIL_API cog_result_t graphrag_get_entity(
    graphrag_context_t ctx,
    uint64_t entity_id,
    graphrag_entity_t* entity
);

/**
 * Search entities by name
 */
COGUTIL_API cog_result_t graphrag_search_entities(
    graphrag_context_t ctx,
    const char* query,
    size_t max_results,
    graphrag_entity_t** entities,
    size_t* count
);

/**
 * Get entity relationships
 */
COGUTIL_API cog_result_t graphrag_get_relationships(
    graphrag_context_t ctx,
    uint64_t entity_id,
    bool incoming,
    bool outgoing,
    graphrag_relationship_t** relationships,
    size_t* count
);

/**
 * Merge duplicate entities (entity resolution)
 */
COGUTIL_API cog_result_t graphrag_merge_entities(
    graphrag_context_t ctx,
    uint64_t primary_id,
    uint64_t* secondary_ids,
    size_t secondary_count
);

/*===========================================================================
 * Community Detection
 *===========================================================================*/

/**
 * Detect communities in the knowledge graph
 */
COGUTIL_API cog_result_t graphrag_detect_communities(
    graphrag_context_t ctx,
    size_t* community_count
);

/**
 * Generate community summaries using LLM
 */
COGUTIL_API cog_result_t graphrag_summarize_communities(
    graphrag_context_t ctx,
    uint32_t level
);

/**
 * Get community hierarchy
 */
COGUTIL_API cog_result_t graphrag_get_communities(
    graphrag_context_t ctx,
    uint32_t level,
    graphrag_community_t** communities,
    size_t* count
);

/*===========================================================================
 * Query Interface
 *===========================================================================*/

/**
 * Local search - entity-centric retrieval
 */
COGUTIL_API cog_result_t graphrag_local_search(
    graphrag_context_t ctx,
    const char* query,
    size_t max_results,
    graphrag_search_result_t** results,
    size_t* count
);

/**
 * Global search - community-centric retrieval
 */
COGUTIL_API cog_result_t graphrag_global_search(
    graphrag_context_t ctx,
    const char* query,
    graphrag_response_t* response
);

/**
 * Hybrid search combining local and global
 */
COGUTIL_API cog_result_t graphrag_query(
    graphrag_context_t ctx,
    const char* query,
    bool use_local,
    bool use_global,
    graphrag_response_t* response
);

/**
 * Free query response
 */
COGUTIL_API void graphrag_response_free(graphrag_response_t* response);

/*===========================================================================
 * AtomSpace Integration
 *===========================================================================*/

/**
 * Sync knowledge graph to AtomSpace
 */
COGUTIL_API cog_result_t graphrag_sync_to_atomspace(
    graphrag_context_t ctx,
    size_t* atoms_created
);

/**
 * Import entities from AtomSpace
 */
COGUTIL_API cog_result_t graphrag_import_from_atomspace(
    graphrag_context_t ctx,
    atom_handle_t root_atom,
    size_t* entities_imported
);

/**
 * Query AtomSpace using GraphRAG semantics
 */
COGUTIL_API cog_result_t graphrag_query_atomspace(
    graphrag_context_t ctx,
    const char* query,
    atom_handle_t** results,
    size_t* count
);

/*===========================================================================
 * Statistics
 *===========================================================================*/

/**
 * Get statistics
 */
COGUTIL_API cog_result_t graphrag_get_stats(
    graphrag_context_t ctx,
    graphrag_stats_t* stats
);

/**
 * Reset statistics
 */
COGUTIL_API void graphrag_reset_stats(graphrag_context_t ctx);

#ifdef __cplusplus
}
#endif

#endif /* COGWXP_GRAPHRAG_H */
