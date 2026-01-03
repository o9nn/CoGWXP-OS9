/**
 * @file pln.h
 * @brief PLN - Probabilistic Logic Networks for CoGWXP-OS9
 * 
 * PLN provides uncertain inference capabilities for the OpenCog framework,
 * enabling reasoning under uncertainty with probabilistic truth values.
 * 
 * @copyright AGPL-3.0
 */

#ifndef _COGWXP_PLN_H_
#define _COGWXP_PLN_H_

#include "../cogutil/cogutil.h"
#include "../atomspace/atomspace.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * PLN Rule Types
 *===========================================================================*/

typedef enum {
    /* Deduction rules */
    PLN_RULE_DEDUCTION = 0,
    PLN_RULE_MODUS_PONENS,
    PLN_RULE_MODUS_TOLLENS,
    
    /* Induction rules */
    PLN_RULE_INDUCTION,
    PLN_RULE_ABDUCTION,
    
    /* Boolean rules */
    PLN_RULE_AND,
    PLN_RULE_OR,
    PLN_RULE_NOT,
    
    /* Quantifier rules */
    PLN_RULE_FORALL_INSTANTIATION,
    PLN_RULE_EXISTS_INSTANTIATION,
    
    /* Similarity rules */
    PLN_RULE_SIMILARITY_SUBSTITUTION,
    PLN_RULE_INHERITANCE_TO_SIMILARITY,
    PLN_RULE_SIMILARITY_TO_INHERITANCE,
    
    /* Set-theoretic rules */
    PLN_RULE_SUBSET_DEDUCTION,
    PLN_RULE_MEMBER_TO_INHERITANCE,
    
    /* Evaluation rules */
    PLN_RULE_EVALUATION_IMPLICATION,
    PLN_RULE_IMPLICATION_INSTANTIATION,
    
    PLN_RULE_MAX
} pln_rule_type_t;

/*===========================================================================
 * PLN Configuration
 *===========================================================================*/

typedef struct pln_config {
    /* Inference control */
    size_t max_inference_steps;
    double min_confidence_threshold;
    double min_strength_threshold;
    
    /* Rule selection */
    bool enabled_rules[PLN_RULE_MAX];
    double rule_weights[PLN_RULE_MAX];
    
    /* Attention integration */
    bool use_attention_allocation;
    double attention_weight;
    
    /* Caching */
    bool enable_inference_cache;
    size_t cache_size;
    
    /* Distributed inference (9P/Dis) */
    bool enable_distributed;
    const char* distributed_endpoint;
} pln_config_t;

COGUTIL_API pln_config_t pln_config_default(void);

/*===========================================================================
 * PLN Engine
 *===========================================================================*/

typedef struct pln_engine* pln_engine_t;

COGUTIL_API pln_engine_t pln_engine_create(atomspace_t as, pln_config_t* config);
COGUTIL_API void         pln_engine_destroy(pln_engine_t engine);
COGUTIL_API void         pln_engine_set_config(pln_engine_t engine, pln_config_t* config);
COGUTIL_API pln_config_t pln_engine_get_config(pln_engine_t engine);

/*===========================================================================
 * Forward Chaining
 *===========================================================================*/

typedef struct forward_chain_result {
    atom_handle_t* derived_atoms;
    size_t derived_count;
    size_t steps_taken;
    double total_time_ms;
} forward_chain_result_t;

COGUTIL_API forward_chain_result_t* pln_forward_chain(
    pln_engine_t engine,
    atom_handle_t source,
    size_t max_steps
);

COGUTIL_API forward_chain_result_t* pln_forward_chain_multi(
    pln_engine_t engine,
    atom_handle_t* sources,
    size_t source_count,
    size_t max_steps
);

COGUTIL_API void pln_forward_chain_result_free(forward_chain_result_t* result);

/*===========================================================================
 * Backward Chaining
 *===========================================================================*/

typedef struct backward_chain_result {
    atom_handle_t* proof_atoms;
    size_t proof_count;
    bool goal_achieved;
    double goal_truth_strength;
    double goal_truth_confidence;
    size_t steps_taken;
    double total_time_ms;
} backward_chain_result_t;

COGUTIL_API backward_chain_result_t* pln_backward_chain(
    pln_engine_t engine,
    atom_handle_t goal,
    size_t max_depth
);

COGUTIL_API backward_chain_result_t* pln_backward_chain_with_premises(
    pln_engine_t engine,
    atom_handle_t goal,
    atom_handle_t* premises,
    size_t premise_count,
    size_t max_depth
);

COGUTIL_API void pln_backward_chain_result_free(backward_chain_result_t* result);

/*===========================================================================
 * Rule Application
 *===========================================================================*/

typedef struct rule_application {
    pln_rule_type_t rule;
    atom_handle_t* premises;
    size_t premise_count;
    atom_handle_t conclusion;
    truth_value_t conclusion_tv;
    double confidence_gain;
} rule_application_t;

COGUTIL_API rule_application_t* pln_apply_rule(
    pln_engine_t engine,
    pln_rule_type_t rule,
    atom_handle_t* premises,
    size_t premise_count
);

COGUTIL_API atom_handle_t* pln_get_applicable_rules(
    pln_engine_t engine,
    atom_handle_t atom,
    size_t* count
);

COGUTIL_API void pln_rule_application_free(rule_application_t* app);

/*===========================================================================
 * Truth Value Formulas
 *===========================================================================*/

/* Deduction formula: P(B|A) * P(A) / P(B) */
COGUTIL_API truth_value_t pln_tv_deduction(
    truth_value_t tv_ab,    /* P(B|A) - premise 1 */
    truth_value_t tv_bc,    /* P(C|B) - premise 2 */
    truth_value_t tv_a,     /* P(A) - prior */
    truth_value_t tv_b,     /* P(B) - prior */
    truth_value_t tv_c      /* P(C) - prior */
);

/* Induction formula */
COGUTIL_API truth_value_t pln_tv_induction(
    truth_value_t tv_ab,
    truth_value_t tv_ac,
    truth_value_t tv_a,
    truth_value_t tv_b,
    truth_value_t tv_c
);

/* Abduction formula */
COGUTIL_API truth_value_t pln_tv_abduction(
    truth_value_t tv_ab,
    truth_value_t tv_cb,
    truth_value_t tv_a,
    truth_value_t tv_b,
    truth_value_t tv_c
);

/* Boolean formulas */
COGUTIL_API truth_value_t pln_tv_and(truth_value_t tv1, truth_value_t tv2);
COGUTIL_API truth_value_t pln_tv_or(truth_value_t tv1, truth_value_t tv2);
COGUTIL_API truth_value_t pln_tv_not(truth_value_t tv);

/* Revision formula - combining evidence */
COGUTIL_API truth_value_t pln_tv_revision(truth_value_t tv1, truth_value_t tv2);

/*===========================================================================
 * Inference Control
 *===========================================================================*/

typedef enum {
    PLN_CONTROL_RANDOM = 0,
    PLN_CONTROL_STOCHASTIC,
    PLN_CONTROL_ATTENTION_BASED,
    PLN_CONTROL_GOAL_DIRECTED,
    PLN_CONTROL_HYBRID
} pln_control_strategy_t;

COGUTIL_API void pln_set_control_strategy(pln_engine_t engine, pln_control_strategy_t strategy);

typedef double (*pln_rule_selector_t)(
    pln_engine_t engine,
    pln_rule_type_t rule,
    atom_handle_t* premises,
    size_t premise_count,
    void* user_data
);

COGUTIL_API void pln_set_rule_selector(
    pln_engine_t engine,
    pln_rule_selector_t selector,
    void* user_data
);

/*===========================================================================
 * Inference History
 *===========================================================================*/

typedef struct inference_step {
    pln_rule_type_t rule;
    atom_handle_t* premises;
    size_t premise_count;
    atom_handle_t conclusion;
    truth_value_t before_tv;
    truth_value_t after_tv;
    uint64_t timestamp;
} inference_step_t;

COGUTIL_API inference_step_t* pln_get_inference_history(
    pln_engine_t engine,
    size_t* count
);

COGUTIL_API void pln_clear_inference_history(pln_engine_t engine);
COGUTIL_API void pln_inference_history_free(inference_step_t* history, size_t count);

/*===========================================================================
 * Distributed PLN (9P/Dis Integration)
 *===========================================================================*/

/* Distributed inference across 9P network */
COGUTIL_API cog_result_t pln_distributed_forward_chain(
    pln_engine_t engine,
    atom_handle_t source,
    const char** endpoints,
    size_t endpoint_count,
    forward_chain_result_t** results,
    size_t* result_count
);

/* Execute PLN rule in Dis VM */
COGUTIL_API cog_result_t pln_execute_in_dis(
    pln_engine_t engine,
    pln_rule_type_t rule,
    atom_handle_t* premises,
    size_t premise_count,
    const char* dis_endpoint,
    atom_handle_t* result
);

/* Register custom rule implemented in Limbo */
COGUTIL_API cog_result_t pln_register_limbo_rule(
    pln_engine_t engine,
    const char* rule_name,
    const char* limbo_module,
    const char* limbo_function
);

/*===========================================================================
 * Statistics
 *===========================================================================*/

typedef struct pln_stats {
    size_t total_inferences;
    size_t successful_inferences;
    size_t failed_inferences;
    size_t cache_hits;
    size_t cache_misses;
    double avg_inference_time_ms;
    size_t rules_applied[PLN_RULE_MAX];
} pln_stats_t;

COGUTIL_API void pln_get_stats(pln_engine_t engine, pln_stats_t* stats);
COGUTIL_API void pln_reset_stats(pln_engine_t engine);

#ifdef __cplusplus
}
#endif

#endif /* _COGWXP_PLN_H_ */
