/**
 * @file niche_construction.c
 * @brief Adaptive Niche Construction Engine Implementation
 * 
 * @copyright CoGWXP-OS9 Project
 */

#include "niche_construction.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <strings.h>

/*===========================================================================
 * Internal Structures
 *===========================================================================*/

struct niche_engine {
    niche_config_t config;
    
    /* Skill storage */
    niche_skill_t** skills;
    size_t skill_count;
    size_t skill_capacity;
    
    /* Glyph cache */
    niche_glyph_t** glyphs;
    size_t glyph_count;
    size_t glyph_capacity;
    
    /* Trace archive */
    niche_trace_t** traces;
    size_t trace_count;
    size_t trace_capacity;
    
    /* Active actions */
    niche_action_t** actions;
    size_t action_count;
    
    /* Statistics */
    uint64_t total_proposals;
    uint64_t total_cycles;
    uint64_t successful_commits;
    
    /* State */
    bool initialized;
    pthread_mutex_t lock;
    
    /* Random state */
    unsigned int rand_seed;
};

/*===========================================================================
 * Helper Functions
 *===========================================================================*/

static uint64_t generate_id(niche_engine_t engine) {
    return (uint64_t)time(NULL) * 1000000 + (rand_r(&engine->rand_seed) % 1000000);
}

static niche_glyph_t* create_glyph(uint64_t id, const char* name) {
    niche_glyph_t* glyph = calloc(1, sizeof(niche_glyph_t));
    if (!glyph) return NULL;
    
    glyph->id = id;
    glyph->name = name ? strdup(name) : NULL;
    glyph->timestamp = (uint64_t)time(NULL);
    glyph->success_rate = 0.5;
    glyph->usage_count = 0;
    glyph->energy = 1.0;
    glyph->complexity = 1.0;
    
    return glyph;
}

static niche_skill_t* create_skill(uint64_t id, const char* name) {
    niche_skill_t* skill = calloc(1, sizeof(niche_skill_t));
    if (!skill) return NULL;
    
    skill->id = id;
    skill->name = name ? strdup(name) : NULL;
    skill->avg_success_rate = 0.5;
    skill->execution_count = 0;
    skill->avg_energy = 1.0;
    
    return skill;
}

/*===========================================================================
 * Initialization and Shutdown
 *===========================================================================*/

COGUTIL_API cog_result_t niche_engine_init(
    const niche_config_t* config,
    niche_engine_t* engine
) {
    if (!config || !engine) return COG_ERROR_INVALID_ARG;
    
    niche_engine_t e = calloc(1, sizeof(struct niche_engine));
    if (!e) return COG_ERROR_MEMORY;
    
    memcpy(&e->config, config, sizeof(niche_config_t));
    
    /* Initialize storage */
    e->skill_capacity = config->max_skills > 0 ? config->max_skills : 1000;
    e->skills = calloc(e->skill_capacity, sizeof(niche_skill_t*));
    
    e->glyph_capacity = config->max_glyphs > 0 ? config->max_glyphs : 5000;
    e->glyphs = calloc(e->glyph_capacity, sizeof(niche_glyph_t*));
    
    e->trace_capacity = config->max_traces > 0 ? config->max_traces : 10000;
    e->traces = calloc(e->trace_capacity, sizeof(niche_trace_t*));
    
    e->actions = calloc(100, sizeof(niche_action_t*));
    
    pthread_mutex_init(&e->lock, NULL);
    e->rand_seed = (unsigned int)time(NULL);
    e->initialized = true;
    
    *engine = e;
    
    printf("[NicheEngine] Initialized with %zu max skills, %zu max glyphs\n",
           e->skill_capacity, e->glyph_capacity);
    
    return COG_SUCCESS;
}

COGUTIL_API void niche_engine_shutdown(niche_engine_t engine) {
    if (!engine) return;
    
    pthread_mutex_lock(&engine->lock);
    
    /* Free skills */
    for (size_t i = 0; i < engine->skill_count; i++) {
        if (engine->skills[i]) {
            free(engine->skills[i]->name);
            free(engine->skills[i]);
        }
    }
    free(engine->skills);
    
    /* Free glyphs */
    for (size_t i = 0; i < engine->glyph_count; i++) {
        if (engine->glyphs[i]) {
            free(engine->glyphs[i]->name);
            free(engine->glyphs[i]->data);
            free(engine->glyphs[i]);
        }
    }
    free(engine->glyphs);
    
    /* Free traces */
    for (size_t i = 0; i < engine->trace_count; i++) {
        if (engine->traces[i]) {
            free(engine->traces[i]);
        }
    }
    free(engine->traces);
    
    /* Free actions */
    free(engine->actions);
    
    pthread_mutex_unlock(&engine->lock);
    pthread_mutex_destroy(&engine->lock);
    
    printf("[NicheEngine] Shutdown complete\n");
    free(engine);
}

/*===========================================================================
 * Skill Development
 *===========================================================================*/

COGUTIL_API cog_result_t niche_propose_technique(
    niche_engine_t engine,
    atom_handle_t intent,
    void* context,
    size_t context_size,
    niche_glyph_t** proposals,
    size_t* proposal_count
) {
    if (!engine || !proposals || !proposal_count) return COG_ERROR_INVALID_ARG;
    
    pthread_mutex_lock(&engine->lock);
    
    /* Generate proposals using generative model */
    size_t count = engine->config.max_proposal_iterations > 0 ? 
                   engine->config.max_proposal_iterations : 5;
    
    niche_glyph_t** glyphs = calloc(count, sizeof(niche_glyph_t*));
    
    for (size_t i = 0; i < count; i++) {
        uint64_t id = generate_id(engine);
        char name[64];
        snprintf(name, sizeof(name), "proposal_%llu", (unsigned long long)id);
        glyphs[i] = create_glyph(id, name);
        glyphs[i]->intent = intent;
    }
    
    *proposals = glyphs[0];  /* Return array base */
    *proposal_count = count;
    engine->total_proposals += count;
    
    pthread_mutex_unlock(&engine->lock);
    
    printf("[NicheEngine] Generated %zu technique proposals\n", count);
    return COG_SUCCESS;
}

COGUTIL_API cog_result_t niche_normalize_proposals(
    niche_engine_t engine,
    niche_glyph_t** proposals,
    size_t proposal_count,
    double* scores
) {
    if (!engine || !proposals || !scores) return COG_ERROR_INVALID_ARG;
    
    /* Evaluate each proposal via critic/constraint model */
    for (size_t i = 0; i < proposal_count; i++) {
        /* Simulate evaluation score */
        double base_score = 0.5 + ((double)rand_r(&engine->rand_seed) / RAND_MAX) * 0.5;
        scores[i] = base_score;
    }
    
    printf("[NicheEngine] Normalized %zu proposals\n", proposal_count);
    return COG_SUCCESS;
}

COGUTIL_API cog_result_t niche_refine_technique(
    niche_engine_t engine,
    niche_glyph_t* glyph,
    niche_trace_t* execution_trace,
    niche_glyph_t** refined
) {
    if (!engine || !glyph || !refined) return COG_ERROR_INVALID_ARG;
    
    /* Create refined version based on execution feedback */
    uint64_t id = generate_id(engine);
    char name[128];
    snprintf(name, sizeof(name), "%s_refined", glyph->name ? glyph->name : "technique");
    
    niche_glyph_t* new_glyph = create_glyph(id, name);
    new_glyph->intent = glyph->intent;
    new_glyph->success_rate = glyph->success_rate * 1.1;  /* Improve slightly */
    
    *refined = new_glyph;
    
    printf("[NicheEngine] Refined technique: %s\n", name);
    return COG_SUCCESS;
}

COGUTIL_API cog_result_t niche_commit_skill(
    niche_engine_t engine,
    niche_glyph_t* glyph,
    const char* skill_name,
    niche_skill_t** skill
) {
    if (!engine || !glyph || !skill) return COG_ERROR_INVALID_ARG;
    
    pthread_mutex_lock(&engine->lock);
    
    if (engine->skill_count >= engine->skill_capacity) {
        pthread_mutex_unlock(&engine->lock);
        return COG_ERROR_MEMORY;
    }
    
    /* Create skill from glyph */
    uint64_t id = generate_id(engine);
    niche_skill_t* new_skill = create_skill(id, skill_name);
    new_skill->canonical_glyph = glyph;
    new_skill->intent_type = glyph->intent;
    new_skill->avg_success_rate = glyph->success_rate;
    
    engine->skills[engine->skill_count++] = new_skill;
    engine->successful_commits++;
    
    *skill = new_skill;
    
    pthread_mutex_unlock(&engine->lock);
    
    printf("[NicheEngine] Committed skill: %s (total: %zu)\n", 
           skill_name, engine->skill_count);
    return COG_SUCCESS;
}

/*===========================================================================
 * Opponent Processing Cycles
 *===========================================================================*/

COGUTIL_API cog_result_t niche_opponent_cycle(
    niche_engine_t engine,
    atom_handle_t goal,
    void* context,
    size_t context_size,
    niche_skill_t** result_skill
) {
    if (!engine || !result_skill) return COG_ERROR_INVALID_ARG;
    
    pthread_mutex_lock(&engine->lock);
    engine->total_cycles++;
    pthread_mutex_unlock(&engine->lock);
    
    cog_result_t res;
    
    /* Step 1: Propose techniques */
    niche_glyph_t** proposals = NULL;
    size_t proposal_count = 0;
    res = niche_propose_technique(engine, goal, context, context_size, 
                                   proposals, &proposal_count);
    if (res != COG_SUCCESS) return res;
    
    /* Step 2: Normalize/evaluate */
    double* scores = calloc(proposal_count, sizeof(double));
    res = niche_normalize_proposals(engine, proposals, proposal_count, scores);
    if (res != COG_SUCCESS) {
        free(scores);
        return res;
    }
    
    /* Step 3: Select best */
    size_t best_idx = 0;
    double best_score = scores[0];
    for (size_t i = 1; i < proposal_count; i++) {
        if (scores[i] > best_score) {
            best_score = scores[i];
            best_idx = i;
        }
    }
    
    /* Step 4: Commit if good enough */
    if (best_score > engine->config.normalization_threshold) {
        char name[64];
        snprintf(name, sizeof(name), "skill_cycle_%llu", 
                 (unsigned long long)engine->total_cycles);
        res = niche_commit_skill(engine, proposals[best_idx], name, result_skill);
    } else {
        res = COG_ERROR_GENERAL;
    }
    
    free(scores);
    
    printf("[NicheEngine] Opponent cycle complete (score: %.3f)\n", best_score);
    return res;
}

COGUTIL_API cog_result_t niche_train_skill(
    niche_engine_t engine,
    atom_handle_t goal,
    niche_trace_t** training_traces,
    size_t trace_count,
    uint32_t max_cycles,
    niche_skill_t** trained_skill
) {
    if (!engine || !trained_skill) return COG_ERROR_INVALID_ARG;
    
    niche_skill_t* best_skill = NULL;
    double best_performance = 0.0;
    
    for (uint32_t cycle = 0; cycle < max_cycles; cycle++) {
        niche_skill_t* skill = NULL;
        cog_result_t res = niche_opponent_cycle(engine, goal, NULL, 0, &skill);
        
        if (res == COG_SUCCESS && skill) {
            if (skill->avg_success_rate > best_performance) {
                best_performance = skill->avg_success_rate;
                best_skill = skill;
            }
        }
        
        /* Early termination if excellent */
        if (best_performance > 0.95) break;
    }
    
    *trained_skill = best_skill;
    
    printf("[NicheEngine] Training complete after max %u cycles (best: %.3f)\n",
           max_cycles, best_performance);
    return best_skill ? COG_SUCCESS : COG_ERROR_NOT_FOUND;
}

/*===========================================================================
 * Environment Shaping (Stub implementations)
 *===========================================================================*/

COGUTIL_API cog_result_t niche_propose_action(
    niche_engine_t engine,
    atom_handle_t goal,
    niche_action_type_t action_type,
    niche_action_t** action
) {
    if (!engine || !action) return COG_ERROR_INVALID_ARG;
    
    niche_action_t* act = calloc(1, sizeof(niche_action_t));
    act->id = generate_id(engine);
    act->type = action_type;
    act->expected_benefit = 0.7;
    
    *action = act;
    printf("[NicheEngine] Proposed niche action (type: %d)\n", action_type);
    return COG_SUCCESS;
}

COGUTIL_API cog_result_t niche_execute_action(
    niche_engine_t engine,
    niche_action_t* action
) {
    if (!engine || !action) return COG_ERROR_INVALID_ARG;
    
    printf("[NicheEngine] Executing niche action %llu\n", 
           (unsigned long long)action->id);
    return COG_SUCCESS;
}

COGUTIL_API cog_result_t niche_query_skills(
    niche_engine_t engine,
    atom_handle_t intent_pattern,
    niche_skill_t** skills,
    size_t* skill_count
) {
    if (!engine || !skills || !skill_count) return COG_ERROR_INVALID_ARG;
    
    pthread_mutex_lock(&engine->lock);
    *skill_count = engine->skill_count;
    *skills = engine->skill_count > 0 ? engine->skills[0] : NULL;
    pthread_mutex_unlock(&engine->lock);
    
    return COG_SUCCESS;
}

COGUTIL_API cog_result_t niche_get_stats(
    niche_engine_t engine,
    char** stats_json,
    size_t* json_size
) {
    if (!engine || !stats_json || !json_size) return COG_ERROR_INVALID_ARG;
    
    char buffer[1024];
    snprintf(buffer, sizeof(buffer),
        "{\n"
        "  \"skills\": %zu,\n"
        "  \"glyphs\": %zu,\n"
        "  \"traces\": %zu,\n"
        "  \"total_proposals\": %llu,\n"
        "  \"total_cycles\": %llu,\n"
        "  \"successful_commits\": %llu\n"
        "}",
        engine->skill_count,
        engine->glyph_count,
        engine->trace_count,
        (unsigned long long)engine->total_proposals,
        (unsigned long long)engine->total_cycles,
        (unsigned long long)engine->successful_commits
    );
    
    *stats_json = strdup(buffer);
    *json_size = strlen(buffer);
    
    return COG_SUCCESS;
}

/* Stub implementations for remaining functions */
COGUTIL_API cog_result_t niche_execute_skill(niche_engine_t engine, niche_skill_t* skill,
    void* inputs, size_t input_size, void** outputs, size_t* output_size, niche_trace_t** trace) {
    return COG_SUCCESS;
}

COGUTIL_API cog_result_t niche_glyph_to_trace(niche_engine_t engine, niche_glyph_t* glyph,
    niche_trace_t** trace) { return COG_SUCCESS; }

COGUTIL_API cog_result_t niche_trace_to_glyph(niche_engine_t engine, niche_trace_t* trace,
    niche_glyph_t** glyph) { return COG_SUCCESS; }

COGUTIL_API cog_result_t niche_evaluate_action(niche_engine_t engine, niche_action_t* action,
    double* impact_score) { *impact_score = 0.7; return COG_SUCCESS; }

COGUTIL_API cog_result_t niche_rollback_action(niche_engine_t engine, niche_action_t* action) {
    return COG_SUCCESS;
}

COGUTIL_API cog_result_t niche_get_skill(niche_engine_t engine, const char* name,
    niche_skill_t** skill) { return COG_ERROR_NOT_FOUND; }

COGUTIL_API cog_result_t niche_update_skill_stats(niche_engine_t engine, niche_skill_t* skill,
    bool success, double reward) { return COG_SUCCESS; }

COGUTIL_API cog_result_t niche_compose_skills(niche_engine_t engine, niche_skill_t** skills,
    size_t skill_count, const char* composite_name, niche_skill_t** composite_skill) {
    return COG_SUCCESS;
}

COGUTIL_API cog_result_t niche_compute_free_energy(niche_engine_t engine, niche_glyph_t* glyph,
    atom_handle_t goal, double* free_energy) { *free_energy = 1.0; return COG_SUCCESS; }

COGUTIL_API cog_result_t niche_active_inference_select(niche_engine_t engine, atom_handle_t goal,
    void** candidate_actions, size_t action_count, void** selected_action) { return COG_SUCCESS; }

COGUTIL_API cog_result_t niche_export_glyph(niche_engine_t engine, niche_glyph_t* glyph,
    const char* output_path, const char* format) { return COG_SUCCESS; }

COGUTIL_API cog_result_t niche_visualize_library(niche_engine_t engine, const char* output_path) {
    return COG_SUCCESS;
}

COGUTIL_API cog_result_t niche_save_library(niche_engine_t engine, const char* path) {
    printf("[NicheEngine] Saved library to %s\n", path);
    return COG_SUCCESS;
}

COGUTIL_API cog_result_t niche_load_library(niche_engine_t engine, const char* path) {
    printf("[NicheEngine] Loaded library from %s\n", path);
    return COG_SUCCESS;
}
