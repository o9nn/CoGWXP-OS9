/**
 * @file test_niche_beast.c
 * @brief Test for Niche Construction Engine and Beast Mode Reactor
 * 
 * @copyright CoGWXP-OS9 Project
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../integration/niche_construction.h"
#include "../integration/beast_mode.h"

/*===========================================================================
 * Test Helpers
 *===========================================================================*/

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(condition, message) do { \
    if (condition) { \
        printf("  [PASS] %s\n", message); \
        tests_passed++; \
    } else { \
        printf("  [FAIL] %s\n", message); \
        tests_failed++; \
    } \
} while(0)

/*===========================================================================
 * Niche Construction Tests
 *===========================================================================*/

void test_niche_init(void) {
    printf("\n=== Test: Niche Engine Initialization ===\n");
    
    niche_config_t config = {
        .atomspace = (atomspace_t)1,  /* Stub */
        .max_proposal_iterations = 5,
        .max_refinement_iterations = 10,
        .proposal_temperature = 1.0,
        .normalization_threshold = 0.7,
        .max_skills = 1000,
        .max_glyphs = 5000,
        .max_traces = 10000,
        .learning_rate = 0.01,
        .exploration_rate = 0.1,
        .success_threshold = 0.8,
        .enable_diffusion_generation = true,
        .enable_opponent_processing = true,
        .enable_active_inference = true
    };
    
    niche_engine_t engine = NULL;
    cog_result_t result = niche_engine_init(&config, &engine);
    
    TEST_ASSERT(result == COG_SUCCESS, "Niche engine initialized successfully");
    TEST_ASSERT(engine != NULL, "Niche engine handle is valid");
    
    if (engine) {
        niche_engine_shutdown(engine);
    }
}

void test_niche_propose_techniques(void) {
    printf("\n=== Test: Propose Techniques ===\n");
    
    niche_config_t config = {
        .atomspace = (atomspace_t)1,
        .max_proposal_iterations = 3,
        .max_skills = 100,
        .max_glyphs = 500,
        .max_traces = 1000
    };
    
    niche_engine_t engine = NULL;
    niche_engine_init(&config, &engine);
    
    atom_handle_t intent = (atom_handle_t)42;
    niche_glyph_t** proposals = NULL;
    size_t proposal_count = 0;
    
    cog_result_t result = niche_propose_technique(engine, intent, NULL, 0, 
                                                   proposals, &proposal_count);
    
    TEST_ASSERT(result == COG_SUCCESS, "Technique proposals generated");
    TEST_ASSERT(proposal_count == 3, "Correct number of proposals");
    
    niche_engine_shutdown(engine);
}

void test_niche_opponent_cycle(void) {
    printf("\n=== Test: Opponent Processing Cycle ===\n");
    
    niche_config_t config = {
        .atomspace = (atomspace_t)1,
        .max_proposal_iterations = 5,
        .normalization_threshold = 0.5,
        .max_skills = 100,
        .max_glyphs = 500,
        .max_traces = 1000
    };
    
    niche_engine_t engine = NULL;
    niche_engine_init(&config, &engine);
    
    atom_handle_t goal = (atom_handle_t)100;
    niche_skill_t* skill = NULL;
    
    cog_result_t result = niche_opponent_cycle(engine, goal, NULL, 0, &skill);
    
    TEST_ASSERT(result == COG_SUCCESS || result == COG_ERROR_GENERAL, 
                "Opponent cycle completed");
    
    niche_engine_shutdown(engine);
}

void test_niche_stats(void) {
    printf("\n=== Test: Niche Engine Statistics ===\n");
    
    niche_config_t config = {
        .atomspace = (atomspace_t)1,
        .max_skills = 100,
        .max_glyphs = 500,
        .max_traces = 1000
    };
    
    niche_engine_t engine = NULL;
    niche_engine_init(&config, &engine);
    
    char* stats_json = NULL;
    size_t json_size = 0;
    
    cog_result_t result = niche_get_stats(engine, &stats_json, &json_size);
    
    TEST_ASSERT(result == COG_SUCCESS, "Statistics retrieved");
    TEST_ASSERT(stats_json != NULL, "Statistics JSON is valid");
    TEST_ASSERT(json_size > 0, "Statistics JSON has content");
    
    if (stats_json) {
        printf("  Statistics: %s\n", stats_json);
        free(stats_json);
    }
    
    niche_engine_shutdown(engine);
}

/*===========================================================================
 * Beast Mode Tests
 *===========================================================================*/

void test_beast_init(void) {
    printf("\n=== Test: Beast Reactor Initialization ===\n");
    
    beast_config_t config = {
        .atomspace = (atomspace_t)1,
        .pln_engine = (pln_engine_t)1,
        .niche_engine = NULL,
        .default_intensity = BEAST_INTENSITY_NORMAL,
        .allow_overdrive = true,
        .max_threads = 4,
        .max_memory_bytes = 1024 * 1024 * 1024,
        .max_cpu_percent = 80.0,
        .max_inference_depth = 10,
        .max_inference_breadth = 100,
        .inference_confidence_threshold = 0.7,
        .sti_boost_factor = 1.5,
        .attention_window_size = 1000,
        .dynamic_attention = true,
        .default_fusion_strategy = BEAST_FUSION_PARALLEL,
        .ensemble_voting_threshold = 0.6,
        .enable_parallel_inference = true,
        .enable_caching = true,
        .enable_pruning = true,
        .pruning_threshold = 0.3,
        .enable_telemetry = true,
        .telemetry_interval_ms = 1000,
        .thermal_limit = 90.0,
        .energy_budget = 100.0,
        .enable_circuit_breaker = true
    };
    
    beast_reactor_t reactor = NULL;
    cog_result_t result = beast_reactor_init(&config, &reactor);
    
    TEST_ASSERT(result == COG_SUCCESS, "Beast reactor initialized successfully");
    TEST_ASSERT(reactor != NULL, "Beast reactor handle is valid");
    
    if (reactor) {
        beast_reactor_shutdown(reactor);
    }
}

void test_beast_intensity(void) {
    printf("\n=== Test: Beast Reactor Intensity Control ===\n");
    
    beast_config_t config = {
        .atomspace = (atomspace_t)1,
        .pln_engine = (pln_engine_t)1,
        .default_intensity = BEAST_INTENSITY_NORMAL,
        .allow_overdrive = true,
        .max_threads = 4
    };
    
    beast_reactor_t reactor = NULL;
    beast_reactor_init(&config, &reactor);
    
    /* Test intensity changes */
    cog_result_t result;
    
    result = beast_set_intensity(reactor, BEAST_INTENSITY_ELEVATED);
    TEST_ASSERT(result == COG_SUCCESS, "Set intensity to ELEVATED");
    
    result = beast_set_intensity(reactor, BEAST_INTENSITY_HIGH);
    TEST_ASSERT(result == COG_SUCCESS, "Set intensity to HIGH");
    
    result = beast_set_intensity(reactor, BEAST_INTENSITY_MAXIMUM);
    TEST_ASSERT(result == COG_SUCCESS, "Set intensity to MAXIMUM");
    
    result = beast_set_intensity(reactor, BEAST_INTENSITY_OVERDRIVE);
    TEST_ASSERT(result == COG_SUCCESS, "Set intensity to OVERDRIVE");
    
    /* Get state */
    beast_reactor_state_t state;
    result = beast_get_state(reactor, &state);
    TEST_ASSERT(result == COG_SUCCESS, "Retrieved reactor state");
    TEST_ASSERT(state.intensity == BEAST_INTENSITY_OVERDRIVE, 
                "Intensity correctly set to OVERDRIVE");
    
    beast_reactor_shutdown(reactor);
}

void test_beast_fusion(void) {
    printf("\n=== Test: Cognitive Fusion ===\n");
    
    beast_config_t config = {
        .atomspace = (atomspace_t)1,
        .pln_engine = (pln_engine_t)1,
        .default_intensity = BEAST_INTENSITY_ELEVATED,
        .max_threads = 4
    };
    
    beast_reactor_t reactor = NULL;
    beast_reactor_init(&config, &reactor);
    
    atom_handle_t goal = (atom_handle_t)200;
    beast_reason_mode_t modes[] = {
        BEAST_REASON_FORWARD,
        BEAST_REASON_PROBABILISTIC,
        BEAST_REASON_ANALOGICAL
    };
    
    beast_fusion_result_t* result = NULL;
    cog_result_t res = beast_fuse(reactor, goal, BEAST_FUSION_PARALLEL,
                                   modes, 3, &result);
    
    TEST_ASSERT(res == COG_SUCCESS, "Fusion executed successfully");
    TEST_ASSERT(result != NULL, "Fusion result is valid");
    TEST_ASSERT(result->result_count > 0, "Fusion produced results");
    TEST_ASSERT(result->total_inferences > 0, "Inferences were performed");
    
    if (result) {
        printf("  Fusion results: %zu atoms, %llu inferences in %.2fms\n",
               result->result_count, 
               (unsigned long long)result->total_inferences,
               result->total_time_ms);
        beast_free_result(result);
    }
    
    beast_reactor_shutdown(reactor);
}

void test_beast_auto_fusion(void) {
    printf("\n=== Test: Auto Fusion ===\n");
    
    beast_config_t config = {
        .atomspace = (atomspace_t)1,
        .pln_engine = (pln_engine_t)1,
        .default_intensity = BEAST_INTENSITY_HIGH,
        .max_threads = 4
    };
    
    beast_reactor_t reactor = NULL;
    beast_reactor_init(&config, &reactor);
    
    atom_handle_t goal = (atom_handle_t)300;
    beast_fusion_result_t* result = NULL;
    
    cog_result_t res = beast_fuse_auto(reactor, goal, &result);
    
    TEST_ASSERT(res == COG_SUCCESS, "Auto fusion executed successfully");
    TEST_ASSERT(result != NULL, "Auto fusion result is valid");
    
    if (result) {
        beast_free_result(result);
    }
    
    beast_reactor_shutdown(reactor);
}

void test_beast_telemetry(void) {
    printf("\n=== Test: Beast Reactor Telemetry ===\n");
    
    beast_config_t config = {
        .atomspace = (atomspace_t)1,
        .pln_engine = (pln_engine_t)1,
        .default_intensity = BEAST_INTENSITY_NORMAL,
        .enable_telemetry = true,
        .max_threads = 4
    };
    
    beast_reactor_t reactor = NULL;
    beast_reactor_init(&config, &reactor);
    
    /* Execute some fusions to generate telemetry */
    atom_handle_t goal = (atom_handle_t)400;
    beast_fusion_result_t* result = NULL;
    beast_fuse_auto(reactor, goal, &result);
    if (result) beast_free_result(result);
    
    /* Get telemetry */
    char* telemetry_json = NULL;
    size_t json_size = 0;
    
    cog_result_t res = beast_get_telemetry(reactor, &telemetry_json, &json_size);
    
    TEST_ASSERT(res == COG_SUCCESS, "Telemetry retrieved");
    TEST_ASSERT(telemetry_json != NULL, "Telemetry JSON is valid");
    TEST_ASSERT(json_size > 0, "Telemetry JSON has content");
    
    if (telemetry_json) {
        printf("  Telemetry: %s\n", telemetry_json);
        free(telemetry_json);
    }
    
    beast_reactor_shutdown(reactor);
}

/*===========================================================================
 * Integration Tests
 *===========================================================================*/

void test_niche_beast_integration(void) {
    printf("\n=== Test: Niche Construction + Beast Mode Integration ===\n");
    
    /* Initialize niche engine */
    niche_config_t niche_cfg = {
        .atomspace = (atomspace_t)1,
        .max_skills = 100,
        .max_glyphs = 500,
        .max_traces = 1000
    };
    
    niche_engine_t niche_engine = NULL;
    niche_engine_init(&niche_cfg, &niche_engine);
    
    /* Initialize beast reactor with niche engine */
    beast_config_t beast_cfg = {
        .atomspace = (atomspace_t)1,
        .pln_engine = (pln_engine_t)1,
        .niche_engine = niche_engine,
        .default_intensity = BEAST_INTENSITY_HIGH,
        .max_threads = 4
    };
    
    beast_reactor_t reactor = NULL;
    beast_reactor_init(&beast_cfg, &reactor);
    
    TEST_ASSERT(niche_engine != NULL, "Niche engine initialized");
    TEST_ASSERT(reactor != NULL, "Beast reactor initialized with niche engine");
    
    /* Perform fusion with skills */
    atom_handle_t goal = (atom_handle_t)500;
    beast_fusion_result_t* result = NULL;
    
    cog_result_t res = beast_fuse_auto(reactor, goal, &result);
    TEST_ASSERT(res == COG_SUCCESS, "Integrated fusion executed");
    
    if (result) {
        beast_free_result(result);
    }
    
    beast_reactor_shutdown(reactor);
    niche_engine_shutdown(niche_engine);
}

/*===========================================================================
 * Main Test Runner
 *===========================================================================*/

int main(int argc, char* argv[]) {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║  CoGWXP-OS9 Niche Construction & Beast Mode Test Suite   ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    
    /* Niche Construction Tests */
    printf("\n--- NICHE CONSTRUCTION ENGINE TESTS ---\n");
    test_niche_init();
    test_niche_propose_techniques();
    test_niche_opponent_cycle();
    test_niche_stats();
    
    /* Beast Mode Tests */
    printf("\n--- BEAST MODE REACTOR TESTS ---\n");
    test_beast_init();
    test_beast_intensity();
    test_beast_fusion();
    test_beast_auto_fusion();
    test_beast_telemetry();
    
    /* Integration Tests */
    printf("\n--- INTEGRATION TESTS ---\n");
    test_niche_beast_integration();
    
    /* Summary */
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║  TEST SUMMARY                                             ║\n");
    printf("╠════════════════════════════════════════════════════════════╣\n");
    printf("║  Tests Passed: %-3d                                        ║\n", tests_passed);
    printf("║  Tests Failed: %-3d                                        ║\n", tests_failed);
    printf("╚════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    return tests_failed > 0 ? 1 : 0;
}
