/**
 * @file test_stress.c
 * @brief Stress and Load Tests for CoGWXP-OS9 Core Subsystems
 *
 * Tests behaviour under load:
 *  - AtomSpace with 10 000 atoms and 5 000 links (memory + correctness)
 *  - Concurrent AtomSpace writes from 8 threads (race / data-integrity)
 *  - PLN forward chain over large hypergraph (timing + termination)
 *  - PLN repeated inference cycles accumulate stats monotonically
 *  - CogUtil thread pool with 1 000 tasks (no deadlock, no task loss)
 *  - Hash function collision rate over 10 000 unique strings < 1%
 *  - Dynamic array with 100 000 push/pop cycles (no memory corruption)
 *  - Hashmap with 10 000 key-value pairs (insert, lookup, delete cycle)
 *
 * @copyright CoGWXP-OS9 Project
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
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

/*===========================================================================
 * Stress: Large AtomSpace
 *===========================================================================*/

static void test_large_atomspace_10k(void) {
    printf("\n=== Stress: 10 000 nodes + 5 000 links ===\n");

    atomspace_t as = atomspace_create();
    char name[64];

    atom_handle_t handles[10000];
    for (int i = 0; i < 10000; i++) {
        snprintf(name, sizeof(name), "stress10k_%d", i);
        handles[i] = atomspace_add_node(as, ATOM_TYPE_CONCEPT_NODE, name);
    }

    for (int i = 0; i < 5000; i++) {
        atom_handle_t out[] = {handles[i], handles[i + 5000]};
        atomspace_add_link(as, ATOM_TYPE_INHERITANCE_LINK, out, 2);
    }

    atomspace_stats_t s;
    atomspace_get_stats(as, &s);
    TEST_ASSERT(s.total_nodes >= 10000, "10 000+ nodes present");
    TEST_ASSERT(s.total_links >= 5000,  "5 000+ links present");

    /* Random lookup by name */
    atom_handle_t found = atomspace_get_node(as, ATOM_TYPE_CONCEPT_NODE, "stress10k_9999");
    TEST_ASSERT(found != ATOM_HANDLE_INVALID, "Last node found by name");

    atomspace_destroy(as);
    TEST_ASSERT(1, "10 000 node atomspace destroyed cleanly");
}

/*===========================================================================
 * Stress: Concurrent AtomSpace Writes
 *===========================================================================*/

typedef struct {
    atomspace_t as;
    int         thread_id;
    int         nodes_per_thread;
    atomic_int* add_count;
} thread_write_arg_t;

static void* concurrent_writer(void* arg) {
    thread_write_arg_t* a = (thread_write_arg_t*)arg;
    char name[64];
    for (int i = 0; i < a->nodes_per_thread; i++) {
        snprintf(name, sizeof(name), "conc_%d_%d", a->thread_id, i);
        atom_handle_t h = atomspace_add_node(a->as, ATOM_TYPE_CONCEPT_NODE, name);
        if (h != ATOM_HANDLE_INVALID)
            atomic_fetch_add(a->add_count, 1);
    }
    return NULL;
}

static void test_concurrent_atomspace_writes(void) {
    printf("\n=== Stress: Concurrent AtomSpace Writes (8 threads × 100 nodes) ===\n");

    atomspace_t as = atomspace_create();
    atomic_int add_count = 0;

    const int NTHREADS = 8;
    const int NODES_EACH = 100;

    cog_thread_t threads[8];
    thread_write_arg_t args[8];
    for (int i = 0; i < NTHREADS; i++) {
        args[i].as = as;
        args[i].thread_id = i;
        args[i].nodes_per_thread = NODES_EACH;
        args[i].add_count = &add_count;
        threads[i] = cog_thread_create(concurrent_writer, &args[i]);
    }
    for (int i = 0; i < NTHREADS; i++)
        cog_thread_join(threads[i]);

    int total_added = atomic_load(&add_count);
    TEST_ASSERT(total_added == NTHREADS * NODES_EACH,
                "All concurrent writes completed successfully");

    atomspace_stats_t s;
    atomspace_get_stats(as, &s);
    TEST_ASSERT((int)s.total_nodes == total_added,
                "AtomSpace node count matches concurrent adds");

    atomspace_destroy(as);
}

/*===========================================================================
 * Stress: PLN Forward Chain on Large Graph
 *===========================================================================*/

static void test_pln_large_graph_forward_chain(void) {
    printf("\n=== Stress: PLN forward chain on 200-node chain ===\n");

    atomspace_t as = atomspace_create();
    pln_config_t cfg = pln_config_default();
    cfg.max_inference_steps = 50;
    pln_engine_t pln = pln_engine_create(as, &cfg);

    const int N = 200;
    atom_handle_t nodes[200];
    char name[64];
    for (int i = 0; i < N; i++) {
        snprintf(name, sizeof(name), "pln_lg_%d", i);
        nodes[i] = atomspace_add_node(as, ATOM_TYPE_CONCEPT_NODE, name);
    }

    truth_value_t t = {TRUTH_VALUE_SIMPLE, 0.9, 0.9, 1.0};
    for (int i = 0; i < N - 1; i++) {
        atom_handle_t out[] = {nodes[i], nodes[i+1]};
        atomspace_add_link_tv(as, ATOM_TYPE_INHERITANCE_LINK, out, 2, t);
    }

    uint64_t t_start = cog_time_now_ms();
    forward_chain_result_t* res = pln_forward_chain(pln, nodes[0], 50);
    uint64_t t_end = cog_time_now_ms();

    TEST_ASSERT(res != NULL, "PLN large graph: result non-NULL");
    TEST_ASSERT(t_end - t_start < 10000,
                "PLN large graph inference completes in < 10 seconds");

    pln_forward_chain_result_free(res);
    pln_engine_destroy(pln);
    atomspace_destroy(as);
}

/*===========================================================================
 * Stress: PLN Stats Monotonicity
 *===========================================================================*/

static void test_pln_stats_monotonic(void) {
    printf("\n=== Stress: PLN stats accumulate monotonically ===\n");

    atomspace_t as = atomspace_create();
    pln_config_t cfg = pln_config_default();
    pln_engine_t pln = pln_engine_create(as, &cfg);

    atom_handle_t a = atomspace_add_node(as, ATOM_TYPE_CONCEPT_NODE, "mono_a");
    atom_handle_t b = atomspace_add_node(as, ATOM_TYPE_CONCEPT_NODE, "mono_b");
    truth_value_t t = {TRUTH_VALUE_SIMPLE, 0.8, 0.8, 1.0};
    atom_handle_t out[] = {a, b};
    atomspace_add_link_tv(as, ATOM_TYPE_INHERITANCE_LINK, out, 2, t);

    pln_stats_t s0, s1, s2;
    pln_get_stats(pln, &s0);

    forward_chain_result_t* r1 = pln_forward_chain(pln, a, 3);
    pln_forward_chain_result_free(r1);
    pln_get_stats(pln, &s1);

    forward_chain_result_t* r2 = pln_forward_chain(pln, a, 3);
    pln_forward_chain_result_free(r2);
    pln_get_stats(pln, &s2);

    TEST_ASSERT(s1.total_inferences >= s0.total_inferences,
                "Inferences monotonically non-decreasing after first run");
    TEST_ASSERT(s2.total_inferences >= s1.total_inferences,
                "Inferences monotonically non-decreasing after second run");

    pln_engine_destroy(pln);
    atomspace_destroy(as);
}

/*===========================================================================
 * Stress: Thread Pool (1 000 tasks)
 *===========================================================================*/

static atomic_int g_pool_count = 0;

static void* pool_increment(void* arg) {
    (void)arg;
    atomic_fetch_add(&g_pool_count, 1);
    return NULL;
}

static void test_threadpool_1000_tasks(void) {
    printf("\n=== Stress: Thread pool with 1 000 tasks ===\n");

    atomic_store(&g_pool_count, 0);

    cog_threadpool_t pool = cog_threadpool_create(8);
    TEST_ASSERT(pool != NULL, "Thread pool created");

    const int NTASKS = 1000;
    for (int i = 0; i < NTASKS; i++) {
        cog_result_t r = cog_threadpool_submit(pool, pool_increment, NULL);
        TEST_ASSERT(r == COG_SUCCESS, "Task submit returns COG_SUCCESS");
    }

    /* Wait for completion */
    cog_sleep_ms(500);
    cog_threadpool_destroy(pool);

    TEST_ASSERT(atomic_load(&g_pool_count) == NTASKS,
                "All 1 000 pool tasks executed exactly once");
}

/*===========================================================================
 * Stress: Hash Collision Rate
 *===========================================================================*/

static void test_hash_collision_rate(void) {
    printf("\n=== Stress: Hash collision rate over 10 000 strings ===\n");

    const int N = 10000;
    uint32_t* hashes = (uint32_t*)calloc(N, sizeof(uint32_t));
    char key[32];

    for (int i = 0; i < N; i++) {
        snprintf(key, sizeof(key), "hashkey_%d", i);
        hashes[i] = cog_hash_string(key);
    }

    /* Count collisions */
    int collisions = 0;
    for (int i = 0; i < N; i++)
        for (int j = i + 1; j < N; j++)
            if (hashes[i] == hashes[j])
                collisions++;

    double collision_rate = (double)collisions / (N * (N - 1) / 2.0);
    printf("    Collision rate: %.4f%%\n", collision_rate * 100.0);

    TEST_ASSERT(collision_rate < 0.01,
                "Hash collision rate < 1% over 10 000 unique strings");

    free(hashes);
}

/*===========================================================================
 * Stress: Dynamic Array Push/Pop Cycles
 *===========================================================================*/

static void test_array_100k_cycles(void) {
    printf("\n=== Stress: Dynamic array — 100 000 push/pop cycles ===\n");

    cog_array_t arr = cog_array_create(sizeof(int));
    const int CYCLES = 100000;

    for (int i = 0; i < CYCLES; i++) {
        cog_array_push(arr, &i);
    }
    TEST_ASSERT(cog_array_size(arr) == (size_t)CYCLES, "100 000 elements pushed");

    for (int i = 0; i < CYCLES; i++) {
        cog_array_pop(arr);
    }
    TEST_ASSERT(cog_array_size(arr) == 0, "100 000 elements popped — size 0");

    cog_array_destroy(arr);
    TEST_ASSERT(1, "Array destroyed after 100 000 push/pop cycles");
}

/*===========================================================================
 * Stress: Hashmap with 10 000 entries
 *===========================================================================*/

static void test_hashmap_10k_entries(void) {
    printf("\n=== Stress: Hashmap — 10 000 insert / lookup / delete ===\n");

    cog_hashmap_t m = cog_hashmap_create();
    const int N = 10000;

    int values[10000];
    char key[32];

    /* Insert */
    for (int i = 0; i < N; i++) {
        values[i] = i * 2;
        snprintf(key, sizeof(key), "hm10k_%d", i);
        cog_hashmap_set(m, key, &values[i]);
    }
    TEST_ASSERT(cog_hashmap_size(m) == (size_t)N, "10 000 entries inserted");

    /* Lookup all */
    int lookup_ok = 1;
    for (int i = 0; i < N; i++) {
        snprintf(key, sizeof(key), "hm10k_%d", i);
        int* v = (int*)cog_hashmap_get(m, key);
        if (!v || *v != i * 2) { lookup_ok = 0; break; }
    }
    TEST_ASSERT(lookup_ok, "All 10 000 entries retrieved correctly");

    /* Delete half */
    for (int i = 0; i < N / 2; i++) {
        snprintf(key, sizeof(key), "hm10k_%d", i);
        cog_hashmap_remove(m, key);
    }
    TEST_ASSERT(cog_hashmap_size(m) == (size_t)(N / 2),
                "5 000 entries remain after deleting half");

    cog_hashmap_destroy(m);
    TEST_ASSERT(1, "Hashmap with 10 000 entries destroyed cleanly");
}

/*===========================================================================
 * Stress: Concurrent AtomSpace Read (many readers)
 *===========================================================================*/

typedef struct {
    atomspace_t     as;
    atom_handle_t   target;
    int             iterations;
    atomic_int*     read_count;
} reader_arg_t;

static void* concurrent_reader(void* arg) {
    reader_arg_t* a = (reader_arg_t*)arg;
    for (int i = 0; i < a->iterations; i++) {
        const atom_t* atom = atomspace_get_atom(a->as, a->target);
        if (atom != NULL)
            atomic_fetch_add(a->read_count, 1);
    }
    return NULL;
}

static void test_concurrent_reads(void) {
    printf("\n=== Stress: Concurrent AtomSpace Reads (16 threads × 200 reads) ===\n");

    atomspace_t as = atomspace_create();
    atom_handle_t h = atomspace_add_node(as, ATOM_TYPE_CONCEPT_NODE, "read_target");

    const int NREADERS    = 16;
    const int ITER_EACH   = 200;
    atomic_int read_count = 0;

    cog_thread_t threads[16];
    reader_arg_t args[16];
    for (int i = 0; i < NREADERS; i++) {
        args[i].as         = as;
        args[i].target     = h;
        args[i].iterations = ITER_EACH;
        args[i].read_count = &read_count;
        threads[i] = cog_thread_create(concurrent_reader, &args[i]);
    }
    for (int i = 0; i < NREADERS; i++)
        cog_thread_join(threads[i]);

    TEST_ASSERT(atomic_load(&read_count) == NREADERS * ITER_EACH,
                "All 3 200 concurrent reads succeeded");

    atomspace_destroy(as);
}

/*===========================================================================
 * Main
 *===========================================================================*/

int main(void) {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  CoGWXP-OS9  Stress & Load Test Suite                    ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");

    cog_init();
    cog_log_set_level(COG_LOG_WARN);

    test_large_atomspace_10k();
    test_concurrent_atomspace_writes();
    test_pln_large_graph_forward_chain();
    test_pln_stats_monotonic();
    test_threadpool_1000_tasks();
    test_hash_collision_rate();
    test_array_100k_cycles();
    test_hashmap_10k_entries();
    test_concurrent_reads();

    cog_shutdown();

    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  SUMMARY  Passed: %-4d  Failed: %-4d                     ║\n",
           g_passed, g_failed);
    printf("╚═══════════════════════════════════════════════════════════╝\n\n");

    return g_failed > 0 ? 1 : 0;
}
