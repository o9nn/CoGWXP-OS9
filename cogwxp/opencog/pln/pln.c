/**
 * @file pln.c
 * @brief PLN (Probabilistic Logic Networks) Reasoning Engine Implementation
 * 
 * Implements probabilistic inference rules for uncertain reasoning
 * over the AtomSpace hypergraph.
 * 
 * @copyright CoGWXP-OS9 Project
 */

#include "pln.h"
#include "../atomspace/atomspace.h"
#include "../cogutil/cogutil.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <math.h>

/*===========================================================================
 * Constants
 *===========================================================================*/

#define PLN_DEFAULT_K 800.0          /* Default confidence discount factor */
#define PLN_EPSILON 1e-10            /* Small value to avoid division by zero */
#define PLN_MAX_CHAIN_DEPTH 10       /* Maximum inference chain depth */
#define PLN_MAX_RULE_APPLICATIONS 1000

/*===========================================================================
 * Internal Structures
 *===========================================================================*/

/* Rule application result */
typedef struct {
    atom_handle_t conclusion;
    truth_value_t tv;
    double confidence;
} rule_result_t;

/* Inference step */
typedef struct inference_step {
    pln_rule_type_t rule;
    atom_handle_t* premises;
    size_t premise_count;
    atom_handle_t conclusion;
    truth_value_t tv;
    struct inference_step* next;
} inference_step_t;

/* PLN context */
struct pln_context {
    atomspace_t atomspace;
    pln_config_t config;
    
    /* Rule registry */
    struct {
        pln_rule_t* rules;
        size_t count;
        size_t capacity;
    } rules;
    pthread_rwlock_t rules_lock;
    
    /* Inference history */
    inference_step_t* history_head;
    inference_step_t* history_tail;
    size_t history_size;
    pthread_mutex_t history_lock;
    
    /* Statistics */
    pln_stats_t stats;
    pthread_mutex_t stats_lock;
    
    /* Inference control */
    bool running;
    pthread_mutex_t control_lock;
};

/*===========================================================================
 * Truth Value Formulas
 *===========================================================================*/

/* Convert strength/confidence to count */
static double tv_to_count(double confidence, double k) {
    if (confidence >= 1.0 - PLN_EPSILON) return k * 1000;
    return k * confidence / (1.0 - confidence);
}

/* Convert count to confidence */
static double count_to_confidence(double count, double k) {
    return count / (count + k);
}

/* Deduction formula: (A->B, B->C) => A->C */
static truth_value_t formula_deduction(
    const truth_value_t* ab,
    const truth_value_t* bc,
    double k
) {
    truth_value_t result = {.type = TV_SIMPLE};
    
    double sAB = ab->strength;
    double sBC = bc->strength;
    double sB = 0.5; /* Default assumption */
    double sC = 0.5;
    double sNotB = 1.0 - sB;
    
    /* Deduction strength formula */
    result.strength = sAB * sBC + (1.0 - sAB) * (sC - sB * sBC) / sNotB;
    result.strength = fmax(0.0, fmin(1.0, result.strength));
    
    /* Confidence based on weakest link */
    result.confidence = fmin(ab->confidence, bc->confidence);
    
    return result;
}

/* Induction formula: (A->B, A->C) => B->C */
static truth_value_t formula_induction(
    const truth_value_t* ab,
    const truth_value_t* ac,
    double k
) {
    truth_value_t result = {.type = TV_SIMPLE};
    
    double sAB = ab->strength;
    double sAC = ac->strength;
    double sA = 0.5;
    double sB = 0.5;
    
    /* Induction strength formula */
    if (sB > PLN_EPSILON) {
        result.strength = sA * sAB * sAC / sB;
        result.strength = fmax(0.0, fmin(1.0, result.strength));
    } else {
        result.strength = 0.0;
    }
    
    /* Lower confidence for induction */
    result.confidence = fmin(ab->confidence, ac->confidence) * 0.5;
    
    return result;
}

/* Abduction formula: (A->B, C->B) => A->C */
static truth_value_t formula_abduction(
    const truth_value_t* ab,
    const truth_value_t* cb,
    double k
) {
    truth_value_t result = {.type = TV_SIMPLE};
    
    double sAB = ab->strength;
    double sCB = cb->strength;
    double sA = 0.5;
    double sC = 0.5;
    double sB = 0.5;
    
    /* Abduction strength formula */
    if (sB > PLN_EPSILON) {
        result.strength = sA * sAB * sCB / sB;
        result.strength = fmax(0.0, fmin(1.0, result.strength));
    } else {
        result.strength = 0.0;
    }
    
    /* Lower confidence for abduction */
    result.confidence = fmin(ab->confidence, cb->confidence) * 0.3;
    
    return result;
}

/* Modus Ponens: (A, A->B) => B */
static truth_value_t formula_modus_ponens(
    const truth_value_t* a,
    const truth_value_t* ab,
    double k
) {
    truth_value_t result = {.type = TV_SIMPLE};
    
    /* Strength: P(B) = P(A) * P(B|A) + P(~A) * P(B|~A) */
    /* Simplified: assume P(B|~A) = P(B) */
    double sA = a->strength;
    double sAB = ab->strength;
    double sB = 0.5; /* Prior */
    
    result.strength = sA * sAB + (1.0 - sA) * sB;
    result.strength = fmax(0.0, fmin(1.0, result.strength));
    
    result.confidence = fmin(a->confidence, ab->confidence);
    
    return result;
}

/* AND formula */
static truth_value_t formula_and(
    const truth_value_t* a,
    const truth_value_t* b,
    double k
) {
    truth_value_t result = {.type = TV_SIMPLE};
    
    /* Assuming independence */
    result.strength = a->strength * b->strength;
    result.confidence = fmin(a->confidence, b->confidence);
    
    return result;
}

/* OR formula */
static truth_value_t formula_or(
    const truth_value_t* a,
    const truth_value_t* b,
    double k
) {
    truth_value_t result = {.type = TV_SIMPLE};
    
    /* Assuming independence */
    result.strength = a->strength + b->strength - a->strength * b->strength;
    result.confidence = fmin(a->confidence, b->confidence);
    
    return result;
}

/* NOT formula */
static truth_value_t formula_not(
    const truth_value_t* a,
    double k
) {
    truth_value_t result = {.type = TV_SIMPLE};
    
    result.strength = 1.0 - a->strength;
    result.confidence = a->confidence;
    
    return result;
}

/* Revision formula: merge two truth values */
static truth_value_t formula_revision(
    const truth_value_t* a,
    const truth_value_t* b,
    double k
) {
    truth_value_t result = {.type = TV_SIMPLE};
    
    double nA = tv_to_count(a->confidence, k);
    double nB = tv_to_count(b->confidence, k);
    double nAB = nA + nB;
    
    if (nAB > PLN_EPSILON) {
        result.strength = (a->strength * nA + b->strength * nB) / nAB;
        result.confidence = count_to_confidence(nAB, k);
    } else {
        result.strength = (a->strength + b->strength) / 2.0;
        result.confidence = 0.0;
    }
    
    return result;
}

/*===========================================================================
 * PLN Context Lifecycle
 *===========================================================================*/

COGUTIL_API cog_result_t pln_init(
    atomspace_t atomspace,
    const pln_config_t* config,
    pln_context_t* ctx
) {
    if (!atomspace || !ctx) return COG_ERROR_INVALID_PARAM;
    
    pln_context_t c = COG_CALLOC(1, sizeof(struct pln_context));
    if (!c) return COG_ERROR_MEMORY;
    
    c->atomspace = atomspace;
    
    /* Copy config */
    if (config) {
        memcpy(&c->config, config, sizeof(pln_config_t));
    } else {
        c->config.k = PLN_DEFAULT_K;
        c->config.max_chain_depth = PLN_MAX_CHAIN_DEPTH;
        c->config.confidence_threshold = 0.1;
        c->config.enable_backward_chaining = true;
        c->config.enable_forward_chaining = true;
    }
    
    /* Initialize rule registry */
    c->rules.capacity = 32;
    c->rules.rules = COG_CALLOC(c->rules.capacity, sizeof(pln_rule_t));
    
    /* Initialize locks */
    pthread_rwlock_init(&c->rules_lock, NULL);
    pthread_mutex_init(&c->history_lock, NULL);
    pthread_mutex_init(&c->stats_lock, NULL);
    pthread_mutex_init(&c->control_lock, NULL);
    
    /* Register built-in rules */
    pln_rule_t deduction = {
        .type = PLN_RULE_DEDUCTION,
        .name = "Deduction",
        .premise_count = 2,
        .enabled = true
    };
    pln_register_rule(c, &deduction);
    
    pln_rule_t induction = {
        .type = PLN_RULE_INDUCTION,
        .name = "Induction",
        .premise_count = 2,
        .enabled = true
    };
    pln_register_rule(c, &induction);
    
    pln_rule_t abduction = {
        .type = PLN_RULE_ABDUCTION,
        .name = "Abduction",
        .premise_count = 2,
        .enabled = true
    };
    pln_register_rule(c, &abduction);
    
    pln_rule_t modus_ponens = {
        .type = PLN_RULE_MODUS_PONENS,
        .name = "ModusPonens",
        .premise_count = 2,
        .enabled = true
    };
    pln_register_rule(c, &modus_ponens);
    
    *ctx = c;
    
    COG_LOG_INFO("PLN reasoning engine initialized");
    return COG_OK;
}

COGUTIL_API void pln_shutdown(pln_context_t ctx) {
    if (!ctx) return;
    
    /* Free rules */
    COG_FREE(ctx->rules.rules);
    
    /* Free history */
    inference_step_t* step = ctx->history_head;
    while (step) {
        inference_step_t* next = step->next;
        COG_FREE(step->premises);
        COG_FREE(step);
        step = next;
    }
    
    /* Destroy locks */
    pthread_rwlock_destroy(&ctx->rules_lock);
    pthread_mutex_destroy(&ctx->history_lock);
    pthread_mutex_destroy(&ctx->stats_lock);
    pthread_mutex_destroy(&ctx->control_lock);
    
    COG_FREE(ctx);
    
    COG_LOG_INFO("PLN reasoning engine shutdown");
}

/*===========================================================================
 * Rule Management
 *===========================================================================*/

COGUTIL_API cog_result_t pln_register_rule(pln_context_t ctx, const pln_rule_t* rule) {
    if (!ctx || !rule) return COG_ERROR_INVALID_PARAM;
    
    pthread_rwlock_wrlock(&ctx->rules_lock);
    
    /* Grow array if needed */
    if (ctx->rules.count >= ctx->rules.capacity) {
        size_t new_capacity = ctx->rules.capacity * 2;
        pln_rule_t* new_rules = COG_REALLOC(ctx->rules.rules, 
            new_capacity * sizeof(pln_rule_t));
        if (!new_rules) {
            pthread_rwlock_unlock(&ctx->rules_lock);
            return COG_ERROR_MEMORY;
        }
        ctx->rules.rules = new_rules;
        ctx->rules.capacity = new_capacity;
    }
    
    memcpy(&ctx->rules.rules[ctx->rules.count], rule, sizeof(pln_rule_t));
    ctx->rules.count++;
    
    pthread_rwlock_unlock(&ctx->rules_lock);
    
    COG_LOG_DEBUG("Registered PLN rule: %s", rule->name);
    return COG_OK;
}

COGUTIL_API cog_result_t pln_enable_rule(pln_context_t ctx, pln_rule_type_t type, bool enabled) {
    if (!ctx) return COG_ERROR_INVALID_PARAM;
    
    pthread_rwlock_wrlock(&ctx->rules_lock);
    
    for (size_t i = 0; i < ctx->rules.count; i++) {
        if (ctx->rules.rules[i].type == type) {
            ctx->rules.rules[i].enabled = enabled;
            pthread_rwlock_unlock(&ctx->rules_lock);
            return COG_OK;
        }
    }
    
    pthread_rwlock_unlock(&ctx->rules_lock);
    return COG_ERROR_NOT_FOUND;
}

/*===========================================================================
 * Rule Application
 *===========================================================================*/

static cog_result_t apply_deduction(
    pln_context_t ctx,
    atom_handle_t ab,
    atom_handle_t bc,
    atom_handle_t* result
) {
    /* Get outgoing sets */
    atom_handle_t* ab_out = NULL;
    atom_handle_t* bc_out = NULL;
    size_t ab_count, bc_count;
    
    atomspace_get_outgoing(ctx->atomspace, ab, &ab_out, &ab_count);
    atomspace_get_outgoing(ctx->atomspace, bc, &bc_out, &bc_count);
    
    if (ab_count != 2 || bc_count != 2) {
        COG_FREE(ab_out);
        COG_FREE(bc_out);
        return COG_ERROR_INVALID_PARAM;
    }
    
    /* Check if B matches */
    if (ab_out[1] != bc_out[0]) {
        COG_FREE(ab_out);
        COG_FREE(bc_out);
        return COG_ERROR_INVALID_PARAM;
    }
    
    /* Get truth values */
    truth_value_t tv_ab, tv_bc;
    atomspace_get_tv(ctx->atomspace, ab, &tv_ab);
    atomspace_get_tv(ctx->atomspace, bc, &tv_bc);
    
    /* Apply deduction formula */
    truth_value_t tv_ac = formula_deduction(&tv_ab, &tv_bc, ctx->config.k);
    
    /* Create conclusion A->C */
    atom_handle_t outgoing[] = {ab_out[0], bc_out[1]};
    *result = atomspace_add_link(ctx->atomspace, ATOM_TYPE_INHERITANCE, outgoing, 2, &tv_ac);
    
    COG_FREE(ab_out);
    COG_FREE(bc_out);
    
    return COG_OK;
}

static cog_result_t apply_modus_ponens(
    pln_context_t ctx,
    atom_handle_t a,
    atom_handle_t ab,
    atom_handle_t* result
) {
    /* Get outgoing set of implication */
    atom_handle_t* ab_out = NULL;
    size_t ab_count;
    
    atomspace_get_outgoing(ctx->atomspace, ab, &ab_out, &ab_count);
    
    if (ab_count != 2) {
        COG_FREE(ab_out);
        return COG_ERROR_INVALID_PARAM;
    }
    
    /* Check if A matches */
    if (ab_out[0] != a) {
        COG_FREE(ab_out);
        return COG_ERROR_INVALID_PARAM;
    }
    
    /* Get truth values */
    truth_value_t tv_a, tv_ab;
    atomspace_get_tv(ctx->atomspace, a, &tv_a);
    atomspace_get_tv(ctx->atomspace, ab, &tv_ab);
    
    /* Apply modus ponens formula */
    truth_value_t tv_b = formula_modus_ponens(&tv_a, &tv_ab, ctx->config.k);
    
    /* Update or create B with new truth value */
    atom_handle_t b = ab_out[1];
    
    truth_value_t existing_tv;
    if (atomspace_get_tv(ctx->atomspace, b, &existing_tv) == COG_OK) {
        /* Merge with existing truth value */
        tv_b = formula_revision(&existing_tv, &tv_b, ctx->config.k);
    }
    
    atomspace_set_tv(ctx->atomspace, b, &tv_b);
    *result = b;
    
    COG_FREE(ab_out);
    
    return COG_OK;
}

COGUTIL_API cog_result_t pln_apply_rule(
    pln_context_t ctx,
    pln_rule_type_t rule_type,
    const atom_handle_t* premises,
    size_t premise_count,
    atom_handle_t* conclusion,
    truth_value_t* conclusion_tv
) {
    if (!ctx || !premises || !conclusion) return COG_ERROR_INVALID_PARAM;
    
    cog_result_t result = COG_ERROR_INVALID_PARAM;
    
    switch (rule_type) {
        case PLN_RULE_DEDUCTION:
            if (premise_count == 2) {
                result = apply_deduction(ctx, premises[0], premises[1], conclusion);
            }
            break;
            
        case PLN_RULE_MODUS_PONENS:
            if (premise_count == 2) {
                result = apply_modus_ponens(ctx, premises[0], premises[1], conclusion);
            }
            break;
            
        /* Add more rule implementations */
        default:
            result = COG_ERROR_NOT_FOUND;
            break;
    }
    
    if (result == COG_OK && conclusion_tv) {
        atomspace_get_tv(ctx->atomspace, *conclusion, conclusion_tv);
    }
    
    /* Update statistics */
    pthread_mutex_lock(&ctx->stats_lock);
    if (result == COG_OK) {
        ctx->stats.rules_applied++;
    }
    pthread_mutex_unlock(&ctx->stats_lock);
    
    return result;
}

/*===========================================================================
 * Forward Chaining
 *===========================================================================*/

COGUTIL_API cog_result_t pln_forward_chain(
    pln_context_t ctx,
    atom_handle_t source,
    uint32_t max_steps,
    atom_handle_t** conclusions,
    size_t* conclusion_count
) {
    if (!ctx || !conclusions || !conclusion_count) return COG_ERROR_INVALID_PARAM;
    
    if (!ctx->config.enable_forward_chaining) {
        return COG_ERROR_CONFIG;
    }
    
    /* Allocate result array */
    size_t capacity = 64;
    *conclusions = COG_CALLOC(capacity, sizeof(atom_handle_t));
    *conclusion_count = 0;
    
    /* Get all implications involving source */
    atom_handle_t* incoming = NULL;
    size_t incoming_count;
    atomspace_get_incoming(ctx->atomspace, source, &incoming, &incoming_count);
    
    uint32_t steps = 0;
    
    for (size_t i = 0; i < incoming_count && steps < max_steps; i++) {
        atom_type_t type = atomspace_get_type(ctx->atomspace, incoming[i]);
        
        if (type == ATOM_TYPE_INHERITANCE || type == ATOM_TYPE_IMPLICATION) {
            /* Try to apply modus ponens */
            atom_handle_t conclusion;
            if (pln_apply_rule(ctx, PLN_RULE_MODUS_PONENS, 
                (atom_handle_t[]){source, incoming[i]}, 2, &conclusion, NULL) == COG_OK) {
                
                /* Add to results */
                if (*conclusion_count >= capacity) {
                    capacity *= 2;
                    *conclusions = COG_REALLOC(*conclusions, capacity * sizeof(atom_handle_t));
                }
                (*conclusions)[(*conclusion_count)++] = conclusion;
                steps++;
            }
        }
    }
    
    COG_FREE(incoming);
    
    /* Update statistics */
    pthread_mutex_lock(&ctx->stats_lock);
    ctx->stats.forward_chains++;
    ctx->stats.inferences_made += *conclusion_count;
    pthread_mutex_unlock(&ctx->stats_lock);
    
    return COG_OK;
}

/*===========================================================================
 * Backward Chaining
 *===========================================================================*/

static cog_result_t backward_chain_recursive(
    pln_context_t ctx,
    atom_handle_t target,
    uint32_t depth,
    uint32_t max_depth,
    atom_handle_t** proof,
    size_t* proof_size
) {
    if (depth >= max_depth) return COG_ERROR_TIMEOUT;
    
    /* Check if target already has sufficient truth value */
    truth_value_t tv;
    if (atomspace_get_tv(ctx->atomspace, target, &tv) == COG_OK) {
        if (tv.confidence >= ctx->config.confidence_threshold) {
            return COG_OK;
        }
    }
    
    /* Find implications that could prove target */
    atom_handle_t* atoms = NULL;
    size_t atom_count;
    atomspace_get_atoms_by_type(ctx->atomspace, ATOM_TYPE_IMPLICATION, false, &atoms, &atom_count);
    
    for (size_t i = 0; i < atom_count; i++) {
        atom_handle_t* outgoing = NULL;
        size_t out_count;
        atomspace_get_outgoing(ctx->atomspace, atoms[i], &outgoing, &out_count);
        
        if (out_count == 2 && outgoing[1] == target) {
            /* Found A->target, try to prove A */
            cog_result_t result = backward_chain_recursive(ctx, outgoing[0], 
                depth + 1, max_depth, proof, proof_size);
            
            if (result == COG_OK) {
                /* Apply modus ponens */
                atom_handle_t conclusion;
                pln_apply_rule(ctx, PLN_RULE_MODUS_PONENS,
                    (atom_handle_t[]){outgoing[0], atoms[i]}, 2, &conclusion, NULL);
                
                COG_FREE(outgoing);
                COG_FREE(atoms);
                return COG_OK;
            }
        }
        
        COG_FREE(outgoing);
    }
    
    COG_FREE(atoms);
    return COG_ERROR_NOT_FOUND;
}

COGUTIL_API cog_result_t pln_backward_chain(
    pln_context_t ctx,
    atom_handle_t target,
    uint32_t max_depth,
    atom_handle_t** proof,
    size_t* proof_size
) {
    if (!ctx || !proof || !proof_size) return COG_ERROR_INVALID_PARAM;
    
    if (!ctx->config.enable_backward_chaining) {
        return COG_ERROR_CONFIG;
    }
    
    *proof = NULL;
    *proof_size = 0;
    
    cog_result_t result = backward_chain_recursive(ctx, target, 0, max_depth, proof, proof_size);
    
    /* Update statistics */
    pthread_mutex_lock(&ctx->stats_lock);
    ctx->stats.backward_chains++;
    pthread_mutex_unlock(&ctx->stats_lock);
    
    return result;
}

/*===========================================================================
 * Truth Value Operations
 *===========================================================================*/

COGUTIL_API truth_value_t pln_tv_and(const truth_value_t* a, const truth_value_t* b) {
    return formula_and(a, b, PLN_DEFAULT_K);
}

COGUTIL_API truth_value_t pln_tv_or(const truth_value_t* a, const truth_value_t* b) {
    return formula_or(a, b, PLN_DEFAULT_K);
}

COGUTIL_API truth_value_t pln_tv_not(const truth_value_t* a) {
    return formula_not(a, PLN_DEFAULT_K);
}

COGUTIL_API truth_value_t pln_tv_revision(const truth_value_t* a, const truth_value_t* b) {
    return formula_revision(a, b, PLN_DEFAULT_K);
}

/*===========================================================================
 * Statistics
 *===========================================================================*/

COGUTIL_API cog_result_t pln_get_stats(pln_context_t ctx, pln_stats_t* stats) {
    if (!ctx || !stats) return COG_ERROR_INVALID_PARAM;
    
    pthread_mutex_lock(&ctx->stats_lock);
    memcpy(stats, &ctx->stats, sizeof(pln_stats_t));
    pthread_mutex_unlock(&ctx->stats_lock);
    
    return COG_OK;
}

COGUTIL_API void pln_reset_stats(pln_context_t ctx) {
    if (!ctx) return;
    
    pthread_mutex_lock(&ctx->stats_lock);
    memset(&ctx->stats, 0, sizeof(pln_stats_t));
    pthread_mutex_unlock(&ctx->stats_lock);
}
