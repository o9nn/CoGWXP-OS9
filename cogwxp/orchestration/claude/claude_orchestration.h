/**
 * @file claude_orchestration.h
 * @brief Claude/LLM Orchestration Interface
 *
 * Provides unified interface for Claude AI and other LLMs,
 * with deep integration into OpenCog AGI infrastructure.
 *
 * @copyright CoGWXP-OS9 Project
 */

#ifndef COGWXP_CLAUDE_ORCHESTRATION_H
#define COGWXP_CLAUDE_ORCHESTRATION_H

#include "../orchestration.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Types and Structures
 *===========================================================================*/

/* LLM Provider types */
typedef enum {
    CLAUDE_PROVIDER_ANTHROPIC,
    CLAUDE_PROVIDER_AZURE_OPENAI,
    CLAUDE_PROVIDER_OPENAI,
    CLAUDE_PROVIDER_LOCAL,
    CLAUDE_PROVIDER_CUSTOM
} claude_provider_t;

/* Model types */
typedef enum {
    CLAUDE_MODEL_OPUS,
    CLAUDE_MODEL_SONNET,
    CLAUDE_MODEL_HAIKU,
    CLAUDE_MODEL_GPT4,
    CLAUDE_MODEL_GPT35,
    CLAUDE_MODEL_LOCAL_LLAMA,
    CLAUDE_MODEL_CUSTOM
} claude_model_t;

/* Tool definition for function calling */
typedef struct {
    const char* name;
    const char* description;
    const char* parameters_json_schema;
    void* (*handler)(const char* args_json);
} claude_tool_t;

/* Message role */
typedef enum {
    CLAUDE_ROLE_SYSTEM,
    CLAUDE_ROLE_USER,
    CLAUDE_ROLE_ASSISTANT,
    CLAUDE_ROLE_TOOL
} claude_role_t;

/* Message structure */
typedef struct {
    claude_role_t role;
    const char* content;
    const char* name;              /* For tool messages */
    const char* tool_call_id;      /* For tool responses */
} claude_message_t;

/* Tool call result */
typedef struct {
    const char* id;
    const char* name;
    const char* arguments_json;
} claude_tool_call_t;

/* Response structure */
typedef struct {
    const char* content;
    claude_tool_call_t* tool_calls;
    size_t tool_call_count;
    const char* stop_reason;
    uint64_t input_tokens;
    uint64_t output_tokens;
    float latency_ms;
} claude_response_t;

/* Streaming callback */
typedef void (*claude_stream_callback_t)(
    const char* chunk,
    bool is_final,
    void* user_data
);

/* Configuration */
typedef struct {
    claude_provider_t provider;
    claude_model_t model;
    const char* api_key;
    const char* api_endpoint;
    const char* organization_id;

    /* Model parameters */
    float temperature;
    float top_p;
    uint32_t max_tokens;
    const char** stop_sequences;
    size_t stop_sequence_count;

    /* System prompt */
    const char* system_prompt;

    /* Tools */
    claude_tool_t* tools;
    size_t tool_count;

    /* AtomSpace integration */
    atomspace_t atomspace;
    bool enable_atomspace_tools;

    /* Caching */
    bool enable_caching;
    uint32_t cache_ttl_seconds;

    /* Retry configuration */
    uint32_t max_retries;
    uint32_t retry_delay_ms;
} claude_config_t;

/* Agent configuration (for autonomous agents) */
typedef struct {
    const char* name;
    const char* description;
    const char* system_prompt;
    claude_tool_t* tools;
    size_t tool_count;
    uint32_t max_iterations;
    float goal_threshold;
} claude_agent_config_t;

/* Agent state */
typedef struct claude_agent* claude_agent_t;

/* Statistics */
typedef struct {
    uint64_t total_requests;
    uint64_t total_input_tokens;
    uint64_t total_output_tokens;
    uint64_t cache_hits;
    uint64_t cache_misses;
    double avg_latency_ms;
    uint64_t errors;
} claude_stats_t;

/* Context handle */
typedef struct claude_context* claude_context_t;

/*===========================================================================
 * Lifecycle
 *===========================================================================*/

/**
 * Initialize Claude orchestration context
 */
COGUTIL_API cog_result_t claude_init(
    const claude_config_t* config,
    claude_context_t* ctx
);

/**
 * Shutdown Claude orchestration context
 */
COGUTIL_API void claude_shutdown(claude_context_t ctx);

/*===========================================================================
 * Completion API
 *===========================================================================*/

/**
 * Send a completion request
 */
COGUTIL_API cog_result_t claude_complete(
    claude_context_t ctx,
    const claude_message_t* messages,
    size_t message_count,
    claude_response_t* response
);

/**
 * Send a streaming completion request
 */
COGUTIL_API cog_result_t claude_complete_stream(
    claude_context_t ctx,
    const claude_message_t* messages,
    size_t message_count,
    claude_stream_callback_t callback,
    void* user_data
);

/**
 * Simple text completion
 */
COGUTIL_API cog_result_t claude_text(
    claude_context_t ctx,
    const char* prompt,
    char** response
);

/**
 * Free response resources
 */
COGUTIL_API void claude_response_free(claude_response_t* response);

/*===========================================================================
 * Tool Calling
 *===========================================================================*/

/**
 * Register a tool
 */
COGUTIL_API cog_result_t claude_register_tool(
    claude_context_t ctx,
    const claude_tool_t* tool
);

/**
 * Execute tool calls from response
 */
COGUTIL_API cog_result_t claude_execute_tools(
    claude_context_t ctx,
    const claude_response_t* response,
    claude_message_t** tool_results,
    size_t* result_count
);

/**
 * Run agentic loop until completion
 */
COGUTIL_API cog_result_t claude_agentic_loop(
    claude_context_t ctx,
    const char* goal,
    uint32_t max_iterations,
    char** final_response
);

/*===========================================================================
 * Agent API
 *===========================================================================*/

/**
 * Create an autonomous agent
 */
COGUTIL_API cog_result_t claude_create_agent(
    claude_context_t ctx,
    const claude_agent_config_t* config,
    claude_agent_t* agent
);

/**
 * Run agent on a task
 */
COGUTIL_API cog_result_t claude_run_agent(
    claude_agent_t agent,
    const char* task,
    char** result
);

/**
 * Destroy agent
 */
COGUTIL_API void claude_destroy_agent(claude_agent_t agent);

/*===========================================================================
 * AtomSpace Integration
 *===========================================================================*/

/**
 * Query AtomSpace using natural language
 */
COGUTIL_API cog_result_t claude_query_atomspace(
    claude_context_t ctx,
    const char* query,
    atom_handle_t** results,
    size_t* count
);

/**
 * Generate AtomSpace operations from description
 */
COGUTIL_API cog_result_t claude_generate_atomspace_ops(
    claude_context_t ctx,
    const char* description,
    char** atomese_code
);

/**
 * Explain AtomSpace content in natural language
 */
COGUTIL_API cog_result_t claude_explain_atoms(
    claude_context_t ctx,
    const atom_handle_t* atoms,
    size_t count,
    char** explanation
);

/*===========================================================================
 * Embedding & Semantic Search
 *===========================================================================*/

/**
 * Generate embeddings
 */
COGUTIL_API cog_result_t claude_embed(
    claude_context_t ctx,
    const char** texts,
    size_t count,
    float** embeddings,
    size_t* dim
);

/**
 * Semantic search
 */
COGUTIL_API cog_result_t claude_semantic_search(
    claude_context_t ctx,
    const char* query,
    const char** corpus,
    size_t corpus_count,
    size_t top_k,
    size_t* result_indices,
    float* scores
);

/*===========================================================================
 * Caching
 *===========================================================================*/

/**
 * Enable/disable caching
 */
COGUTIL_API cog_result_t claude_set_caching(
    claude_context_t ctx,
    bool enable
);

/**
 * Clear cache
 */
COGUTIL_API cog_result_t claude_clear_cache(claude_context_t ctx);

/*===========================================================================
 * Statistics
 *===========================================================================*/

/**
 * Get statistics
 */
COGUTIL_API cog_result_t claude_get_stats(
    claude_context_t ctx,
    claude_stats_t* stats
);

/**
 * Reset statistics
 */
COGUTIL_API void claude_reset_stats(claude_context_t ctx);

#ifdef __cplusplus
}
#endif

#endif /* COGWXP_CLAUDE_ORCHESTRATION_H */
