/**
 * @file agent_zero.h
 * @brief Agent Zero - Autonomous AI Agent Framework
 *
 * Agent Zero is a comprehensive autonomous agent framework featuring:
 * - Persistent memory (short-term, long-term, episodic)
 * - Tool use and function calling
 * - Code execution sandbox
 * - Multi-agent communication
 * - Self-reflection and improvement
 * - Integration with OpenCog AtomSpace
 *
 * @copyright CoGWXP-OS9 Project
 */

#ifndef COGWXP_AGENT_ZERO_H
#define COGWXP_AGENT_ZERO_H

#include "../orchestration.h"
#include "../claude/claude_orchestration.h"
#include "../graphrag/graphrag.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Agent State & Identity
 *===========================================================================*/

typedef enum {
    AGENT_ZERO_STATE_IDLE,
    AGENT_ZERO_STATE_THINKING,
    AGENT_ZERO_STATE_ACTING,
    AGENT_ZERO_STATE_WAITING,
    AGENT_ZERO_STATE_REFLECTING,
    AGENT_ZERO_STATE_LEARNING,
    AGENT_ZERO_STATE_SLEEPING,
    AGENT_ZERO_STATE_ERROR
} agent_zero_state_t;

typedef enum {
    AGENT_ZERO_ROLE_GENERAL,
    AGENT_ZERO_ROLE_CODER,
    AGENT_ZERO_ROLE_RESEARCHER,
    AGENT_ZERO_ROLE_ANALYST,
    AGENT_ZERO_ROLE_WRITER,
    AGENT_ZERO_ROLE_PLANNER,
    AGENT_ZERO_ROLE_EXECUTOR,
    AGENT_ZERO_ROLE_SUPERVISOR,
    AGENT_ZERO_ROLE_CUSTOM
} agent_zero_role_t;

/*===========================================================================
 * Memory System
 *===========================================================================*/

typedef enum {
    MEMORY_TYPE_WORKING,      /* Short-term working memory */
    MEMORY_TYPE_EPISODIC,     /* Event-based memories */
    MEMORY_TYPE_SEMANTIC,     /* Factual knowledge */
    MEMORY_TYPE_PROCEDURAL,   /* How-to knowledge */
    MEMORY_TYPE_CONVERSATION, /* Chat history */
    MEMORY_TYPE_REFLECTION,   /* Self-reflections */
    MEMORY_TYPE_TOOL_RESULT,  /* Tool execution results */
    MEMORY_TYPE_CODE          /* Code snippets */
} agent_zero_memory_type_t;

typedef struct {
    uint64_t id;
    agent_zero_memory_type_t type;
    const char* content;
    float* embedding;
    size_t embedding_dim;
    float importance;
    int64_t timestamp;
    int64_t last_accessed;
    uint32_t access_count;
    const char* metadata_json;
    atom_handle_t atom_handle;
} agent_zero_memory_t;

typedef struct {
    size_t working_memory_limit;
    size_t episodic_memory_limit;
    size_t semantic_memory_limit;
    float importance_threshold;
    float decay_rate;
    bool enable_consolidation;
    uint32_t consolidation_interval_sec;
} agent_zero_memory_config_t;

/*===========================================================================
 * Tool System
 *===========================================================================*/

typedef enum {
    TOOL_TYPE_BUILTIN,
    TOOL_TYPE_PYTHON,
    TOOL_TYPE_SHELL,
    TOOL_TYPE_API,
    TOOL_TYPE_MCP,       /* Model Context Protocol */
    TOOL_TYPE_CUSTOM
} agent_zero_tool_type_t;

typedef struct {
    const char* name;
    const char* description;
    const char* parameters_schema;
    agent_zero_tool_type_t type;
    const char* implementation;  /* Code or endpoint */
    bool requires_confirmation;
    bool is_dangerous;
    uint32_t timeout_ms;
} agent_zero_tool_def_t;

typedef struct {
    const char* tool_name;
    const char* arguments_json;
    const char* result;
    bool success;
    int64_t start_time;
    int64_t end_time;
    const char* error_message;
} agent_zero_tool_result_t;

/*===========================================================================
 * Code Execution Sandbox
 *===========================================================================*/

typedef enum {
    SANDBOX_PYTHON,
    SANDBOX_JAVASCRIPT,
    SANDBOX_SHELL,
    SANDBOX_SCHEME,      /* For AtomSpace Scheme */
    SANDBOX_LIMBO        /* For Inferno/Plan9 */
} agent_zero_sandbox_type_t;

typedef struct {
    agent_zero_sandbox_type_t type;
    const char* code;
    const char* stdin_data;
    char* stdout_data;
    char* stderr_data;
    int exit_code;
    uint64_t execution_time_ms;
    uint64_t memory_used_bytes;
} agent_zero_execution_t;

typedef struct {
    bool enable_network;
    bool enable_filesystem;
    const char** allowed_paths;
    size_t allowed_path_count;
    uint64_t memory_limit_mb;
    uint32_t timeout_ms;
    uint32_t max_processes;
} agent_zero_sandbox_config_t;

/*===========================================================================
 * Reasoning & Planning
 *===========================================================================*/

typedef enum {
    REASONING_CHAIN_OF_THOUGHT,
    REASONING_TREE_OF_THOUGHT,
    REASONING_REFLECTION,
    REASONING_SELF_CONSISTENCY,
    REASONING_REACT,     /* Reason + Act */
    REASONING_PLN        /* Probabilistic Logic Networks */
} agent_zero_reasoning_t;

typedef struct {
    uint64_t id;
    const char* description;
    const char* expected_outcome;
    bool completed;
    bool blocked;
    const char* blockers;
    struct agent_zero_plan_step* substeps;
    size_t substep_count;
} agent_zero_plan_step_t;

typedef struct {
    uint64_t id;
    const char* goal;
    agent_zero_plan_step_t* steps;
    size_t step_count;
    uint32_t current_step;
    int64_t created_at;
    int64_t completed_at;
    float progress;
    const char* status;
} agent_zero_plan_t;

/*===========================================================================
 * Multi-Agent System
 *===========================================================================*/

typedef struct {
    uint64_t agent_id;
    const char* agent_name;
    agent_zero_role_t role;
    const char* message;
    int64_t timestamp;
} agent_zero_message_t;

typedef struct {
    const char* channel_name;
    uint64_t* participant_ids;
    size_t participant_count;
    agent_zero_message_t* messages;
    size_t message_count;
} agent_zero_channel_t;

/*===========================================================================
 * Agent Configuration
 *===========================================================================*/

typedef struct {
    /* Identity */
    const char* name;
    const char* description;
    agent_zero_role_t role;
    const char* system_prompt;
    const char* personality;

    /* LLM */
    claude_config_t* llm_config;

    /* Memory */
    agent_zero_memory_config_t memory_config;

    /* Tools */
    agent_zero_tool_def_t* tools;
    size_t tool_count;

    /* Sandbox */
    agent_zero_sandbox_config_t sandbox_config;

    /* Reasoning */
    agent_zero_reasoning_t default_reasoning;
    uint32_t max_reasoning_steps;
    float confidence_threshold;

    /* Behavior */
    bool auto_reflect;
    uint32_t reflect_interval;
    bool auto_learn;
    bool verbose;

    /* AtomSpace */
    atomspace_t atomspace;
    bool sync_to_atomspace;

    /* GraphRAG */
    graphrag_context_t graphrag;

} agent_zero_config_t;

/*===========================================================================
 * Statistics
 *===========================================================================*/

typedef struct {
    uint64_t tasks_completed;
    uint64_t tasks_failed;
    uint64_t tools_invoked;
    uint64_t code_executions;
    uint64_t llm_calls;
    uint64_t tokens_used;
    uint64_t memories_created;
    uint64_t reflections;
    uint64_t messages_sent;
    uint64_t messages_received;
    double avg_task_time_ms;
    double avg_tool_time_ms;
} agent_zero_stats_t;

/*===========================================================================
 * Context Handles
 *===========================================================================*/

typedef struct agent_zero_context* agent_zero_context_t;
typedef struct agent_zero_instance* agent_zero_t;

/*===========================================================================
 * Framework Lifecycle
 *===========================================================================*/

/**
 * Initialize Agent Zero framework
 */
COGUTIL_API cog_result_t agent_zero_init(
    agent_zero_context_t* ctx
);

/**
 * Shutdown framework
 */
COGUTIL_API void agent_zero_shutdown(agent_zero_context_t ctx);

/*===========================================================================
 * Agent Lifecycle
 *===========================================================================*/

/**
 * Create a new agent
 */
COGUTIL_API cog_result_t agent_zero_create(
    agent_zero_context_t ctx,
    const agent_zero_config_t* config,
    agent_zero_t* agent
);

/**
 * Destroy an agent
 */
COGUTIL_API void agent_zero_destroy(agent_zero_t agent);

/**
 * Get agent state
 */
COGUTIL_API agent_zero_state_t agent_zero_get_state(agent_zero_t agent);

/**
 * Get agent ID
 */
COGUTIL_API uint64_t agent_zero_get_id(agent_zero_t agent);

/*===========================================================================
 * Task Execution
 *===========================================================================*/

/**
 * Execute a task autonomously
 */
COGUTIL_API cog_result_t agent_zero_execute(
    agent_zero_t agent,
    const char* task,
    char** result
);

/**
 * Execute with streaming response
 */
COGUTIL_API cog_result_t agent_zero_execute_stream(
    agent_zero_t agent,
    const char* task,
    void (*callback)(const char* chunk, bool is_final, void* user_data),
    void* user_data
);

/**
 * Execute single step (for manual control)
 */
COGUTIL_API cog_result_t agent_zero_step(
    agent_zero_t agent,
    const char* input,
    char** thought,
    char** action,
    char** observation
);

/**
 * Interrupt current execution
 */
COGUTIL_API cog_result_t agent_zero_interrupt(agent_zero_t agent);

/*===========================================================================
 * Memory Operations
 *===========================================================================*/

/**
 * Add memory
 */
COGUTIL_API cog_result_t agent_zero_remember(
    agent_zero_t agent,
    agent_zero_memory_type_t type,
    const char* content,
    float importance
);

/**
 * Recall memories
 */
COGUTIL_API cog_result_t agent_zero_recall(
    agent_zero_t agent,
    const char* query,
    size_t max_results,
    agent_zero_memory_t** memories,
    size_t* count
);

/**
 * Forget memory
 */
COGUTIL_API cog_result_t agent_zero_forget(
    agent_zero_t agent,
    uint64_t memory_id
);

/**
 * Consolidate memories (move working to long-term)
 */
COGUTIL_API cog_result_t agent_zero_consolidate_memories(
    agent_zero_t agent
);

/**
 * Get conversation history
 */
COGUTIL_API cog_result_t agent_zero_get_history(
    agent_zero_t agent,
    size_t max_messages,
    agent_zero_message_t** messages,
    size_t* count
);

/**
 * Clear conversation history
 */
COGUTIL_API cog_result_t agent_zero_clear_history(agent_zero_t agent);

/*===========================================================================
 * Tool Operations
 *===========================================================================*/

/**
 * Register a tool
 */
COGUTIL_API cog_result_t agent_zero_register_tool(
    agent_zero_t agent,
    const agent_zero_tool_def_t* tool
);

/**
 * Unregister a tool
 */
COGUTIL_API cog_result_t agent_zero_unregister_tool(
    agent_zero_t agent,
    const char* tool_name
);

/**
 * List available tools
 */
COGUTIL_API cog_result_t agent_zero_list_tools(
    agent_zero_t agent,
    agent_zero_tool_def_t** tools,
    size_t* count
);

/**
 * Execute a tool directly
 */
COGUTIL_API cog_result_t agent_zero_invoke_tool(
    agent_zero_t agent,
    const char* tool_name,
    const char* arguments_json,
    agent_zero_tool_result_t* result
);

/*===========================================================================
 * Code Execution
 *===========================================================================*/

/**
 * Execute code in sandbox
 */
COGUTIL_API cog_result_t agent_zero_execute_code(
    agent_zero_t agent,
    agent_zero_sandbox_type_t type,
    const char* code,
    const char* stdin_data,
    agent_zero_execution_t* result
);

/**
 * Install Python package
 */
COGUTIL_API cog_result_t agent_zero_pip_install(
    agent_zero_t agent,
    const char* package
);

/*===========================================================================
 * Planning
 *===========================================================================*/

/**
 * Create a plan for a goal
 */
COGUTIL_API cog_result_t agent_zero_plan(
    agent_zero_t agent,
    const char* goal,
    agent_zero_plan_t** plan
);

/**
 * Execute a plan
 */
COGUTIL_API cog_result_t agent_zero_execute_plan(
    agent_zero_t agent,
    agent_zero_plan_t* plan,
    char** result
);

/**
 * Get current plan
 */
COGUTIL_API cog_result_t agent_zero_get_plan(
    agent_zero_t agent,
    agent_zero_plan_t** plan
);

/**
 * Update plan step
 */
COGUTIL_API cog_result_t agent_zero_update_step(
    agent_zero_t agent,
    uint64_t step_id,
    bool completed,
    const char* result
);

/*===========================================================================
 * Reflection & Learning
 *===========================================================================*/

/**
 * Trigger reflection
 */
COGUTIL_API cog_result_t agent_zero_reflect(
    agent_zero_t agent,
    char** reflection
);

/**
 * Learn from experience
 */
COGUTIL_API cog_result_t agent_zero_learn(
    agent_zero_t agent,
    const char* experience,
    const char* lesson
);

/**
 * Self-improve based on feedback
 */
COGUTIL_API cog_result_t agent_zero_improve(
    agent_zero_t agent,
    const char* feedback
);

/*===========================================================================
 * Multi-Agent Communication
 *===========================================================================*/

/**
 * Send message to another agent
 */
COGUTIL_API cog_result_t agent_zero_send_message(
    agent_zero_t sender,
    agent_zero_t recipient,
    const char* message
);

/**
 * Receive messages
 */
COGUTIL_API cog_result_t agent_zero_receive_messages(
    agent_zero_t agent,
    agent_zero_message_t** messages,
    size_t* count
);

/**
 * Create agent channel
 */
COGUTIL_API cog_result_t agent_zero_create_channel(
    agent_zero_context_t ctx,
    const char* name,
    agent_zero_t* agents,
    size_t agent_count,
    agent_zero_channel_t** channel
);

/**
 * Broadcast to channel
 */
COGUTIL_API cog_result_t agent_zero_broadcast(
    agent_zero_t sender,
    agent_zero_channel_t* channel,
    const char* message
);

/*===========================================================================
 * Delegation
 *===========================================================================*/

/**
 * Delegate task to sub-agent
 */
COGUTIL_API cog_result_t agent_zero_delegate(
    agent_zero_t agent,
    agent_zero_role_t role,
    const char* task,
    char** result
);

/**
 * Spawn sub-agent
 */
COGUTIL_API cog_result_t agent_zero_spawn(
    agent_zero_t parent,
    agent_zero_role_t role,
    const char* name,
    agent_zero_t* child
);

/*===========================================================================
 * AtomSpace Integration
 *===========================================================================*/

/**
 * Sync agent state to AtomSpace
 */
COGUTIL_API cog_result_t agent_zero_sync_atomspace(
    agent_zero_t agent
);

/**
 * Query AtomSpace
 */
COGUTIL_API cog_result_t agent_zero_query_atomspace(
    agent_zero_t agent,
    const char* query,
    atom_handle_t** results,
    size_t* count
);

/**
 * Run PLN inference
 */
COGUTIL_API cog_result_t agent_zero_infer(
    agent_zero_t agent,
    const char* query,
    uint32_t max_steps,
    char** conclusion
);

/*===========================================================================
 * Statistics
 *===========================================================================*/

/**
 * Get agent statistics
 */
COGUTIL_API cog_result_t agent_zero_get_stats(
    agent_zero_t agent,
    agent_zero_stats_t* stats
);

/**
 * Reset statistics
 */
COGUTIL_API void agent_zero_reset_stats(agent_zero_t agent);

/*===========================================================================
 * Persistence
 *===========================================================================*/

/**
 * Save agent state
 */
COGUTIL_API cog_result_t agent_zero_save(
    agent_zero_t agent,
    const char* path
);

/**
 * Load agent state
 */
COGUTIL_API cog_result_t agent_zero_load(
    agent_zero_context_t ctx,
    const char* path,
    agent_zero_t* agent
);

/**
 * Export agent to JSON
 */
COGUTIL_API cog_result_t agent_zero_export_json(
    agent_zero_t agent,
    char** json
);

#ifdef __cplusplus
}
#endif

#endif /* COGWXP_AGENT_ZERO_H */
