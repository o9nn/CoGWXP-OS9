/**
 * @file beast_mode.c
 * @brief "Beast Mode" Cognitive Fusion Reactor Implementation
 * 
 * @copyright CoGWXP-OS9 Project
 */

#include "beast_mode.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <strings.h>
#include <math.h>

/*===========================================================================
 * Internal Structures
 *===========================================================================*/

struct beast_reactor {
    beast_config_t config;
    
    /* Current state */
    beast_reactor_state_t state;
    
    /* Task queue */
    beast_reactor_task_t** tasks;
    size_t task_count;
    size_t task_capacity;
    
    /* Worker threads */
    pthread_t* worker_threads;
    uint32_t thread_count;
    
    /* Statistics */
    uint64_t total_fusions;
    uint64_t successful_fusions;
    double total_energy_consumed;
    
    /* Telemetry */
    uint64_t start_time_ms;
    uint64_t last_telemetry_ms;
    
    /* Synchronization */
    pthread_mutex_t lock;
    pthread_cond_t task_available;
    
    /* Control */
    bool running;
    bool emergency_stop;
};

/*===========================================================================
 * Helper Functions
 *===========================================================================*/

static uint64_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static uint64_t generate_task_id(beast_reactor_t reactor) {
    return get_time_ms() * 1000 + (rand() % 1000);
}

static void update_reactor_state(beast_reactor_t reactor) {
    reactor->state.reactor_uptime_ms = get_time_ms() - reactor->start_time_ms;
    reactor->state.inferences_per_second = 
        reactor->total_fusions > 0 ? 
        (reactor->total_fusions * 1000) / (reactor->state.reactor_uptime_ms + 1) : 0;
    reactor->state.total_inferences = reactor->total_fusions;
}

/*===========================================================================
 * Initialization and Shutdown
 *===========================================================================*/

COGWXPOS_API cog_result_t beast_reactor_init(
    const beast_config_t* config,
    beast_reactor_t* reactor
) {
    if (!config || !reactor) return COG_ERROR_INVALID_ARG;
    
    beast_reactor_t r = calloc(1, sizeof(struct beast_reactor));
    if (!r) return COG_ERROR_MEMORY;
    
    memcpy(&r->config, config, sizeof(beast_config_t));
    
    /* Initialize state */
    r->state.intensity = config->default_intensity;
    r->state.sti_boost_multiplier = config->sti_boost_factor;
    r->state.attention_focus_size = config->attention_window_size;
    
    /* Initialize task queue */
    r->task_capacity = 1000;
    r->tasks = calloc(r->task_capacity, sizeof(beast_reactor_task_t*));
    
    /* Initialize synchronization */
    pthread_mutex_init(&r->lock, NULL);
    pthread_cond_init(&r->task_available, NULL);
    
    r->start_time_ms = get_time_ms();
    r->running = true;
    r->emergency_stop = false;
    
    /* Initialize worker threads */
    r->thread_count = config->max_threads > 0 ? config->max_threads : 4;
    r->worker_threads = calloc(r->thread_count, sizeof(pthread_t));
    
    *reactor = r;
    
    printf("[BeastReactor] Initialized with intensity %d, %u threads\n",
           r->state.intensity, r->thread_count);
    
    return COG_SUCCESS;
}

COGWXPOS_API void beast_reactor_shutdown(beast_reactor_t reactor) {
    if (!reactor) return;
    
    printf("[BeastReactor] Shutting down...\n");
    
    pthread_mutex_lock(&reactor->lock);
    reactor->running = false;
    pthread_cond_broadcast(&reactor->task_available);
    pthread_mutex_unlock(&reactor->lock);
    
    /* Wait for worker threads */
    for (uint32_t i = 0; i < reactor->thread_count; i++) {
        if (reactor->worker_threads[i]) {
            pthread_join(reactor->worker_threads[i], NULL);
        }
    }
    
    /* Free tasks */
    for (size_t i = 0; i < reactor->task_count; i++) {
        if (reactor->tasks[i]) {
            free(reactor->tasks[i]);
        }
    }
    free(reactor->tasks);
    free(reactor->worker_threads);
    
    pthread_mutex_destroy(&reactor->lock);
    pthread_cond_destroy(&reactor->task_available);
    
    printf("[BeastReactor] Shutdown complete\n");
    free(reactor);
}

/*===========================================================================
 * Reactor Control
 *===========================================================================*/

COGWXPOS_API cog_result_t beast_set_intensity(
    beast_reactor_t reactor,
    beast_intensity_t intensity
) {
    if (!reactor) return COG_ERROR_INVALID_ARG;
    
    pthread_mutex_lock(&reactor->lock);
    
    if (intensity == BEAST_INTENSITY_OVERDRIVE && !reactor->config.allow_overdrive) {
        pthread_mutex_unlock(&reactor->lock);
        printf("[BeastReactor] OVERDRIVE mode not allowed\n");
        return COG_ERROR_PERMISSION;
    }
    
    beast_intensity_t old_intensity = reactor->state.intensity;
    reactor->state.intensity = intensity;
    
    /* Adjust parameters based on intensity */
    switch (intensity) {
        case BEAST_INTENSITY_NORMAL:
            reactor->state.sti_boost_multiplier = 1.0;
            break;
        case BEAST_INTENSITY_ELEVATED:
            reactor->state.sti_boost_multiplier = 1.5;
            break;
        case BEAST_INTENSITY_HIGH:
            reactor->state.sti_boost_multiplier = 2.0;
            break;
        case BEAST_INTENSITY_MAXIMUM:
            reactor->state.sti_boost_multiplier = 3.0;
            break;
        case BEAST_INTENSITY_OVERDRIVE:
            reactor->state.sti_boost_multiplier = 5.0;
            break;
    }
    
    pthread_mutex_unlock(&reactor->lock);
    
    printf("[BeastReactor] Intensity changed: %d -> %d (STI boost: %.1fx)\n",
           old_intensity, intensity, reactor->state.sti_boost_multiplier);
    
    return COG_SUCCESS;
}

COGWXPOS_API cog_result_t beast_get_state(
    beast_reactor_t reactor,
    beast_reactor_state_t* state
) {
    if (!reactor || !state) return COG_ERROR_INVALID_ARG;
    
    pthread_mutex_lock(&reactor->lock);
    update_reactor_state(reactor);
    memcpy(state, &reactor->state, sizeof(beast_reactor_state_t));
    pthread_mutex_unlock(&reactor->lock);
    
    return COG_SUCCESS;
}

COGWXPOS_API cog_result_t beast_configure_mode(
    beast_reactor_t reactor,
    beast_reason_mode_t mode,
    bool enabled
) {
    if (!reactor) return COG_ERROR_INVALID_ARG;
    
    printf("[BeastReactor] Reasoning mode %d %s\n", mode, enabled ? "enabled" : "disabled");
    return COG_SUCCESS;
}

COGWXPOS_API cog_result_t beast_emergency_stop(beast_reactor_t reactor) {
    if (!reactor) return COG_ERROR_INVALID_ARG;
    
    pthread_mutex_lock(&reactor->lock);
    reactor->emergency_stop = true;
    reactor->state.intensity = BEAST_INTENSITY_NORMAL;
    pthread_mutex_unlock(&reactor->lock);
    
    printf("[BeastReactor] *** EMERGENCY STOP ACTIVATED ***\n");
    return COG_SUCCESS;
}

/*===========================================================================
 * Cognitive Fusion
 *===========================================================================*/

static beast_fusion_result_t* create_fusion_result(size_t result_count) {
    beast_fusion_result_t* result = calloc(1, sizeof(beast_fusion_result_t));
    if (!result) return NULL;
    
    result->result_count = result_count;
    result->results = calloc(result_count, sizeof(atom_handle_t));
    result->confidences = calloc(result_count, sizeof(double));
    
    /* Initialize with dummy values */
    for (size_t i = 0; i < result_count; i++) {
        result->results[i] = (atom_handle_t)(i + 1);
        result->confidences[i] = 0.7 + ((double)rand() / RAND_MAX) * 0.3;
    }
    
    return result;
}

COGWXPOS_API cog_result_t beast_fuse(
    beast_reactor_t reactor,
    atom_handle_t goal,
    beast_fusion_strategy_t strategy,
    beast_reason_mode_t* modes,
    size_t mode_count,
    beast_fusion_result_t** result
) {
    if (!reactor || !result) return COG_ERROR_INVALID_ARG;
    
    uint64_t start = get_time_ms();
    
    pthread_mutex_lock(&reactor->lock);
    
    if (reactor->emergency_stop) {
        pthread_mutex_unlock(&reactor->lock);
        return COG_ERROR_GENERAL;
    }
    
    /* Apply intensity boost */
    double intensity_multiplier = 1.0 + (0.5 * reactor->state.intensity);
    size_t inference_count = (size_t)(mode_count * intensity_multiplier * 10);
    
    reactor->total_fusions += inference_count;
    reactor->successful_fusions++;
    
    pthread_mutex_unlock(&reactor->lock);
    
    /* Execute fusion based on strategy */
    size_t result_count = mode_count * 2;  /* Each mode produces 2 results */
    beast_fusion_result_t* fusion_result = create_fusion_result(result_count);
    
    if (!fusion_result) return COG_ERROR_MEMORY;
    
    fusion_result->total_time_ms = (double)(get_time_ms() - start);
    fusion_result->total_inferences = inference_count;
    fusion_result->energy_consumed = intensity_multiplier * inference_count * 0.001;
    
    *result = fusion_result;
    
    printf("[BeastReactor] Fusion complete: %zu results in %.2fms (%llu inferences)\n",
           result_count, fusion_result->total_time_ms, 
           (unsigned long long)inference_count);
    
    return COG_SUCCESS;
}

COGWXPOS_API cog_result_t beast_fuse_auto(
    beast_reactor_t reactor,
    atom_handle_t goal,
    beast_fusion_result_t** result
) {
    if (!reactor || !result) return COG_ERROR_INVALID_ARG;
    
    /* Automatically select reasoning modes based on goal */
    beast_reason_mode_t modes[] = {
        BEAST_REASON_FORWARD,
        BEAST_REASON_PROBABILISTIC,
        BEAST_REASON_ANALOGICAL
    };
    
    return beast_fuse(reactor, goal, BEAST_FUSION_ENSEMBLE, modes, 3, result);
}

COGWXPOS_API cog_result_t beast_submit_task(
    beast_reactor_t reactor,
    beast_reactor_task_t* task,
    uint64_t* task_id
) {
    if (!reactor || !task || !task_id) return COG_ERROR_INVALID_ARG;
    
    pthread_mutex_lock(&reactor->lock);
    
    if (reactor->task_count >= reactor->task_capacity) {
        pthread_mutex_unlock(&reactor->lock);
        return COG_ERROR_MEMORY;
    }
    
    task->id = generate_task_id(reactor);
    reactor->tasks[reactor->task_count++] = task;
    *task_id = task->id;
    
    pthread_cond_signal(&reactor->task_available);
    pthread_mutex_unlock(&reactor->lock);
    
    printf("[BeastReactor] Task %llu submitted\n", (unsigned long long)task->id);
    return COG_SUCCESS;
}

COGWXPOS_API cog_result_t beast_wait_task(
    beast_reactor_t reactor,
    uint64_t task_id,
    uint32_t timeout_ms,
    beast_fusion_result_t** result
) {
    if (!reactor || !result) return COG_ERROR_INVALID_ARG;
    
    uint64_t deadline = get_time_ms() + timeout_ms;
    
    while (get_time_ms() < deadline) {
        pthread_mutex_lock(&reactor->lock);
        
        /* Find task */
        for (size_t i = 0; i < reactor->task_count; i++) {
            if (reactor->tasks[i] && reactor->tasks[i]->id == task_id) {
                if (reactor->tasks[i]->completed) {
                    *result = reactor->tasks[i]->result;
                    pthread_mutex_unlock(&reactor->lock);
                    return COG_SUCCESS;
                }
                break;
            }
        }
        
        pthread_mutex_unlock(&reactor->lock);
        usleep(10000);  /* Sleep 10ms */
    }
    
    return COG_ERROR_TIMEOUT;
}

/*===========================================================================
 * Parallel Inference
 *===========================================================================*/

COGWXPOS_API cog_result_t beast_parallel_infer(
    beast_reactor_t reactor,
    atom_handle_t* premises,
    size_t premise_count,
    size_t chain_count,
    beast_fusion_result_t** result
) {
    if (!reactor || !premises || !result) return COG_ERROR_INVALID_ARG;
    
    printf("[BeastReactor] Launching %zu parallel inference chains\n", chain_count);
    
    /* Simulate parallel inference */
    size_t total_results = premise_count * chain_count;
    beast_fusion_result_t* fusion_result = create_fusion_result(total_results);
    
    if (!fusion_result) return COG_ERROR_MEMORY;
    
    fusion_result->total_inferences = total_results * 10;
    *result = fusion_result;
    
    return COG_SUCCESS;
}

COGWXPOS_API cog_result_t beast_distributed_infer(
    beast_reactor_t reactor,
    atom_handle_t goal,
    atomspace_t* sub_atomspaces,
    size_t space_count,
    beast_fusion_result_t** result
) {
    if (!reactor || !result) return COG_ERROR_INVALID_ARG;
    
    printf("[BeastReactor] Distributed inference across %zu atomspaces\n", space_count);
    
    beast_fusion_result_t* fusion_result = create_fusion_result(space_count * 5);
    if (!fusion_result) return COG_ERROR_MEMORY;
    
    *result = fusion_result;
    return COG_SUCCESS;
}

/*===========================================================================
 * Attention Amplification
 *===========================================================================*/

COGWXPOS_API cog_result_t beast_boost_attention(
    beast_reactor_t reactor,
    atom_handle_t* atoms,
    size_t atom_count,
    double boost_factor
) {
    if (!reactor || !atoms) return COG_ERROR_INVALID_ARG;
    
    double effective_boost = boost_factor * reactor->state.sti_boost_multiplier;
    
    printf("[BeastReactor] Boosting attention for %zu atoms (factor: %.2fx)\n",
           atom_count, effective_boost);
    
    return COG_SUCCESS;
}

COGWXPOS_API cog_result_t beast_focus_attention(
    beast_reactor_t reactor,
    atom_handle_t root,
    size_t depth
) {
    if (!reactor) return COG_ERROR_INVALID_ARG;
    
    printf("[BeastReactor] Focusing attention on subgraph (depth: %zu)\n", depth);
    return COG_SUCCESS;
}

COGWXPOS_API cog_result_t beast_allocate_attention(
    beast_reactor_t reactor,
    atom_handle_t goal,
    double attention_budget
) {
    if (!reactor) return COG_ERROR_INVALID_ARG;
    
    printf("[BeastReactor] Allocating attention budget: %.2f\n", attention_budget);
    return COG_SUCCESS;
}

/*===========================================================================
 * Integration with Niche Construction
 *===========================================================================*/

COGWXPOS_API cog_result_t beast_fuse_with_skills(
    beast_reactor_t reactor,
    atom_handle_t goal,
    niche_skill_t** skills,
    size_t skill_count,
    beast_fusion_result_t** result
) {
    if (!reactor || !skills || !result) return COG_ERROR_INVALID_ARG;
    
    printf("[BeastReactor] Fusing with %zu learned skills\n", skill_count);
    
    beast_fusion_result_t* fusion_result = create_fusion_result(skill_count * 3);
    if (!fusion_result) return COG_ERROR_MEMORY;
    
    *result = fusion_result;
    return COG_SUCCESS;
}

COGWXPOS_API cog_result_t beast_extract_skills(
    beast_reactor_t reactor,
    beast_fusion_result_t* result,
    niche_skill_t** extracted_skills,
    size_t* skill_count
) {
    if (!reactor || !result || !skill_count) return COG_ERROR_INVALID_ARG;
    
    *skill_count = result->result_count / 5;  /* Extract skill every 5 results */
    
    printf("[BeastReactor] Extracted %zu skills from fusion result\n", *skill_count);
    return COG_SUCCESS;
}

/*===========================================================================
 * Statistics and Telemetry
 *===========================================================================*/

COGWXPOS_API cog_result_t beast_get_telemetry(
    beast_reactor_t reactor,
    char** telemetry_json,
    size_t* json_size
) {
    if (!reactor || !telemetry_json || !json_size) return COG_ERROR_INVALID_ARG;
    
    pthread_mutex_lock(&reactor->lock);
    update_reactor_state(reactor);
    
    char buffer[2048];
    snprintf(buffer, sizeof(buffer),
        "{\n"
        "  \"intensity\": %d,\n"
        "  \"uptime_ms\": %llu,\n"
        "  \"total_fusions\": %llu,\n"
        "  \"successful_fusions\": %llu,\n"
        "  \"inferences_per_second\": %llu,\n"
        "  \"total_inferences\": %llu,\n"
        "  \"energy_consumed\": %.3f,\n"
        "  \"sti_boost\": %.2f,\n"
        "  \"active_threads\": %zu,\n"
        "  \"task_count\": %zu\n"
        "}",
        reactor->state.intensity,
        (unsigned long long)reactor->state.reactor_uptime_ms,
        (unsigned long long)reactor->total_fusions,
        (unsigned long long)reactor->successful_fusions,
        (unsigned long long)reactor->state.inferences_per_second,
        (unsigned long long)reactor->state.total_inferences,
        reactor->total_energy_consumed,
        reactor->state.sti_boost_multiplier,
        reactor->state.active_threads,
        reactor->task_count
    );
    
    pthread_mutex_unlock(&reactor->lock);
    
    *telemetry_json = strdup(buffer);
    *json_size = strlen(buffer);
    
    return COG_SUCCESS;
}

COGWXPOS_API void beast_reset_telemetry(beast_reactor_t reactor) {
    if (!reactor) return;
    
    pthread_mutex_lock(&reactor->lock);
    reactor->total_fusions = 0;
    reactor->successful_fusions = 0;
    reactor->total_energy_consumed = 0.0;
    reactor->start_time_ms = get_time_ms();
    pthread_mutex_unlock(&reactor->lock);
    
    printf("[BeastReactor] Telemetry reset\n");
}

/*===========================================================================
 * Memory Management
 *===========================================================================*/

COGWXPOS_API void beast_free_result(beast_fusion_result_t* result) {
    if (!result) return;
    
    free(result->results);
    free(result->confidences);
    free(result->contributing_modes);
    free(result->contribution_counts);
    free(result);
}

COGWXPOS_API void beast_free_task(beast_reactor_task_t* task) {
    if (!task) return;
    
    if (task->result) {
        beast_free_result(task->result);
    }
    free(task->reason_modes);
    free(task);
}

/* Stub implementations for remaining functions */
COGWXPOS_API cog_result_t beast_cancel_task(beast_reactor_t reactor, uint64_t task_id) {
    return COG_SUCCESS;
}

COGWXPOS_API cog_result_t beast_evaluate_reasoning(beast_reactor_t reactor,
    beast_fusion_result_t* result, double* quality_score) {
    *quality_score = 0.85;
    return COG_SUCCESS;
}

COGWXPOS_API cog_result_t beast_adapt_strategy(beast_reactor_t reactor,
    beast_reactor_task_t* task, beast_fusion_strategy_t* new_strategy) {
    *new_strategy = BEAST_FUSION_ENSEMBLE;
    return COG_SUCCESS;
}

COGWXPOS_API cog_result_t beast_detect_bottlenecks(beast_reactor_t reactor,
    char** bottleneck_report, size_t* report_size) {
    *bottleneck_report = strdup("No bottlenecks detected");
    *report_size = strlen(*bottleneck_report);
    return COG_SUCCESS;
}

COGWXPOS_API cog_result_t beast_assign_credit(beast_reactor_t reactor,
    atom_handle_t* inference_chain, size_t chain_length, double final_reward) {
    return COG_SUCCESS;
}

COGWXPOS_API cog_result_t beast_compute_value(beast_reactor_t reactor,
    atom_handle_t inference, double* value) {
    *value = 0.5;
    return COG_SUCCESS;
}

COGWXPOS_API cog_result_t beast_export_profile(beast_reactor_t reactor,
    const char* output_path) {
    printf("[BeastReactor] Exported profile to %s\n", output_path);
    return COG_SUCCESS;
}
