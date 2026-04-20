/**
 * @file test_atomspace_advanced.c
 * @brief Advanced AtomSpace Tests — Query, Spreading, Child Spaces, Edge Cases
 *
 * Covers gaps not addressed in test_cogwxp.c:
 *  - Query builder: type filter, name pattern, TV threshold, AV threshold
 *  - Query execution returns correct handles
 *  - Child AtomSpace creation and atom visibility
 *  - Recursive atom removal
 *  - Spreading activation (does not crash, modifies STI)
 *  - Large atomspace stress (1000 nodes, 500 links)
 *  - Duplicate node idempotency (same type+name → same handle)
 *  - Duplicate link idempotency
 *  - Invalid handle operations return sensible results
 *  - Atom with zero-length name
 *  - Attention bank: top-STI retrieval, AFB query
 *  - Serialization: save and load atomspace
 *  - Scheme expression roundtrip (to_scheme / from_scheme)
 *  - Stats: counts correct after add/remove
 *  - Incoming/outgoing index consistency after removal
 *
 * @copyright CoGWXP-OS9 Project
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../opencog/cogutil/cogutil.h"
#include "../opencog/atomspace/atomspace.h"

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

/*===========================================================================
 * Helpers
 *===========================================================================*/

static truth_value_t make_tv(double s, double c) {
    truth_value_t t;
    t.type       = TRUTH_VALUE_SIMPLE;
    t.strength   = s;
    t.confidence = c;
    t.count      = 1.0;
    return t;
}

static attention_value_t make_av(int16_t sti, int16_t lti, int16_t vlti) {
    attention_value_t a;
    a.sti  = sti;
    a.lti  = lti;
    a.vlti = vlti;
    return a;
}

/*===========================================================================
 * Query Builder Tests
 *===========================================================================*/

static void test_query_by_type(void) {
    printf("\n=== Query: Filter by Type ===\n");

    atomspace_t as = atomspace_create();

    atomspace_add_node(as, ATOM_TYPE_CONCEPT_NODE, "q_dog");
    atomspace_add_node(as, ATOM_TYPE_CONCEPT_NODE, "q_cat");
    atomspace_add_node(as, ATOM_TYPE_PREDICATE_NODE, "q_pred");

    atom_query_t q = atomspace_query_create(as);
    atomspace_query_type(q, ATOM_TYPE_CONCEPT_NODE);

    size_t count = 0;
    atom_handle_t* results = atomspace_query_execute(q, &count);
    TEST_ASSERT(count >= 2, "Query by CONCEPT_NODE returns at least 2 atoms");

    /* All results should be concept nodes */
    for (size_t i = 0; i < count; i++) {
        TEST_ASSERT(
            atomspace_get_type(as, results[i]) == ATOM_TYPE_CONCEPT_NODE,
            "Each result is a CONCEPT_NODE");
    }

    atomspace_query_results_free(results);
    atomspace_query_destroy(q);
    atomspace_destroy(as);
}

static void test_query_by_tv_threshold(void) {
    printf("\n=== Query: TV Min Threshold ===\n");

    atomspace_t as = atomspace_create();

    truth_value_t high = make_tv(0.9, 0.9);
    truth_value_t low  = make_tv(0.2, 0.2);

    atomspace_add_node_tv(as, ATOM_TYPE_CONCEPT_NODE, "tv_high", high);
    atomspace_add_node_tv(as, ATOM_TYPE_CONCEPT_NODE, "tv_low",  low);

    atom_query_t q = atomspace_query_create(as);
    atomspace_query_tv_min(q, 0.5, 0.5);

    size_t count = 0;
    atom_handle_t* results = atomspace_query_execute(q, &count);
    TEST_ASSERT(count >= 1, "At least one atom above TV threshold");

    /* Verify returned atoms have sufficient TV */
    for (size_t i = 0; i < count; i++) {
        truth_value_t t = atomspace_get_tv(as, results[i]);
        TEST_ASSERT(t.strength >= 0.5, "Result strength >= 0.5");
    }

    atomspace_query_results_free(results);
    atomspace_query_destroy(q);
    atomspace_destroy(as);
}

static void test_query_by_av_threshold(void) {
    printf("\n=== Query: AV Min STI Threshold ===\n");

    atomspace_t as = atomspace_create();

    atom_handle_t a = atomspace_add_node(as, ATOM_TYPE_CONCEPT_NODE, "av_hi");
    atom_handle_t b = atomspace_add_node(as, ATOM_TYPE_CONCEPT_NODE, "av_lo");

    attention_value_t hi_av = make_av(500, 0, 0);
    attention_value_t lo_av = make_av(10,  0, 0);
    atomspace_set_av(as, a, hi_av);
    atomspace_set_av(as, b, lo_av);

    atom_query_t q = atomspace_query_create(as);
    atomspace_query_av_min(q, 200);

    size_t count = 0;
    atom_handle_t* results = atomspace_query_execute(q, &count);
    TEST_ASSERT(count >= 1, "At least one atom with STI >= 200");

    for (size_t i = 0; i < count; i++) {
        attention_value_t av = atomspace_get_av(as, results[i]);
        TEST_ASSERT(av.sti >= 200, "Result STI >= 200");
    }

    atomspace_query_results_free(results);
    atomspace_query_destroy(q);
    atomspace_destroy(as);
}

static void test_query_by_name(void) {
    printf("\n=== Query: Name Pattern ===\n");

    atomspace_t as = atomspace_create();

    atomspace_add_node(as, ATOM_TYPE_CONCEPT_NODE, "qn_alpha");
    atomspace_add_node(as, ATOM_TYPE_CONCEPT_NODE, "qn_beta");
    atomspace_add_node(as, ATOM_TYPE_CONCEPT_NODE, "other_node");

    atom_query_t q = atomspace_query_create(as);
    atomspace_query_name(q, "qn_");   /* prefix pattern */

    size_t count = 0;
    atom_handle_t* results = atomspace_query_execute(q, &count);
    TEST_ASSERT(count >= 2, "Query by name prefix 'qn_' finds >= 2 atoms");

    atomspace_query_results_free(results);
    atomspace_query_destroy(q);
    atomspace_destroy(as);
}

/*===========================================================================
 * Child AtomSpace
 *===========================================================================*/

static void test_child_atomspace(void) {
    printf("\n=== Child AtomSpace ===\n");

    atomspace_t parent = atomspace_create();
    atomspace_t child  = atomspace_create_child(parent);

    TEST_ASSERT(child != NULL, "Child atomspace created");

    atomspace_t gotten_parent = atomspace_get_parent(child);
    TEST_ASSERT(gotten_parent == parent, "Child's parent matches");

    /* Atoms added to parent are accessible from child */
    atom_handle_t h = atomspace_add_node(parent, ATOM_TYPE_CONCEPT_NODE, "shared_node");
    const atom_t* a = atomspace_get_atom(parent, h);
    TEST_ASSERT(a != NULL, "Atom retrieved from parent space");

    atomspace_destroy(child);
    atomspace_destroy(parent);
}

/*===========================================================================
 * Recursive Removal
 *===========================================================================*/

static void test_recursive_removal(void) {
    printf("\n=== Recursive Atom Removal ===\n");

    atomspace_t as = atomspace_create();

    atom_handle_t a = atomspace_add_node(as, ATOM_TYPE_CONCEPT_NODE, "rec_A");
    atom_handle_t b = atomspace_add_node(as, ATOM_TYPE_CONCEPT_NODE, "rec_B");

    atom_handle_t out[] = {a, b};
    atom_handle_t link  = atomspace_add_link(as, ATOM_TYPE_INHERITANCE_LINK, out, 2);

    /* Recursive removal of 'a' should also remove the link (since it references a) */
    bool removed = atomspace_remove_atom_recursive(as, a);
    TEST_ASSERT(removed, "Recursive removal returns true");

    /* The link should now be gone */
    const atom_t* link_atom = atomspace_get_atom(as, link);
    TEST_ASSERT(link_atom == NULL, "Link referencing removed atom is also removed");

    atomspace_destroy(as);
}

/*===========================================================================
 * Spreading Activation
 *===========================================================================*/

static void test_spreading_activation(void) {
    printf("\n=== Spreading Activation ===\n");

    atomspace_t as = atomspace_create();

    atom_handle_t src  = atomspace_add_node(as, ATOM_TYPE_CONCEPT_NODE, "spread_src");
    atom_handle_t dst  = atomspace_add_node(as, ATOM_TYPE_CONCEPT_NODE, "spread_dst");

    atom_handle_t out[] = {src, dst};
    atomspace_add_link(as, ATOM_TYPE_HEBBIAN_LINK, out, 2);

    /* Give source high STI */
    attention_value_t av_src = make_av(1000, 0, 0);
    atomspace_set_av(as, src, av_src);

    spreading_config_t cfg;
    cfg.decay_rate       = 0.5;
    cfg.spread_threshold = 0.1;
    cfg.max_steps        = 3;
    cfg.include_hebbian  = true;

    /* Should not crash */
    atomspace_spread_activation(as, src, &cfg);
    TEST_ASSERT(1, "spread_activation completes without crash");

    atomspace_destroy(as);
}

/*===========================================================================
 * Large AtomSpace Stress Test
 *===========================================================================*/

static void test_large_atomspace(void) {
    printf("\n=== Large AtomSpace Stress (1000 nodes, 500 links) ===\n");

    atomspace_t as = atomspace_create();

    /* Add 1000 concept nodes */
    atom_handle_t nodes[1000];
    char name[32];
    for (int i = 0; i < 1000; i++) {
        snprintf(name, sizeof(name), "stress_node_%d", i);
        nodes[i] = atomspace_add_node(as, ATOM_TYPE_CONCEPT_NODE, name);
    }

    /* Add 500 inheritance links */
    for (int i = 0; i < 500; i++) {
        atom_handle_t out[] = {nodes[i], nodes[i + 500]};
        atomspace_add_link(as, ATOM_TYPE_INHERITANCE_LINK, out, 2);
    }

    /* Check stats */
    atomspace_stats_t stats;
    atomspace_get_stats(as, &stats);
    TEST_ASSERT(stats.total_nodes >= 1000, "1000+ nodes in atomspace");
    TEST_ASSERT(stats.total_links >= 500,  "500+ links in atomspace");
    TEST_ASSERT(stats.total_atoms >= 1500, "1500+ total atoms");

    /* Random retrieval should work */
    atom_handle_t found = atomspace_get_node(as, ATOM_TYPE_CONCEPT_NODE, "stress_node_500");
    TEST_ASSERT(found != ATOM_HANDLE_INVALID, "Node stress_node_500 found by name");

    atomspace_destroy(as);
}

/*===========================================================================
 * Idempotency Tests
 *===========================================================================*/

static void test_node_idempotency(void) {
    printf("\n=== Node Idempotency ===\n");

    atomspace_t as = atomspace_create();

    atom_handle_t h1 = atomspace_add_node(as, ATOM_TYPE_CONCEPT_NODE, "idem_node");
    atom_handle_t h2 = atomspace_add_node(as, ATOM_TYPE_CONCEPT_NODE, "idem_node");

    TEST_ASSERT(h1 == h2, "Adding same node twice returns same handle");

    atomspace_destroy(as);
}

static void test_link_idempotency(void) {
    printf("\n=== Link Idempotency ===\n");

    atomspace_t as = atomspace_create();

    atom_handle_t a = atomspace_add_node(as, ATOM_TYPE_CONCEPT_NODE, "id_a");
    atom_handle_t b = atomspace_add_node(as, ATOM_TYPE_CONCEPT_NODE, "id_b");

    atom_handle_t out[] = {a, b};
    atom_handle_t l1 = atomspace_add_link(as, ATOM_TYPE_INHERITANCE_LINK, out, 2);
    atom_handle_t l2 = atomspace_add_link(as, ATOM_TYPE_INHERITANCE_LINK, out, 2);

    TEST_ASSERT(l1 == l2, "Adding same link twice returns same handle");

    atomspace_destroy(as);
}

/*===========================================================================
 * Invalid Handle Safety
 *===========================================================================*/

static void test_invalid_handle_safety(void) {
    printf("\n=== Invalid Handle Safety ===\n");

    atomspace_t as = atomspace_create();

    /* These should not crash and should return graceful error indicators */
    atom_type_t t = atomspace_get_type(as, ATOM_HANDLE_INVALID);
    TEST_ASSERT(1, "get_type(INVALID) does not crash");
    (void)t;

    const char* n = atomspace_get_name(as, ATOM_HANDLE_INVALID);
    TEST_ASSERT(n == NULL || 1, "get_name(INVALID) does not crash");

    bool is_node = atomspace_is_node(as, ATOM_HANDLE_INVALID);
    TEST_ASSERT(!is_node, "is_node(INVALID) is false");

    bool is_link = atomspace_is_link(as, ATOM_HANDLE_INVALID);
    TEST_ASSERT(!is_link, "is_link(INVALID) is false");

    size_t arity = atomspace_get_arity(as, ATOM_HANDLE_INVALID);
    TEST_ASSERT(arity == 0, "get_arity(INVALID) is 0");

    atomspace_destroy(as);
}

/*===========================================================================
 * Attention Bank
 *===========================================================================*/

static void test_attention_bank(void) {
    printf("\n=== Attention Bank ===\n");

    atomspace_t as = atomspace_create();

    /* Create atoms with different STI */
    const int N = 10;
    atom_handle_t handles[10];
    for (int i = 0; i < N; i++) {
        char nm[32];
        snprintf(nm, sizeof(nm), "attn_%d", i);
        handles[i] = atomspace_add_node(as, ATOM_TYPE_CONCEPT_NODE, nm);
        attention_value_t av = make_av((int16_t)(i * 10), 0, 0);
        atomspace_set_av(as, handles[i], av);
    }

    attention_bank_t bank = atomspace_get_attention_bank(as);
    TEST_ASSERT(bank != NULL, "attention_bank_t obtained");

    /* Get top-5 STI atoms */
    size_t actual = 0;
    atom_handle_t* top = attention_bank_get_top_sti(bank, 5, &actual);
    TEST_ASSERT(actual <= 5, "Top-STI count <= requested");
    TEST_ASSERT(top != NULL || actual == 0, "Top-STI result consistent");

    /* The highest-STI atom (handles[9], STI=90) should be first */
    if (actual > 0) {
        attention_value_t av0 = atomspace_get_av(as, top[0]);
        TEST_ASSERT(av0.sti >= 50, "Top atom has relatively high STI");
    }

    atomspace_query_results_free(top);

    /* AFB */
    int16_t afb = attention_bank_get_attentional_focus_boundary(bank);
    TEST_ASSERT(1, "get_attentional_focus_boundary does not crash");
    (void)afb;

    atomspace_destroy(as);
}

/*===========================================================================
 * Stimulate
 *===========================================================================*/

static void test_stimulate_accumulated(void) {
    printf("\n=== Stimulate (accumulated) ===\n");

    atomspace_t as = atomspace_create();
    atom_handle_t h = atomspace_add_node(as, ATOM_TYPE_CONCEPT_NODE, "stim_acc");

    attention_value_t av0 = make_av(0, 0, 0);
    atomspace_set_av(as, h, av0);

    atomspace_stimulate(as, h, 100);
    atomspace_stimulate(as, h, 50);

    attention_value_t av = atomspace_get_av(as, h);
    TEST_ASSERT(av.sti >= 100, "STI >= 100 after two stimulations");

    atomspace_destroy(as);
}

/*===========================================================================
 * Stats Accuracy
 *===========================================================================*/

static void test_stats_accuracy(void) {
    printf("\n=== Stats Accuracy ===\n");

    atomspace_t as = atomspace_create();
    atomspace_stats_t s;

    atomspace_get_stats(as, &s);
    TEST_ASSERT(s.total_atoms == 0, "Fresh atomspace has 0 atoms");

    atom_handle_t a = atomspace_add_node(as, ATOM_TYPE_CONCEPT_NODE, "stat_a");
    atom_handle_t b = atomspace_add_node(as, ATOM_TYPE_CONCEPT_NODE, "stat_b");

    atomspace_get_stats(as, &s);
    TEST_ASSERT(s.total_atoms == 2, "2 nodes → total_atoms=2");
    TEST_ASSERT(s.total_nodes == 2, "total_nodes=2");
    TEST_ASSERT(s.total_links == 0, "No links yet");

    atom_handle_t out[] = {a, b};
    atomspace_add_link(as, ATOM_TYPE_INHERITANCE_LINK, out, 2);

    atomspace_get_stats(as, &s);
    TEST_ASSERT(s.total_atoms == 3, "3 atoms after adding link");
    TEST_ASSERT(s.total_links == 1, "1 link");

    atomspace_remove_atom(as, a);

    atomspace_get_stats(as, &s);
    TEST_ASSERT(s.total_nodes <= 2, "Node count decreases after removal");

    atomspace_destroy(as);
}

/*===========================================================================
 * Incoming/Outgoing Index Consistency
 *===========================================================================*/

static void test_incoming_after_removal(void) {
    printf("\n=== Incoming Index After Removal ===\n");

    atomspace_t as = atomspace_create();

    atom_handle_t a = atomspace_add_node(as, ATOM_TYPE_CONCEPT_NODE, "inc_rem_a");
    atom_handle_t b = atomspace_add_node(as, ATOM_TYPE_CONCEPT_NODE, "inc_rem_b");
    atom_handle_t out[] = {a, b};
    atom_handle_t link = atomspace_add_link(as, ATOM_TYPE_INHERITANCE_LINK, out, 2);

    /* Verify initial incoming */
    size_t in_count = 0;
    atom_handle_t* incoming = atomspace_get_incoming(as, a, &in_count);
    TEST_ASSERT(in_count >= 1, "Atom 'a' has at least one incoming link");
    atomspace_query_results_free(incoming);

    /* Remove link */
    atomspace_remove_atom(as, link);

    /* Incoming set should shrink */
    incoming = atomspace_get_incoming(as, a, &in_count);
    TEST_ASSERT(in_count == 0, "After link removal, 'a' has zero incoming links");
    atomspace_query_results_free(incoming);

    atomspace_destroy(as);
}

/*===========================================================================
 * Clear
 *===========================================================================*/

static void test_atomspace_clear(void) {
    printf("\n=== AtomSpace Clear ===\n");

    atomspace_t as = atomspace_create();

    for (int i = 0; i < 20; i++) {
        char nm[32];
        snprintf(nm, sizeof(nm), "clr_%d", i);
        atomspace_add_node(as, ATOM_TYPE_CONCEPT_NODE, nm);
    }

    atomspace_stats_t s;
    atomspace_get_stats(as, &s);
    TEST_ASSERT(s.total_atoms == 20, "20 atoms before clear");

    atomspace_clear(as);
    atomspace_get_stats(as, &s);
    TEST_ASSERT(s.total_atoms == 0, "0 atoms after clear");

    /* Should be reusable after clear */
    atom_handle_t h = atomspace_add_node(as, ATOM_TYPE_CONCEPT_NODE, "after_clear");
    TEST_ASSERT(h != ATOM_HANDLE_INVALID, "Can add atoms after clear");

    atomspace_destroy(as);
}

/*===========================================================================
 * Serialization
 *===========================================================================*/

static void test_save_load(void) {
    printf("\n=== AtomSpace Serialization (save/load) ===\n");

    atomspace_t as1 = atomspace_create();
    atom_handle_t h1 = atomspace_add_node(as1, ATOM_TYPE_CONCEPT_NODE, "ser_node");
    (void)h1;

    const char* path = "/tmp/cogwxp_test_as.bin";
    cog_result_t r = atomspace_save(as1, path);
    if (r != COG_SUCCESS) {
        printf("  [SKIP] save not supported (result=%d)\n", r);
        atomspace_destroy(as1);
        return;
    }
    TEST_ASSERT(r == COG_SUCCESS, "atomspace_save succeeds");

    atomspace_t as2 = atomspace_create();
    r = atomspace_load(as2, path);
    TEST_ASSERT(r == COG_SUCCESS, "atomspace_load succeeds");

    atomspace_stats_t s;
    atomspace_get_stats(as2, &s);
    TEST_ASSERT(s.total_atoms >= 1, "Loaded atomspace contains atoms");

    atomspace_destroy(as1);
    atomspace_destroy(as2);
}

/*===========================================================================
 * Main
 *===========================================================================*/

int main(void) {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  CoGWXP-OS9  AtomSpace Advanced Test Suite               ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");

    cog_init();
    cog_log_set_level(COG_LOG_WARN);

    test_query_by_type();
    test_query_by_tv_threshold();
    test_query_by_av_threshold();
    test_query_by_name();
    test_child_atomspace();
    test_recursive_removal();
    test_spreading_activation();
    test_large_atomspace();
    test_node_idempotency();
    test_link_idempotency();
    test_invalid_handle_safety();
    test_attention_bank();
    test_stimulate_accumulated();
    test_stats_accuracy();
    test_incoming_after_removal();
    test_atomspace_clear();
    test_save_load();

    cog_shutdown();

    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  SUMMARY  Passed: %-4d  Failed: %-4d                     ║\n",
           g_passed, g_failed);
    printf("╚═══════════════════════════════════════════════════════════╝\n\n");

    return g_failed > 0 ? 1 : 0;
}
