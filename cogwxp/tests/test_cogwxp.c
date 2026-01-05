/**
 * @file test_cogwxp.c
 * @brief Comprehensive Test Suite for CoGWXP-OS9 Components
 * 
 * Tests for CogUtil, AtomSpace, PLN, 9P, and CogW7OS kernel.
 * 
 * @copyright CoGWXP-OS9 Project
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Include all component headers */
#include "../opencog/cogutil/cogutil.h"
#include "../opencog/atomspace/atomspace.h"
#include "../opencog/pln/pln.h"
#include "../plan9/9p/9p.h"
#include "../cogw7os/kernel/cogw7os.h"

/*===========================================================================
 * Test Framework
 *===========================================================================*/

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("  FAIL: %s\n", msg); \
        return 0; \
    } \
} while(0)

#define RUN_TEST(test) do { \
    printf("Running %s...\n", #test); \
    tests_run++; \
    if (test()) { \
        tests_passed++; \
        printf("  PASS\n"); \
    } else { \
        tests_failed++; \
    } \
} while(0)

/*===========================================================================
 * CogUtil Tests
 *===========================================================================*/

static int test_cogutil_init(void) {
    cog_result_t result = cog_init();
    TEST_ASSERT(result == COG_OK, "cog_init() should succeed");
    
    const char* version = cog_version();
    TEST_ASSERT(version != NULL, "cog_version() should return non-NULL");
    TEST_ASSERT(strlen(version) > 0, "Version string should not be empty");
    
    return 1;
}

static int test_cogutil_logging(void) {
    cog_log_set_level(COG_LOG_DEBUG);
    TEST_ASSERT(cog_log_get_level() == COG_LOG_DEBUG, "Log level should be DEBUG");
    
    cog_log_set_level(COG_LOG_INFO);
    TEST_ASSERT(cog_log_get_level() == COG_LOG_INFO, "Log level should be INFO");
    
    /* Test logging (should not crash) */
    COG_LOG_INFO("Test info message");
    COG_LOG_DEBUG("Test debug message");
    COG_LOG_WARN("Test warning message");
    
    return 1;
}

static int test_cogutil_memory(void) {
    /* Test allocation */
    void* ptr = COG_MALLOC(1024);
    TEST_ASSERT(ptr != NULL, "COG_MALLOC should succeed");
    
    /* Test reallocation */
    ptr = COG_REALLOC(ptr, 2048);
    TEST_ASSERT(ptr != NULL, "COG_REALLOC should succeed");
    
    /* Test free */
    COG_FREE(ptr);
    
    /* Test calloc */
    ptr = COG_CALLOC(10, sizeof(int));
    TEST_ASSERT(ptr != NULL, "COG_CALLOC should succeed");
    
    /* Verify zeroed */
    int* arr = (int*)ptr;
    for (int i = 0; i < 10; i++) {
        TEST_ASSERT(arr[i] == 0, "COG_CALLOC should zero memory");
    }
    
    COG_FREE(ptr);
    
    /* Test strdup */
    char* str = COG_STRDUP("Hello, CoGWXP!");
    TEST_ASSERT(str != NULL, "COG_STRDUP should succeed");
    TEST_ASSERT(strcmp(str, "Hello, CoGWXP!") == 0, "COG_STRDUP should copy string");
    COG_FREE(str);
    
    /* Check memory stats */
    size_t allocated, count, peak;
    cog_mem_stats(&allocated, &count, &peak);
    TEST_ASSERT(count == 0, "All allocations should be freed");
    
    return 1;
}

static int test_cogutil_config(void) {
    cog_config_t* config;
    cog_result_t result = cog_config_create(&config);
    TEST_ASSERT(result == COG_OK, "cog_config_create should succeed");
    
    /* Test set/get string */
    result = cog_config_set(config, "test.string", "hello");
    TEST_ASSERT(result == COG_OK, "cog_config_set should succeed");
    
    const char* value = cog_config_get(config, "test.string", "default");
    TEST_ASSERT(strcmp(value, "hello") == 0, "Config value should match");
    
    /* Test default value */
    value = cog_config_get(config, "nonexistent", "default");
    TEST_ASSERT(strcmp(value, "default") == 0, "Should return default for missing key");
    
    /* Test integer */
    cog_config_set(config, "test.int", "42");
    int int_val = cog_config_get_int(config, "test.int", 0);
    TEST_ASSERT(int_val == 42, "Integer config should work");
    
    /* Test boolean */
    cog_config_set(config, "test.bool", "true");
    bool bool_val = cog_config_get_bool(config, "test.bool", false);
    TEST_ASSERT(bool_val == true, "Boolean config should work");
    
    cog_config_destroy(config);
    return 1;
}

static int test_cogutil_uuid(void) {
    cog_uuid_t uuid1, uuid2;
    
    cog_uuid_generate(&uuid1);
    cog_uuid_generate(&uuid2);
    
    TEST_ASSERT(!cog_uuid_equals(&uuid1, &uuid2), "UUIDs should be unique");
    
    /* Test string conversion */
    char str[37];
    cog_uuid_to_string(&uuid1, str);
    TEST_ASSERT(strlen(str) == 36, "UUID string should be 36 chars");
    
    cog_uuid_t parsed;
    TEST_ASSERT(cog_uuid_from_string(&parsed, str), "UUID parsing should succeed");
    TEST_ASSERT(cog_uuid_equals(&uuid1, &parsed), "Parsed UUID should match original");
    
    return 1;
}

static int test_cogutil_task_queue(void) {
    cog_task_queue_t* queue;
    cog_result_t result = cog_task_queue_create(&queue, 10);
    TEST_ASSERT(result == COG_OK, "Task queue creation should succeed");
    
    TEST_ASSERT(cog_task_queue_size(queue) == 0, "Queue should be empty initially");
    
    /* Push some tasks */
    static int counter = 0;
    void dummy_task(void* arg) { counter++; }
    
    for (int i = 0; i < 5; i++) {
        result = cog_task_queue_push(queue, dummy_task, NULL);
        TEST_ASSERT(result == COG_OK, "Push should succeed");
    }
    
    TEST_ASSERT(cog_task_queue_size(queue) == 5, "Queue should have 5 tasks");
    
    /* Pop and execute tasks */
    cog_task_func_t func;
    void* arg;
    for (int i = 0; i < 5; i++) {
        result = cog_task_queue_pop(queue, &func, &arg);
        TEST_ASSERT(result == COG_OK, "Pop should succeed");
        func(arg);
    }
    
    TEST_ASSERT(counter == 5, "All tasks should have executed");
    TEST_ASSERT(cog_task_queue_size(queue) == 0, "Queue should be empty");
    
    cog_task_queue_destroy(queue);
    return 1;
}

/*===========================================================================
 * AtomSpace Tests
 *===========================================================================*/

static int test_atomspace_create(void) {
    atomspace_t as;
    atomspace_config_t config = {0};
    
    cog_result_t result = atomspace_create(&config, &as);
    TEST_ASSERT(result == COG_OK, "AtomSpace creation should succeed");
    TEST_ASSERT(as != NULL, "AtomSpace should not be NULL");
    
    TEST_ASSERT(atomspace_size(as) == 0, "AtomSpace should be empty initially");
    
    atomspace_destroy(as);
    return 1;
}

static int test_atomspace_nodes(void) {
    atomspace_t as;
    atomspace_config_t config = {0};
    atomspace_create(&config, &as);
    
    /* Add concept nodes */
    atom_handle_t cat = atomspace_add_node(as, ATOM_TYPE_CONCEPT, "cat", NULL);
    TEST_ASSERT(cat != ATOM_HANDLE_INVALID, "Adding node should succeed");
    
    atom_handle_t dog = atomspace_add_node(as, ATOM_TYPE_CONCEPT, "dog", NULL);
    TEST_ASSERT(dog != ATOM_HANDLE_INVALID, "Adding second node should succeed");
    TEST_ASSERT(cat != dog, "Different nodes should have different handles");
    
    /* Retrieve node */
    atom_handle_t found = atomspace_get_node(as, ATOM_TYPE_CONCEPT, "cat");
    TEST_ASSERT(found == cat, "Should find existing node");
    
    /* Check type */
    atom_type_t type = atomspace_get_type(as, cat);
    TEST_ASSERT(type == ATOM_TYPE_CONCEPT, "Type should be CONCEPT");
    
    /* Check name */
    const char* name = atomspace_get_name(as, cat);
    TEST_ASSERT(strcmp(name, "cat") == 0, "Name should match");
    
    /* Non-existent node */
    found = atomspace_get_node(as, ATOM_TYPE_CONCEPT, "nonexistent");
    TEST_ASSERT(found == ATOM_HANDLE_INVALID, "Should not find non-existent node");
    
    atomspace_destroy(as);
    return 1;
}

static int test_atomspace_links(void) {
    atomspace_t as;
    atomspace_config_t config = {0};
    atomspace_create(&config, &as);
    
    /* Create nodes */
    atom_handle_t cat = atomspace_add_node(as, ATOM_TYPE_CONCEPT, "cat", NULL);
    atom_handle_t animal = atomspace_add_node(as, ATOM_TYPE_CONCEPT, "animal", NULL);
    
    /* Create inheritance link: cat -> animal */
    atom_handle_t outgoing[] = {cat, animal};
    atom_handle_t link = atomspace_add_link(as, ATOM_TYPE_INHERITANCE, outgoing, 2, NULL);
    TEST_ASSERT(link != ATOM_HANDLE_INVALID, "Adding link should succeed");
    
    /* Retrieve link */
    atom_handle_t found = atomspace_get_link(as, ATOM_TYPE_INHERITANCE, outgoing, 2);
    TEST_ASSERT(found == link, "Should find existing link");
    
    /* Check outgoing set */
    atom_handle_t* out;
    size_t count;
    cog_result_t result = atomspace_get_outgoing(as, link, &out, &count);
    TEST_ASSERT(result == COG_OK, "Getting outgoing should succeed");
    TEST_ASSERT(count == 2, "Outgoing count should be 2");
    TEST_ASSERT(out[0] == cat, "First outgoing should be cat");
    TEST_ASSERT(out[1] == animal, "Second outgoing should be animal");
    COG_FREE(out);
    
    /* Check incoming set */
    atom_handle_t* in;
    result = atomspace_get_incoming(as, cat, &in, &count);
    TEST_ASSERT(result == COG_OK, "Getting incoming should succeed");
    TEST_ASSERT(count == 1, "Incoming count should be 1");
    TEST_ASSERT(in[0] == link, "Incoming should be the link");
    COG_FREE(in);
    
    atomspace_destroy(as);
    return 1;
}

static int test_atomspace_truth_values(void) {
    atomspace_t as;
    atomspace_config_t config = {0};
    atomspace_create(&config, &as);
    
    /* Create node with truth value */
    truth_value_t tv = tv_simple(0.8, 0.9);
    atom_handle_t node = atomspace_add_node(as, ATOM_TYPE_CONCEPT, "test", &tv);
    
    /* Retrieve truth value */
    truth_value_t retrieved;
    cog_result_t result = atomspace_get_tv(as, node, &retrieved);
    TEST_ASSERT(result == COG_OK, "Getting TV should succeed");
    TEST_ASSERT(retrieved.strength == 0.8, "Strength should match");
    TEST_ASSERT(retrieved.confidence == 0.9, "Confidence should match");
    
    /* Update truth value */
    tv = tv_simple(0.5, 0.7);
    result = atomspace_set_tv(as, node, &tv);
    TEST_ASSERT(result == COG_OK, "Setting TV should succeed");
    
    atomspace_get_tv(as, node, &retrieved);
    TEST_ASSERT(retrieved.strength == 0.5, "Updated strength should match");
    
    /* Test TV merge */
    truth_value_t tv1 = tv_simple(0.8, 0.5);
    truth_value_t tv2 = tv_simple(0.6, 0.5);
    truth_value_t merged = tv_merge(&tv1, &tv2);
    TEST_ASSERT(merged.strength > 0.6 && merged.strength < 0.8, "Merged strength should be between inputs");
    
    atomspace_destroy(as);
    return 1;
}

static int test_atomspace_attention_values(void) {
    atomspace_t as;
    atomspace_config_t config = {0};
    atomspace_create(&config, &as);
    
    atom_handle_t node = atomspace_add_node(as, ATOM_TYPE_CONCEPT, "test", NULL);
    
    /* Set attention value */
    attention_value_t av = {.sti = 100, .lti = 50, .vlti = true};
    cog_result_t result = atomspace_set_av(as, node, &av);
    TEST_ASSERT(result == COG_OK, "Setting AV should succeed");
    
    /* Retrieve attention value */
    attention_value_t retrieved;
    result = atomspace_get_av(as, node, &retrieved);
    TEST_ASSERT(result == COG_OK, "Getting AV should succeed");
    TEST_ASSERT(retrieved.sti == 100, "STI should match");
    TEST_ASSERT(retrieved.lti == 50, "LTI should match");
    TEST_ASSERT(retrieved.vlti == true, "VLTI should match");
    
    /* Test stimulation */
    result = atomspace_stimulate(as, node, 10);
    TEST_ASSERT(result == COG_OK, "Stimulation should succeed");
    
    atomspace_get_av(as, node, &retrieved);
    TEST_ASSERT(retrieved.sti == 110, "STI should increase after stimulation");
    
    atomspace_destroy(as);
    return 1;
}

/*===========================================================================
 * PLN Tests
 *===========================================================================*/

static int test_pln_init(void) {
    atomspace_t as;
    atomspace_config_t as_config = {0};
    atomspace_create(&as_config, &as);
    
    pln_context_t pln;
    pln_config_t config = {
        .k = 800.0,
        .max_chain_depth = 10,
        .confidence_threshold = 0.1,
        .enable_backward_chaining = true,
        .enable_forward_chaining = true
    };
    
    cog_result_t result = pln_init(as, &config, &pln);
    TEST_ASSERT(result == COG_OK, "PLN init should succeed");
    TEST_ASSERT(pln != NULL, "PLN context should not be NULL");
    
    pln_shutdown(pln);
    atomspace_destroy(as);
    return 1;
}

static int test_pln_truth_value_operations(void) {
    /* Test AND */
    truth_value_t a = tv_simple(0.8, 0.9);
    truth_value_t b = tv_simple(0.6, 0.8);
    truth_value_t result = pln_tv_and(&a, &b);
    TEST_ASSERT(result.strength < a.strength && result.strength < b.strength,
        "AND strength should be less than both inputs");
    
    /* Test OR */
    result = pln_tv_or(&a, &b);
    TEST_ASSERT(result.strength > a.strength || result.strength > b.strength,
        "OR strength should be greater than at least one input");
    
    /* Test NOT */
    result = pln_tv_not(&a);
    TEST_ASSERT(result.strength == 1.0 - a.strength, "NOT should invert strength");
    TEST_ASSERT(result.confidence == a.confidence, "NOT should preserve confidence");
    
    /* Test revision */
    result = pln_tv_revision(&a, &b);
    TEST_ASSERT(result.confidence > a.confidence || result.confidence > b.confidence,
        "Revision should increase confidence");
    
    return 1;
}

static int test_pln_deduction(void) {
    atomspace_t as;
    atomspace_config_t as_config = {0};
    atomspace_create(&as_config, &as);
    
    pln_context_t pln;
    pln_config_t config = {.k = 800.0, .max_chain_depth = 10};
    pln_init(as, &config, &pln);
    
    /* Create: cat -> animal, animal -> living_thing */
    atom_handle_t cat = atomspace_add_node(as, ATOM_TYPE_CONCEPT, "cat", NULL);
    atom_handle_t animal = atomspace_add_node(as, ATOM_TYPE_CONCEPT, "animal", NULL);
    atom_handle_t living = atomspace_add_node(as, ATOM_TYPE_CONCEPT, "living_thing", NULL);
    
    truth_value_t tv = tv_simple(0.9, 0.8);
    atom_handle_t out1[] = {cat, animal};
    atom_handle_t cat_animal = atomspace_add_link(as, ATOM_TYPE_INHERITANCE, out1, 2, &tv);
    
    atom_handle_t out2[] = {animal, living};
    atom_handle_t animal_living = atomspace_add_link(as, ATOM_TYPE_INHERITANCE, out2, 2, &tv);
    
    /* Apply deduction: should infer cat -> living_thing */
    atom_handle_t conclusion;
    truth_value_t conclusion_tv;
    atom_handle_t premises[] = {cat_animal, animal_living};
    
    cog_result_t result = pln_apply_rule(pln, PLN_RULE_DEDUCTION, premises, 2,
        &conclusion, &conclusion_tv);
    TEST_ASSERT(result == COG_OK, "Deduction should succeed");
    TEST_ASSERT(conclusion != ATOM_HANDLE_INVALID, "Should produce conclusion");
    TEST_ASSERT(conclusion_tv.strength > 0, "Conclusion should have positive strength");
    
    pln_shutdown(pln);
    atomspace_destroy(as);
    return 1;
}

/*===========================================================================
 * 9P Server Tests
 *===========================================================================*/

static int test_9p_server_create(void) {
    p9_server_t* server;
    p9_server_config_t config = {
        .port = 9564,
        .msize = 8192
    };
    
    cog_result_t result = p9_server_create(&config, &server);
    TEST_ASSERT(result == COG_OK, "9P server creation should succeed");
    TEST_ASSERT(server != NULL, "Server should not be NULL");
    
    p9_server_destroy(server);
    return 1;
}

static int test_9p_server_atomspace_integration(void) {
    atomspace_t as;
    atomspace_config_t as_config = {0};
    atomspace_create(&as_config, &as);
    
    p9_server_t* server;
    p9_server_config_t config = {.port = 9565, .msize = 8192};
    p9_server_create(&config, &server);
    
    cog_result_t result = p9_server_set_atomspace(server, as);
    TEST_ASSERT(result == COG_OK, "Setting AtomSpace should succeed");
    
    p9_server_destroy(server);
    atomspace_destroy(as);
    return 1;
}

/*===========================================================================
 * CogW7OS Kernel Tests
 *===========================================================================*/

static int test_cogw7_kernel_create(void) {
    cogw7_kernel_t kernel;
    cogw7_config_t config = {
        .max_processes = 256,
        .max_threads_per_process = 64,
        .default_stack_size = 1024 * 1024,
        .scheduler_quantum_ms = 20,
        .enable_cognitive_scheduling = true,
        .enable_attention_allocation = true,
        .reasoning_interval_ms = 100
    };
    
    cog_result_t result = cogw7_kernel_create(&config, &kernel);
    TEST_ASSERT(result == COG_OK, "Kernel creation should succeed");
    TEST_ASSERT(kernel != NULL, "Kernel should not be NULL");
    
    TEST_ASSERT(cogw7_kernel_get_state(kernel) == COGW7_STATE_INIT,
        "Initial state should be INIT");
    
    cogw7_kernel_destroy(kernel);
    return 1;
}

static int test_cogw7_kernel_boot(void) {
    cogw7_kernel_t kernel;
    cogw7_config_t config = {
        .max_processes = 256,
        .scheduler_quantum_ms = 20,
        .reasoning_interval_ms = 100
    };
    
    cogw7_kernel_create(&config, &kernel);
    
    cog_result_t result = cogw7_kernel_boot(kernel);
    TEST_ASSERT(result == COG_OK, "Kernel boot should succeed");
    TEST_ASSERT(cogw7_kernel_get_state(kernel) == COGW7_STATE_RUNNING,
        "State should be RUNNING after boot");
    
    /* Let it run briefly */
    cog_time_sleep_ms(50);
    
    result = cogw7_kernel_shutdown(kernel);
    TEST_ASSERT(result == COG_OK, "Kernel shutdown should succeed");
    TEST_ASSERT(cogw7_kernel_get_state(kernel) == COGW7_STATE_HALTED,
        "State should be HALTED after shutdown");
    
    cogw7_kernel_destroy(kernel);
    return 1;
}

static int test_cogw7_process_management(void) {
    cogw7_kernel_t kernel;
    cogw7_config_t config = {.max_processes = 256, .reasoning_interval_ms = 100};
    cogw7_kernel_create(&config, &kernel);
    cogw7_kernel_boot(kernel);
    
    /* Create process */
    uint32_t pid;
    cog_result_t result = cogw7_process_create(kernel, "TestProcess", 0, &pid);
    TEST_ASSERT(result == COG_OK, "Process creation should succeed");
    TEST_ASSERT(pid > 0, "PID should be positive");
    
    /* Get process info */
    cogw7_process_info_t info;
    result = cogw7_process_get_info(kernel, pid, &info);
    TEST_ASSERT(result == COG_OK, "Getting process info should succeed");
    TEST_ASSERT(strcmp(info.name, "TestProcess") == 0, "Process name should match");
    TEST_ASSERT(info.pid == pid, "PID should match");
    
    /* Terminate process */
    result = cogw7_process_terminate(kernel, pid, 0);
    TEST_ASSERT(result == COG_OK, "Process termination should succeed");
    
    cogw7_kernel_shutdown(kernel);
    cogw7_kernel_destroy(kernel);
    return 1;
}

static int test_cogw7_cognitive_integration(void) {
    cogw7_kernel_t kernel;
    cogw7_config_t config = {
        .enable_cognitive_scheduling = true,
        .enable_attention_allocation = true,
        .reasoning_interval_ms = 50
    };
    cogw7_kernel_create(&config, &kernel);
    cogw7_kernel_boot(kernel);
    
    /* Get AtomSpace */
    atomspace_t as = cogw7_get_atomspace(kernel);
    TEST_ASSERT(as != NULL, "Kernel should have AtomSpace");
    
    /* Get PLN */
    pln_context_t pln = cogw7_get_pln(kernel);
    TEST_ASSERT(pln != NULL, "Kernel should have PLN context");
    
    /* Add some atoms */
    atom_handle_t test = atomspace_add_node(as, ATOM_TYPE_CONCEPT, "TestAtom", NULL);
    TEST_ASSERT(test != ATOM_HANDLE_INVALID, "Adding atom should succeed");
    
    /* Let reasoning run */
    cog_time_sleep_ms(100);
    
    /* Check stats */
    cogw7_stats_t stats;
    cogw7_get_stats(kernel, &stats);
    TEST_ASSERT(stats.reasoning_cycles > 0, "Reasoning should have run");
    
    cogw7_kernel_shutdown(kernel);
    cogw7_kernel_destroy(kernel);
    return 1;
}

/*===========================================================================
 * Main Test Runner
 *===========================================================================*/

int main(int argc, char** argv) {
    printf("========================================\n");
    printf("CoGWXP-OS9 Test Suite\n");
    printf("========================================\n\n");
    
    /* Initialize CogUtil */
    cog_init();
    cog_log_set_level(COG_LOG_WARN);  /* Reduce noise during tests */
    
    printf("--- CogUtil Tests ---\n");
    RUN_TEST(test_cogutil_init);
    RUN_TEST(test_cogutil_logging);
    RUN_TEST(test_cogutil_memory);
    RUN_TEST(test_cogutil_config);
    RUN_TEST(test_cogutil_uuid);
    RUN_TEST(test_cogutil_task_queue);
    
    printf("\n--- AtomSpace Tests ---\n");
    RUN_TEST(test_atomspace_create);
    RUN_TEST(test_atomspace_nodes);
    RUN_TEST(test_atomspace_links);
    RUN_TEST(test_atomspace_truth_values);
    RUN_TEST(test_atomspace_attention_values);
    
    printf("\n--- PLN Tests ---\n");
    RUN_TEST(test_pln_init);
    RUN_TEST(test_pln_truth_value_operations);
    RUN_TEST(test_pln_deduction);
    
    printf("\n--- 9P Server Tests ---\n");
    RUN_TEST(test_9p_server_create);
    RUN_TEST(test_9p_server_atomspace_integration);
    
    printf("\n--- CogW7OS Kernel Tests ---\n");
    RUN_TEST(test_cogw7_kernel_create);
    RUN_TEST(test_cogw7_kernel_boot);
    RUN_TEST(test_cogw7_process_management);
    RUN_TEST(test_cogw7_cognitive_integration);
    
    /* Cleanup */
    cog_shutdown();
    
    /* Print summary */
    printf("\n========================================\n");
    printf("Test Summary\n");
    printf("========================================\n");
    printf("Total:  %d\n", tests_run);
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    printf("========================================\n");
    
    return tests_failed > 0 ? 1 : 0;
}
