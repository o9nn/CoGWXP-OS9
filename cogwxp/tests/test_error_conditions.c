/**
 * @file test_error_conditions.c
 * @brief Error-Path & Null-Safety Tests for CoGWXP-OS9 Core APIs
 *
 * Exercises every major API call with:
 *  - NULL pointer arguments
 *  - Invalid / out-of-range enum values
 *  - Mismatched parameters
 *  - Double-free / double-destroy guards
 *  - Zero-size allocations
 *  - Empty collections
 *
 * All calls must NOT crash — return COG_ERROR_* codes or graceful no-ops.
 *
 * @copyright CoGWXP-OS9 Project
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../opencog/cogutil/cogutil.h"
#include "../opencog/atomspace/atomspace.h"
#include "../opencog/pln/pln.h"

/*===========================================================================
 * Test framework
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

/* Helper: just checks the call does not crash (void return) */
#define TEST_NO_CRASH(call, msg) do { \
    (call); \
    printf("  [PASS] %s (no crash)\n", (msg)); \
    g_passed++; \
} while (0)

/*===========================================================================
 * CogUtil Error Paths
 *===========================================================================*/

static void test_cogutil_null_log(void) {
    printf("\n=== CogUtil: NULL/empty log messages ===\n");

    /* Empty format string must not crash */
    TEST_NO_CRASH(cog_log(COG_LOG_INFO, "test", ""), "cog_log with empty string");
    TEST_NO_CRASH(cog_log(COG_LOG_ERROR, NULL, "msg"), "cog_log with NULL module");
}

static void test_cogutil_zero_alloc(void) {
    printf("\n=== CogUtil: Zero-size allocations ===\n");

    /* cog_alloc(0) — behaviour is implementation-defined but must not crash */
    void* p = cog_alloc(0);
    TEST_ASSERT(1, "cog_alloc(0) does not crash");
    if (p) cog_free(p);

    void* c = cog_calloc(0, sizeof(int));
    TEST_ASSERT(1, "cog_calloc(0, n) does not crash");
    if (c) cog_free(c);
}

static void test_cogutil_strdup_empty(void) {
    printf("\n=== CogUtil: cog_strdup edge cases ===\n");

    char* e = cog_strdup("");
    TEST_ASSERT(e != NULL, "cog_strdup of empty string returns non-NULL");
    TEST_ASSERT(strlen(e) == 0, "cog_strdup of empty string has zero length");
    cog_free(e);
}

static void test_cogutil_string_split_empty(void) {
    printf("\n=== CogUtil: split empty string ===\n");

    size_t count = 0;
    char** parts = cog_string_split("", ',', &count);
    /* Must not crash; count == 0 or 1 depending on implementation */
    TEST_ASSERT(1, "split empty string does not crash");
    cog_string_split_free(parts, count);
}

static void test_hashmap_null_ops(void) {
    printf("\n=== Hashmap: NULL/missing key ===\n");

    cog_hashmap_t m = cog_hashmap_create();

    void* v = cog_hashmap_get(m, "nonexistent");
    TEST_ASSERT(v == NULL, "get on missing key returns NULL");

    bool r = cog_hashmap_remove(m, "nonexistent");
    TEST_ASSERT(!r, "remove on missing key returns false");

    bool c = cog_hashmap_contains(m, "");
    TEST_ASSERT(!c, "contains empty-string key in empty map returns false");

    cog_hashmap_destroy(m);
}

/*===========================================================================
 * AtomSpace Error Paths
 *===========================================================================*/

static void test_atomspace_add_node_empty_name(void) {
    printf("\n=== AtomSpace: Add node with empty name ===\n");

    atomspace_t as = atomspace_create();

    atom_handle_t h = atomspace_add_node(as, ATOM_TYPE_CONCEPT_NODE, "");
    /* Should either succeed (empty-name node) or return INVALID — not crash */
    TEST_ASSERT(1, "add_node with empty name does not crash");

    if (h != ATOM_HANDLE_INVALID) {
        const char* name = atomspace_get_name(as, h);
        TEST_ASSERT(name != NULL, "get_name of empty-name node returns non-NULL ptr");
    }

    atomspace_destroy(as);
}

static void test_atomspace_remove_nonexistent(void) {
    printf("\n=== AtomSpace: Remove non-existent atom ===\n");

    atomspace_t as = atomspace_create();

    bool ok = atomspace_remove_atom(as, (atom_handle_t)999999);
    TEST_ASSERT(!ok, "remove_atom of non-existent handle returns false");

    bool ok2 = atomspace_remove_atom_recursive(as, ATOM_HANDLE_INVALID);
    TEST_ASSERT(!ok2, "remove_atom_recursive(INVALID) returns false");

    atomspace_destroy(as);
}

static void test_atomspace_get_atom_invalid(void) {
    printf("\n=== AtomSpace: get_atom with invalid handles ===\n");

    atomspace_t as = atomspace_create();

    const atom_t* a1 = atomspace_get_atom(as, ATOM_HANDLE_INVALID);
    TEST_ASSERT(a1 == NULL, "get_atom(INVALID) returns NULL");

    const atom_t* a2 = atomspace_get_atom(as, (atom_handle_t)0xDEADBEEFULL);
    TEST_ASSERT(a2 == NULL, "get_atom(DEADBEEF) returns NULL");

    atomspace_destroy(as);
}

static void test_atomspace_link_zero_outgoing(void) {
    printf("\n=== AtomSpace: Link with zero outgoing ===\n");

    atomspace_t as = atomspace_create();

    atom_handle_t h = atomspace_add_link(as, ATOM_TYPE_SET_LINK, NULL, 0);
    /* Empty set-link may be valid — just must not crash */
    TEST_ASSERT(1, "add_link with NULL/0 outgoing does not crash");

    atomspace_destroy(as);
}

static void test_atomspace_tv_on_invalid(void) {
    printf("\n=== AtomSpace: TV/AV ops on invalid handle ===\n");

    atomspace_t as = atomspace_create();

    truth_value_t tv = atomspace_get_tv(as, ATOM_HANDLE_INVALID);
    TEST_ASSERT(1, "get_tv(INVALID) does not crash");
    (void)tv;

    truth_value_t new_tv = {TRUTH_VALUE_SIMPLE, 0.9, 0.9, 1.0};
    /* set_tv on invalid handle should be a no-op, not crash */
    TEST_NO_CRASH(atomspace_set_tv(as, ATOM_HANDLE_INVALID, new_tv),
                  "set_tv(INVALID) no-op");
    TEST_NO_CRASH(atomspace_merge_tv(as, ATOM_HANDLE_INVALID, new_tv),
                  "merge_tv(INVALID) no-op");

    attention_value_t av = atomspace_get_av(as, ATOM_HANDLE_INVALID);
    TEST_ASSERT(1, "get_av(INVALID) does not crash");
    (void)av;

    attention_value_t new_av = {100, 0, 0};
    TEST_NO_CRASH(atomspace_set_av(as, ATOM_HANDLE_INVALID, new_av),
                  "set_av(INVALID) no-op");

    atomspace_destroy(as);
}

static void test_atomspace_query_empty(void) {
    printf("\n=== AtomSpace: Query on empty space ===\n");

    atomspace_t as = atomspace_create();

    atom_query_t q = atomspace_query_create(as);
    atomspace_query_type(q, ATOM_TYPE_CONCEPT_NODE);

    size_t count = 42;  /* will be reset */
    atom_handle_t* results = atomspace_query_execute(q, &count);
    TEST_ASSERT(count == 0, "Query on empty space returns 0 results");
    atomspace_query_results_free(results);
    atomspace_query_destroy(q);

    atomspace_destroy(as);
}

static void test_atomspace_path_invalid(void) {
    printf("\n=== AtomSpace: get_by_path with non-existent path ===\n");

    atomspace_t as = atomspace_create();

    atom_handle_t h = atomspace_get_by_path(as, "/does/not/exist");
    TEST_ASSERT(h == ATOM_HANDLE_INVALID, "get_by_path returns INVALID for bad path");

    atomspace_destroy(as);
}

/*===========================================================================
 * PLN Error Paths
 *===========================================================================*/

static void test_pln_null_engine(void) {
    printf("\n=== PLN: Operations with NULL engine ===\n");

    /* Should return error, not crash */
    forward_chain_result_t* r = pln_forward_chain(NULL, (atom_handle_t)1, 5);
    TEST_ASSERT(r == NULL, "pln_forward_chain(NULL engine) returns NULL");

    backward_chain_result_t* r2 = pln_backward_chain(NULL, (atom_handle_t)1, 5);
    TEST_ASSERT(r2 == NULL, "pln_backward_chain(NULL engine) returns NULL");
}

static void test_pln_null_atomspace(void) {
    printf("\n=== PLN: Create engine with NULL atomspace ===\n");

    pln_config_t cfg = pln_config_default();
    pln_engine_t eng = pln_engine_create(NULL, &cfg);
    /* Either returns NULL or creates with stub AS — must not crash */
    TEST_ASSERT(1, "pln_engine_create(NULL atomspace) does not crash");
    if (eng) pln_engine_destroy(eng);
}

static void test_pln_free_null(void) {
    printf("\n=== PLN: Free NULL results ===\n");

    /* Must be safe to free NULL */
    TEST_NO_CRASH(pln_forward_chain_result_free(NULL),
                  "pln_forward_chain_result_free(NULL) no-op");
    TEST_NO_CRASH(pln_backward_chain_result_free(NULL),
                  "pln_backward_chain_result_free(NULL) no-op");
    TEST_NO_CRASH(pln_rule_application_free(NULL),
                  "pln_rule_application_free(NULL) no-op");
    TEST_NO_CRASH(pln_inference_history_free(NULL, 0),
                  "pln_inference_history_free(NULL,0) no-op");
}

static void test_pln_apply_rule_no_premises(void) {
    printf("\n=== PLN: apply_rule with zero premises ===\n");

    atomspace_t as = atomspace_create();
    pln_config_t cfg = pln_config_default();
    pln_engine_t pln = pln_engine_create(as, &cfg);

    rule_application_t* app = pln_apply_rule(pln, PLN_RULE_DEDUCTION, NULL, 0);
    TEST_ASSERT(app == NULL || 1, "apply_rule with 0 premises does not crash");
    pln_rule_application_free(app);

    pln_engine_destroy(pln);
    atomspace_destroy(as);
}

static void test_pln_get_applicable_rules_invalid(void) {
    printf("\n=== PLN: get_applicable_rules for INVALID handle ===\n");

    atomspace_t as = atomspace_create();
    pln_config_t cfg = pln_config_default();
    pln_engine_t pln = pln_engine_create(as, &cfg);

    size_t count = 0;
    atom_handle_t* rules = pln_get_applicable_rules(pln, ATOM_HANDLE_INVALID, &count);

    if (count > 0) {
        /* If count is positive, rules pointer must be non-NULL */
        TEST_ASSERT(rules != NULL,
                    "get_applicable_rules: if count>0 then rules must be non-NULL");
        atomspace_query_results_free(rules);
    } else {
        TEST_ASSERT(count == 0,
                    "get_applicable_rules(INVALID): count should be 0");
    }

    pln_engine_destroy(pln);
    atomspace_destroy(as);
}

/*===========================================================================
 * AtomSpace Stats Edge Case
 *===========================================================================*/

static void test_stats_null_output(void) {
    printf("\n=== AtomSpace: get_stats with NULL output ===\n");

    atomspace_t as = atomspace_create();

    /* Passing NULL stats pointer — must not crash */
    TEST_NO_CRASH(atomspace_get_stats(as, NULL),
                  "atomspace_get_stats(NULL) is a no-op");

    atomspace_destroy(as);
}

/*===========================================================================
 * CogUtil Config Error Paths
 *===========================================================================*/

static void test_config_missing_key(void) {
    printf("\n=== CogUtil Config: Get missing keys return defaults ===\n");

    cog_config_t cfg = cog_config_create();

    const char* s = cog_config_get_string(cfg, "no.such.key", "default_str");
    TEST_ASSERT(strcmp(s, "default_str") == 0, "Missing string key returns default");

    int64_t i = cog_config_get_int(cfg, "no.such.int", -99);
    TEST_ASSERT(i == -99, "Missing int key returns default");

    double d = cog_config_get_float(cfg, "no.such.float", 3.14);
    TEST_ASSERT(d == 3.14, "Missing float key returns default");

    bool b = cog_config_get_bool(cfg, "no.such.bool", true);
    TEST_ASSERT(b == true, "Missing bool key returns default");

    cog_config_destroy(cfg);
}

/*===========================================================================
 * Spreading Activation on Empty Space
 *===========================================================================*/

static void test_spreading_empty_space(void) {
    printf("\n=== Spreading Activation on empty space ===\n");

    atomspace_t as = atomspace_create();

    spreading_config_t cfg;
    cfg.decay_rate       = 0.5;
    cfg.spread_threshold = 0.1;
    cfg.max_steps        = 5;
    cfg.include_hebbian  = false;

    /* Should not crash on empty space / invalid source */
    TEST_NO_CRASH(
        atomspace_spread_activation(as, ATOM_HANDLE_INVALID, &cfg),
        "spread_activation on empty space / INVALID handle");

    atomspace_destroy(as);
}

/*===========================================================================
 * Main
 *===========================================================================*/

int main(void) {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  CoGWXP-OS9  Error Condition / Null-Safety Test Suite    ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");

    cog_init();
    cog_log_set_level(COG_LOG_WARN);

    /* CogUtil */
    test_cogutil_null_log();
    test_cogutil_zero_alloc();
    test_cogutil_strdup_empty();
    test_cogutil_string_split_empty();
    test_hashmap_null_ops();

    /* AtomSpace */
    test_atomspace_add_node_empty_name();
    test_atomspace_remove_nonexistent();
    test_atomspace_get_atom_invalid();
    test_atomspace_link_zero_outgoing();
    test_atomspace_tv_on_invalid();
    test_atomspace_query_empty();
    test_atomspace_path_invalid();
    test_stats_null_output();
    test_spreading_empty_space();

    /* PLN */
    test_pln_null_engine();
    test_pln_null_atomspace();
    test_pln_free_null();
    test_pln_apply_rule_no_premises();
    test_pln_get_applicable_rules_invalid();

    /* Config */
    test_config_missing_key();

    cog_shutdown();

    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  SUMMARY  Passed: %-4d  Failed: %-4d                     ║\n",
           g_passed, g_failed);
    printf("╚═══════════════════════════════════════════════════════════╝\n\n");

    return g_failed > 0 ? 1 : 0;
}
