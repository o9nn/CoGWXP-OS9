/**
 * @file hypermind.c
 * @brief HyperMind - Distributed Neural Network Framework Implementation
 *
 * Actor-based concurrent neural computation with reactive streams,
 * GPU acceleration, and distributed training.
 *
 * @copyright CoGWXP-OS9 Project
 */

#include "hypermind.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

/*===========================================================================
 * Internal Structures
 *===========================================================================*/

#define HM_MAX_ACTORS 1024
#define HM_MAX_STREAMS 256
#define HM_MAX_LAYERS 128
#define HM_MESSAGE_QUEUE_SIZE 4096

typedef struct hm_actor_internal {
    hm_actor_id_t id;
    hm_actor_type_t type;
    char name[256];
    hm_message_handler_t handler;
    void* user_data;
    bool active;

    /* Message queue */
    hm_message_t* queue;
    size_t queue_head;
    size_t queue_tail;
    size_t queue_capacity;
} hm_actor_internal_t;

typedef struct hm_stream_internal {
    hm_stream_id_t id;
    hm_stream_type_t type;
    hm_stream_callback_t callback;
    void* user_data;
    bool active;
    int64_t sequence;
} hm_stream_internal_t;

typedef struct hm_layer_internal {
    hm_layer_type_t type;
    char name[128];
    size_t input_shape[8];
    size_t input_ndim;
    size_t output_shape[8];
    size_t output_ndim;
    hm_activation_t activation;

    /* Weights and biases */
    float* weights;
    float* bias;
    size_t weight_count;
    size_t bias_count;

    /* Gradients */
    float* grad_weights;
    float* grad_bias;

    /* Cached activations for backward pass */
    float* cached_input;
    float* cached_output;
} hm_layer_internal_t;

struct hm_model {
    char name[256];
    hm_layer_internal_t layers[HM_MAX_LAYERS];
    size_t layer_count;
    float learning_rate;
    char optimizer[64];
    char loss[64];
    struct hm_context* ctx;
};

struct hm_reactor {
    hm_actor_id_t id;
    char name[256];
    size_t high_priority_queue_size;
    size_t normal_priority_queue_size;
    size_t state_cache_mb;
    int gpu_device_id;
    bool enable_cuda;
    bool enable_opencl;
    bool running;
};

struct hm_session {
    char name[256];
    struct hm_reactor** reactors;
    size_t reactor_count;
    bool enable_load_balancing;
    float load_threshold;
    char topology[64];
    bool enable_checkpointing;
    uint32_t checkpoint_interval_sec;
    char checkpoint_path[512];
    bool running;
    struct hm_context* ctx;

    /* Command tracking */
    hm_command_t* commands;
    size_t command_count;
    size_t command_capacity;
    hm_command_id_t next_command_id;
};

struct hm_context {
    hm_config_t config;

    /* Actor system */
    hm_actor_internal_t actors[HM_MAX_ACTORS];
    size_t actor_count;
    hm_actor_id_t next_actor_id;

    /* Streams */
    hm_stream_internal_t streams[HM_MAX_STREAMS];
    size_t stream_count;
    hm_stream_id_t next_stream_id;

    /* Statistics */
    hm_stats_t stats;

    /* State */
    bool initialized;
    bool in_cluster;
};

/*===========================================================================
 * Helper Functions
 *===========================================================================*/

static int64_t hm_current_time_ms(void) {
    return (int64_t)(clock() * 1000 / CLOCKS_PER_SEC);
}

static size_t hm_dtype_size(hm_dtype_t dtype) {
    switch (dtype) {
        case HM_DTYPE_FLOAT32: return sizeof(float);
        case HM_DTYPE_FLOAT64: return sizeof(double);
        case HM_DTYPE_FLOAT16: return 2;
        case HM_DTYPE_BFLOAT16: return 2;
        case HM_DTYPE_INT32: return sizeof(int32_t);
        case HM_DTYPE_INT64: return sizeof(int64_t);
        case HM_DTYPE_INT8: return sizeof(int8_t);
        case HM_DTYPE_UINT8: return sizeof(uint8_t);
        default: return 4;
    }
}

static size_t hm_shape_size(const size_t* shape, size_t ndim) {
    size_t size = 1;
    for (size_t i = 0; i < ndim; i++) {
        size *= shape[i];
    }
    return size;
}

static float hm_random_normal(void) {
    /* Box-Muller transform for normal distribution */
    float u1 = (float)rand() / RAND_MAX;
    float u2 = (float)rand() / RAND_MAX;
    return sqrtf(-2.0f * logf(u1 + 1e-7f)) * cosf(2.0f * 3.14159265f * u2);
}

static float hm_activation_forward(hm_activation_t activation, float x) {
    switch (activation) {
        case HM_ACTIVATION_RELU:
            return x > 0 ? x : 0;
        case HM_ACTIVATION_GELU:
            return 0.5f * x * (1.0f + tanhf(0.7978845608f * (x + 0.044715f * x * x * x)));
        case HM_ACTIVATION_TANH:
            return tanhf(x);
        case HM_ACTIVATION_SIGMOID:
            return 1.0f / (1.0f + expf(-x));
        case HM_ACTIVATION_SWISH:
            return x / (1.0f + expf(-x));
        default:
            return x;
    }
}

static float hm_activation_backward(hm_activation_t activation, float x, float grad) {
    switch (activation) {
        case HM_ACTIVATION_RELU:
            return x > 0 ? grad : 0;
        case HM_ACTIVATION_TANH: {
            float t = tanhf(x);
            return grad * (1.0f - t * t);
        }
        case HM_ACTIVATION_SIGMOID: {
            float s = 1.0f / (1.0f + expf(-x));
            return grad * s * (1.0f - s);
        }
        default:
            return grad;
    }
}

/*===========================================================================
 * Lifecycle
 *===========================================================================*/

ATENSPACE_API cog_result_t hm_init(
    const hm_config_t* config,
    hm_context_t* ctx
) {
    if (!config || !ctx) {
        return COG_ERROR_INVALID_ARGUMENT;
    }

    struct hm_context* context = calloc(1, sizeof(struct hm_context));
    if (!context) {
        return COG_ERROR_OUT_OF_MEMORY;
    }

    context->config = *config;
    context->next_actor_id = 1;
    context->next_stream_id = 1;
    context->initialized = true;

    /* Initialize random seed */
    srand((unsigned int)time(NULL));

    *ctx = context;
    return COG_OK;
}

ATENSPACE_API void hm_shutdown(hm_context_t ctx) {
    if (!ctx) return;

    /* Clean up actors */
    for (size_t i = 0; i < ctx->actor_count; i++) {
        if (ctx->actors[i].queue) {
            free(ctx->actors[i].queue);
        }
    }

    free(ctx);
}

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
) {
    if (!ctx || !name || !handler || !actor_id) {
        return COG_ERROR_INVALID_ARGUMENT;
    }

    if (ctx->actor_count >= HM_MAX_ACTORS) {
        return COG_ERROR_OUT_OF_MEMORY;
    }

    hm_actor_internal_t* actor = &ctx->actors[ctx->actor_count];
    actor->id = ctx->next_actor_id++;
    actor->type = type;
    strncpy(actor->name, name, sizeof(actor->name) - 1);
    actor->handler = handler;
    actor->user_data = user_data;
    actor->active = true;

    /* Initialize message queue */
    actor->queue_capacity = ctx->config.message_queue_size > 0 ?
                           ctx->config.message_queue_size : HM_MESSAGE_QUEUE_SIZE;
    actor->queue = calloc(actor->queue_capacity, sizeof(hm_message_t));
    if (!actor->queue) {
        return COG_ERROR_OUT_OF_MEMORY;
    }

    ctx->actor_count++;
    ctx->stats.active_actors++;
    *actor_id = actor->id;

    return COG_OK;
}

ATENSPACE_API cog_result_t hm_destroy_actor(
    hm_context_t ctx,
    hm_actor_id_t actor_id
) {
    if (!ctx) return COG_ERROR_INVALID_ARGUMENT;

    for (size_t i = 0; i < ctx->actor_count; i++) {
        if (ctx->actors[i].id == actor_id && ctx->actors[i].active) {
            ctx->actors[i].active = false;
            if (ctx->actors[i].queue) {
                free(ctx->actors[i].queue);
                ctx->actors[i].queue = NULL;
            }
            ctx->stats.active_actors--;
            return COG_OK;
        }
    }

    return COG_ERROR_NOT_FOUND;
}

ATENSPACE_API cog_result_t hm_send_message(
    hm_context_t ctx,
    const hm_message_t* message
) {
    if (!ctx || !message) {
        return COG_ERROR_INVALID_ARGUMENT;
    }

    /* Find recipient actor */
    hm_actor_internal_t* recipient = NULL;
    for (size_t i = 0; i < ctx->actor_count; i++) {
        if (ctx->actors[i].id == message->recipient && ctx->actors[i].active) {
            recipient = &ctx->actors[i];
            break;
        }
    }

    if (!recipient) {
        return COG_ERROR_NOT_FOUND;
    }

    /* Add to queue */
    size_t next_tail = (recipient->queue_tail + 1) % recipient->queue_capacity;
    if (next_tail == recipient->queue_head) {
        return COG_ERROR_OUT_OF_MEMORY; /* Queue full */
    }

    recipient->queue[recipient->queue_tail] = *message;
    recipient->queue[recipient->queue_tail].timestamp = hm_current_time_ms();
    recipient->queue_tail = next_tail;

    ctx->stats.messages_pending++;

    /* Process immediately for simplicity */
    recipient->handler(recipient->id, message, recipient->user_data);
    ctx->stats.messages_processed++;
    ctx->stats.messages_pending--;

    return COG_OK;
}

ATENSPACE_API cog_result_t hm_broadcast_message(
    hm_context_t ctx,
    hm_actor_type_t target_type,
    const hm_message_t* message
) {
    if (!ctx || !message) {
        return COG_ERROR_INVALID_ARGUMENT;
    }

    for (size_t i = 0; i < ctx->actor_count; i++) {
        if (ctx->actors[i].type == target_type && ctx->actors[i].active) {
            hm_message_t msg = *message;
            msg.recipient = ctx->actors[i].id;
            hm_send_message(ctx, &msg);
        }
    }

    return COG_OK;
}

/*===========================================================================
 * Reactive Streams
 *===========================================================================*/

ATENSPACE_API cog_result_t hm_create_stream(
    hm_context_t ctx,
    hm_stream_type_t type,
    hm_stream_callback_t callback,
    void* user_data,
    hm_stream_id_t* stream_id
) {
    if (!ctx || !callback || !stream_id) {
        return COG_ERROR_INVALID_ARGUMENT;
    }

    if (ctx->stream_count >= HM_MAX_STREAMS) {
        return COG_ERROR_OUT_OF_MEMORY;
    }

    hm_stream_internal_t* stream = &ctx->streams[ctx->stream_count];
    stream->id = ctx->next_stream_id++;
    stream->type = type;
    stream->callback = callback;
    stream->user_data = user_data;
    stream->active = true;
    stream->sequence = 0;

    ctx->stream_count++;
    *stream_id = stream->id;

    return COG_OK;
}

ATENSPACE_API cog_result_t hm_stream_push(
    hm_context_t ctx,
    hm_stream_id_t stream_id,
    const void* data,
    size_t size
) {
    if (!ctx || !data) {
        return COG_ERROR_INVALID_ARGUMENT;
    }

    hm_stream_internal_t* stream = NULL;
    for (size_t i = 0; i < ctx->stream_count; i++) {
        if (ctx->streams[i].id == stream_id && ctx->streams[i].active) {
            stream = &ctx->streams[i];
            break;
        }
    }

    if (!stream) {
        return COG_ERROR_NOT_FOUND;
    }

    hm_stream_event_t event = {
        .data = (void*)data,
        .size = size,
        .sequence = stream->sequence++,
        .is_error = false,
        .error_message = NULL
    };

    stream->callback(stream_id, &event, stream->user_data);

    return COG_OK;
}

ATENSPACE_API cog_result_t hm_stream_close(
    hm_context_t ctx,
    hm_stream_id_t stream_id
) {
    if (!ctx) return COG_ERROR_INVALID_ARGUMENT;

    for (size_t i = 0; i < ctx->stream_count; i++) {
        if (ctx->streams[i].id == stream_id) {
            ctx->streams[i].active = false;
            return COG_OK;
        }
    }

    return COG_ERROR_NOT_FOUND;
}

/*===========================================================================
 * Session Management
 *===========================================================================*/

ATENSPACE_API cog_result_t hm_create_session(
    hm_context_t ctx,
    const hm_session_config_t* config,
    hm_session_t* session
) {
    if (!ctx || !config || !session) {
        return COG_ERROR_INVALID_ARGUMENT;
    }

    struct hm_session* s = calloc(1, sizeof(struct hm_session));
    if (!s) {
        return COG_ERROR_OUT_OF_MEMORY;
    }

    if (config->session_name) {
        strncpy(s->name, config->session_name, sizeof(s->name) - 1);
    }

    s->enable_load_balancing = config->enable_load_balancing;
    s->load_threshold = config->load_threshold;

    if (config->topology) {
        strncpy(s->topology, config->topology, sizeof(s->topology) - 1);
    }

    s->enable_checkpointing = config->enable_checkpointing;
    s->checkpoint_interval_sec = config->checkpoint_interval_sec;

    if (config->checkpoint_path) {
        strncpy(s->checkpoint_path, config->checkpoint_path, sizeof(s->checkpoint_path) - 1);
    }

    s->ctx = ctx;

    /* Create reactors */
    if (config->reactor_count > 0 && config->reactor_configs) {
        s->reactors = calloc(config->reactor_count, sizeof(struct hm_reactor*));
        if (!s->reactors) {
            free(s);
            return COG_ERROR_OUT_OF_MEMORY;
        }

        for (size_t i = 0; i < config->reactor_count; i++) {
            struct hm_reactor* r = calloc(1, sizeof(struct hm_reactor));
            if (!r) {
                /* Cleanup */
                for (size_t j = 0; j < i; j++) {
                    free(s->reactors[j]);
                }
                free(s->reactors);
                free(s);
                return COG_ERROR_OUT_OF_MEMORY;
            }

            r->id = config->reactor_configs[i].id;
            if (config->reactor_configs[i].name) {
                strncpy(r->name, config->reactor_configs[i].name, sizeof(r->name) - 1);
            }
            r->high_priority_queue_size = config->reactor_configs[i].high_priority_queue_size;
            r->normal_priority_queue_size = config->reactor_configs[i].normal_priority_queue_size;
            r->state_cache_mb = config->reactor_configs[i].state_cache_mb;
            r->gpu_device_id = config->reactor_configs[i].gpu_device_id;
            r->enable_cuda = config->reactor_configs[i].enable_cuda;
            r->enable_opencl = config->reactor_configs[i].enable_opencl;

            s->reactors[i] = r;
            s->reactor_count++;
        }
    }

    /* Initialize command tracking */
    s->command_capacity = 256;
    s->commands = calloc(s->command_capacity, sizeof(hm_command_t));
    s->next_command_id = 1;

    *session = s;
    return COG_OK;
}

ATENSPACE_API cog_result_t hm_start_session(hm_session_t session) {
    if (!session) return COG_ERROR_INVALID_ARGUMENT;

    for (size_t i = 0; i < session->reactor_count; i++) {
        session->reactors[i]->running = true;
    }

    session->running = true;
    return COG_OK;
}

ATENSPACE_API cog_result_t hm_stop_session(hm_session_t session) {
    if (!session) return COG_ERROR_INVALID_ARGUMENT;

    for (size_t i = 0; i < session->reactor_count; i++) {
        session->reactors[i]->running = false;
    }

    session->running = false;
    return COG_OK;
}

ATENSPACE_API void hm_destroy_session(hm_session_t session) {
    if (!session) return;

    for (size_t i = 0; i < session->reactor_count; i++) {
        free(session->reactors[i]);
    }
    free(session->reactors);
    free(session->commands);
    free(session);
}

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
) {
    if (!session || !cmd_id) {
        return COG_ERROR_INVALID_ARGUMENT;
    }

    if (session->command_count >= session->command_capacity) {
        return COG_ERROR_OUT_OF_MEMORY;
    }

    hm_command_t* cmd = &session->commands[session->command_count];
    cmd->id = session->next_command_id++;
    cmd->type = type;
    cmd->status = HM_CMD_PENDING;
    cmd->input = (void*)input;
    cmd->input_size = input_size;
    cmd->submitted_at = hm_current_time_ms();

    session->command_count++;
    *cmd_id = cmd->id;

    /* Execute immediately for simplicity */
    cmd->status = HM_CMD_RUNNING;
    cmd->started_at = hm_current_time_ms();

    /* Simulate execution based on type */
    cmd->status = HM_CMD_COMPLETED;
    cmd->completed_at = hm_current_time_ms();

    if (callback) {
        callback(cmd, user_data);
    }

    return COG_OK;
}

ATENSPACE_API cog_result_t hm_wait_command(
    hm_session_t session,
    hm_command_id_t cmd_id,
    uint32_t timeout_ms,
    hm_command_t* result
) {
    if (!session || !result) {
        return COG_ERROR_INVALID_ARGUMENT;
    }

    for (size_t i = 0; i < session->command_count; i++) {
        if (session->commands[i].id == cmd_id) {
            *result = session->commands[i];
            return COG_OK;
        }
    }

    return COG_ERROR_NOT_FOUND;
}

ATENSPACE_API cog_result_t hm_cancel_command(
    hm_session_t session,
    hm_command_id_t cmd_id
) {
    if (!session) return COG_ERROR_INVALID_ARGUMENT;

    for (size_t i = 0; i < session->command_count; i++) {
        if (session->commands[i].id == cmd_id) {
            if (session->commands[i].status == HM_CMD_PENDING ||
                session->commands[i].status == HM_CMD_RUNNING) {
                session->commands[i].status = HM_CMD_CANCELLED;
                return COG_OK;
            }
            return COG_ERROR_INVALID_STATE;
        }
    }

    return COG_ERROR_NOT_FOUND;
}

/*===========================================================================
 * Model Operations
 *===========================================================================*/

static void hm_init_layer_weights(hm_layer_internal_t* layer) {
    /* Xavier/He initialization */
    float scale = sqrtf(2.0f / (float)(layer->weight_count > 0 ? layer->weight_count : 1));

    if (layer->weights) {
        for (size_t i = 0; i < layer->weight_count; i++) {
            layer->weights[i] = hm_random_normal() * scale;
        }
    }

    if (layer->bias) {
        for (size_t i = 0; i < layer->bias_count; i++) {
            layer->bias[i] = 0.0f;
        }
    }
}

ATENSPACE_API cog_result_t hm_create_model(
    hm_context_t ctx,
    const hm_model_config_t* config,
    hm_model_t* model
) {
    if (!ctx || !config || !model) {
        return COG_ERROR_INVALID_ARGUMENT;
    }

    struct hm_model* m = calloc(1, sizeof(struct hm_model));
    if (!m) {
        return COG_ERROR_OUT_OF_MEMORY;
    }

    if (config->name) {
        strncpy(m->name, config->name, sizeof(m->name) - 1);
    }

    m->learning_rate = config->learning_rate > 0 ? config->learning_rate : 0.001f;

    if (config->optimizer) {
        strncpy(m->optimizer, config->optimizer, sizeof(m->optimizer) - 1);
    } else {
        strcpy(m->optimizer, "adam");
    }

    if (config->loss) {
        strncpy(m->loss, config->loss, sizeof(m->loss) - 1);
    } else {
        strcpy(m->loss, "mse");
    }

    m->ctx = ctx;

    /* Initialize layers */
    for (size_t i = 0; i < config->layer_count && i < HM_MAX_LAYERS; i++) {
        hm_layer_internal_t* layer = &m->layers[i];
        const hm_layer_config_t* cfg = &config->layers[i];

        layer->type = cfg->type;
        if (cfg->name) {
            strncpy(layer->name, cfg->name, sizeof(layer->name) - 1);
        }

        /* Copy shapes */
        if (cfg->input_shape && cfg->input_ndim > 0) {
            memcpy(layer->input_shape, cfg->input_shape,
                   cfg->input_ndim * sizeof(size_t));
            layer->input_ndim = cfg->input_ndim;
        }

        if (cfg->output_shape && cfg->output_ndim > 0) {
            memcpy(layer->output_shape, cfg->output_shape,
                   cfg->output_ndim * sizeof(size_t));
            layer->output_ndim = cfg->output_ndim;
        }

        layer->activation = cfg->activation;

        /* Allocate weights based on layer type */
        if (layer->type == HM_LAYER_LINEAR) {
            size_t in_features = layer->input_ndim > 0 ?
                                 layer->input_shape[layer->input_ndim - 1] : 128;
            size_t out_features = layer->output_ndim > 0 ?
                                  layer->output_shape[layer->output_ndim - 1] : 128;

            layer->weight_count = in_features * out_features;
            layer->bias_count = out_features;

            layer->weights = calloc(layer->weight_count, sizeof(float));
            layer->bias = calloc(layer->bias_count, sizeof(float));
            layer->grad_weights = calloc(layer->weight_count, sizeof(float));
            layer->grad_bias = calloc(layer->bias_count, sizeof(float));

            hm_init_layer_weights(layer);
        }

        m->layer_count++;
    }

    *model = m;
    return COG_OK;
}

ATENSPACE_API cog_result_t hm_model_forward(
    hm_model_t model,
    const hm_tensor_t* input,
    hm_tensor_t** output
) {
    if (!model || !input || !output) {
        return COG_ERROR_INVALID_ARGUMENT;
    }

    model->ctx->stats.forward_passes++;

    /* Carry activations through each layer */
    float* current_data = (float*)input->data;
    size_t current_size = input->size;
    float* allocated_buf = NULL; /* Intermediate buffer owned by us */

    for (size_t l = 0; l < model->layer_count; l++) {
        hm_layer_internal_t* layer = &model->layers[l];

        if (layer->type == HM_LAYER_LINEAR &&
            layer->weights && layer->weight_count > 0) {

            size_t in_features = layer->input_ndim > 0 ?
                                 layer->input_shape[layer->input_ndim - 1] : current_size;
            size_t out_features = layer->output_ndim > 0 ?
                                  layer->output_shape[layer->output_ndim - 1] : layer->bias_count;

            if (in_features == 0 || out_features == 0) {
                /* Dimensions not set: pass through as identity */
                continue;
            }

            /* Cache input for backward pass */
            free(layer->cached_input);
            layer->cached_input = malloc(current_size * sizeof(float));
            if (layer->cached_input) {
                memcpy(layer->cached_input, current_data, current_size * sizeof(float));
            }

            size_t batch = current_size / in_features;
            if (batch == 0) batch = 1;

            float* new_buf = calloc(batch * out_features, sizeof(float));
            if (!new_buf) {
                free(allocated_buf);
                return COG_ERROR_OUT_OF_MEMORY;
            }

            /* Matrix multiply: out[b, j] = sum_i(in[b, i] * W[i, j]) + bias[j] */
            for (size_t b = 0; b < batch; b++) {
                for (size_t j = 0; j < out_features; j++) {
                    float sum = layer->bias ? layer->bias[j] : 0.0f;
                    for (size_t i = 0; i < in_features && (b * in_features + i) < current_size; i++) {
                        size_t w_idx = i * out_features + j;
                        if (w_idx < layer->weight_count) {
                            sum += current_data[b * in_features + i] * layer->weights[w_idx];
                        }
                    }
                    /* Apply activation */
                    new_buf[b * out_features + j] = hm_activation_forward(layer->activation, sum);
                }
            }

            /* Cache output for backward pass */
            free(layer->cached_output);
            layer->cached_output = malloc(batch * out_features * sizeof(float));
            if (layer->cached_output) {
                memcpy(layer->cached_output, new_buf, batch * out_features * sizeof(float));
            }

            free(allocated_buf);
            allocated_buf = new_buf;
            current_data = new_buf;
            current_size = batch * out_features;
        } else {
            /* Non-linear layers: apply element-wise activation */
            float* new_buf = malloc(current_size * sizeof(float));
            if (!new_buf) {
                free(allocated_buf);
                return COG_ERROR_OUT_OF_MEMORY;
            }

            for (size_t i = 0; i < current_size; i++) {
                new_buf[i] = hm_activation_forward(layer->activation, current_data[i]);
            }

            free(allocated_buf);
            allocated_buf = new_buf;
            current_data = new_buf;
        }
    }

    /* Build output tensor */
    hm_tensor_t* out = calloc(1, sizeof(hm_tensor_t));
    if (!out) {
        free(allocated_buf);
        return COG_ERROR_OUT_OF_MEMORY;
    }

    out->ndim = 1;
    out->shape = malloc(sizeof(size_t));
    if (!out->shape) {
        free(out);
        free(allocated_buf);
        return COG_ERROR_OUT_OF_MEMORY;
    }
    out->shape[0] = current_size;
    out->size = current_size;
    out->dtype = input->dtype;
    out->device_id = input->device_id;
    out->requires_grad = input->requires_grad;

    out->data = malloc(current_size * sizeof(float));
    if (!out->data) {
        free(out->shape);
        free(out);
        free(allocated_buf);
        return COG_ERROR_OUT_OF_MEMORY;
    }
    memcpy(out->data, current_data, current_size * sizeof(float));

    free(allocated_buf);

    model->ctx->stats.tensor_ops++;
    *output = out;

    return COG_OK;
}

ATENSPACE_API cog_result_t hm_model_backward(
    hm_model_t model,
    const hm_tensor_t* grad_output
) {
    if (!model || !grad_output) {
        return COG_ERROR_INVALID_ARGUMENT;
    }

    model->ctx->stats.backward_passes++;

    /* Simplified backward pass - compute gradients */
    for (size_t l = model->layer_count; l > 0; l--) {
        hm_layer_internal_t* layer = &model->layers[l - 1];

        /* Zero gradients first */
        if (layer->grad_weights) {
            memset(layer->grad_weights, 0, layer->weight_count * sizeof(float));
        }
        if (layer->grad_bias) {
            memset(layer->grad_bias, 0, layer->bias_count * sizeof(float));
        }

        /* Accumulate gradients (simplified) */
        float* grad_data = (float*)grad_output->data;
        for (size_t i = 0; i < grad_output->size && i < layer->bias_count; i++) {
            layer->grad_bias[i] += grad_data[i];
        }
    }

    return COG_OK;
}

ATENSPACE_API cog_result_t hm_model_update(hm_model_t model) {
    if (!model) return COG_ERROR_INVALID_ARGUMENT;

    /* Apply gradients with learning rate */
    for (size_t l = 0; l < model->layer_count; l++) {
        hm_layer_internal_t* layer = &model->layers[l];

        /* Update weights */
        if (layer->weights && layer->grad_weights) {
            for (size_t i = 0; i < layer->weight_count; i++) {
                layer->weights[i] -= model->learning_rate * layer->grad_weights[i];
            }
        }

        /* Update biases */
        if (layer->bias && layer->grad_bias) {
            for (size_t i = 0; i < layer->bias_count; i++) {
                layer->bias[i] -= model->learning_rate * layer->grad_bias[i];
            }
        }
    }

    return COG_OK;
}

ATENSPACE_API cog_result_t hm_model_train_batch(
    hm_model_t model,
    const hm_tensor_t* input,
    const hm_tensor_t* target,
    float* loss
) {
    if (!model || !input || !target || !loss) {
        return COG_ERROR_INVALID_ARGUMENT;
    }

    /* Forward pass */
    hm_tensor_t* output = NULL;
    cog_result_t result = hm_model_forward(model, input, &output);
    if (result != COG_OK) {
        return result;
    }

    /* Compute loss (MSE) */
    float total_loss = 0.0f;
    float* out_data = (float*)output->data;
    float* target_data = (float*)target->data;

    size_t n = output->size < target->size ? output->size : target->size;
    for (size_t i = 0; i < n; i++) {
        float diff = out_data[i] - target_data[i];
        total_loss += diff * diff;
    }
    *loss = total_loss / (float)n;

    /* Compute gradients */
    hm_tensor_t grad;
    grad.data = malloc(output->size * sizeof(float));
    grad.size = output->size;

    float* grad_data = (float*)grad.data;
    for (size_t i = 0; i < n; i++) {
        grad_data[i] = 2.0f * (out_data[i] - target_data[i]) / (float)n;
    }

    /* Backward pass */
    hm_model_backward(model, &grad);

    /* Update weights */
    hm_model_update(model);

    /* Cleanup */
    free(grad.data);
    hm_tensor_free(output);

    return COG_OK;
}

ATENSPACE_API cog_result_t hm_model_save(hm_model_t model, const char* path) {
    if (!model || !path) {
        return COG_ERROR_INVALID_ARGUMENT;
    }

    FILE* f = fopen(path, "wb");
    if (!f) {
        return COG_ERROR_IO;
    }

    /* Write header */
    fwrite("HMMD", 4, 1, f);  /* HyperMind Model Data */
    fwrite(&model->layer_count, sizeof(size_t), 1, f);
    fwrite(&model->learning_rate, sizeof(float), 1, f);

    /* Write layers */
    for (size_t i = 0; i < model->layer_count; i++) {
        hm_layer_internal_t* layer = &model->layers[i];
        fwrite(&layer->type, sizeof(hm_layer_type_t), 1, f);
        fwrite(&layer->weight_count, sizeof(size_t), 1, f);
        fwrite(&layer->bias_count, sizeof(size_t), 1, f);

        if (layer->weights) {
            fwrite(layer->weights, sizeof(float), layer->weight_count, f);
        }
        if (layer->bias) {
            fwrite(layer->bias, sizeof(float), layer->bias_count, f);
        }
    }

    fclose(f);
    return COG_OK;
}

ATENSPACE_API cog_result_t hm_model_load(
    hm_context_t ctx,
    const char* path,
    hm_model_t* model
) {
    if (!ctx || !path || !model) {
        return COG_ERROR_INVALID_ARGUMENT;
    }

    FILE* f = fopen(path, "rb");
    if (!f) {
        return COG_ERROR_NOT_FOUND;
    }

    /* Read header */
    char magic[4];
    fread(magic, 4, 1, f);
    if (memcmp(magic, "HMMD", 4) != 0) {
        fclose(f);
        return COG_ERROR_INVALID_FORMAT;
    }

    struct hm_model* m = calloc(1, sizeof(struct hm_model));
    if (!m) {
        fclose(f);
        return COG_ERROR_OUT_OF_MEMORY;
    }

    fread(&m->layer_count, sizeof(size_t), 1, f);
    fread(&m->learning_rate, sizeof(float), 1, f);
    m->ctx = ctx;

    /* Read layers */
    for (size_t i = 0; i < m->layer_count && i < HM_MAX_LAYERS; i++) {
        hm_layer_internal_t* layer = &m->layers[i];
        fread(&layer->type, sizeof(hm_layer_type_t), 1, f);
        fread(&layer->weight_count, sizeof(size_t), 1, f);
        fread(&layer->bias_count, sizeof(size_t), 1, f);

        if (layer->weight_count > 0) {
            layer->weights = malloc(layer->weight_count * sizeof(float));
            layer->grad_weights = calloc(layer->weight_count, sizeof(float));
            fread(layer->weights, sizeof(float), layer->weight_count, f);
        }

        if (layer->bias_count > 0) {
            layer->bias = malloc(layer->bias_count * sizeof(float));
            layer->grad_bias = calloc(layer->bias_count, sizeof(float));
            fread(layer->bias, sizeof(float), layer->bias_count, f);
        }
    }

    fclose(f);
    *model = m;
    return COG_OK;
}

ATENSPACE_API void hm_destroy_model(hm_model_t model) {
    if (!model) return;

    for (size_t i = 0; i < model->layer_count; i++) {
        hm_layer_internal_t* layer = &model->layers[i];
        free(layer->weights);
        free(layer->bias);
        free(layer->grad_weights);
        free(layer->grad_bias);
        free(layer->cached_input);
        free(layer->cached_output);
    }

    free(model);
}

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
) {
    if (!ctx || !shape || ndim == 0 || !tensor) {
        return COG_ERROR_INVALID_ARGUMENT;
    }

    hm_tensor_t* t = calloc(1, sizeof(hm_tensor_t));
    if (!t) {
        return COG_ERROR_OUT_OF_MEMORY;
    }

    t->ndim = ndim;
    t->shape = malloc(ndim * sizeof(size_t));
    if (!t->shape) {
        free(t);
        return COG_ERROR_OUT_OF_MEMORY;
    }

    memcpy(t->shape, shape, ndim * sizeof(size_t));
    t->size = hm_shape_size(shape, ndim);
    t->dtype = dtype;
    t->device_id = device_id;

    t->data = calloc(t->size, hm_dtype_size(dtype));
    if (!t->data) {
        free(t->shape);
        free(t);
        return COG_ERROR_OUT_OF_MEMORY;
    }

    ctx->stats.tensor_ops++;
    *tensor = t;

    return COG_OK;
}

ATENSPACE_API cog_result_t hm_tensor_from_data(
    hm_context_t ctx,
    const void* data,
    const size_t* shape,
    size_t ndim,
    hm_dtype_t dtype,
    hm_tensor_t** tensor
) {
    cog_result_t result = hm_tensor_create(ctx, shape, ndim, dtype, -1, tensor);
    if (result != COG_OK) {
        return result;
    }

    size_t byte_size = (*tensor)->size * hm_dtype_size(dtype);
    memcpy((*tensor)->data, data, byte_size);

    return COG_OK;
}

ATENSPACE_API cog_result_t hm_tensor_to_device(hm_tensor_t* tensor, int device_id) {
    if (!tensor) return COG_ERROR_INVALID_ARGUMENT;
    tensor->device_id = device_id;
    return COG_OK;
}

ATENSPACE_API cog_result_t hm_tensor_matmul(
    hm_context_t ctx,
    const hm_tensor_t* a,
    const hm_tensor_t* b,
    hm_tensor_t** result
) {
    if (!ctx || !a || !b || !result) {
        return COG_ERROR_INVALID_ARGUMENT;
    }

    if (a->ndim < 2 || b->ndim < 2) {
        return COG_ERROR_INVALID_ARGUMENT;
    }

    size_t m = a->shape[a->ndim - 2];
    size_t k1 = a->shape[a->ndim - 1];
    size_t k2 = b->shape[b->ndim - 2];
    size_t n = b->shape[b->ndim - 1];

    if (k1 != k2) {
        return COG_ERROR_DIMENSION_MISMATCH;
    }

    size_t out_shape[2] = {m, n};
    cog_result_t res = hm_tensor_create(ctx, out_shape, 2, a->dtype, a->device_id, result);
    if (res != COG_OK) {
        return res;
    }

    /* Matrix multiplication */
    float* a_data = (float*)a->data;
    float* b_data = (float*)b->data;
    float* out_data = (float*)(*result)->data;

    for (size_t i = 0; i < m; i++) {
        for (size_t j = 0; j < n; j++) {
            float sum = 0.0f;
            for (size_t p = 0; p < k1; p++) {
                sum += a_data[i * k1 + p] * b_data[p * n + j];
            }
            out_data[i * n + j] = sum;
        }
    }

    ctx->stats.tensor_ops++;
    return COG_OK;
}

ATENSPACE_API cog_result_t hm_tensor_add(
    hm_context_t ctx,
    const hm_tensor_t* a,
    const hm_tensor_t* b,
    hm_tensor_t** result
) {
    if (!ctx || !a || !b || !result) {
        return COG_ERROR_INVALID_ARGUMENT;
    }

    if (a->size != b->size) {
        return COG_ERROR_DIMENSION_MISMATCH;
    }

    cog_result_t res = hm_tensor_create(ctx, a->shape, a->ndim, a->dtype, a->device_id, result);
    if (res != COG_OK) {
        return res;
    }

    float* a_data = (float*)a->data;
    float* b_data = (float*)b->data;
    float* out_data = (float*)(*result)->data;

    for (size_t i = 0; i < a->size; i++) {
        out_data[i] = a_data[i] + b_data[i];
    }

    ctx->stats.tensor_ops++;
    return COG_OK;
}

ATENSPACE_API void hm_tensor_free(hm_tensor_t* tensor) {
    if (!tensor) return;
    free(tensor->data);
    free(tensor->shape);
    free(tensor);
}

/*===========================================================================
 * Distributed Operations
 *===========================================================================*/

ATENSPACE_API cog_result_t hm_cluster_join(
    hm_context_t ctx,
    const char* cluster_address,
    uint16_t port
) {
    if (!ctx || !cluster_address) {
        return COG_ERROR_INVALID_ARGUMENT;
    }

    /* Simulated cluster join */
    ctx->in_cluster = true;
    return COG_OK;
}

ATENSPACE_API cog_result_t hm_cluster_leave(hm_context_t ctx) {
    if (!ctx) return COG_ERROR_INVALID_ARGUMENT;
    ctx->in_cluster = false;
    return COG_OK;
}

ATENSPACE_API cog_result_t hm_distributed_all_reduce(
    hm_context_t ctx,
    hm_tensor_t* tensor
) {
    if (!ctx || !tensor) {
        return COG_ERROR_INVALID_ARGUMENT;
    }

    if (!ctx->in_cluster) {
        return COG_OK;  /* No-op if not in cluster */
    }

    /* Simulated all-reduce - in real impl would sync across nodes */
    ctx->stats.bytes_sent += tensor->size * hm_dtype_size(tensor->dtype);
    ctx->stats.bytes_received += tensor->size * hm_dtype_size(tensor->dtype);

    return COG_OK;
}

ATENSPACE_API cog_result_t hm_distributed_broadcast(
    hm_context_t ctx,
    hm_tensor_t* tensor,
    int root_rank
) {
    if (!ctx || !tensor) {
        return COG_ERROR_INVALID_ARGUMENT;
    }

    if (!ctx->in_cluster) {
        return COG_OK;
    }

    ctx->stats.bytes_sent += tensor->size * hm_dtype_size(tensor->dtype);

    return COG_OK;
}

/*===========================================================================
 * Statistics
 *===========================================================================*/

ATENSPACE_API cog_result_t hm_get_stats(hm_context_t ctx, hm_stats_t* stats) {
    if (!ctx || !stats) {
        return COG_ERROR_INVALID_ARGUMENT;
    }

    *stats = ctx->stats;
    return COG_OK;
}

ATENSPACE_API void hm_reset_stats(hm_context_t ctx) {
    if (!ctx) return;
    memset(&ctx->stats, 0, sizeof(hm_stats_t));
    ctx->stats.active_actors = (uint32_t)ctx->actor_count;
}
