/**
 * @file beast_mode.h
 * @brief "Beast Mode" Cognitive Fusion Reactor
 * 
 * Implements high-intensity cognitive processing mode that combines multiple
 * reasoning strategies, parallel inference chains, and amplified attention
 * allocation for enhanced problem-solving capability.
 * 
 * Beast Mode characteristics:
 * - Parallel multi-modal inference (PLN + neural + symbolic)
 * - Elevated attention allocation (STI boost)
 * - Aggressive resource utilization
 * - Temporal credit assignment across long horizons
 * - Meta-cognitive monitoring and adaptation
 * 
 * @copyright CoGWXP-OS9 Project
 */

#ifndef _COGWXP_BEAST_MODE_H_
#define _COGWXP_BEAST_MODE_H_

#include "../opencog/cogutil/cogutil.h"
#include "../opencog/atomspace/atomspace.h"
#include "../opencog/pln/pln.h"
#include "niche_construction.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Beast Mode Types
 *===========================================================================*/

/**
 * Cognitive fusion strategy
 */
typedef enum {
    BEAST_FUSION_PARALLEL,      /* Run all strategies in parallel */
    BEAST_FUSION_SEQUENTIAL,    /* Run strategies in sequence */
    BEAST_FUSION_COMPETITIVE,   /* Compete strategies, select best */
    BEAST_FUSION_COLLABORATIVE, /* Strategies collaborate */
    BEAST_FUSION_ENSEMBLE       /* Ensemble voting */
} beast_fusion_strategy_t;

/**
 * Reasoning mode
 */
typedef enum {
    BEAST_REASON_FORWARD,       /* Forward chaining */
    BEAST_REASON_BACKWARD,      /* Backward chaining */
    BEAST_REASON_ABDUCTIVE,     /* Abductive reasoning */
    BEAST_REASON_ANALOGICAL,    /* Analogical reasoning */
    BEAST_REASON_PROBABILISTIC, /* PLN probabilistic */
    BEAST_REASON_CAUSAL,        /* Causal inference */
    BEAST_REASON_TEMPORAL,      /* Temporal reasoning */
    BEAST_REASON_SPATIAL        /* Spatial reasoning */
} beast_reason_mode_t;

/**
 * Intensity level
 */
typedef enum {
    BEAST_INTENSITY_NORMAL = 0,    /* Standard processing */
    BEAST_INTENSITY_ELEVATED = 1,  /* Increased resources */
    BEAST_INTENSITY_HIGH = 2,      /* High intensity */
    BEAST_INTENSITY_MAXIMUM = 3,   /* Maximum capability */
    BEAST_INTENSITY_OVERDRIVE = 4  /* Beyond safe limits */
} beast_intensity_t;

/**
 * Cognitive reactor state
 */
typedef struct {
    beast_intensity_t intensity;
    
    /* Active reasoning modes */
    beast_reason_mode_t* active_modes;
    size_t mode_count;
    
    /* Resource utilization */
    float cpu_utilization;
    float memory_utilization;
    size_t active_threads;
    
    /* Inference statistics */
    uint64_t inferences_per_second;
    uint64_t total_inferences;
    double avg_inference_depth;
    
    /* Attention allocation */
    double sti_boost_multiplier;
    size_t attention_focus_size;
    
    /* Timing */
    uint64_t reactor_uptime_ms;
    uint64_t last_cycle_time_ms;
} beast_reactor_state_t;

/**
 * Fusion result
 */
typedef struct {
    /* Result atoms */
    atom_handle_t* results;
    size_t result_count;
    
    /* Confidence scores */
    double* confidences;
    
    /* Strategy contributions */
    beast_reason_mode_t* contributing_modes;
    size_t* contribution_counts;
    
    /* Performance metrics */
    double total_time_ms;
    uint64_t total_inferences;
    double energy_consumed;
} beast_fusion_result_t;

/**
 * Reactor task
 */
typedef struct {
    uint64_t id;
    
    /* Goal */
    atom_handle_t goal;
    
    /* Strategy */
    beast_fusion_strategy_t fusion_strategy;
    beast_reason_mode_t* reason_modes;
    size_t mode_count;
    
    /* Resource limits */
    uint32_t max_time_ms;
    uint32_t max_inferences;
    double max_energy;
    
    /* Priority */
    int16_t priority;
    
    /* Status */
    bool completed;
    bool success;
    
    /* Result */
    beast_fusion_result_t* result;
} beast_reactor_task_t;

/*===========================================================================
 * Beast Mode Reactor
 *===========================================================================*/

typedef struct beast_reactor* beast_reactor_t;

/**
 * Configuration for beast mode reactor
 */
typedef struct {
    /* Core components */
    atomspace_t atomspace;
    pln_engine_t pln_engine;
    niche_engine_t niche_engine;  /* For skill-based reasoning */
    
    /* Intensity settings */
    beast_intensity_t default_intensity;
    bool allow_overdrive;
    
    /* Resource limits */
    uint32_t max_threads;
    size_t max_memory_bytes;
    double max_cpu_percent;
    
    /* Reasoning configuration */
    size_t max_inference_depth;
    size_t max_inference_breadth;
    double inference_confidence_threshold;
    
    /* Attention allocation */
    double sti_boost_factor;
    size_t attention_window_size;
    bool dynamic_attention;
    
    /* Fusion parameters */
    beast_fusion_strategy_t default_fusion_strategy;
    double ensemble_voting_threshold;
    
    /* Performance tuning */
    bool enable_parallel_inference;
    bool enable_caching;
    bool enable_pruning;
    double pruning_threshold;
    
    /* Monitoring */
    bool enable_telemetry;
    uint32_t telemetry_interval_ms;
    
    /* Safety */
    double thermal_limit;
    double energy_budget;
    bool enable_circuit_breaker;
} beast_config_t;

/**
 * Initialize beast mode reactor
 */
COGUTIL_API cog_result_t beast_reactor_init(
    const beast_config_t* config,
    beast_reactor_t* reactor
);

/**
 * Shutdown beast mode reactor
 */
COGUTIL_API void beast_reactor_shutdown(beast_reactor_t reactor);

/*===========================================================================
 * Reactor Control
 *===========================================================================*/

/**
 * Set reactor intensity level
 */
COGUTIL_API cog_result_t beast_set_intensity(
    beast_reactor_t reactor,
    beast_intensity_t intensity
);

/**
 * Get current reactor state
 */
COGUTIL_API cog_result_t beast_get_state(
    beast_reactor_t reactor,
    beast_reactor_state_t* state
);

/**
 * Enable/disable specific reasoning mode
 */
COGUTIL_API cog_result_t beast_configure_mode(
    beast_reactor_t reactor,
    beast_reason_mode_t mode,
    bool enabled
);

/**
 * Emergency shutdown (rapid cooldown)
 */
COGUTIL_API cog_result_t beast_emergency_stop(
    beast_reactor_t reactor
);

/*===========================================================================
 * Cognitive Fusion
 *===========================================================================*/

/**
 * Execute cognitive fusion on a goal
 */
COGUTIL_API cog_result_t beast_fuse(
    beast_reactor_t reactor,
    atom_handle_t goal,
    beast_fusion_strategy_t strategy,
    beast_reason_mode_t* modes,
    size_t mode_count,
    beast_fusion_result_t** result
);

/**
 * Execute fusion with automatic mode selection
 */
COGUTIL_API cog_result_t beast_fuse_auto(
    beast_reactor_t reactor,
    atom_handle_t goal,
    beast_fusion_result_t** result
);

/**
 * Submit task for asynchronous fusion
 */
COGUTIL_API cog_result_t beast_submit_task(
    beast_reactor_t reactor,
    beast_reactor_task_t* task,
    uint64_t* task_id
);

/**
 * Wait for task completion
 */
COGUTIL_API cog_result_t beast_wait_task(
    beast_reactor_t reactor,
    uint64_t task_id,
    uint32_t timeout_ms,
    beast_fusion_result_t** result
);

/**
 * Cancel task
 */
COGUTIL_API cog_result_t beast_cancel_task(
    beast_reactor_t reactor,
    uint64_t task_id
);

/*===========================================================================
 * Parallel Inference
 *===========================================================================*/

/**
 * Launch parallel inference chains
 */
COGUTIL_API cog_result_t beast_parallel_infer(
    beast_reactor_t reactor,
    atom_handle_t* premises,
    size_t premise_count,
    size_t chain_count,
    beast_fusion_result_t** result
);

/**
 * Distributed inference across multiple reasoning contexts
 */
COGUTIL_API cog_result_t beast_distributed_infer(
    beast_reactor_t reactor,
    atom_handle_t goal,
    atomspace_t* sub_atomspaces,
    size_t space_count,
    beast_fusion_result_t** result
);

/*===========================================================================
 * Attention Amplification
 *===========================================================================*/

/**
 * Boost STI (Short-Term Importance) for atoms
 */
COGUTIL_API cog_result_t beast_boost_attention(
    beast_reactor_t reactor,
    atom_handle_t* atoms,
    size_t atom_count,
    double boost_factor
);

/**
 * Focus attention on subgraph
 */
COGUTIL_API cog_result_t beast_focus_attention(
    beast_reactor_t reactor,
    atom_handle_t root,
    size_t depth
);

/**
 * Dynamic attention allocation based on task
 */
COGUTIL_API cog_result_t beast_allocate_attention(
    beast_reactor_t reactor,
    atom_handle_t goal,
    double attention_budget
);

/*===========================================================================
 * Meta-Cognitive Monitoring
 *===========================================================================*/

/**
 * Evaluate reasoning quality
 */
COGUTIL_API cog_result_t beast_evaluate_reasoning(
    beast_reactor_t reactor,
    beast_fusion_result_t* result,
    double* quality_score
);

/**
 * Adapt strategy based on performance
 */
COGUTIL_API cog_result_t beast_adapt_strategy(
    beast_reactor_t reactor,
    beast_reactor_task_t* task,
    beast_fusion_strategy_t* new_strategy
);

/**
 * Detect reasoning bottlenecks
 */
COGUTIL_API cog_result_t beast_detect_bottlenecks(
    beast_reactor_t reactor,
    char** bottleneck_report,
    size_t* report_size
);

/*===========================================================================
 * Integration with Niche Construction
 *===========================================================================*/

/**
 * Use learned skills in fusion
 */
COGUTIL_API cog_result_t beast_fuse_with_skills(
    beast_reactor_t reactor,
    atom_handle_t goal,
    niche_skill_t** skills,
    size_t skill_count,
    beast_fusion_result_t** result
);

/**
 * Learn new skills from successful fusion
 */
COGUTIL_API cog_result_t beast_extract_skills(
    beast_reactor_t reactor,
    beast_fusion_result_t* result,
    niche_skill_t** extracted_skills,
    size_t* skill_count
);

/*===========================================================================
 * Temporal Credit Assignment
 *===========================================================================*/

/**
 * Assign credit across long inference chains
 */
COGUTIL_API cog_result_t beast_assign_credit(
    beast_reactor_t reactor,
    atom_handle_t* inference_chain,
    size_t chain_length,
    double final_reward
);

/**
 * Compute value of intermediate inferences
 */
COGUTIL_API cog_result_t beast_compute_value(
    beast_reactor_t reactor,
    atom_handle_t inference,
    double* value
);

/*===========================================================================
 * Statistics and Telemetry
 *===========================================================================*/

/**
 * Get reactor telemetry
 */
COGUTIL_API cog_result_t beast_get_telemetry(
    beast_reactor_t reactor,
    char** telemetry_json,
    size_t* json_size
);

/**
 * Reset telemetry counters
 */
COGUTIL_API void beast_reset_telemetry(beast_reactor_t reactor);

/**
 * Export performance profile
 */
COGUTIL_API cog_result_t beast_export_profile(
    beast_reactor_t reactor,
    const char* output_path
);

/*===========================================================================
 * Memory Management
 *===========================================================================*/

/**
 * Free fusion result
 */
COGUTIL_API void beast_free_result(beast_fusion_result_t* result);

/**
 * Free reactor task
 */
COGUTIL_API void beast_free_task(beast_reactor_task_t* task);

#ifdef __cplusplus
}
#endif

#endif /* _COGWXP_BEAST_MODE_H_ */
