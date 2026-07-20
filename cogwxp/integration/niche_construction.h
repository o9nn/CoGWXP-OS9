/**
 * @file niche_construction.h
 * @brief Adaptive Niche Construction Engine
 * 
 * Implements environment-shaping techniques where the system can write to its
 * environment (tools, UI, memory, prompts, external notes) to improve future
 * efficiency and reliability. Based on closed-loop transformation:
 * 
 *   intent → (policy/plan) → execution trace → outcome → belief update
 * 
 * Uses opponent cycles (propose/normalize) and diffusion-based technique
 * generation for adaptive learning and skill development.
 * 
 * @copyright CoGWXP-OS9 Project
 */

#ifndef _COGWXP_NICHE_CONSTRUCTION_H_
#define _COGWXP_NICHE_CONSTRUCTION_H_

#include "../opencog/cogutil/cogutil.h"
#include "../opencog/atomspace/atomspace.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef COGUTIL_PLATFORM_NT
    #ifndef COGWXPOS_API
        #ifdef COGWXPOS_EXPORTS
            #define COGWXPOS_API __declspec(dllexport)
        #else
            #define COGWXPOS_API __declspec(dllimport)
        #endif
    #endif
#else
    #ifndef COGWXPOS_API
        #define COGWXPOS_API
    #endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Niche Construction Types
 *===========================================================================*/

/**
 * Technique glyph representation - a visual/structural encoding of an
 * execution sequence for diagnostic and learning purposes
 */
typedef struct niche_glyph {
    uint64_t id;
    char* name;
    
    /* Glyph data (can be image, vector field, or structured data) */
    uint8_t* data;
    size_t data_size;
    
    /* Metadata */
    uint64_t timestamp;
    double success_rate;
    uint32_t usage_count;
    
    /* Associated intent */
    atom_handle_t intent;
    
    /* Technique parameters */
    double energy;
    double complexity;
} niche_glyph_t;

/**
 * Execution trace - time series of states, actions, and observations
 */
typedef struct niche_trace {
    uint64_t id;
    
    /* Trace components */
    void** states;      /* latent states (s_t) */
    void** actions;     /* actions/gestures (a_t) */
    void** observations; /* observations (o_t) */
    size_t length;
    
    /* Goal/intent */
    atom_handle_t goal;
    
    /* Outcome */
    bool success;
    double reward;
    
    /* Timing */
    uint64_t start_time;
    uint64_t end_time;
} niche_trace_t;

/**
 * Skill distribution - conditional generative model over trajectories
 */
typedef struct niche_skill {
    uint64_t id;
    char* name;
    
    /* Generator model */
    void* generator_model;
    
    /* Evaluator/critic */
    void* evaluator_model;
    
    /* Glyph codec */
    niche_glyph_t* canonical_glyph;
    
    /* Intent schema */
    atom_handle_t intent_type;
    
    /* Performance metrics */
    double avg_success_rate;
    uint32_t execution_count;
    double avg_energy;
    
    /* Context constraints */
    char** context_tags;
    size_t context_count;
} niche_skill_t;

/**
 * Opponent processing mode
 */
typedef enum {
    NICHE_MODE_PROPOSE,      /* Generative proposal (creative/objective) */
    NICHE_MODE_NORMALIZE,    /* Critic/constraint (subjective) */
    NICHE_MODE_REFINE,       /* Iterative refinement */
    NICHE_MODE_COMMIT        /* Finalize and store */
} niche_processing_mode_t;

/**
 * Niche action type - modifications to the environment
 */
typedef enum {
    NICHE_ACTION_CREATE_TOOL,      /* Create new tool/utility */
    NICHE_ACTION_MODIFY_PROMPT,    /* Adjust prompt template */
    NICHE_ACTION_CACHE_RESULT,     /* Cache computation result */
    NICHE_ACTION_CREATE_SCAFFOLD,  /* Create structural scaffold */
    NICHE_ACTION_DEFINE_MACRO,     /* Define reusable macro */
    NICHE_ACTION_UPDATE_MEMORY,    /* Update external memory */
    NICHE_ACTION_ANNOTATE_CODE,    /* Add code annotation */
    NICHE_ACTION_CREATE_ALIAS      /* Create command alias */
} niche_action_type_t;

/**
 * Niche action - concrete environment modification
 */
typedef struct niche_action {
    uint64_t id;
    niche_action_type_t type;
    
    /* Action parameters */
    char* target_path;
    void* action_data;
    size_t data_size;
    
    /* Justification */
    char* rationale;
    double expected_benefit;
    
    /* Provenance */
    atom_handle_t source_skill;
    uint64_t timestamp;
} niche_action_t;

/*===========================================================================
 * Niche Construction Engine
 *===========================================================================*/

typedef struct niche_engine* niche_engine_t;

/**
 * Configuration for niche construction engine
 */
typedef struct {
    /* AtomSpace for skill storage */
    atomspace_t atomspace;
    
    /* Processing parameters */
    uint32_t max_proposal_iterations;
    uint32_t max_refinement_iterations;
    double proposal_temperature;
    double normalization_threshold;
    
    /* Resource limits */
    size_t max_skills;
    size_t max_glyphs;
    size_t max_traces;
    
    /* Learning parameters */
    double learning_rate;
    double exploration_rate;
    double success_threshold;
    
    /* Storage paths */
    const char* glyph_cache_dir;
    const char* skill_library_path;
    const char* trace_archive_path;
    
    /* Enable features */
    bool enable_diffusion_generation;
    bool enable_opponent_processing;
    bool enable_active_inference;
} niche_config_t;

/**
 * Initialize niche construction engine
 */
COGWXPOS_API cog_result_t niche_engine_init(
    const niche_config_t* config,
    niche_engine_t* engine
);

/**
 * Shutdown niche construction engine
 */
COGWXPOS_API void niche_engine_shutdown(niche_engine_t engine);

/*===========================================================================
 * Skill Development
 *===========================================================================*/

/**
 * Propose a new technique via generative model
 */
COGWXPOS_API cog_result_t niche_propose_technique(
    niche_engine_t engine,
    atom_handle_t intent,
    void* context,
    size_t context_size,
    niche_glyph_t** proposals,
    size_t* proposal_count
);

/**
 * Normalize/evaluate proposals via critic
 */
COGWXPOS_API cog_result_t niche_normalize_proposals(
    niche_engine_t engine,
    niche_glyph_t** proposals,
    size_t proposal_count,
    double* scores
);

/**
 * Refine a technique based on feedback
 */
COGWXPOS_API cog_result_t niche_refine_technique(
    niche_engine_t engine,
    niche_glyph_t* glyph,
    niche_trace_t* execution_trace,
    niche_glyph_t** refined
);

/**
 * Commit a technique as a learned skill
 */
COGWXPOS_API cog_result_t niche_commit_skill(
    niche_engine_t engine,
    niche_glyph_t* glyph,
    const char* skill_name,
    niche_skill_t** skill
);

/*===========================================================================
 * Opponent Processing Cycles
 *===========================================================================*/

/**
 * Execute one opponent processing cycle
 * 
 * propose → score/correct → refine → (optional) commit
 */
COGWXPOS_API cog_result_t niche_opponent_cycle(
    niche_engine_t engine,
    atom_handle_t goal,
    void* context,
    size_t context_size,
    niche_skill_t** result_skill
);

/**
 * Run multiple opponent cycles until convergence
 */
COGWXPOS_API cog_result_t niche_train_skill(
    niche_engine_t engine,
    atom_handle_t goal,
    niche_trace_t** training_traces,
    size_t trace_count,
    uint32_t max_cycles,
    niche_skill_t** trained_skill
);

/*===========================================================================
 * Execution and Tracing
 *===========================================================================*/

/**
 * Execute a skill and record trace
 */
COGWXPOS_API cog_result_t niche_execute_skill(
    niche_engine_t engine,
    niche_skill_t* skill,
    void* inputs,
    size_t input_size,
    void** outputs,
    size_t* output_size,
    niche_trace_t** trace
);

/**
 * Decode glyph to execution trace
 */
COGWXPOS_API cog_result_t niche_glyph_to_trace(
    niche_engine_t engine,
    niche_glyph_t* glyph,
    niche_trace_t** trace
);

/**
 * Encode execution trace to glyph
 */
COGWXPOS_API cog_result_t niche_trace_to_glyph(
    niche_engine_t engine,
    niche_trace_t* trace,
    niche_glyph_t** glyph
);

/*===========================================================================
 * Environment Shaping (Niche Actions)
 *===========================================================================*/

/**
 * Propose niche action to modify environment
 */
COGWXPOS_API cog_result_t niche_propose_action(
    niche_engine_t engine,
    atom_handle_t goal,
    niche_action_type_t action_type,
    niche_action_t** action
);

/**
 * Execute niche action (modify environment)
 */
COGWXPOS_API cog_result_t niche_execute_action(
    niche_engine_t engine,
    niche_action_t* action
);

/**
 * Evaluate impact of niche action
 */
COGWXPOS_API cog_result_t niche_evaluate_action(
    niche_engine_t engine,
    niche_action_t* action,
    double* impact_score
);

/**
 * Rollback niche action (undo environment change)
 */
COGWXPOS_API cog_result_t niche_rollback_action(
    niche_engine_t engine,
    niche_action_t* action
);

/*===========================================================================
 * Skill Library Management
 *===========================================================================*/

/**
 * Query skills by intent
 */
COGWXPOS_API cog_result_t niche_query_skills(
    niche_engine_t engine,
    atom_handle_t intent_pattern,
    niche_skill_t** skills,
    size_t* skill_count
);

/**
 * Retrieve skill by name
 */
COGWXPOS_API cog_result_t niche_get_skill(
    niche_engine_t engine,
    const char* name,
    niche_skill_t** skill
);

/**
 * Update skill statistics after execution
 */
COGWXPOS_API cog_result_t niche_update_skill_stats(
    niche_engine_t engine,
    niche_skill_t* skill,
    bool success,
    double reward
);

/**
 * Compose multiple skills
 */
COGWXPOS_API cog_result_t niche_compose_skills(
    niche_engine_t engine,
    niche_skill_t** skills,
    size_t skill_count,
    const char* composite_name,
    niche_skill_t** composite_skill
);

/*===========================================================================
 * Active Inference Integration
 *===========================================================================*/

/**
 * Compute expected free energy for a technique
 */
COGWXPOS_API cog_result_t niche_compute_free_energy(
    niche_engine_t engine,
    niche_glyph_t* glyph,
    atom_handle_t goal,
    double* free_energy
);

/**
 * Select action via active inference (minimize expected free energy)
 */
COGWXPOS_API cog_result_t niche_active_inference_select(
    niche_engine_t engine,
    atom_handle_t goal,
    void** candidate_actions,
    size_t action_count,
    void** selected_action
);

/*===========================================================================
 * Visualization and Debugging
 *===========================================================================*/

/**
 * Export glyph as image/diagram
 */
COGWXPOS_API cog_result_t niche_export_glyph(
    niche_engine_t engine,
    niche_glyph_t* glyph,
    const char* output_path,
    const char* format  /* "png", "svg", "json" */
);

/**
 * Visualize skill library
 */
COGWXPOS_API cog_result_t niche_visualize_library(
    niche_engine_t engine,
    const char* output_path
);

/**
 * Dump engine statistics
 */
COGWXPOS_API cog_result_t niche_get_stats(
    niche_engine_t engine,
    char** stats_json,
    size_t* json_size
);

/*===========================================================================
 * Persistence
 *===========================================================================*/

/**
 * Save skill library to disk
 */
COGWXPOS_API cog_result_t niche_save_library(
    niche_engine_t engine,
    const char* path
);

/**
 * Load skill library from disk
 */
COGWXPOS_API cog_result_t niche_load_library(
    niche_engine_t engine,
    const char* path
);

#ifdef __cplusplus
}
#endif

#endif /* _COGWXP_NICHE_CONSTRUCTION_H_ */
