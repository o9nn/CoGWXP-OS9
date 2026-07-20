/**
 * @file pln.c
 * @brief Probabilistic Logic Networks Implementation
 *
 * Implementation of the public PLN engine API declared in pln.h (the header
 * is the canonical interface contract consumed by the kernel, integration
 * and orchestration layers).
 *
 * @copyright CoGWXP-OS9 Project
 */

#include "pln.h"
#include "../atomspace/atomspace.h"
#include "../cogutil/cogutil.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define PLN_EPSILON 1e-9
#define PLN_DEFAULT_K 800.0

/*===========================================================================
 * Engine Structure
 *===========================================================================*/

struct pln_engine {
    atomspace_t atomspace;
    pln_config_t config;

    pln_control_strategy_t strategy;
    pln_rule_selector_t rule_selector;
    void* rule_selector_data;

    /* Inference history */
    inference_step_t* history;
    size_t history_count;
    size_t history_capacity;

    /* Statistics */
    pln_stats_t stats;
};

/*===========================================================================
 * Internal Helpers
 *===========================================================================*/

static double clamp01(double x) {
    if (x < 0.0) return 0.0;
    if (x > 1.0) return 1.0;
    return x;
}

/* Convert strength/confidence to count */
static double tv_to_evidence_count(double confidence, double k) {
    if (confidence >= 1.0 - PLN_EPSILON) return k * 1000.0;
    return k * confidence / (1.0 - confidence);
}

/* Convert count to confidence */
static double evidence_count_to_confidence(double count, double k) {
    return count / (count + k);
}

static uint64_t now_ms(void) {
    struct timespec ts;
#if defined(CLOCK_REALTIME)
    clock_gettime(CLOCK_REALTIME, &ts);
#else
    ts.tv_sec = time(NULL);
    ts.tv_nsec = 0;
#endif
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)(ts.tv_nsec / 1000000L);
}

static void history_record(
    pln_engine_t engine,
    pln_rule_type_t rule,
    const atom_handle_t* premises,
    size_t premise_count,
    atom_handle_t conclusion,
    truth_value_t before_tv,
    truth_value_t after_tv
) {
    if (!engine) return;
    if (engine->history_count >= engine->history_capacity) {
        size_t cap = engine->history_capacity ? engine->history_capacity * 2 : 16;
        inference_step_t* grown =
            realloc(engine->history, cap * sizeof(inference_step_t));
        if (!grown) return;
        engine->history = grown;
        engine->history_capacity = cap;
    }
    inference_step_t* step = &engine->history[engine->history_count];
    memset(step, 0, sizeof(*step));
    step->rule = rule;
    if (premise_count > 0 && premises) {
        step->premises = malloc(premise_count * sizeof(atom_handle_t));
        if (step->premises) {
            memcpy(step->premises, premises, premise_count * sizeof(atom_handle_t));
            step->premise_count = premise_count;
        }
    }
    step->conclusion = conclusion;
    step->before_tv = before_tv;
    step->after_tv = after_tv;
    step->timestamp = now_ms();
    engine->history_count++;

    if (rule >= 0 && rule < PLN_RULE_MAX) {
        engine->stats.rules_applied[rule]++;
    }
}

/*===========================================================================
 * Truth Value Formulas
 *===========================================================================*/

COGUTIL_API truth_value_t pln_tv_deduction(
    truth_value_t tv_ab,
    truth_value_t tv_bc,
    truth_value_t tv_a,
    truth_value_t tv_b,
    truth_value_t tv_c
) {
    (void)tv_a;
    truth_value_t result = { TRUTH_VALUE_SIMPLE, 0.0, 0.0, 1.0 };

    double sAB = tv_ab.strength;
    double sBC = tv_bc.strength;
    double sB = tv_b.strength;
    double sC = tv_c.strength;
    double sNotB = 1.0 - sB;

    if (sNotB > PLN_EPSILON) {
        result.strength = sAB * sBC + (1.0 - sAB) * (sC - sB * sBC) / sNotB;
    } else {
        result.strength = sAB * sBC;
    }
    result.strength = clamp01(result.strength);
    result.confidence = fmin(tv_ab.confidence, tv_bc.confidence);
    return result;
}

COGUTIL_API truth_value_t pln_tv_induction(
    truth_value_t tv_ab,
    truth_value_t tv_ac,
    truth_value_t tv_a,
    truth_value_t tv_b,
    truth_value_t tv_c
) {
    (void)tv_c;
    truth_value_t result = { TRUTH_VALUE_SIMPLE, 0.0, 0.0, 1.0 };

    double sAB = tv_ab.strength;
    double sAC = tv_ac.strength;
    double sA = tv_a.strength;
    double sB = tv_b.strength;

    if (sB > PLN_EPSILON) {
        result.strength = clamp01(sA * sAB * sAC / sB);
    }
    result.confidence = clamp01(fmin(tv_ab.confidence, tv_ac.confidence) * 0.5);
    return result;
}

COGUTIL_API truth_value_t pln_tv_abduction(
    truth_value_t tv_ab,
    truth_value_t tv_cb,
    truth_value_t tv_a,
    truth_value_t tv_b,
    truth_value_t tv_c
) {
    (void)tv_c;
    truth_value_t result = { TRUTH_VALUE_SIMPLE, 0.0, 0.0, 1.0 };

    double sAB = tv_ab.strength;
    double sCB = tv_cb.strength;
    double sA = tv_a.strength;
    double sB = tv_b.strength;

    if (sB > PLN_EPSILON) {
        result.strength = clamp01(sA * sAB * sCB / sB);
    }
    result.confidence = clamp01(fmin(tv_ab.confidence, tv_cb.confidence) * 0.3);
    return result;
}

COGUTIL_API truth_value_t pln_tv_and(truth_value_t tv1, truth_value_t tv2) {
    truth_value_t result = { TRUTH_VALUE_SIMPLE, 0.0, 0.0, 1.0 };
    result.strength = clamp01(tv1.strength * tv2.strength);
    result.confidence = fmin(tv1.confidence, tv2.confidence);
    return result;
}

COGUTIL_API truth_value_t pln_tv_or(truth_value_t tv1, truth_value_t tv2) {
    truth_value_t result = { TRUTH_VALUE_SIMPLE, 0.0, 0.0, 1.0 };
    result.strength =
        clamp01(tv1.strength + tv2.strength - tv1.strength * tv2.strength);
    result.confidence = fmin(tv1.confidence, tv2.confidence);
    return result;
}

COGUTIL_API truth_value_t pln_tv_not(truth_value_t tv) {
    truth_value_t result = { TRUTH_VALUE_SIMPLE, 0.0, 0.0, 1.0 };
    result.strength = clamp01(1.0 - tv.strength);
    result.confidence = tv.confidence;
    return result;
}

COGUTIL_API truth_value_t pln_tv_revision(truth_value_t tv1, truth_value_t tv2) {
    truth_value_t result = { TRUTH_VALUE_SIMPLE, 0.0, 0.0, 1.0 };

    double n1 = tv_to_evidence_count(tv1.confidence, PLN_DEFAULT_K);
    double n2 = tv_to_evidence_count(tv2.confidence, PLN_DEFAULT_K);
    double n = n1 + n2;

    if (n > PLN_EPSILON) {
        result.strength = clamp01((tv1.strength * n1 + tv2.strength * n2) / n);
        result.confidence = clamp01(evidence_count_to_confidence(n, PLN_DEFAULT_K));
    } else {
        result.strength = clamp01((tv1.strength + tv2.strength) / 2.0);
        result.confidence = 0.0;
    }
    if (result.confidence < fmax(tv1.confidence, tv2.confidence)) {
        result.confidence = fmax(tv1.confidence, tv2.confidence);
    }
    return result;
}

/*===========================================================================
 * Configuration
 *===========================================================================*/

COGUTIL_API pln_config_t pln_config_default(void) {
    pln_config_t config;
    memset(&config, 0, sizeof(config));
    config.max_inference_steps = 100;
    config.min_confidence_threshold = 0.1;
    config.min_strength_threshold = 0.01;
    for (int i = 0; i < PLN_RULE_MAX; i++) {
        config.enabled_rules[i] = true;
        config.rule_weights[i] = 1.0;
    }
    config.use_attention_allocation = false;
    config.attention_weight = 0.5;
    config.enable_inference_cache = false;
    config.cache_size = 1024;
    config.enable_distributed = false;
    config.distributed_endpoint = NULL;
    return config;
}

/*===========================================================================
 * Engine Lifecycle
 *===========================================================================*/

COGUTIL_API pln_engine_t pln_engine_create(atomspace_t as, pln_config_t* config) {
    struct pln_engine* engine = calloc(1, sizeof(struct pln_engine));
    if (!engine) return NULL;
    engine->atomspace = as;
    engine->config = config ? *config : pln_config_default();
    engine->strategy = PLN_CONTROL_RANDOM;
    return engine;
}

COGUTIL_API void pln_engine_destroy(pln_engine_t engine) {
    if (!engine) return;
    pln_clear_inference_history(engine);
    free(engine->history);
    free(engine);
}

COGUTIL_API void pln_engine_set_config(pln_engine_t engine, pln_config_t* config) {
    if (!engine || !config) return;
    engine->config = *config;
}

COGUTIL_API pln_config_t pln_engine_get_config(pln_engine_t engine) {
    if (!engine) return pln_config_default();
    return engine->config;
}

/*===========================================================================
 * Rule Application
 *===========================================================================*/

/* Attempt to apply deduction over two inheritance links A->B, B->C. */
static bool try_deduction(
    pln_engine_t engine,
    atom_handle_t link_ab,
    atom_handle_t link_bc,
    atom_handle_t* conclusion,
    truth_value_t* conclusion_tv
) {
    atomspace_t as = engine->atomspace;

    if (atomspace_get_arity(as, link_ab) != 2 ||
        atomspace_get_arity(as, link_bc) != 2) {
        return false;
    }

    atom_handle_t a = atomspace_get_outgoing_at(as, link_ab, 0);
    atom_handle_t b1 = atomspace_get_outgoing_at(as, link_ab, 1);
    atom_handle_t b2 = atomspace_get_outgoing_at(as, link_bc, 0);
    atom_handle_t c = atomspace_get_outgoing_at(as, link_bc, 1);

    if (b1 != b2 || a == c) return false;

    truth_value_t tv_ab = atomspace_get_tv(as, link_ab);
    truth_value_t tv_bc = atomspace_get_tv(as, link_bc);
    truth_value_t tv_a = atomspace_get_tv(as, a);
    truth_value_t tv_b = atomspace_get_tv(as, b1);
    truth_value_t tv_c = atomspace_get_tv(as, c);

    truth_value_t tv = pln_tv_deduction(tv_ab, tv_bc, tv_a, tv_b, tv_c);
    if (tv.confidence < engine->config.min_confidence_threshold &&
        tv.confidence > 0.0) {
        /* still record very-low-confidence conclusions as failures */
    }

    atom_handle_t out[2] = { a, c };
    atom_type_t link_type = atomspace_get_type(as, link_ab);
    atom_handle_t derived = atomspace_add_link_tv(as, link_type, out, 2, tv);
    if (derived == ATOM_HANDLE_INVALID) return false;

    if (conclusion) *conclusion = derived;
    if (conclusion_tv) *conclusion_tv = tv;
    return true;
}

COGUTIL_API rule_application_t* pln_apply_rule(
    pln_engine_t engine,
    pln_rule_type_t rule,
    atom_handle_t* premises,
    size_t premise_count
) {
    if (!engine || !engine->atomspace) return NULL;
    if (rule < 0 || rule >= PLN_RULE_MAX) return NULL;
    if (!engine->config.enabled_rules[rule]) return NULL;
    if (premise_count == 0 || !premises) return NULL;

    atomspace_t as = engine->atomspace;
    engine->stats.total_inferences++;

    rule_application_t* app = calloc(1, sizeof(rule_application_t));
    if (!app) return NULL;
    app->rule = rule;
    app->conclusion = ATOM_HANDLE_INVALID;

    truth_value_t tv1 = atomspace_get_tv(as, premises[0]);
    truth_value_t tv2 = premise_count > 1
        ? atomspace_get_tv(as, premises[1])
        : tv1;

    bool applied = false;
    truth_value_t tv = tv1;

    switch (rule) {
        case PLN_RULE_DEDUCTION:
        case PLN_RULE_SUBSET_DEDUCTION:
            if (premise_count >= 2) {
                applied = try_deduction(engine, premises[0], premises[1],
                                        &app->conclusion, &tv);
            }
            break;

        case PLN_RULE_AND:
            if (premise_count >= 2) {
                tv = pln_tv_and(tv1, tv2);
                atom_handle_t out[2] = { premises[0], premises[1] };
                app->conclusion =
                    atomspace_add_link_tv(as, ATOM_TYPE_AND_LINK, out, 2, tv);
                applied = app->conclusion != ATOM_HANDLE_INVALID;
            }
            break;

        case PLN_RULE_OR:
            if (premise_count >= 2) {
                tv = pln_tv_or(tv1, tv2);
                atom_handle_t out[2] = { premises[0], premises[1] };
                app->conclusion =
                    atomspace_add_link_tv(as, ATOM_TYPE_OR_LINK, out, 2, tv);
                applied = app->conclusion != ATOM_HANDLE_INVALID;
            }
            break;

        case PLN_RULE_NOT: {
            tv = pln_tv_not(tv1);
            atom_handle_t out[1] = { premises[0] };
            app->conclusion =
                atomspace_add_link_tv(as, ATOM_TYPE_NOT_LINK, out, 1, tv);
            applied = app->conclusion != ATOM_HANDLE_INVALID;
            break;
        }

        default:
            /* Rules without a structural implementation revise the premise
             * truth value in place. */
            if (premise_count >= 2) {
                tv = pln_tv_revision(tv1, tv2);
                atomspace_set_tv(as, premises[0], tv);
                app->conclusion = premises[0];
                applied = true;
            }
            break;
    }

    if (!applied) {
        engine->stats.failed_inferences++;
        free(app);
        return NULL;
    }

    engine->stats.successful_inferences++;

    app->premises = malloc(premise_count * sizeof(atom_handle_t));
    if (app->premises) {
        memcpy(app->premises, premises, premise_count * sizeof(atom_handle_t));
        app->premise_count = premise_count;
    }
    app->conclusion_tv = tv;
    app->confidence_gain = tv.confidence - tv1.confidence;

    history_record(engine, rule, premises, premise_count,
                   app->conclusion, tv1, tv);
    return app;
}

COGUTIL_API atom_handle_t* pln_get_applicable_rules(
    pln_engine_t engine,
    atom_handle_t atom,
    size_t* count
) {
    if (count) *count = 0;
    if (!engine || !count) return NULL;
    (void)atom;

    /* Return the enabled rule identifiers as handles */
    atom_handle_t* rules = malloc(PLN_RULE_MAX * sizeof(atom_handle_t));
    if (!rules) return NULL;
    size_t n = 0;
    for (int i = 0; i < PLN_RULE_MAX; i++) {
        if (engine->config.enabled_rules[i]) {
            rules[n++] = (atom_handle_t)i;
        }
    }
    *count = n;
    return rules;
}

COGUTIL_API void pln_rule_application_free(rule_application_t* app) {
    if (!app) return;
    free(app->premises);
    free(app);
}

/*===========================================================================
 * Forward Chaining
 *===========================================================================*/

static void result_append(forward_chain_result_t* result, atom_handle_t handle,
                          size_t* capacity) {
    for (size_t i = 0; i < result->derived_count; i++) {
        if (result->derived_atoms[i] == handle) return;
    }
    if (result->derived_count >= *capacity) {
        size_t cap = *capacity ? *capacity * 2 : 16;
        atom_handle_t* grown =
            realloc(result->derived_atoms, cap * sizeof(atom_handle_t));
        if (!grown) return;
        result->derived_atoms = grown;
        *capacity = cap;
    }
    result->derived_atoms[result->derived_count++] = handle;
}

/* One forward-chaining pass: try deduction on pairs of inheritance-style
 * links that touch the frontier atom. Returns number of new conclusions. */
static size_t forward_chain_step(
    pln_engine_t engine,
    atom_handle_t source,
    forward_chain_result_t* result,
    size_t* capacity
) {
    atomspace_t as = engine->atomspace;
    size_t derived = 0;

    size_t in_count = 0;
    atom_handle_t* incoming = atomspace_get_incoming(as, source, &in_count);

    for (size_t i = 0; i < in_count; i++) {
        atom_handle_t link_ab = incoming[i];
        if (atomspace_get_arity(as, link_ab) != 2) continue;
        /* source must be the first element (A of A->B) */
        if (atomspace_get_outgoing_at(as, link_ab, 0) != source) continue;

        atom_handle_t b = atomspace_get_outgoing_at(as, link_ab, 1);
        size_t b_in_count = 0;
        atom_handle_t* b_incoming = atomspace_get_incoming(as, b, &b_in_count);
        for (size_t j = 0; j < b_in_count; j++) {
            atom_handle_t link_bc = b_incoming[j];
            if (link_bc == link_ab) continue;
            if (atomspace_get_arity(as, link_bc) != 2) continue;
            if (atomspace_get_outgoing_at(as, link_bc, 0) != b) continue;

            atom_handle_t premises[2] = { link_ab, link_bc };
            rule_application_t* app =
                pln_apply_rule(engine, PLN_RULE_DEDUCTION, premises, 2);
            if (app) {
                if (app->conclusion != ATOM_HANDLE_INVALID) {
                    result_append(result, app->conclusion, capacity);
                    derived++;
                }
                pln_rule_application_free(app);
            }
        }
        atomspace_query_results_free(b_incoming);
    }
    atomspace_query_results_free(incoming);
    return derived;
}

COGUTIL_API forward_chain_result_t* pln_forward_chain(
    pln_engine_t engine,
    atom_handle_t source,
    size_t max_steps
) {
    if (!engine || !engine->atomspace) return NULL;

    forward_chain_result_t* result = calloc(1, sizeof(forward_chain_result_t));
    if (!result) return NULL;

    uint64_t start = now_ms();
    size_t capacity = 0;
    size_t limit = max_steps < engine->config.max_inference_steps
        ? max_steps : engine->config.max_inference_steps;
    if (limit == 0) limit = 1;

    /* Expand from the source, then only from newly derived conclusions
     * (frontier) on subsequent steps to avoid re-deriving the whole set. */
    result->steps_taken++;
    forward_chain_step(engine, source, result, &capacity);
    size_t frontier_start = 0;

    for (size_t step = 1; step < limit; step++) {
        size_t frontier_end = result->derived_count;
        if (frontier_start == frontier_end) break;

        result->steps_taken++;
        for (size_t i = frontier_start; i < frontier_end; i++) {
            atom_handle_t derived_link = result->derived_atoms[i];
            atom_handle_t next_source =
                atomspace_get_outgoing_at(engine->atomspace, derived_link, 1);
            if (next_source != ATOM_HANDLE_INVALID) {
                forward_chain_step(engine, next_source, result, &capacity);
            }
        }
        frontier_start = frontier_end;
    }

    result->total_time_ms = (double)(now_ms() - start);
    return result;
}

COGUTIL_API forward_chain_result_t* pln_forward_chain_multi(
    pln_engine_t engine,
    atom_handle_t* sources,
    size_t source_count,
    size_t max_steps
) {
    if (!engine || !engine->atomspace) return NULL;
    if (source_count > 0 && !sources) return NULL;

    forward_chain_result_t* combined = calloc(1, sizeof(forward_chain_result_t));
    if (!combined) return NULL;

    uint64_t start = now_ms();
    size_t capacity = 0;

    for (size_t i = 0; i < source_count; i++) {
        forward_chain_result_t* r =
            pln_forward_chain(engine, sources[i], max_steps);
        if (!r) continue;
        for (size_t j = 0; j < r->derived_count; j++) {
            result_append(combined, r->derived_atoms[j], &capacity);
        }
        combined->steps_taken += r->steps_taken;
        pln_forward_chain_result_free(r);
    }

    combined->total_time_ms = (double)(now_ms() - start);
    return combined;
}

COGUTIL_API void pln_forward_chain_result_free(forward_chain_result_t* result) {
    if (!result) return;
    free(result->derived_atoms);
    free(result);
}

/*===========================================================================
 * Backward Chaining
 *===========================================================================*/

static void proof_append(backward_chain_result_t* result, atom_handle_t handle,
                         size_t* capacity) {
    for (size_t i = 0; i < result->proof_count; i++) {
        if (result->proof_atoms[i] == handle) return;
    }
    if (result->proof_count >= *capacity) {
        size_t cap = *capacity ? *capacity * 2 : 16;
        atom_handle_t* grown =
            realloc(result->proof_atoms, cap * sizeof(atom_handle_t));
        if (!grown) return;
        result->proof_atoms = grown;
        *capacity = cap;
    }
    result->proof_atoms[result->proof_count++] = handle;
}

/* Recursively search for support of `goal` via inheritance-style links. */
static bool backward_search(
    pln_engine_t engine,
    atom_handle_t goal,
    size_t depth,
    backward_chain_result_t* result,
    size_t* capacity
) {
    if (depth == 0) return false;
    atomspace_t as = engine->atomspace;

    result->steps_taken++;

    truth_value_t goal_tv = atomspace_get_tv(as, goal);
    if (goal_tv.confidence >= engine->config.min_confidence_threshold &&
        goal_tv.strength >= engine->config.min_strength_threshold) {
        proof_append(result, goal, capacity);
        result->goal_truth_strength = goal_tv.strength;
        result->goal_truth_confidence = goal_tv.confidence;
        return true;
    }

    /* Look for links that conclude in the goal: X->goal */
    size_t in_count = 0;
    atom_handle_t* incoming = atomspace_get_incoming(as, goal, &in_count);
    bool achieved = false;

    for (size_t i = 0; i < in_count && !achieved; i++) {
        atom_handle_t link = incoming[i];
        if (atomspace_get_arity(as, link) != 2) continue;
        if (atomspace_get_outgoing_at(as, link, 1) != goal) continue;

        atom_handle_t sub_goal = atomspace_get_outgoing_at(as, link, 0);
        truth_value_t link_tv = atomspace_get_tv(as, link);
        if (link_tv.confidence < engine->config.min_confidence_threshold)
            continue;

        if (backward_search(engine, sub_goal, depth - 1, result, capacity)) {
            proof_append(result, link, capacity);
            proof_append(result, goal, capacity);
            truth_value_t sub_tv = atomspace_get_tv(as, sub_goal);
            truth_value_t combined = pln_tv_and(sub_tv, link_tv);
            result->goal_truth_strength = combined.strength;
            result->goal_truth_confidence = combined.confidence;
            achieved = true;
        }
    }

    atomspace_query_results_free(incoming);
    return achieved;
}

COGUTIL_API backward_chain_result_t* pln_backward_chain(
    pln_engine_t engine,
    atom_handle_t goal,
    size_t max_depth
) {
    if (!engine || !engine->atomspace) return NULL;

    backward_chain_result_t* result =
        calloc(1, sizeof(backward_chain_result_t));
    if (!result) return NULL;

    uint64_t start = now_ms();
    size_t capacity = 0;
    if (max_depth == 0) max_depth = 1;

    engine->stats.total_inferences++;
    result->goal_achieved =
        backward_search(engine, goal, max_depth, result, &capacity);
    if (result->goal_achieved) {
        engine->stats.successful_inferences++;
    } else {
        engine->stats.failed_inferences++;
    }

    result->total_time_ms = (double)(now_ms() - start);
    return result;
}

COGUTIL_API backward_chain_result_t* pln_backward_chain_with_premises(
    pln_engine_t engine,
    atom_handle_t goal,
    atom_handle_t* premises,
    size_t premise_count,
    size_t max_depth
) {
    if (!engine || !engine->atomspace) return NULL;

    /* Boost premise truth values as assumptions, then run standard search */
    for (size_t i = 0; i < premise_count && premises; i++) {
        truth_value_t tv = atomspace_get_tv(engine->atomspace, premises[i]);
        if (tv.confidence < 0.9) {
            tv.confidence = 0.9;
            atomspace_set_tv(engine->atomspace, premises[i], tv);
        }
    }
    return pln_backward_chain(engine, goal, max_depth);
}

COGUTIL_API void pln_backward_chain_result_free(backward_chain_result_t* result) {
    if (!result) return;
    free(result->proof_atoms);
    free(result);
}

/*===========================================================================
 * Inference Control
 *===========================================================================*/

COGUTIL_API void pln_set_control_strategy(pln_engine_t engine, pln_control_strategy_t strategy) {
    if (!engine) return;
    engine->strategy = strategy;
}

COGUTIL_API void pln_set_rule_selector(
    pln_engine_t engine,
    pln_rule_selector_t selector,
    void* user_data
) {
    if (!engine) return;
    engine->rule_selector = selector;
    engine->rule_selector_data = user_data;
}

/*===========================================================================
 * Inference History
 *===========================================================================*/

COGUTIL_API inference_step_t* pln_get_inference_history(
    pln_engine_t engine,
    size_t* count
) {
    if (count) *count = 0;
    if (!engine || !count || engine->history_count == 0) return NULL;

    inference_step_t* copy =
        malloc(engine->history_count * sizeof(inference_step_t));
    if (!copy) return NULL;

    for (size_t i = 0; i < engine->history_count; i++) {
        copy[i] = engine->history[i];
        if (engine->history[i].premise_count > 0 &&
            engine->history[i].premises) {
            copy[i].premises = malloc(
                engine->history[i].premise_count * sizeof(atom_handle_t));
            if (copy[i].premises) {
                memcpy(copy[i].premises, engine->history[i].premises,
                       engine->history[i].premise_count * sizeof(atom_handle_t));
            } else {
                copy[i].premise_count = 0;
            }
        } else {
            copy[i].premises = NULL;
            copy[i].premise_count = 0;
        }
    }
    *count = engine->history_count;
    return copy;
}

COGUTIL_API void pln_clear_inference_history(pln_engine_t engine) {
    if (!engine) return;
    for (size_t i = 0; i < engine->history_count; i++) {
        free(engine->history[i].premises);
    }
    engine->history_count = 0;
}

COGUTIL_API void pln_inference_history_free(inference_step_t* history, size_t count) {
    if (!history) return;
    for (size_t i = 0; i < count; i++) {
        free(history[i].premises);
    }
    free(history);
}

/*===========================================================================
 * Distributed PLN (9P/Dis Integration)
 *===========================================================================*/

COGUTIL_API cog_result_t pln_distributed_forward_chain(
    pln_engine_t engine,
    atom_handle_t source,
    const char** endpoints,
    size_t endpoint_count,
    forward_chain_result_t** results,
    size_t* result_count
) {
    if (!engine || !results || !result_count) return COG_ERROR_INVALID_ARG;
    (void)source;
    (void)endpoints;
    (void)endpoint_count;
    *results = NULL;
    *result_count = 0;
    if (!engine->config.enable_distributed) return COG_ERROR_STATE;
    return COG_ERROR_NOT_IMPLEMENTED;
}

COGUTIL_API cog_result_t pln_execute_in_dis(
    pln_engine_t engine,
    pln_rule_type_t rule,
    atom_handle_t* premises,
    size_t premise_count,
    const char* dis_endpoint,
    atom_handle_t* result
) {
    if (!engine || !result) return COG_ERROR_INVALID_ARG;
    (void)rule;
    (void)premises;
    (void)premise_count;
    (void)dis_endpoint;
    *result = ATOM_HANDLE_INVALID;
    return COG_ERROR_NOT_IMPLEMENTED;
}

COGUTIL_API cog_result_t pln_register_limbo_rule(
    pln_engine_t engine,
    const char* rule_name,
    const char* limbo_module,
    const char* limbo_function
) {
    if (!engine || !rule_name || !limbo_module || !limbo_function) {
        return COG_ERROR_INVALID_ARG;
    }
    return COG_ERROR_NOT_IMPLEMENTED;
}

/*===========================================================================
 * Statistics
 *===========================================================================*/

COGUTIL_API void pln_get_stats(pln_engine_t engine, pln_stats_t* stats) {
    if (!stats) return;
    memset(stats, 0, sizeof(*stats));
    if (!engine) return;
    *stats = engine->stats;
}

COGUTIL_API void pln_reset_stats(pln_engine_t engine) {
    if (!engine) return;
    memset(&engine->stats, 0, sizeof(engine->stats));
}
