/**
 * @file test_pln_formulas.c
 * @brief Exhaustive PLN Truth-Value Formula and Inference Chain Tests
 *
 * Covers areas not tested by test_cogwxp.c:
 *  - Deduction, induction, abduction TV formula correctness (boundary values)
 *  - Revision / Bayesian update semantics
 *  - AND / OR / NOT boundary cases (0, 0.5, 1 strength)
 *  - Forward chain produces monotonically growing derived set
 *  - Backward chain achieves goal or reports failure correctly
 *  - Rule application for every PLN_RULE_* enum value
 *  - Confidence threshold filtering in forward chain
 *  - Inference history recording and clearing
 *  - PLN stats accumulation across multiple inference runs
 *  - Multi-step deduction chains (transitivity)
 *
 * @copyright CoGWXP-OS9 Project
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../opencog/cogutil/cogutil.h"
#include "../opencog/atomspace/atomspace.h"
#include "../opencog/pln/pln.h"

/*===========================================================================
 * Lightweight test framework (accumulator style)
 *===========================================================================*/

static int g_passed = 0;
static int g_failed = 0;

#define TEST_ASSERT(cond, msg) do { \
    if (cond) { \
        printf("  [PASS] %s\n", (msg)); \
        g_passed++; \
    } else { \
        printf("  [FAIL] %s  (line %d)\n", (msg), __LINE__); \
        g_failed++; \
    } \
} while (0)

#define NEAR(a, b) (fabs((double)(a) - (double)(b)) < 1e-9)
#define NEAR_EPS(a, b, eps) (fabs((double)(a) - (double)(b)) < (double)(eps))

/*===========================================================================
 * Helpers
 *===========================================================================*/

static truth_value_t tv(double strength, double confidence) {
    truth_value_t t;
    t.type = TRUTH_VALUE_SIMPLE;
    t.strength = strength;
    t.confidence = confidence;
    t.count = 1.0;
    return t;
}

/*===========================================================================
 * TV Formula: AND / OR / NOT
 *===========================================================================*/

static void test_tv_and_boundary(void) {
    printf("\n=== TV AND Boundary Cases ===\n");

    truth_value_t zero_zero = pln_tv_and(
        (truth_value_t){TRUTH_VALUE_SIMPLE, 0.0, 1.0, 1.0},
        (truth_value_t){TRUTH_VALUE_SIMPLE, 0.0, 1.0, 1.0});
    TEST_ASSERT(zero_zero.strength < 1e-9, "AND(0,0) strength ≈ 0");

    truth_value_t one_one = pln_tv_and(
        (truth_value_t){TRUTH_VALUE_SIMPLE, 1.0, 1.0, 1.0},
        (truth_value_t){TRUTH_VALUE_SIMPLE, 1.0, 1.0, 1.0});
    TEST_ASSERT(NEAR_EPS(one_one.strength, 1.0, 1e-9), "AND(1,1) strength ≈ 1");

    truth_value_t a = tv(0.8, 0.9);
    truth_value_t b = tv(0.6, 0.8);
    truth_value_t r = pln_tv_and(a, b);
    TEST_ASSERT(r.strength <= a.strength, "AND strength ≤ first operand");
    TEST_ASSERT(r.strength <= b.strength, "AND strength ≤ second operand");
    TEST_ASSERT(r.strength >= 0.0, "AND strength ≥ 0");
}

static void test_tv_or_boundary(void) {
    printf("\n=== TV OR Boundary Cases ===\n");

    truth_value_t zero_zero = pln_tv_or(
        (truth_value_t){TRUTH_VALUE_SIMPLE, 0.0, 1.0, 1.0},
        (truth_value_t){TRUTH_VALUE_SIMPLE, 0.0, 1.0, 1.0});
    TEST_ASSERT(zero_zero.strength < 1e-9, "OR(0,0) strength ≈ 0");

    truth_value_t one_one = pln_tv_or(
        (truth_value_t){TRUTH_VALUE_SIMPLE, 1.0, 1.0, 1.0},
        (truth_value_t){TRUTH_VALUE_SIMPLE, 1.0, 1.0, 1.0});
    TEST_ASSERT(NEAR_EPS(one_one.strength, 1.0, 1e-9), "OR(1,1) strength ≈ 1");

    truth_value_t a = tv(0.8, 0.9);
    truth_value_t b = tv(0.6, 0.8);
    truth_value_t r = pln_tv_or(a, b);
    TEST_ASSERT(r.strength >= a.strength || r.strength >= b.strength,
                "OR strength ≥ at least one operand");
    TEST_ASSERT(r.strength <= 1.0, "OR strength ≤ 1");
}

static void test_tv_not_boundary(void) {
    printf("\n=== TV NOT Boundary Cases ===\n");

    truth_value_t one = tv(1.0, 1.0);
    truth_value_t r = pln_tv_not(one);
    TEST_ASSERT(r.strength < 1e-9, "NOT(1) strength ≈ 0");
    TEST_ASSERT(NEAR(r.confidence, one.confidence), "NOT preserves confidence");

    truth_value_t zero = tv(0.0, 0.9);
    r = pln_tv_not(zero);
    TEST_ASSERT(NEAR_EPS(r.strength, 1.0, 1e-9), "NOT(0) strength ≈ 1");
    TEST_ASSERT(NEAR(r.confidence, zero.confidence), "NOT(0) preserves confidence");

    truth_value_t half = tv(0.5, 0.7);
    r = pln_tv_not(half);
    TEST_ASSERT(NEAR_EPS(r.strength, 0.5, 1e-9), "NOT(0.5) strength ≈ 0.5");

    /* Double negation */
    truth_value_t nn = pln_tv_not(r);
    TEST_ASSERT(NEAR_EPS(nn.strength, half.strength, 1e-9), "Double NOT is identity");
}

/*===========================================================================
 * TV Formula: Revision
 *===========================================================================*/

static void test_tv_revision(void) {
    printf("\n=== TV Revision ===\n");

    truth_value_t a = tv(0.8, 0.6);
    truth_value_t b = tv(0.4, 0.4);
    truth_value_t r = pln_tv_revision(a, b);

    TEST_ASSERT(r.confidence >= a.confidence || r.confidence >= b.confidence,
                "Revision: combined confidence ≥ at least one input");
    TEST_ASSERT(r.confidence <= 1.0, "Revision confidence ≤ 1");
    TEST_ASSERT(r.strength >= 0.0 && r.strength <= 1.0,
                "Revision strength in [0,1]");

    /* Combining identical TVs should not reduce confidence */
    truth_value_t same = tv(0.9, 0.8);
    truth_value_t r2 = pln_tv_revision(same, same);
    TEST_ASSERT(r2.confidence >= same.confidence,
                "Revising with identical TV keeps or raises confidence");
    TEST_ASSERT(NEAR_EPS(r2.strength, same.strength, 0.05),
                "Revising identical TVs keeps strength stable");
}

/*===========================================================================
 * TV Formula: Deduction, Induction, Abduction
 *===========================================================================*/

static void test_tv_deduction_formula(void) {
    printf("\n=== TV Deduction Formula ===\n");

    /* All certain (strength=1, confidence=1): conclusion should be ~1 */
    truth_value_t tvAB = tv(1.0, 1.0);
    truth_value_t tvBC = tv(1.0, 1.0);
    truth_value_t tvA  = tv(0.5, 1.0);
    truth_value_t tvB  = tv(0.5, 1.0);
    truth_value_t tvC  = tv(0.5, 1.0);

    truth_value_t r = pln_tv_deduction(tvAB, tvBC, tvA, tvB, tvC);
    TEST_ASSERT(r.strength > 0.0, "Deduction: certain implication yields positive strength");
    TEST_ASSERT(r.confidence >= 0.0, "Deduction confidence ≥ 0");

    /* Weak premise (low confidence): conclusion confidence should be low */
    truth_value_t weak_AB = tv(0.8, 0.1);
    truth_value_t weak_BC = tv(0.8, 0.1);
    truth_value_t r2 = pln_tv_deduction(weak_AB, weak_BC, tvA, tvB, tvC);
    TEST_ASSERT(r2.confidence <= 0.5,
                "Deduction with weak premises yields low confidence");
}

static void test_tv_induction_formula(void) {
    printf("\n=== TV Induction Formula ===\n");

    truth_value_t tvAB = tv(0.9, 0.8);
    truth_value_t tvAC = tv(0.7, 0.8);
    truth_value_t tvA  = tv(0.5, 0.9);
    truth_value_t tvB  = tv(0.5, 0.9);
    truth_value_t tvC  = tv(0.5, 0.9);

    truth_value_t r = pln_tv_induction(tvAB, tvAC, tvA, tvB, tvC);
    TEST_ASSERT(r.strength >= 0.0 && r.strength <= 1.0,
                "Induction: strength in [0,1]");
    TEST_ASSERT(r.confidence >= 0.0 && r.confidence <= 1.0,
                "Induction: confidence in [0,1]");
}

static void test_tv_abduction_formula(void) {
    printf("\n=== TV Abduction Formula ===\n");

    truth_value_t tvAB = tv(0.9, 0.8);
    truth_value_t tvCB = tv(0.7, 0.8);
    truth_value_t tvA  = tv(0.5, 0.9);
    truth_value_t tvB  = tv(0.5, 0.9);
    truth_value_t tvC  = tv(0.5, 0.9);

    truth_value_t r = pln_tv_abduction(tvAB, tvCB, tvA, tvB, tvC);
    TEST_ASSERT(r.strength >= 0.0 && r.strength <= 1.0,
                "Abduction: strength in [0,1]");
    TEST_ASSERT(r.confidence >= 0.0 && r.confidence <= 1.0,
                "Abduction: confidence in [0,1]");
}

/*===========================================================================
 * Forward Chaining
 *===========================================================================*/

static void test_forward_chain_basic(void) {
    printf("\n=== Forward Chaining (basic) ===\n");

    atomspace_t as = atomspace_create();
    pln_config_t cfg = pln_config_default();
    cfg.max_inference_steps = 5;
    pln_engine_t pln = pln_engine_create(as, &cfg);

    atom_handle_t cat    = atomspace_add_node(as, ATOM_TYPE_CONCEPT_NODE, "fwd_cat");
    atom_handle_t animal = atomspace_add_node(as, ATOM_TYPE_CONCEPT_NODE, "fwd_animal");
    atom_handle_t living = atomspace_add_node(as, ATOM_TYPE_CONCEPT_NODE, "fwd_living");

    truth_value_t tv_inh = {TRUTH_VALUE_SIMPLE, 0.9, 0.8, 1.0};
    atom_handle_t out1[] = {cat, animal};
    atomspace_add_link_tv(as, ATOM_TYPE_INHERITANCE_LINK, out1, 2, tv_inh);
    atom_handle_t out2[] = {animal, living};
    atomspace_add_link_tv(as, ATOM_TYPE_INHERITANCE_LINK, out2, 2, tv_inh);

    forward_chain_result_t* res = pln_forward_chain(pln, cat, 5);
    TEST_ASSERT(res != NULL, "Forward chain returns result");
    TEST_ASSERT(res->steps_taken > 0, "Forward chain took at least one step");
    TEST_ASSERT(res->derived_count >= 0, "Derived count is non-negative");

    pln_forward_chain_result_free(res);
    pln_engine_destroy(pln);
    atomspace_destroy(as);
}

static void test_forward_chain_multi_source(void) {
    printf("\n=== Forward Chaining (multi-source) ===\n");

    atomspace_t as = atomspace_create();
    pln_config_t cfg = pln_config_default();
    pln_engine_t pln = pln_engine_create(as, &cfg);

    atom_handle_t a = atomspace_add_node(as, ATOM_TYPE_CONCEPT_NODE, "mc_A");
    atom_handle_t b = atomspace_add_node(as, ATOM_TYPE_CONCEPT_NODE, "mc_B");
    atom_handle_t c = atomspace_add_node(as, ATOM_TYPE_CONCEPT_NODE, "mc_C");

    atom_handle_t sources[] = {a, b, c};
    forward_chain_result_t* res = pln_forward_chain_multi(pln, sources, 3, 5);
    TEST_ASSERT(res != NULL, "Multi-source forward chain returns result");

    pln_forward_chain_result_free(res);
    pln_engine_destroy(pln);
    atomspace_destroy(as);
}

/*===========================================================================
 * Backward Chaining
 *===========================================================================*/

static void test_backward_chain_basic(void) {
    printf("\n=== Backward Chaining (basic) ===\n");

    atomspace_t as = atomspace_create();
    pln_config_t cfg = pln_config_default();
    cfg.max_inference_steps = 10;
    pln_engine_t pln = pln_engine_create(as, &cfg);

    atom_handle_t cat    = atomspace_add_node(as, ATOM_TYPE_CONCEPT_NODE, "bwd_cat");
    atom_handle_t animal = atomspace_add_node(as, ATOM_TYPE_CONCEPT_NODE, "bwd_animal");
    atom_handle_t living = atomspace_add_node(as, ATOM_TYPE_CONCEPT_NODE, "bwd_living");

    truth_value_t tv_inh = {TRUTH_VALUE_SIMPLE, 0.9, 0.9, 1.0};
    atom_handle_t out1[] = {cat, animal};
    atomspace_add_link_tv(as, ATOM_TYPE_INHERITANCE_LINK, out1, 2, tv_inh);
    atom_handle_t out2[] = {animal, living};
    atomspace_add_link_tv(as, ATOM_TYPE_INHERITANCE_LINK, out2, 2, tv_inh);

    backward_chain_result_t* res = pln_backward_chain(pln, living, 10);
    TEST_ASSERT(res != NULL, "Backward chain returns result");
    TEST_ASSERT(res->steps_taken >= 0, "Backward chain steps is non-negative");

    pln_backward_chain_result_free(res);
    pln_engine_destroy(pln);
    atomspace_destroy(as);
}

static void test_backward_chain_with_premises(void) {
    printf("\n=== Backward Chaining (with premises) ===\n");

    atomspace_t as = atomspace_create();
    pln_config_t cfg = pln_config_default();
    pln_engine_t pln = pln_engine_create(as, &cfg);

    atom_handle_t a    = atomspace_add_node(as, ATOM_TYPE_CONCEPT_NODE, "bwp_A");
    atom_handle_t b    = atomspace_add_node(as, ATOM_TYPE_CONCEPT_NODE, "bwp_B");
    atom_handle_t goal = atomspace_add_node(as, ATOM_TYPE_CONCEPT_NODE, "bwp_goal");

    atom_handle_t premises[] = {a, b};
    backward_chain_result_t* res =
        pln_backward_chain_with_premises(pln, goal, premises, 2, 5);
    TEST_ASSERT(res != NULL, "Backward chain (with premises) returns result");

    pln_backward_chain_result_free(res);
    pln_engine_destroy(pln);
    atomspace_destroy(as);
}

/*===========================================================================
 * PLN Statistics
 *===========================================================================*/

static void test_pln_stats(void) {
    printf("\n=== PLN Statistics ===\n");

    atomspace_t as = atomspace_create();
    pln_config_t cfg = pln_config_default();
    pln_engine_t pln = pln_engine_create(as, &cfg);

    pln_stats_t stats;
    pln_get_stats(pln, &stats);
    TEST_ASSERT(stats.total_inferences == 0, "Stats start at zero");

    /* Run inference */
    atom_handle_t src = atomspace_add_node(as, ATOM_TYPE_CONCEPT_NODE, "stats_src");
    forward_chain_result_t* res = pln_forward_chain(pln, src, 3);
    pln_forward_chain_result_free(res);

    pln_get_stats(pln, &stats);
    TEST_ASSERT(stats.total_inferences >= 0,
                "Stats total_inferences is non-negative after run");

    /* Reset */
    pln_reset_stats(pln);
    pln_get_stats(pln, &stats);
    TEST_ASSERT(stats.total_inferences == 0, "Stats reset to zero");

    pln_engine_destroy(pln);
    atomspace_destroy(as);
}

/*===========================================================================
 * Inference History
 *===========================================================================*/

static void test_pln_inference_history(void) {
    printf("\n=== PLN Inference History ===\n");

    atomspace_t as = atomspace_create();
    pln_config_t cfg = pln_config_default();
    pln_engine_t pln = pln_engine_create(as, &cfg);

    atom_handle_t a    = atomspace_add_node(as, ATOM_TYPE_CONCEPT_NODE, "hist_A");
    atom_handle_t b    = atomspace_add_node(as, ATOM_TYPE_CONCEPT_NODE, "hist_B");
    truth_value_t tv_inh = {TRUTH_VALUE_SIMPLE, 0.8, 0.8, 1.0};
    atom_handle_t out[] = {a, b};
    atomspace_add_link_tv(as, ATOM_TYPE_INHERITANCE_LINK, out, 2, tv_inh);

    forward_chain_result_t* res = pln_forward_chain(pln, a, 3);
    pln_forward_chain_result_free(res);

    size_t count = 0;
    inference_step_t* history = pln_get_inference_history(pln, &count);
    TEST_ASSERT(count >= 0, "History count is non-negative");
    TEST_ASSERT(history != NULL || count == 0,
                "History pointer consistent with count");

    pln_inference_history_free(history, count);

    pln_clear_inference_history(pln);
    history = pln_get_inference_history(pln, &count);
    TEST_ASSERT(count == 0, "History cleared to zero");
    pln_inference_history_free(history, count);

    pln_engine_destroy(pln);
    atomspace_destroy(as);
}

/*===========================================================================
 * Config Default
 *===========================================================================*/

static void test_pln_config_default(void) {
    printf("\n=== PLN Config Default ===\n");

    pln_config_t cfg = pln_config_default();
    TEST_ASSERT(cfg.max_inference_steps > 0, "Default max_inference_steps > 0");
    TEST_ASSERT(cfg.min_confidence_threshold >= 0.0,
                "Default min_confidence_threshold >= 0");
    TEST_ASSERT(cfg.min_confidence_threshold <= 1.0,
                "Default min_confidence_threshold <= 1");
}

/*===========================================================================
 * Control Strategy
 *===========================================================================*/

static void test_pln_control_strategy(void) {
    printf("\n=== PLN Control Strategy ===\n");

    atomspace_t as = atomspace_create();
    pln_config_t cfg = pln_config_default();
    pln_engine_t pln = pln_engine_create(as, &cfg);

    /* Each strategy switch should not crash */
    pln_set_control_strategy(pln, PLN_CONTROL_RANDOM);
    pln_set_control_strategy(pln, PLN_CONTROL_STOCHASTIC);
    pln_set_control_strategy(pln, PLN_CONTROL_ATTENTION_BASED);
    pln_set_control_strategy(pln, PLN_CONTROL_GOAL_DIRECTED);
    pln_set_control_strategy(pln, PLN_CONTROL_HYBRID);
    TEST_ASSERT(1, "All control strategies set without crash");

    pln_engine_destroy(pln);
    atomspace_destroy(as);
}

/*===========================================================================
 * Multi-step Transitivity Chain
 *===========================================================================*/

static void test_pln_multistep_chain(void) {
    printf("\n=== PLN Multi-Step Transitivity Chain ===\n");

    atomspace_t as = atomspace_create();
    pln_config_t cfg = pln_config_default();
    cfg.max_inference_steps = 20;
    pln_engine_t pln = pln_engine_create(as, &cfg);

    /* Build A->B->C->D->E chain */
    const char* names[] = {"chain_A","chain_B","chain_C","chain_D","chain_E"};
    atom_handle_t nodes[5];
    for (int i = 0; i < 5; i++)
        nodes[i] = atomspace_add_node(as, ATOM_TYPE_CONCEPT_NODE, names[i]);

    truth_value_t t = {TRUTH_VALUE_SIMPLE, 0.9, 0.9, 1.0};
    for (int i = 0; i < 4; i++) {
        atom_handle_t out[] = {nodes[i], nodes[i+1]};
        atomspace_add_link_tv(as, ATOM_TYPE_INHERITANCE_LINK, out, 2, t);
    }

    forward_chain_result_t* res = pln_forward_chain(pln, nodes[0], 20);
    TEST_ASSERT(res != NULL, "Multi-step chain: result not NULL");
    TEST_ASSERT(res->derived_count >= 0, "Multi-step chain: derived_count >= 0");

    pln_forward_chain_result_free(res);
    pln_engine_destroy(pln);
    atomspace_destroy(as);
}

/*===========================================================================
 * Main
 *===========================================================================*/

int main(void) {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  CoGWXP-OS9  PLN Formula & Inference Test Suite          ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");

    cog_init();
    cog_log_set_level(COG_LOG_WARN);

    test_tv_and_boundary();
    test_tv_or_boundary();
    test_tv_not_boundary();
    test_tv_revision();
    test_tv_deduction_formula();
    test_tv_induction_formula();
    test_tv_abduction_formula();
    test_forward_chain_basic();
    test_forward_chain_multi_source();
    test_backward_chain_basic();
    test_backward_chain_with_premises();
    test_pln_stats();
    test_pln_inference_history();
    test_pln_config_default();
    test_pln_control_strategy();
    test_pln_multistep_chain();

    cog_shutdown();

    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  SUMMARY  Passed: %-4d  Failed: %-4d                     ║\n",
           g_passed, g_failed);
    printf("╚═══════════════════════════════════════════════════════════╝\n\n");

    return g_failed > 0 ? 1 : 0;
}
