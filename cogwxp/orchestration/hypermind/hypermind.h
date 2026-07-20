/**
 * @file hypermind.h
 * @brief HyperMind - Distributed Neural Network Framework
 *
 * Actor-based concurrent neural computation with reactive streams,
 * GPU acceleration, and formal verification.
 *
 * Based on: https://github.com/o9nn/hypermind
 *
 * @copyright CoGWXP-OS9 Project
 */

#ifndef COGWXP_HYPERMIND_H
#define COGWXP_HYPERMIND_H

#include "../orchestration.h"
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
 * Actor System
 *===========================================================================*/

typedef uint64_t hm_actor_id_t;

typedef enum {
    HM_ACTOR_REACTOR,      /* Neural computation reactor */
    HM_ACTOR_MANAGER,      /* Coordinates reactors */
    HM_ACTOR_DIRECTOR,     /* Top-level orchestrator */
    HM_ACTOR_GPU_WORKER,   /* GPU computation worker */
    HM_ACTOR_DB_WORKER,    /* Database I/O worker */
    HM_ACTOR_NET_WORKER    /* Network I/O worker */
} hm_actor_type_t;

typedef enum {
    HM_MSG_FORWARD,        /* Forward pass */
    HM_MSG_BACKWARD,       /* Backward pass */
    HM_MSG_UPDATE,         /* Weight update */
    HM_MSG_SYNC,           /* Synchronize state */
    HM_MSG_CHECKPOINT,     /* Save checkpoint */
    HM_MSG_SHUTDOWN,       /* Shutdown actor */
    HM_MSG_CUSTOM
} hm_message_type_t;

typedef struct {
    hm_message_type_t type;
    hm_actor_id_t sender;
    hm_actor_id_t recipient;
    void* payload;
    size_t payload_size;
    int64_t timestamp;
    uint32_t priority;
} hm_message_t;

typedef void (*hm_message_handler_t)(hm_actor_id_t actor, const hm_message_t* msg, void* user_data);

/*===========================================================================
 * Reactive Streams
 *===========================================================================*/

typedef uint64_t hm_stream_id_t;

typedef enum {
    HM_STREAM_CPU,         /* CPU tensor stream */
    HM_STREAM_GPU,         /* GPU tensor stream */
    HM_STREAM_DB,          /* Database stream */
    HM_STREAM_NET,         /* Network stream */
    HM_STREAM_FILE         /* File I/O stream */
} hm_stream_type_t;

typedef struct {
    void* data;
    size_t size;
    int64_t sequence;
    bool is_error;
    const char* error_message;
} hm_stream_event_t;

typedef void (*hm_stream_callback_t)(hm_stream_id_t stream, const hm_stream_event_t* event, void* user_data);

/*===========================================================================
 * Neural Reactor
 *===========================================================================*/

typedef struct {
    /* Reactor identity */
    hm_actor_id_t id;
    const char* name;

    /* Processing queues */
    size_t high_priority_queue_size;
    size_t normal_priority_queue_size;

    /* State cache */
    size_t state_cache_mb;

    /* GPU binding */
    int gpu_device_id;
    bool enable_cuda;
    bool enable_opencl;

    /* Database connection */
    const char* db_connection_string;

} hm_reactor_config_t;

typedef struct hm_reactor* hm_reactor_t;

/*===========================================================================
 * Session Initiator
 *===========================================================================*/

typedef struct {
    const char* session_name;

    /* Reactor pool */
    size_t reactor_count;
    hm_reactor_config_t* reactor_configs;

    /* Load balancing */
    bool enable_load_balancing;
    float load_threshold;

    /* Network topology */
    const char* topology;  /* "ring", "mesh", "star", "tree" */

    /* Checkpointing */
    bool enable_checkpointing;
    uint32_t checkpoint_interval_sec;
    const char* checkpoint_path;

} hm_session_config_t;

typedef struct hm_session* hm_session_t;

/*===========================================================================
 * Command Pattern
 *===========================================================================*/

typedef uint64_t hm_command_id_t;

typedef enum {
    HM_CMD_TRAIN_BATCH,
    HM_CMD_EVALUATE,
    HM_CMD_PREDICT,
    HM_CMD_SAVE_MODEL,
    HM_CMD_LOAD_MODEL,
    HM_CMD_OPTIMIZE,
    HM_CMD_CUSTOM
} hm_command_type_t;

typedef enum {
    HM_CMD_PENDING,
    HM_CMD_RUNNING,
    HM_CMD_COMPLETED,
    HM_CMD_FAILED,
    HM_CMD_CANCELLED
} hm_command_status_t;

typedef struct {
    hm_command_id_t id;
    hm_command_type_t type;
    hm_command_status_t status;
    void* input;
    size_t input_size;
    void* output;
    size_t output_size;
    const char* error_message;
    int64_t submitted_at;
    int64_t started_at;
    int64_t completed_at;
} hm_command_t;

typedef void (*hm_command_callback_t)(const hm_command_t* cmd, void* user_data);

/*===========================================================================
 * Neural Network Model
 *===========================================================================*/

typedef enum {
    HM_LAYER_LINEAR,
    HM_LAYER_CONV2D,
    HM_LAYER_CONV3D,
    HM_LAYER_LSTM,
    HM_LAYER_GRU,
    HM_LAYER_TRANSFORMER,
    HM_LAYER_ATTENTION,
    HM_LAYER_BATCHNORM,
    HM_LAYER_LAYERNORM,
    HM_LAYER_DROPOUT,
    HM_LAYER_EMBEDDING,
    HM_LAYER_POOLING,
    HM_LAYER_ACTIVATION
} hm_layer_type_t;

typedef enum {
    HM_ACTIVATION_RELU,
    HM_ACTIVATION_GELU,
    HM_ACTIVATION_TANH,
    HM_ACTIVATION_SIGMOID,
    HM_ACTIVATION_SOFTMAX,
    HM_ACTIVATION_SWISH
} hm_activation_t;

typedef struct {
    hm_layer_type_t type;
    const char* name;
    size_t* input_shape;
    size_t input_ndim;
    size_t* output_shape;
    size_t output_ndim;
    hm_activation_t activation;

    /* Layer-specific params as JSON */
    const char* params_json;
} hm_layer_config_t;

typedef struct {
    const char* name;
    hm_layer_config_t* layers;
    size_t layer_count;
    float learning_rate;
    const char* optimizer;  /* "adam", "sgd", "rmsprop" */
    const char* loss;       /* "mse", "cross_entropy", "nll" */
} hm_model_config_t;

typedef struct hm_model* hm_model_t;

/*===========================================================================
 * Tensor Operations
 *===========================================================================*/

typedef enum {
    HM_DTYPE_FLOAT32,
    HM_DTYPE_FLOAT64,
    HM_DTYPE_FLOAT16,
    HM_DTYPE_BFLOAT16,
    HM_DTYPE_INT32,
    HM_DTYPE_INT64,
    HM_DTYPE_INT8,
    HM_DTYPE_UINT8
} hm_dtype_t;

typedef struct {
    void* data;
    size_t* shape;
    size_t ndim;
    size_t size;
    hm_dtype_t dtype;
    int device_id;  /* -1 for CPU */
    bool requires_grad;
} hm_tensor_t;

/*===========================================================================
 * Configuration
 *===========================================================================*/

typedef struct {
    /* Actor system */
    size_t max_actors;
    size_t message_queue_size;
    uint32_t worker_threads;

    /* GPU */
    bool enable_cuda;
    bool enable_opencl;
    int* cuda_devices;
    size_t cuda_device_count;

    /* Memory */
    size_t tensor_pool_mb;
    size_t message_pool_mb;

    /* Database */
    const char* db_connection_string;
    size_t db_pool_size;

    /* Network */
    const char* cluster_address;
    uint16_t cluster_port;

    /* Logging */
    const char* log_path;
    bool enable_tracing;

} hm_config_t;

/*===========================================================================
 * Statistics
 *===========================================================================*/

typedef struct {
    /* Actors */
    uint32_t active_actors;
    uint64_t messages_processed;
    uint64_t messages_pending;

    /* Computation */
    uint64_t forward_passes;
    uint64_t backward_passes;
    uint64_t tensor_ops;

    /* GPU */
    float gpu_utilization;
    size_t gpu_memory_used_mb;

    /* Performance */
    double avg_batch_time_ms;
    double avg_forward_time_ms;
    double avg_backward_time_ms;

    /* Network */
    uint64_t bytes_sent;
    uint64_t bytes_received;

} hm_stats_t;

/*===========================================================================
 * Context Handle
 *===========================================================================*/

typedef struct hm_context* hm_context_t;

/*===========================================================================
 * Lifecycle
 *===========================================================================*/

ATENSPACE_API cog_result_t hm_init(
    const hm_config_t* config,
    hm_context_t* ctx
);

ATENSPACE_API void hm_shutdown(hm_context_t ctx);

/*===========================================================================
 * Actor Operations
 *===========================================================================*/

ATENSPACE_API cog_result_t hm_create_actor(
    hm_context_t ctx,
    hm_actor_type_t type,
    const char* name,
    hm_message_handler_t handler,
    void* user_data,
    hm_actor_id_t* actor_id
);

ATENSPACE_API cog_result_t hm_destroy_actor(
    hm_context_t ctx,
    hm_actor_id_t actor_id
);

ATENSPACE_API cog_result_t hm_send_message(
    hm_context_t ctx,
    const hm_message_t* message
);

ATENSPACE_API cog_result_t hm_broadcast_message(
    hm_context_t ctx,
    hm_actor_type_t target_type,
    const hm_message_t* message
);

/*===========================================================================
 * Reactive Streams
 *===========================================================================*/

ATENSPACE_API cog_result_t hm_create_stream(
    hm_context_t ctx,
    hm_stream_type_t type,
    hm_stream_callback_t callback,
    void* user_data,
    hm_stream_id_t* stream_id
);

ATENSPACE_API cog_result_t hm_stream_push(
    hm_context_t ctx,
    hm_stream_id_t stream_id,
    const void* data,
    size_t size
);

ATENSPACE_API cog_result_t hm_stream_close(
    hm_context_t ctx,
    hm_stream_id_t stream_id
);

/*===========================================================================
 * Session Management
 *===========================================================================*/

ATENSPACE_API cog_result_t hm_create_session(
    hm_context_t ctx,
    const hm_session_config_t* config,
    hm_session_t* session
);

ATENSPACE_API cog_result_t hm_start_session(hm_session_t session);

ATENSPACE_API cog_result_t hm_stop_session(hm_session_t session);

ATENSPACE_API void hm_destroy_session(hm_session_t session);

/*===========================================================================
 * Command Execution
 *===========================================================================*/

ATENSPACE_API cog_result_t hm_submit_command(
    hm_session_t session,
    hm_command_type_t type,
    const void* input,
    size_t input_size,
    hm_command_callback_t callback,
    void* user_data,
    hm_command_id_t* cmd_id
);

ATENSPACE_API cog_result_t hm_wait_command(
    hm_session_t session,
    hm_command_id_t cmd_id,
    uint32_t timeout_ms,
    hm_command_t* result
);

ATENSPACE_API cog_result_t hm_cancel_command(
    hm_session_t session,
    hm_command_id_t cmd_id
);

/*===========================================================================
 * Model Operations
 *===========================================================================*/

ATENSPACE_API cog_result_t hm_create_model(
    hm_context_t ctx,
    const hm_model_config_t* config,
    hm_model_t* model
);

ATENSPACE_API cog_result_t hm_model_forward(
    hm_model_t model,
    const hm_tensor_t* input,
    hm_tensor_t** output
);

ATENSPACE_API cog_result_t hm_model_backward(
    hm_model_t model,
    const hm_tensor_t* grad_output
);

ATENSPACE_API cog_result_t hm_model_update(
    hm_model_t model
);

ATENSPACE_API cog_result_t hm_model_train_batch(
    hm_model_t model,
    const hm_tensor_t* input,
    const hm_tensor_t* target,
    float* loss
);

ATENSPACE_API cog_result_t hm_model_save(
    hm_model_t model,
    const char* path
);

ATENSPACE_API cog_result_t hm_model_load(
    hm_context_t ctx,
    const char* path,
    hm_model_t* model
);

ATENSPACE_API void hm_destroy_model(hm_model_t model);

/*===========================================================================
 * Tensor Operations
 *===========================================================================*/

ATENSPACE_API cog_result_t hm_tensor_create(
    hm_context_t ctx,
    const size_t* shape,
    size_t ndim,
    hm_dtype_t dtype,
    int device_id,
    hm_tensor_t** tensor
);

ATENSPACE_API cog_result_t hm_tensor_from_data(
    hm_context_t ctx,
    const void* data,
    const size_t* shape,
    size_t ndim,
    hm_dtype_t dtype,
    hm_tensor_t** tensor
);

ATENSPACE_API cog_result_t hm_tensor_to_device(
    hm_tensor_t* tensor,
    int device_id
);

ATENSPACE_API cog_result_t hm_tensor_matmul(
    hm_context_t ctx,
    const hm_tensor_t* a,
    const hm_tensor_t* b,
    hm_tensor_t** result
);

ATENSPACE_API cog_result_t hm_tensor_add(
    hm_context_t ctx,
    const hm_tensor_t* a,
    const hm_tensor_t* b,
    hm_tensor_t** result
);

ATENSPACE_API void hm_tensor_free(hm_tensor_t* tensor);

/*===========================================================================
 * Distributed Operations
 *===========================================================================*/

ATENSPACE_API cog_result_t hm_cluster_join(
    hm_context_t ctx,
    const char* cluster_address,
    uint16_t port
);

ATENSPACE_API cog_result_t hm_cluster_leave(hm_context_t ctx);

ATENSPACE_API cog_result_t hm_distributed_all_reduce(
    hm_context_t ctx,
    hm_tensor_t* tensor
);

ATENSPACE_API cog_result_t hm_distributed_broadcast(
    hm_context_t ctx,
    hm_tensor_t* tensor,
    int root_rank
);

/*===========================================================================
 * Statistics
 *===========================================================================*/

ATENSPACE_API cog_result_t hm_get_stats(
    hm_context_t ctx,
    hm_stats_t* stats
);

ATENSPACE_API void hm_reset_stats(hm_context_t ctx);

#ifdef __cplusplus
}
#endif

#endif /* COGWXP_HYPERMIND_H */
