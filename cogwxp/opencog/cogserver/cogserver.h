/**
 * @file cogserver.h
 * @brief CogServer - Multi-Agent Orchestration Server for CoGWXP-OS9
 * 
 * The CogServer provides centralized management of cognitive agents,
 * task scheduling, and distributed reasoning coordination.
 * 
 * @copyright AGPL-3.0
 */

#ifndef _COGWXP_COGSERVER_H_
#define _COGWXP_COGSERVER_H_

#include "../cogutil/cogutil.h"
#include "../atomspace/atomspace.h"
#include "../pln/pln.h"

#ifdef COGUTIL_PLATFORM_NT
    #ifdef COGSERVER_EXPORTS
        #define COGSERVER_API __declspec(dllexport)
    #else
        #define COGSERVER_API __declspec(dllimport)
    #endif
#else
    #define COGSERVER_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Agent Types and States
 *===========================================================================*/

typedef enum {
    AGENT_TYPE_GENERIC = 0,
    AGENT_TYPE_REASONING,
    AGENT_TYPE_LEARNING,
    AGENT_TYPE_PERCEPTION,
    AGENT_TYPE_ACTION,
    AGENT_TYPE_MEMORY,
    AGENT_TYPE_ATTENTION,
    AGENT_TYPE_LANGUAGE,
    AGENT_TYPE_PLANNING,
    AGENT_TYPE_CUSTOM
} agent_type_t;

typedef enum {
    AGENT_STATE_CREATED = 0,
    AGENT_STATE_IDLE,
    AGENT_STATE_RUNNING,
    AGENT_STATE_SUSPENDED,
    AGENT_STATE_TERMINATED,
    AGENT_STATE_ERROR
} agent_state_t;

/*===========================================================================
 * Agent Structure
 *===========================================================================*/

typedef struct cog_agent {
    uint64_t id;
    char* name;
    agent_type_t type;
    agent_state_t state;
    
    /* Capabilities */
    char** capabilities;
    size_t capability_count;
    
    /* Resources */
    size_t memory_limit_kb;
    double cpu_quota;           /* 0.0 - 1.0 */
    
    /* AtomSpace */
    atomspace_t local_atomspace;
    atomspace_t shared_atomspace;
    
    /* PLN Engine */
    pln_engine_t pln_engine;
    
    /* Attention */
    attention_value_t attention;
    
    /* Statistics */
    uint64_t tasks_completed;
    uint64_t tasks_failed;
    uint64_t inferences_performed;
    double total_cpu_time_ms;
    
    /* User data */
    void* user_data;
} cog_agent_t;

/*===========================================================================
 * Task Structure
 *===========================================================================*/

typedef enum {
    TASK_STATE_PENDING = 0,
    TASK_STATE_QUEUED,
    TASK_STATE_ASSIGNED,
    TASK_STATE_RUNNING,
    TASK_STATE_COMPLETED,
    TASK_STATE_FAILED,
    TASK_STATE_CANCELLED
} task_state_t;

typedef enum {
    TASK_TYPE_INFERENCE = 0,
    TASK_TYPE_LEARNING,
    TASK_TYPE_QUERY,
    TASK_TYPE_EXECUTION,
    TASK_TYPE_CUSTOM
} task_type_t;

typedef struct cog_task {
    uint64_t id;
    char* name;
    task_type_t type;
    task_state_t state;
    
    /* Task specification */
    atom_handle_t goal;
    atom_handle_t* inputs;
    size_t input_count;
    
    /* Results */
    atom_handle_t* outputs;
    size_t output_count;
    cog_result_t result_code;
    char* result_message;
    
    /* Assignment */
    cog_agent_t* assigned_agent;
    
    /* Priority and scheduling */
    int16_t priority;
    uint64_t deadline_ms;       /* 0 = no deadline */
    
    /* Timing */
    uint64_t created_time;
    uint64_t start_time;
    uint64_t end_time;
    
    /* Dependencies */
    struct cog_task** dependencies;
    size_t dependency_count;
    
    /* Callbacks */
    void (*on_complete)(struct cog_task* task, void* user_data);
    void (*on_fail)(struct cog_task* task, void* user_data);
    void* callback_user_data;
} cog_task_t;

/*===========================================================================
 * CogServer Handle
 *===========================================================================*/

typedef struct cogserver* cogserver_t;

/*===========================================================================
 * CogServer Configuration
 *===========================================================================*/

typedef struct cogserver_config {
    /* Agent limits */
    size_t max_agents;
    size_t default_agent_memory_kb;
    double default_agent_cpu_quota;
    
    /* Task queue */
    size_t task_queue_size;
    size_t worker_thread_count;
    
    /* AtomSpace */
    atomspace_t global_atomspace;
    
    /* PLN */
    pln_config_t pln_config;
    
    /* Network */
    bool enable_network;
    const char* listen_address;
    uint16_t listen_port;
    
    /* 9P export */
    bool enable_9p_export;
    const char* p9_export_name;
    
    /* Logging */
    cog_log_level_t log_level;
} cogserver_config_t;

COGSERVER_API cogserver_config_t cogserver_config_default(void);

/*===========================================================================
 * CogServer Lifecycle
 *===========================================================================*/

COGSERVER_API cogserver_t cogserver_create(cogserver_config_t* config);
COGSERVER_API void        cogserver_destroy(cogserver_t server);
COGSERVER_API cog_result_t cogserver_start(cogserver_t server);
COGSERVER_API void        cogserver_stop(cogserver_t server);
COGSERVER_API bool        cogserver_is_running(cogserver_t server);

/*===========================================================================
 * Agent Management
 *===========================================================================*/

COGSERVER_API cog_result_t cogserver_create_agent(
    cogserver_t server,
    const char* name,
    agent_type_t type,
    const char** capabilities,
    size_t capability_count,
    uint64_t* agent_id
);

COGSERVER_API cog_result_t cogserver_destroy_agent(
    cogserver_t server,
    uint64_t agent_id
);

COGSERVER_API cog_result_t cogserver_start_agent(
    cogserver_t server,
    uint64_t agent_id
);

COGSERVER_API cog_result_t cogserver_stop_agent(
    cogserver_t server,
    uint64_t agent_id
);

COGSERVER_API cog_result_t cogserver_suspend_agent(
    cogserver_t server,
    uint64_t agent_id
);

COGSERVER_API cog_result_t cogserver_resume_agent(
    cogserver_t server,
    uint64_t agent_id
);

COGSERVER_API cog_agent_t* cogserver_get_agent(
    cogserver_t server,
    uint64_t agent_id
);

COGSERVER_API cog_agent_t** cogserver_get_all_agents(
    cogserver_t server,
    size_t* count
);

COGSERVER_API cog_agent_t** cogserver_find_agents_by_capability(
    cogserver_t server,
    const char* capability,
    size_t* count
);

/*===========================================================================
 * Task Management
 *===========================================================================*/

COGSERVER_API cog_result_t cogserver_create_task(
    cogserver_t server,
    const char* name,
    task_type_t type,
    atom_handle_t goal,
    int16_t priority,
    uint64_t* task_id
);

COGSERVER_API cog_result_t cogserver_submit_task(
    cogserver_t server,
    uint64_t task_id
);

COGSERVER_API cog_result_t cogserver_cancel_task(
    cogserver_t server,
    uint64_t task_id
);

COGSERVER_API cog_result_t cogserver_wait_task(
    cogserver_t server,
    uint64_t task_id,
    uint32_t timeout_ms
);

COGSERVER_API cog_task_t* cogserver_get_task(
    cogserver_t server,
    uint64_t task_id
);

COGSERVER_API cog_result_t cogserver_set_task_callback(
    cogserver_t server,
    uint64_t task_id,
    void (*on_complete)(cog_task_t*, void*),
    void (*on_fail)(cog_task_t*, void*),
    void* user_data
);

COGSERVER_API cog_result_t cogserver_add_task_dependency(
    cogserver_t server,
    uint64_t task_id,
    uint64_t dependency_task_id
);

/*===========================================================================
 * Task Orchestration
 *===========================================================================*/

/* Submit multiple tasks and wait for all to complete */
COGSERVER_API cog_result_t cogserver_orchestrate_tasks(
    cogserver_t server,
    uint64_t* task_ids,
    size_t task_count,
    uint32_t timeout_ms
);

/* Dispatch task to best available agent */
COGSERVER_API cog_result_t cogserver_dispatch_task(
    cogserver_t server,
    uint64_t task_id,
    uint64_t* assigned_agent_id
);

/* Dispatch task to specific agent */
COGSERVER_API cog_result_t cogserver_dispatch_task_to_agent(
    cogserver_t server,
    uint64_t task_id,
    uint64_t agent_id
);

/*===========================================================================
 * Agent Communication
 *===========================================================================*/

typedef struct agent_message {
    uint64_t sender_id;
    uint64_t receiver_id;
    char* type;
    atom_handle_t content;
    uint64_t timestamp;
} agent_message_t;

COGSERVER_API cog_result_t cogserver_send_message(
    cogserver_t server,
    uint64_t sender_id,
    uint64_t receiver_id,
    const char* type,
    atom_handle_t content
);

COGSERVER_API cog_result_t cogserver_broadcast_message(
    cogserver_t server,
    uint64_t sender_id,
    const char* type,
    atom_handle_t content
);

COGSERVER_API cog_result_t cogserver_receive_message(
    cogserver_t server,
    uint64_t agent_id,
    agent_message_t* message,
    uint32_t timeout_ms
);

/*===========================================================================
 * Shared AtomSpace
 *===========================================================================*/

COGSERVER_API atomspace_t cogserver_get_global_atomspace(cogserver_t server);

COGSERVER_API cog_result_t cogserver_share_atom(
    cogserver_t server,
    uint64_t agent_id,
    atom_handle_t atom
);

COGSERVER_API cog_result_t cogserver_sync_agent_atomspace(
    cogserver_t server,
    uint64_t agent_id
);

/*===========================================================================
 * Attention Allocation
 *===========================================================================*/

COGSERVER_API cog_result_t cogserver_stimulate_agent(
    cogserver_t server,
    uint64_t agent_id,
    int16_t stimulus
);

COGSERVER_API cog_result_t cogserver_update_agent_attention(
    cogserver_t server,
    uint64_t agent_id,
    attention_value_t av
);

COGSERVER_API cog_agent_t** cogserver_get_top_attention_agents(
    cogserver_t server,
    size_t count,
    size_t* actual_count
);

/*===========================================================================
 * Distributed Operations
 *===========================================================================*/

/* Connect to remote CogServer */
COGSERVER_API cog_result_t cogserver_connect_remote(
    cogserver_t server,
    const char* address,
    uint16_t port,
    uint64_t* connection_id
);

COGSERVER_API cog_result_t cogserver_disconnect_remote(
    cogserver_t server,
    uint64_t connection_id
);

/* Distributed task execution */
COGSERVER_API cog_result_t cogserver_dispatch_remote_task(
    cogserver_t server,
    uint64_t connection_id,
    uint64_t task_id
);

/* Distributed inference */
COGSERVER_API cog_result_t cogserver_distributed_inference(
    cogserver_t server,
    atom_handle_t source,
    uint64_t* connection_ids,
    size_t connection_count,
    atom_handle_t** results,
    size_t* result_count
);

/*===========================================================================
 * 9P Export
 *===========================================================================*/

/*
 * CogServer 9P File System Layout:
 * 
 * /agents/
 *   ├── <agent_id>/
 *   │   ├── ctl              (control: start, stop, suspend, resume)
 *   │   ├── status           (read-only status)
 *   │   ├── atomspace/       (agent's local atomspace)
 *   │   ├── inbox            (message inbox)
 *   │   └── outbox           (message outbox)
 *   └── ...
 * 
 * /tasks/
 *   ├── <task_id>/
 *   │   ├── ctl              (control: submit, cancel)
 *   │   ├── status           (read-only status)
 *   │   ├── inputs/          (task inputs)
 *   │   └── outputs/         (task outputs)
 *   └── ...
 * 
 * /ctl                       (server control)
 * /stats                     (server statistics)
 * /atomspace/                (global atomspace)
 */

COGSERVER_API cog_result_t cogserver_export_9p(
    cogserver_t server,
    const char* address,
    uint16_t port
);

COGSERVER_API cog_result_t cogserver_unexport_9p(cogserver_t server);

/*===========================================================================
 * Statistics
 *===========================================================================*/

typedef struct cogserver_stats {
    /* Agent stats */
    size_t total_agents;
    size_t active_agents;
    size_t idle_agents;
    
    /* Task stats */
    size_t total_tasks;
    size_t pending_tasks;
    size_t running_tasks;
    size_t completed_tasks;
    size_t failed_tasks;
    
    /* Performance */
    double avg_task_completion_time_ms;
    double avg_inference_time_ms;
    uint64_t total_inferences;
    uint64_t total_messages;
    
    /* Resources */
    size_t total_memory_used_kb;
    double total_cpu_usage;
    
    /* Network */
    size_t remote_connections;
    uint64_t bytes_sent;
    uint64_t bytes_received;
} cogserver_stats_t;

COGSERVER_API void cogserver_get_stats(cogserver_t server, cogserver_stats_t* stats);
COGSERVER_API void cogserver_reset_stats(cogserver_t server);

/*===========================================================================
 * Event Callbacks
 *===========================================================================*/

typedef enum {
    COGSERVER_EVENT_AGENT_CREATED,
    COGSERVER_EVENT_AGENT_DESTROYED,
    COGSERVER_EVENT_AGENT_STATE_CHANGED,
    COGSERVER_EVENT_TASK_CREATED,
    COGSERVER_EVENT_TASK_COMPLETED,
    COGSERVER_EVENT_TASK_FAILED,
    COGSERVER_EVENT_MESSAGE_SENT,
    COGSERVER_EVENT_MESSAGE_RECEIVED
} cogserver_event_type_t;

typedef struct cogserver_event {
    cogserver_event_type_t type;
    uint64_t timestamp;
    union {
        struct { uint64_t agent_id; agent_state_t old_state; agent_state_t new_state; } agent;
        struct { uint64_t task_id; task_state_t state; } task;
        struct { uint64_t sender_id; uint64_t receiver_id; } message;
    } data;
} cogserver_event_t;

typedef void (*cogserver_event_handler_t)(cogserver_event_t* event, void* user_data);

COGSERVER_API cog_result_t cogserver_set_event_handler(
    cogserver_t server,
    cogserver_event_handler_t handler,
    void* user_data
);

#ifdef __cplusplus
}
#endif

#endif /* _COGWXP_COGSERVER_H_ */
