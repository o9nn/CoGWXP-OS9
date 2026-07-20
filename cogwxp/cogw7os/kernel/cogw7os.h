/**
 * @file cogw7os.h
 * @brief CogW7OS - Cognitive Windows 7 Operating System Kernel
 * 
 * CogW7OS extends the Windows NT kernel with cognitive computing capabilities,
 * integrating OpenCog AtomSpace, PLN reasoning, and distributed computing
 * through 9P/Styx protocols.
 * 
 * @copyright Research/Educational Use
 */

#ifndef _COGW7OS_KERNEL_H_
#define _COGW7OS_KERNEL_H_

#include "../../opencog/cogutil/cogutil.h"
#include "../../opencog/atomspace/atomspace.h"
#include "../../opencog/pln/pln.h"
#include "../../plan9/9p/9p.h"
#include "../../inferno/dis/dis.h"

#ifdef COGUTIL_PLATFORM_NT
    #ifdef COGW7OS_EXPORTS
        #define COGW7OS_API __declspec(dllexport)
    #else
        #define COGW7OS_API __declspec(dllimport)
    #endif
#else
    #define COGW7OS_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * CogW7OS Version
 *===========================================================================*/

#define COGW7OS_VERSION_MAJOR   1
#define COGW7OS_VERSION_MINOR   0
#define COGW7OS_VERSION_PATCH   0
#define COGW7OS_VERSION_STRING  "1.0.0"
#define COGW7OS_CODENAME        "Cognitive Synergy"

/*===========================================================================
 * Kernel Object Types
 *===========================================================================*/

typedef enum {
    COGW7_OBJECT_PROCESS = 0,
    COGW7_OBJECT_THREAD,
    COGW7_OBJECT_MUTEX,
    COGW7_OBJECT_SEMAPHORE,
    COGW7_OBJECT_EVENT,
    COGW7_OBJECT_TIMER,
    COGW7_OBJECT_FILE,
    COGW7_OBJECT_SECTION,
    COGW7_OBJECT_PORT,
    COGW7_OBJECT_KEY,
    
    /* Cognitive object types */
    COGW7_OBJECT_ATOMSPACE,
    COGW7_OBJECT_ATOM,
    COGW7_OBJECT_PLN_ENGINE,
    COGW7_OBJECT_AGENT,
    COGW7_OBJECT_CHANNEL,
    COGW7_OBJECT_DIS_VM,
    COGW7_OBJECT_9P_CONNECTION,
    
    COGW7_OBJECT_MAX
} cogw7_object_type_t;

/*===========================================================================
 * Kernel Handle
 *===========================================================================*/

typedef uint64_t cogw7_handle_t;
#define COGW7_INVALID_HANDLE    ((cogw7_handle_t)0)
#define COGW7_CURRENT_PROCESS   ((cogw7_handle_t)-1)
#define COGW7_CURRENT_THREAD    ((cogw7_handle_t)-2)

/*===========================================================================
 * Internal implementation types (used by cogw7os.c)
 *===========================================================================*/

/* Process state (used by internal implementation) */
typedef enum {
    COGW7_PROC_CREATED = 0,
    COGW7_PROC_RUNNING,
    COGW7_PROC_SUSPENDED,
    COGW7_PROC_TERMINATED
} cogw7_process_state_t;

/* Service type (used by internal implementation) */
typedef enum {
    COGW7_SVC_MEMORY_MANAGER = 0,
    COGW7_SVC_PROCESS_MANAGER,
    COGW7_SVC_THREAD_SCHEDULER,
    COGW7_SVC_COGNITIVE_ENGINE,
    COGW7_SVC_ATTENTION_ALLOCATOR,
    COGW7_SVC_REASONING_ENGINE
} cogw7_service_type_t;

/* Kernel state (used by internal implementation) */
typedef enum {
    COGW7_KERNEL_UNINITIALIZED = 0,
    COGW7_KERNEL_INITIALIZING,
    COGW7_KERNEL_RUNNING,
    COGW7_KERNEL_SHUTTING_DOWN,
    COGW7_KERNEL_TERMINATED,
    /* Aliases used by implementation */
    COGW7_STATE_INIT         = COGW7_KERNEL_UNINITIALIZED,
    COGW7_STATE_BOOTING      = COGW7_KERNEL_INITIALIZING,
    COGW7_STATE_RUNNING      = COGW7_KERNEL_RUNNING,
    COGW7_STATE_SHUTTING_DOWN = COGW7_KERNEL_SHUTTING_DOWN,
    COGW7_STATE_HALTED       = COGW7_KERNEL_TERMINATED
} cogw7_kernel_state_t;

/* Kernel configuration (used by internal implementation) */
typedef struct {
    size_t max_processes;
    size_t max_threads_per_process;
    size_t default_stack_size;
    uint32_t scheduler_quantum_ms;
    bool enable_cognitive_scheduling;
    bool enable_attention_allocation;
    uint32_t reasoning_interval_ms;
} cogw7_config_t;

/* Internal statistics (used by internal implementation) */
typedef struct {
    size_t process_count;
    size_t thread_count;
    size_t agent_count;
    uint64_t total_processes_created;
    uint64_t total_threads_created;
    uint64_t scheduler_ticks;
    uint64_t reasoning_cycles;
    uint64_t inferences_made;
    uint64_t uptime_ms;
} cogw7_stats_t;

/* Opaque kernel handle (used by internal implementation) */
struct cogw7_kernel;
typedef struct cogw7_kernel* cogw7_kernel_t;

/* pln_context_t is an alias for pln_engine_t */
typedef pln_engine_t pln_context_t;

/* Process info structure (used by cogw7_process_get_info) */
typedef struct {
    uint32_t pid;
    char name[256];
    cogw7_process_state_t state;
    uint32_t parent_pid;
    size_t thread_count;
    size_t handle_count;
    size_t memory_used;
    atom_handle_t process_atom;
} cogw7_process_info_t;

/*===========================================================================
 * Process and Thread Structures
 *===========================================================================*/

#ifndef _COGW7_INTERNAL

typedef struct cogw7_process {
    cogw7_handle_t handle;
    uint32_t process_id;
    char* name;
    
    /* Memory */
    void* address_space;
    size_t virtual_size;
    size_t working_set;
    
    /* Threads */
    struct cogw7_thread** threads;
    size_t thread_count;
    
    /* Cognitive extensions */
    atomspace_t atomspace;          /* Process-local AtomSpace */
    pln_engine_t pln_engine;        /* Process-local PLN engine */
    dis_vm_t* dis_vm;               /* Dis VM instance */
    
    /* 9P namespace */
    p9_namespace_t namespace;
    
    /* Agent association */
    struct cogw7_agent* agent;
    
    /* Statistics */
    uint64_t cpu_time;
    uint64_t kernel_time;
    uint64_t user_time;
} cogw7_process_t;

typedef struct cogw7_thread {
    cogw7_handle_t handle;
    uint32_t thread_id;
    cogw7_process_t* process;
    
    /* Execution state */
    enum {
        COGW7_THREAD_READY,
        COGW7_THREAD_RUNNING,
        COGW7_THREAD_WAITING,
        COGW7_THREAD_SUSPENDED,
        COGW7_THREAD_TERMINATED
    } state;
    
    /* Priority */
    int8_t base_priority;
    int8_t dynamic_priority;
    
    /* Cognitive priority boost */
    int8_t cognitive_boost;         /* Based on attention value */
    
    /* Context */
    void* kernel_stack;
    void* user_stack;
    void* context;
    
    /* Dis thread (if running Limbo code) */
    dis_thread_t* dis_thread;
    
    /* Statistics */
    uint64_t cpu_cycles;
    uint64_t context_switches;
} cogw7_thread_t;

#else /* _COGW7_INTERNAL */

/* When compiled as internal implementation, use opaque forward declarations */
typedef struct cogw7_process cogw7_process_t;
typedef struct cogw7_thread cogw7_thread_t;

#endif /* _COGW7_INTERNAL */

/*===========================================================================
 * Cognitive Agent
 *===========================================================================*/

typedef enum {
    COGW7_AGENT_IDLE = 0,
    COGW7_AGENT_ACTIVE,
    COGW7_AGENT_REASONING,
    COGW7_AGENT_LEARNING,
    COGW7_AGENT_SUSPENDED
} cogw7_agent_state_t;

typedef struct cogw7_agent {
    cogw7_handle_t handle;
    char* name;
    cogw7_agent_state_t state;
    
    /* Capabilities */
    char** capabilities;
    size_t capability_count;
    
    /* Associated process */
    cogw7_process_t* process;
    
    /* AtomSpace view */
    atomspace_t local_atomspace;    /* Agent's local view */
    atomspace_t shared_atomspace;   /* Shared knowledge */
    
    /* Attention allocation */
    attention_value_t attention;
    
    /* Communication */
    dis_channel_t* inbox;
    dis_channel_t* outbox;
    
    /* Goals and tasks */
    atom_handle_t* goals;
    size_t goal_count;
    struct cogw7_task* current_task;
    
    /* Statistics */
    uint64_t inferences_performed;
    uint64_t messages_sent;
    uint64_t messages_received;
} cogw7_agent_t;

/*===========================================================================
 * Task Management
 *===========================================================================*/

typedef enum {
    COGW7_TASK_PENDING = 0,
    COGW7_TASK_RUNNING,
    COGW7_TASK_COMPLETED,
    COGW7_TASK_FAILED,
    COGW7_TASK_CANCELLED
} cogw7_task_state_t;

typedef struct cogw7_task {
    cogw7_handle_t handle;
    char* name;
    cogw7_task_state_t state;
    
    /* Task specification */
    atom_handle_t goal;
    atom_handle_t* inputs;
    size_t input_count;
    atom_handle_t* outputs;
    size_t output_count;
    
    /* Execution */
    cogw7_agent_t* assigned_agent;
    uint64_t start_time;
    uint64_t end_time;
    
    /* Priority */
    int16_t priority;
    
    /* Result */
    cog_result_t result_code;
    char* result_message;
} cogw7_task_t;

/*===========================================================================
 * Kernel Initialization
 *===========================================================================*/

typedef struct cogw7_init_config {
    /* Memory configuration */
    size_t kernel_heap_size;
    size_t atomspace_initial_size;
    
    /* Thread pool */
    size_t worker_thread_count;
    
    /* PLN configuration */
    pln_config_t pln_config;
    
    /* Dis VM configuration */
    dis_vm_config_t dis_config;
    
    /* 9P configuration */
    const char* p9_listen_address;
    uint16_t p9_listen_port;
    
    /* Logging */
    cog_log_level_t log_level;
    const char* log_file;
} cogw7_init_config_t;

COGW7OS_API cogw7_init_config_t cogw7_config_default(void);
COGW7OS_API cog_result_t cogw7_kernel_init(cogw7_init_config_t* config);
COGW7OS_API bool         cogw7_kernel_is_initialized(void);

#ifndef _COGW7_INTERNAL
/* Public API function signatures (matching the kernel implementation) */

/*===========================================================================
 * Kernel Lifecycle (handle-based API)
 *===========================================================================*/

COGW7OS_API cog_result_t cogw7_kernel_create(
    const cogw7_config_t* config,
    cogw7_kernel_t* kernel
);
COGW7OS_API void         cogw7_kernel_destroy(cogw7_kernel_t kernel);
COGW7OS_API cog_result_t cogw7_kernel_boot(cogw7_kernel_t kernel);
COGW7OS_API cog_result_t cogw7_kernel_shutdown(cogw7_kernel_t kernel);
COGW7OS_API cogw7_kernel_state_t cogw7_kernel_get_state(cogw7_kernel_t kernel);
COGW7OS_API cog_result_t cogw7_get_stats(cogw7_kernel_t kernel, cogw7_stats_t* stats);
COGW7OS_API atomspace_t  cogw7_get_atomspace(cogw7_kernel_t kernel);
COGW7OS_API pln_context_t cogw7_get_pln(cogw7_kernel_t kernel);

/*===========================================================================
 * Process Management
 *===========================================================================*/

COGW7OS_API cog_result_t cogw7_process_create(
    cogw7_kernel_t kernel,
    const char* name,
    uint32_t parent_pid,
    uint32_t* pid
);

COGW7OS_API cog_result_t cogw7_process_terminate(
    cogw7_kernel_t kernel,
    uint32_t pid,
    int exit_code
);

COGW7OS_API cog_result_t cogw7_process_get_info(
    cogw7_kernel_t kernel,
    uint32_t pid,
    cogw7_process_info_t* info
);

COGW7OS_API cog_result_t cogw7_process_set_atomspace(
    cogw7_handle_t process_handle,
    atomspace_t atomspace
);

COGW7OS_API atomspace_t cogw7_process_get_atomspace(
    cogw7_handle_t process_handle
);

/*===========================================================================
 * Thread Management
 *===========================================================================*/

COGW7OS_API cog_result_t cogw7_thread_create(
    cogw7_handle_t process_handle,
    cog_thread_func_t entry_point,
    void* parameter,
    cogw7_handle_t* thread_handle
);

COGW7OS_API cog_result_t cogw7_thread_terminate(
    cogw7_handle_t thread_handle,
    uint32_t exit_code
);

COGW7OS_API cog_result_t cogw7_thread_suspend(cogw7_handle_t thread_handle);
COGW7OS_API cog_result_t cogw7_thread_resume(cogw7_handle_t thread_handle);

COGW7OS_API cog_result_t cogw7_thread_set_priority(
    cogw7_handle_t thread_handle,
    int8_t priority
);

COGW7OS_API cog_result_t cogw7_thread_set_cognitive_boost(
    cogw7_handle_t thread_handle,
    int8_t boost
);

#endif /* _COGW7_INTERNAL */

/*===========================================================================
 * Agent Management
 *===========================================================================*/

COGW7OS_API cog_result_t cogw7_agent_create(
    const char* name,
    const char** capabilities,
    size_t capability_count,
    cogw7_handle_t* agent_handle
);

COGW7OS_API cog_result_t cogw7_agent_destroy(cogw7_handle_t agent_handle);

COGW7OS_API cog_result_t cogw7_agent_start(cogw7_handle_t agent_handle);
COGW7OS_API cog_result_t cogw7_agent_stop(cogw7_handle_t agent_handle);

COGW7OS_API cog_result_t cogw7_agent_send_message(
    cogw7_handle_t agent_handle,
    dis_value_t* message
);

COGW7OS_API cog_result_t cogw7_agent_receive_message(
    cogw7_handle_t agent_handle,
    dis_value_t* message,
    uint32_t timeout_ms
);

COGW7OS_API cog_result_t cogw7_agent_set_goal(
    cogw7_handle_t agent_handle,
    atom_handle_t goal
);

COGW7OS_API cog_result_t cogw7_agent_get_state(
    cogw7_handle_t agent_handle,
    cogw7_agent_state_t* state
);

/*===========================================================================
 * Task Scheduling
 *===========================================================================*/

COGW7OS_API cog_result_t cogw7_task_create(
    const char* name,
    atom_handle_t goal,
    int16_t priority,
    cogw7_handle_t* task_handle
);

COGW7OS_API cog_result_t cogw7_task_submit(cogw7_handle_t task_handle);
COGW7OS_API cog_result_t cogw7_task_cancel(cogw7_handle_t task_handle);

COGW7OS_API cog_result_t cogw7_task_wait(
    cogw7_handle_t task_handle,
    uint32_t timeout_ms
);

COGW7OS_API cog_result_t cogw7_task_get_result(
    cogw7_handle_t task_handle,
    atom_handle_t** outputs,
    size_t* output_count
);

/*===========================================================================
 * Global AtomSpace
 *===========================================================================*/

COGW7OS_API atomspace_t cogw7_get_global_atomspace(void);
COGW7OS_API pln_engine_t cogw7_get_global_pln_engine(void);

/*===========================================================================
 * 9P Integration
 *===========================================================================*/

COGW7OS_API cog_result_t cogw7_mount_9p(
    const char* address,
    uint16_t port,
    const char* mountpoint
);

COGW7OS_API cog_result_t cogw7_unmount_9p(const char* mountpoint);

COGW7OS_API cog_result_t cogw7_export_atomspace_9p(
    atomspace_t atomspace,
    const char* export_name
);

/*===========================================================================
 * Dis VM Integration
 *===========================================================================*/

COGW7OS_API dis_vm_t* cogw7_get_dis_vm(void);

COGW7OS_API cog_result_t cogw7_load_limbo_module(
    const char* module_path
);

COGW7OS_API cog_result_t cogw7_run_limbo_function(
    const char* module_name,
    const char* function_name,
    dis_value_t* args,
    size_t arg_count,
    dis_value_t* result
);

/*===========================================================================
 * Inter-Process Communication
 *===========================================================================*/

COGW7OS_API cog_result_t cogw7_channel_create(
    dis_type_t elem_type,
    size_t buffer_size,
    cogw7_handle_t* channel_handle
);

COGW7OS_API cog_result_t cogw7_channel_send(
    cogw7_handle_t channel_handle,
    dis_value_t* value
);

COGW7OS_API cog_result_t cogw7_channel_receive(
    cogw7_handle_t channel_handle,
    dis_value_t* value,
    uint32_t timeout_ms
);

COGW7OS_API cog_result_t cogw7_channel_close(cogw7_handle_t channel_handle);

/*===========================================================================
 * Cognitive Scheduling
 *===========================================================================*/

/* Attention-based scheduling */
COGW7OS_API cog_result_t cogw7_scheduler_set_attention_weight(double weight);
COGW7OS_API cog_result_t cogw7_scheduler_boost_by_attention(
    cogw7_handle_t thread_handle,
    attention_value_t av
);

/* Goal-directed scheduling */
COGW7OS_API cog_result_t cogw7_scheduler_set_goal_priority(
    atom_handle_t goal,
    int16_t priority
);

/*===========================================================================
 * Statistics and Monitoring
 *===========================================================================*/

typedef struct cogw7_kernel_stats {
    /* Process/Thread stats */
    size_t process_count;
    size_t thread_count;
    size_t agent_count;
    size_t task_count;
    
    /* Memory stats */
    size_t total_memory;
    size_t used_memory;
    size_t atomspace_memory;
    
    /* Cognitive stats */
    uint64_t total_inferences;
    uint64_t atoms_created;
    uint64_t messages_exchanged;
    
    /* Performance */
    double avg_inference_time_ms;
    double avg_task_completion_time_ms;
    uint64_t context_switches;
} cogw7_kernel_stats_t;

COGW7OS_API void cogw7_get_kernel_stats(cogw7_kernel_stats_t* stats);

/*===========================================================================
 * Debugging and Tracing
 *===========================================================================*/

typedef void (*cogw7_trace_callback_t)(
    const char* event,
    cogw7_handle_t handle,
    void* data,
    void* user_data
);

COGW7OS_API void cogw7_set_trace_callback(
    cogw7_trace_callback_t callback,
    void* user_data
);

COGW7OS_API void cogw7_dump_process(cogw7_handle_t process_handle);
COGW7OS_API void cogw7_dump_agent(cogw7_handle_t agent_handle);
COGW7OS_API void cogw7_dump_atomspace(atomspace_t atomspace);

#ifdef __cplusplus
}
#endif

#endif /* _COGW7OS_KERNEL_H_ */
