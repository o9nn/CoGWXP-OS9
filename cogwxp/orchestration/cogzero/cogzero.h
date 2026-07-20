/**
 * @file cogzero.h
 * @brief CogZero - High-Performance Agent-Zero for OpenCog
 *
 * A C++ implementation of Agent-Zero optimized for OpenCog integration,
 * featuring modular cognitive architecture with MOSES learning,
 * PLN reasoning, and AtomSpace knowledge representation.
 *
 * Based on: https://github.com/o9nn/cogzero
 *
 * @copyright CoGWXP-OS9 Project
 */

#ifndef COGWXP_COGZERO_H
#define COGWXP_COGZERO_H

#include "../orchestration.h"
#include "../atenspace/atenspace.h"
#include <stdint.h>
#include <stdbool.h>

#ifndef ATENSPACE_API
#ifdef COGUTIL_PLATFORM_NT
    #ifdef ATENSPACE_EXPORTS
        #define ATENSPACE_API __declspec(dllexport)
    #else
        #define ATENSPACE_API __declspec(dllimport)
    #endif
#else
    #define ATENSPACE_API
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Agent State
 *===========================================================================*/

typedef enum {
    CZ_STATE_IDLE,
    CZ_STATE_PERCEIVING,
    CZ_STATE_REASONING,
    CZ_STATE_PLANNING,
    CZ_STATE_EXECUTING,
    CZ_STATE_LEARNING,
    CZ_STATE_COMMUNICATING,
    CZ_STATE_REFLECTING,
    CZ_STATE_ERROR
} cz_state_t;

/*===========================================================================
 * Perception Module
 *===========================================================================*/

typedef enum {
    CZ_PERCEPT_TEXT,
    CZ_PERCEPT_IMAGE,
    CZ_PERCEPT_AUDIO,
    CZ_PERCEPT_SENSOR,
    CZ_PERCEPT_API,
    CZ_PERCEPT_ATOMSPACE
} cz_percept_type_t;

typedef struct {
    cz_percept_type_t type;
    const void* data;
    size_t size;
    int64_t timestamp;
    const char* source;
    float confidence;
    aten_handle_t atom_handle;  /* Representation in AtomSpace */
} cz_percept_t;

typedef struct {
    bool enable_text;
    bool enable_vision;
    bool enable_audio;
    const char* text_model;
    const char* vision_model;
    size_t perception_buffer_size;
} cz_perception_config_t;

/*===========================================================================
 * Planning Module
 *===========================================================================*/

typedef enum {
    CZ_GOAL_ACTIVE,
    CZ_GOAL_ACHIEVED,
    CZ_GOAL_FAILED,
    CZ_GOAL_SUSPENDED,
    CZ_GOAL_CANCELLED
} cz_goal_status_t;

typedef struct {
    uint64_t id;
    const char* description;
    float priority;
    float urgency;
    cz_goal_status_t status;
    aten_handle_t atom_handle;
    struct cz_goal* parent;
    struct cz_goal** subgoals;
    size_t subgoal_count;
} cz_goal_t;

typedef struct {
    uint64_t id;
    const char* action_type;
    const char* parameters_json;
    float expected_utility;
    float cost;
    cz_goal_t* goal;
    bool executed;
    bool succeeded;
} cz_action_t;

typedef struct {
    uint64_t id;
    cz_goal_t* goal;
    cz_action_t* actions;
    size_t action_count;
    size_t current_action;
    float confidence;
    int64_t created_at;
} cz_plan_t;

typedef struct {
    uint32_t max_plan_depth;
    uint32_t max_plan_breadth;
    float min_action_utility;
    bool enable_hierarchical_planning;
    bool enable_replanning;
} cz_planning_config_t;

/*===========================================================================
 * Learning Module (MOSES Integration)
 *===========================================================================*/

typedef enum {
    CZ_LEARN_SUPERVISED,
    CZ_LEARN_REINFORCEMENT,
    CZ_LEARN_IMITATION,
    CZ_LEARN_MOSES,
    CZ_LEARN_PATTERN
} cz_learning_type_t;

typedef struct {
    cz_learning_type_t type;
    const void* input;
    size_t input_size;
    const void* target;
    size_t target_size;
    float reward;
    bool terminal;
} cz_learning_sample_t;

typedef struct {
    cz_learning_type_t default_type;
    size_t replay_buffer_size;
    float learning_rate;
    float discount_factor;

    /* MOSES config */
    bool enable_moses;
    uint32_t moses_max_evals;
    float moses_complexity_penalty;

} cz_learning_config_t;

/*===========================================================================
 * Memory Module
 *===========================================================================*/

typedef enum {
    CZ_MEM_WORKING,
    CZ_MEM_EPISODIC,
    CZ_MEM_SEMANTIC,
    CZ_MEM_PROCEDURAL
} cz_memory_type_t;

typedef struct {
    uint64_t id;
    cz_memory_type_t type;
    const char* content;
    aten_handle_t atom_handle;
    float importance;
    float recency;
    int64_t timestamp;
    int64_t last_accessed;
    uint32_t access_count;
} cz_memory_t;

typedef struct {
    size_t working_memory_capacity;
    size_t episodic_memory_capacity;
    float importance_decay_rate;
    float recency_decay_rate;
    bool enable_consolidation;
} cz_memory_config_t;

/*===========================================================================
 * Communication Module
 *===========================================================================*/

typedef enum {
    CZ_COMM_NL_INPUT,
    CZ_COMM_NL_OUTPUT,
    CZ_COMM_AGENT_MESSAGE,
    CZ_COMM_API_REQUEST,
    CZ_COMM_API_RESPONSE
} cz_comm_type_t;

typedef struct {
    cz_comm_type_t type;
    uint64_t source_agent_id;
    uint64_t target_agent_id;
    const char* content;
    const char* intent;
    float confidence;
    int64_t timestamp;
} cz_message_t;

typedef struct {
    const char* nlp_model;
    const char* generation_model;
    bool enable_multi_agent;
    size_t message_queue_size;
} cz_communication_config_t;

/*===========================================================================
 * Tool Module
 *===========================================================================*/

typedef enum {
    CZ_TOOL_FUNCTION,
    CZ_TOOL_API,
    CZ_TOOL_PYTHON,
    CZ_TOOL_SHELL,
    CZ_TOOL_ATOMSPACE,
    CZ_TOOL_PLN,
    CZ_TOOL_MOSES
} cz_tool_type_t;

typedef struct {
    const char* name;
    const char* description;
    cz_tool_type_t type;
    const char* schema_json;
    void* implementation;
    bool requires_confirmation;
    float cost;
} cz_tool_t;

typedef struct {
    const char* tool_name;
    const char* arguments_json;
    const char* result;
    bool success;
    float execution_time_ms;
} cz_tool_result_t;

/*===========================================================================
 * Distributed Module
 *===========================================================================*/

typedef struct {
    uint64_t agent_id;
    const char* name;
    const char* address;
    uint16_t port;
    bool is_local;
    cz_state_t state;
} cz_remote_agent_t;

typedef struct {
    bool enable_distributed;
    const char* coordinator_address;
    uint16_t coordinator_port;
    uint32_t heartbeat_interval_ms;
} cz_distributed_config_t;

/*===========================================================================
 * Master Configuration
 *===========================================================================*/

typedef struct {
    /* Identity */
    const char* name;
    const char* description;

    /* AtomSpace */
    aten_context_t atomspace;

    /* Modules */
    cz_perception_config_t perception;
    cz_planning_config_t planning;
    cz_learning_config_t learning;
    cz_memory_config_t memory;
    cz_communication_config_t communication;
    cz_distributed_config_t distributed;

    /* Tools */
    cz_tool_t* tools;
    size_t tool_count;

    /* Performance */
    uint32_t cognitive_cycle_ms;
    uint32_t worker_threads;
    size_t max_memory_mb;

    /* Behavior */
    bool enable_reflection;
    uint32_t reflection_interval;
    float action_threshold;

} cz_config_t;

/*===========================================================================
 * Statistics
 *===========================================================================*/

typedef struct {
    /* Cycles */
    uint64_t cognitive_cycles;
    double avg_cycle_time_ms;

    /* Perception */
    uint64_t percepts_processed;

    /* Planning */
    uint64_t goals_created;
    uint64_t goals_achieved;
    uint64_t goals_failed;
    uint64_t plans_created;
    uint64_t actions_executed;

    /* Learning */
    uint64_t learning_samples;
    uint64_t moses_evaluations;

    /* Memory */
    uint64_t memories_created;
    uint64_t memories_recalled;

    /* Communication */
    uint64_t messages_sent;
    uint64_t messages_received;

    /* Tools */
    uint64_t tool_invocations;

    /* AtomSpace */
    uint64_t atoms_created;
    uint64_t inferences_run;

} cz_stats_t;

/*===========================================================================
 * Context Handle
 *===========================================================================*/

typedef struct cz_context* cz_context_t;
typedef struct cz_agent* cz_agent_t;

/*===========================================================================
 * Lifecycle
 *===========================================================================*/

ATENSPACE_API cog_result_t cz_init(
    cz_context_t* ctx
);

ATENSPACE_API void cz_shutdown(cz_context_t ctx);

ATENSPACE_API cog_result_t cz_create_agent(
    cz_context_t ctx,
    const cz_config_t* config,
    cz_agent_t* agent
);

ATENSPACE_API void cz_destroy_agent(cz_agent_t agent);

/*===========================================================================
 * Cognitive Loop
 *===========================================================================*/

/**
 * Run one cognitive cycle
 */
ATENSPACE_API cog_result_t cz_cycle(cz_agent_t agent);

/**
 * Run continuous cognitive loop
 */
ATENSPACE_API cog_result_t cz_run(cz_agent_t agent);

/**
 * Stop cognitive loop
 */
ATENSPACE_API cog_result_t cz_stop(cz_agent_t agent);

/**
 * Get current state
 */
ATENSPACE_API cz_state_t cz_get_state(cz_agent_t agent);

/*===========================================================================
 * Perception
 *===========================================================================*/

ATENSPACE_API cog_result_t cz_perceive(
    cz_agent_t agent,
    cz_percept_type_t type,
    const void* data,
    size_t size,
    const char* source
);

ATENSPACE_API cog_result_t cz_perceive_text(
    cz_agent_t agent,
    const char* text
);

ATENSPACE_API cog_result_t cz_get_percepts(
    cz_agent_t agent,
    cz_percept_t** percepts,
    size_t* count
);

/*===========================================================================
 * Goal & Planning
 *===========================================================================*/

ATENSPACE_API cog_result_t cz_add_goal(
    cz_agent_t agent,
    const char* description,
    float priority,
    cz_goal_t** goal
);

ATENSPACE_API cog_result_t cz_create_plan(
    cz_agent_t agent,
    cz_goal_t* goal,
    cz_plan_t** plan
);

ATENSPACE_API cog_result_t cz_execute_plan(
    cz_agent_t agent,
    cz_plan_t* plan
);

ATENSPACE_API cog_result_t cz_get_current_goal(
    cz_agent_t agent,
    cz_goal_t** goal
);

ATENSPACE_API cog_result_t cz_get_current_plan(
    cz_agent_t agent,
    cz_plan_t** plan
);

/*===========================================================================
 * Learning
 *===========================================================================*/

ATENSPACE_API cog_result_t cz_learn(
    cz_agent_t agent,
    const cz_learning_sample_t* sample
);

ATENSPACE_API cog_result_t cz_learn_from_experience(
    cz_agent_t agent
);

ATENSPACE_API cog_result_t cz_run_moses(
    cz_agent_t agent,
    const char* problem_description,
    char** solution
);

/*===========================================================================
 * Memory
 *===========================================================================*/

ATENSPACE_API cog_result_t cz_remember(
    cz_agent_t agent,
    cz_memory_type_t type,
    const char* content,
    float importance
);

ATENSPACE_API cog_result_t cz_recall(
    cz_agent_t agent,
    const char* query,
    size_t max_results,
    cz_memory_t** memories,
    size_t* count
);

ATENSPACE_API cog_result_t cz_forget(
    cz_agent_t agent,
    uint64_t memory_id
);

ATENSPACE_API cog_result_t cz_consolidate_memories(
    cz_agent_t agent
);

/*===========================================================================
 * Communication
 *===========================================================================*/

ATENSPACE_API cog_result_t cz_send_message(
    cz_agent_t sender,
    cz_agent_t recipient,
    const char* content
);

ATENSPACE_API cog_result_t cz_receive_messages(
    cz_agent_t agent,
    cz_message_t** messages,
    size_t* count
);

ATENSPACE_API cog_result_t cz_generate_response(
    cz_agent_t agent,
    const char* input,
    char** response
);

/*===========================================================================
 * Tools
 *===========================================================================*/

ATENSPACE_API cog_result_t cz_register_tool(
    cz_agent_t agent,
    const cz_tool_t* tool
);

ATENSPACE_API cog_result_t cz_invoke_tool(
    cz_agent_t agent,
    const char* tool_name,
    const char* arguments_json,
    cz_tool_result_t* result
);

ATENSPACE_API cog_result_t cz_list_tools(
    cz_agent_t agent,
    cz_tool_t** tools,
    size_t* count
);

/*===========================================================================
 * AtomSpace Integration
 *===========================================================================*/

ATENSPACE_API cog_result_t cz_get_atomspace(
    cz_agent_t agent,
    aten_context_t* atomspace
);

ATENSPACE_API cog_result_t cz_query_atomspace(
    cz_agent_t agent,
    const char* query,
    aten_handle_t** results,
    size_t* count
);

ATENSPACE_API cog_result_t cz_run_pln(
    cz_agent_t agent,
    const char* query,
    uint32_t max_steps,
    char** conclusion
);

ATENSPACE_API cog_result_t cz_sync_to_atomspace(
    cz_agent_t agent
);

/*===========================================================================
 * Distributed
 *===========================================================================*/

ATENSPACE_API cog_result_t cz_discover_agents(
    cz_context_t ctx,
    cz_remote_agent_t** agents,
    size_t* count
);

ATENSPACE_API cog_result_t cz_connect_agent(
    cz_agent_t local,
    const char* remote_address,
    uint16_t port,
    cz_remote_agent_t** remote
);

ATENSPACE_API cog_result_t cz_delegate_task(
    cz_agent_t local,
    cz_remote_agent_t* remote,
    const char* task,
    char** result
);

/*===========================================================================
 * Reflection
 *===========================================================================*/

ATENSPACE_API cog_result_t cz_reflect(
    cz_agent_t agent,
    char** reflection
);

ATENSPACE_API cog_result_t cz_self_improve(
    cz_agent_t agent,
    const char* feedback
);

/*===========================================================================
 * Persistence
 *===========================================================================*/

ATENSPACE_API cog_result_t cz_save(
    cz_agent_t agent,
    const char* path
);

ATENSPACE_API cog_result_t cz_load(
    cz_context_t ctx,
    const char* path,
    cz_agent_t* agent
);

/*===========================================================================
 * Statistics
 *===========================================================================*/

ATENSPACE_API cog_result_t cz_get_stats(
    cz_agent_t agent,
    cz_stats_t* stats
);

ATENSPACE_API void cz_reset_stats(cz_agent_t agent);

#ifdef __cplusplus
}
#endif

#endif /* COGWXP_COGZERO_H */
