/**
 * @file test_integration_bridge.c
 * @brief Integration Tests for MSHyperGraph <-> AtomSpace Bridge
 *        and CogPilot <-> AtomSpace Bridge
 * 
 * These tests verify the complete data flow:
 *   MS Graph Entity -> AtomSpace Atom -> PLN Reasoning -> Query Results
 *   CogPilot Agent -> AtomSpace Atom -> Cognitive Scheduling
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
#include "../orchestration/msgraph/atomspace/ms_hypergraph.h"
#include "../orchestration/cogpilot/cogpilot.h"

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
 * Shared Test Fixtures
 *===========================================================================*/

static atomspace_t g_atomspace = NULL;

static void setup_atomspace(void) {
    atomspace_config_t config = {0};
    cog_result_t result = atomspace_create(&config, &g_atomspace);
    assert(result == COG_OK && "Failed to create AtomSpace for tests");
}

static void teardown_atomspace(void) {
    if (g_atomspace) {
        atomspace_destroy(g_atomspace);
        g_atomspace = NULL;
    }
}

/*===========================================================================
 * MSHyperGraph -> AtomSpace Integration Tests
 *===========================================================================*/

/**
 * Test: Entity-to-Atom creates a real ConceptNode in AtomSpace
 */
static int test_entity_to_atom_creates_concept_node(void) {
    setup_atomspace();
    
    /* Create a mock MSGraph context with the real AtomSpace */
    msgraph_config_t config = {
        .tenant_id = "test-tenant",
        .client_id = "test-client",
        .client_secret = "test-secret",
        .enable_caching = true,
        .cache_ttl_seconds = 300
    };
    
    msgraph_context_t ctx = NULL;
    cog_result_t result = msgraph_init(&config, g_atomspace, &ctx);
    TEST_ASSERT(result == COG_OK, "MSGraph init should succeed");
    TEST_ASSERT(ctx != NULL, "Context should not be NULL");
    
    /* Create a test entity */
    msgraph_entity_t entity = {
        .id = "user-123-abc",
        .display_name = "Daniel Faucitt",
        .type = MSGRAPH_ENTITY_USER,
        .last_modified = 1700000000
    };
    
    /* Convert entity to atom */
    atom_handle_t atom;
    result = msgraph_entity_to_atom(ctx, &entity, &atom);
    TEST_ASSERT(result == COG_OK, "Entity-to-atom should succeed");
    TEST_ASSERT(atom != ATOM_HANDLE_INVALID, "Atom handle should be valid");
    
    /* Verify the atom exists in AtomSpace */
    atom_handle_t lookup = atomspace_get_node(g_atomspace, ATOM_TYPE_CONCEPT, "Daniel Faucitt");
    TEST_ASSERT(lookup != ATOM_HANDLE_INVALID, "Node should be findable by name");
    TEST_ASSERT(lookup == atom, "Lookup should return same handle");
    
    /* Verify truth value */
    truth_value_t tv;
    atomspace_get_tv(g_atomspace, atom, &tv);
    TEST_ASSERT(tv.strength == 1.0, "Entity strength should be 1.0");
    TEST_ASSERT(tv.confidence == 0.9, "Entity confidence should be 0.9");
    
    msgraph_shutdown(ctx);
    teardown_atomspace();
    return 1;
}

/**
 * Test: Entity-to-Atom creates InheritanceLink to type category
 */
static int test_entity_to_atom_creates_type_hierarchy(void) {
    setup_atomspace();
    
    msgraph_config_t config = {
        .tenant_id = "test-tenant",
        .client_id = "test-client",
        .client_secret = "test-secret"
    };
    
    msgraph_context_t ctx = NULL;
    msgraph_init(&config, g_atomspace, &ctx);
    
    /* Create a user entity */
    msgraph_entity_t entity = {
        .id = "user-456",
        .display_name = "TestUser",
        .type = MSGRAPH_ENTITY_USER
    };
    
    atom_handle_t atom;
    msgraph_entity_to_atom(ctx, &entity, &atom);
    
    /* Verify the category node exists */
    atom_handle_t category = atomspace_get_node(g_atomspace, ATOM_TYPE_CONCEPT, "MSGraph:User");
    TEST_ASSERT(category != ATOM_HANDLE_INVALID, "Category node should exist");
    
    /* Verify InheritanceLink exists: entity -> MSGraph:User */
    atom_handle_t outgoing[2] = {atom, category};
    atom_handle_t inh_link = atomspace_get_link(g_atomspace, ATOM_TYPE_INHERITANCE, outgoing, 2);
    TEST_ASSERT(inh_link != ATOM_HANDLE_INVALID, "InheritanceLink should exist");
    
    /* Verify InheritanceLink truth value */
    truth_value_t tv;
    atomspace_get_tv(g_atomspace, inh_link, &tv);
    TEST_ASSERT(tv.strength == 1.0, "Inheritance strength should be 1.0");
    TEST_ASSERT(tv.confidence == 1.0, "Inheritance confidence should be 1.0");
    
    msgraph_shutdown(ctx);
    teardown_atomspace();
    return 1;
}

/**
 * Test: Entity-to-Atom stores entity ID as EvaluationLink
 */
static int test_entity_to_atom_stores_id_property(void) {
    setup_atomspace();
    
    msgraph_config_t config = {
        .tenant_id = "test-tenant",
        .client_id = "test-client",
        .client_secret = "test-secret"
    };
    
    msgraph_context_t ctx = NULL;
    msgraph_init(&config, g_atomspace, &ctx);
    
    msgraph_entity_t entity = {
        .id = "unique-id-789",
        .display_name = "IDTestEntity",
        .type = MSGRAPH_ENTITY_GROUP
    };
    
    atom_handle_t atom;
    msgraph_entity_to_atom(ctx, &entity, &atom);
    
    /* Verify the ID predicate node exists */
    atom_handle_t id_pred = atomspace_get_node(g_atomspace, ATOM_TYPE_PREDICATE, "msgraph:id");
    TEST_ASSERT(id_pred != ATOM_HANDLE_INVALID, "ID predicate should exist");
    
    /* Verify the ID value node exists */
    atom_handle_t id_value = atomspace_get_node(g_atomspace, ATOM_TYPE_CONCEPT, "id:unique-id-789");
    TEST_ASSERT(id_value != ATOM_HANDLE_INVALID, "ID value node should exist");
    
    /* Verify ListLink(entity, id_value) exists */
    atom_handle_t list_out[2] = {atom, id_value};
    atom_handle_t list_link = atomspace_get_link(g_atomspace, ATOM_TYPE_LIST, list_out, 2);
    TEST_ASSERT(list_link != ATOM_HANDLE_INVALID, "ListLink(entity, id) should exist");
    
    /* Verify EvaluationLink(id_pred, list_link) exists */
    atom_handle_t eval_out[2] = {id_pred, list_link};
    atom_handle_t eval_link = atomspace_get_link(g_atomspace, ATOM_TYPE_EVALUATION, eval_out, 2);
    TEST_ASSERT(eval_link != ATOM_HANDLE_INVALID, "EvaluationLink for ID should exist");
    
    msgraph_shutdown(ctx);
    teardown_atomspace();
    return 1;
}

/**
 * Test: Entity-to-Atom is idempotent (same entity produces same atom)
 */
static int test_entity_to_atom_idempotent(void) {
    setup_atomspace();
    
    msgraph_config_t config = {
        .tenant_id = "test-tenant",
        .client_id = "test-client",
        .client_secret = "test-secret"
    };
    
    msgraph_context_t ctx = NULL;
    msgraph_init(&config, g_atomspace, &ctx);
    
    msgraph_entity_t entity = {
        .id = "idem-123",
        .display_name = "IdempotentEntity",
        .type = MSGRAPH_ENTITY_TEAM
    };
    
    /* Convert same entity twice */
    atom_handle_t atom1, atom2;
    msgraph_entity_to_atom(ctx, &entity, &atom1);
    msgraph_entity_to_atom(ctx, &entity, &atom2);
    
    /* Both should return the same handle */
    TEST_ASSERT(atom1 == atom2, "Same entity should produce same atom handle");
    
    /* AtomSpace should only have one node with this name */
    atom_handle_t lookup = atomspace_get_node(g_atomspace, ATOM_TYPE_CONCEPT, "IdempotentEntity");
    TEST_ASSERT(lookup == atom1, "Lookup should return the same handle");
    
    msgraph_shutdown(ctx);
    teardown_atomspace();
    return 1;
}

/**
 * Test: Entity-to-Atom stimulates the atom (brings into attentional focus)
 */
static int test_entity_to_atom_stimulates(void) {
    setup_atomspace();
    
    msgraph_config_t config = {
        .tenant_id = "test-tenant",
        .client_id = "test-client",
        .client_secret = "test-secret"
    };
    
    msgraph_context_t ctx = NULL;
    msgraph_init(&config, g_atomspace, &ctx);
    
    msgraph_entity_t entity = {
        .id = "stim-123",
        .display_name = "StimulatedEntity",
        .type = MSGRAPH_ENTITY_USER
    };
    
    atom_handle_t atom;
    msgraph_entity_to_atom(ctx, &entity, &atom);
    
    /* Verify the atom has non-zero STI (was stimulated) */
    attention_value_t av;
    atomspace_get_av(g_atomspace, atom, &av);
    TEST_ASSERT(av.sti > 0, "Entity atom should have positive STI after stimulation");
    
    msgraph_shutdown(ctx);
    teardown_atomspace();
    return 1;
}

/**
 * Test: Multiple entity types create correct category hierarchy
 */
static int test_multiple_entity_types(void) {
    setup_atomspace();
    
    msgraph_config_t config = {
        .tenant_id = "test-tenant",
        .client_id = "test-client",
        .client_secret = "test-secret"
    };
    
    msgraph_context_t ctx = NULL;
    msgraph_init(&config, g_atomspace, &ctx);
    
    /* Create entities of different types */
    msgraph_entity_t user = {.id = "u1", .display_name = "User1", .type = MSGRAPH_ENTITY_USER};
    msgraph_entity_t group = {.id = "g1", .display_name = "Group1", .type = MSGRAPH_ENTITY_GROUP};
    msgraph_entity_t team = {.id = "t1", .display_name = "Team1", .type = MSGRAPH_ENTITY_TEAM};
    
    atom_handle_t user_atom, group_atom, team_atom;
    msgraph_entity_to_atom(ctx, &user, &user_atom);
    msgraph_entity_to_atom(ctx, &group, &group_atom);
    msgraph_entity_to_atom(ctx, &team, &team_atom);
    
    /* Verify all category nodes exist */
    TEST_ASSERT(atomspace_get_node(g_atomspace, ATOM_TYPE_CONCEPT, "MSGraph:User") != ATOM_HANDLE_INVALID,
        "User category should exist");
    TEST_ASSERT(atomspace_get_node(g_atomspace, ATOM_TYPE_CONCEPT, "MSGraph:Group") != ATOM_HANDLE_INVALID,
        "Group category should exist");
    TEST_ASSERT(atomspace_get_node(g_atomspace, ATOM_TYPE_CONCEPT, "MSGraph:Team") != ATOM_HANDLE_INVALID,
        "Team category should exist");
    
    /* Verify all entity atoms are distinct */
    TEST_ASSERT(user_atom != group_atom, "User and Group atoms should differ");
    TEST_ASSERT(group_atom != team_atom, "Group and Team atoms should differ");
    TEST_ASSERT(user_atom != team_atom, "User and Team atoms should differ");
    
    msgraph_shutdown(ctx);
    teardown_atomspace();
    return 1;
}

/**
 * Test: Atom-to-Entity reverse lookup works
 */
static int test_atom_to_entity_reverse_lookup(void) {
    setup_atomspace();
    
    msgraph_config_t config = {
        .tenant_id = "test-tenant",
        .client_id = "test-client",
        .client_secret = "test-secret"
    };
    
    msgraph_context_t ctx = NULL;
    msgraph_init(&config, g_atomspace, &ctx);
    
    /* Create entity and convert to atom */
    msgraph_entity_t original = {
        .id = "rev-123",
        .display_name = "ReverseLookup",
        .type = MSGRAPH_ENTITY_USER
    };
    
    atom_handle_t atom;
    msgraph_entity_to_atom(ctx, &original, &atom);
    
    /* Reverse lookup: atom -> entity */
    msgraph_entity_t recovered = {0};
    cog_result_t result = msgraph_atom_to_entity(ctx, atom, &recovered);
    TEST_ASSERT(result == COG_OK, "Atom-to-entity should succeed");
    TEST_ASSERT(recovered.display_name != NULL, "Recovered entity should have display name");
    TEST_ASSERT(strcmp(recovered.display_name, "ReverseLookup") == 0,
        "Recovered display name should match original");
    TEST_ASSERT(recovered.type == MSGRAPH_ENTITY_USER, "Recovered type should be USER");
    
    free((void*)recovered.display_name);
    msgraph_shutdown(ctx);
    teardown_atomspace();
    return 1;
}

/*===========================================================================
 * CogPilot -> AtomSpace Integration Tests
 *===========================================================================*/

/* Forward declarations for bridge functions */
extern cog_result_t cogpilot_bridge_init(atomspace_t atomspace, cogpilot_context_t cogpilot);
extern void cogpilot_bridge_shutdown(void);
extern cog_result_t cogpilot_bridge_sync_agent(const cogpilot_agent_t* agent, atom_handle_t* agent_atom);
extern cog_result_t cogpilot_bridge_record_task_result(uint64_t task_id, atom_handle_t agent_atom, const char* result_summary, bool success, double confidence);
extern cog_result_t cogpilot_bridge_update_agent_status(atom_handle_t agent_atom, const char* status_name, double availability);
extern cog_result_t cogpilot_bridge_update_success_rate(atom_handle_t agent_atom, double success_rate, uint64_t sample_count);
extern cog_result_t cogpilot_bridge_select_best_agent(uint32_t required_capabilities, atom_handle_t* best_agent);

/**
 * Test: Agent sync creates ConceptNode and InheritanceLink
 */
static int test_cogpilot_agent_sync(void) {
    setup_atomspace();
    
    /* Initialize CogPilot */
    cogpilot_config_t cp_config = {.max_concurrent_tasks = 2};
    cogpilot_context_t cogpilot = NULL;
    cogpilot_init(&cp_config, &cogpilot);
    
    /* Initialize bridge */
    cog_result_t result = cogpilot_bridge_init(g_atomspace, cogpilot);
    TEST_ASSERT(result == COG_OK, "Bridge init should succeed");
    
    /* Create and sync an agent */
    cogpilot_agent_t agent = {
        .name = "TestReasoningAgent",
        .description = "A test agent with reasoning capability",
        .capabilities = 0x0001 | 0x0080,  /* reasoning + inference */
    };
    
    atom_handle_t agent_atom;
    result = cogpilot_bridge_sync_agent(&agent, &agent_atom);
    TEST_ASSERT(result == COG_OK, "Agent sync should succeed");
    TEST_ASSERT(agent_atom != ATOM_HANDLE_INVALID, "Agent atom should be valid");
    
    /* Verify agent node exists */
    atom_handle_t lookup = atomspace_get_node(g_atomspace, ATOM_TYPE_CONCEPT, "agent:TestReasoningAgent");
    TEST_ASSERT(lookup == agent_atom, "Agent node should be findable");
    
    /* Verify InheritanceLink to CogPilot:Agent */
    atom_handle_t category = atomspace_get_node(g_atomspace, ATOM_TYPE_CONCEPT, "CogPilot:Agent");
    TEST_ASSERT(category != ATOM_HANDLE_INVALID, "Agent category should exist");
    
    atom_handle_t inh_out[2] = {agent_atom, category};
    atom_handle_t inh_link = atomspace_get_link(g_atomspace, ATOM_TYPE_INHERITANCE, inh_out, 2);
    TEST_ASSERT(inh_link != ATOM_HANDLE_INVALID, "InheritanceLink to Agent category should exist");
    
    /* Verify capability nodes exist */
    atom_handle_t reasoning_cap = atomspace_get_node(g_atomspace, ATOM_TYPE_CONCEPT, "cap:reasoning");
    TEST_ASSERT(reasoning_cap != ATOM_HANDLE_INVALID, "Reasoning capability node should exist");
    
    atom_handle_t inference_cap = atomspace_get_node(g_atomspace, ATOM_TYPE_CONCEPT, "cap:inference");
    TEST_ASSERT(inference_cap != ATOM_HANDLE_INVALID, "Inference capability node should exist");
    
    cogpilot_bridge_shutdown();
    cogpilot_shutdown(cogpilot);
    teardown_atomspace();
    return 1;
}

/**
 * Test: Task result recording creates proper AtomSpace structure
 */
static int test_cogpilot_task_result_recording(void) {
    setup_atomspace();
    
    cogpilot_config_t cp_config = {.max_concurrent_tasks = 2};
    cogpilot_context_t cogpilot = NULL;
    cogpilot_init(&cp_config, &cogpilot);
    cogpilot_bridge_init(g_atomspace, cogpilot);
    
    /* Sync an agent first */
    cogpilot_agent_t agent = {
        .name = "TaskAgent",
        .capabilities = 0x0001,
    };
    atom_handle_t agent_atom;
    cogpilot_bridge_sync_agent(&agent, &agent_atom);
    
    /* Record a successful task result */
    cog_result_t result = cogpilot_bridge_record_task_result(
        42, agent_atom, "Inference completed successfully", true, 0.85);
    TEST_ASSERT(result == COG_OK, "Task result recording should succeed");
    
    /* Verify task node exists */
    atom_handle_t task_node = atomspace_get_node(g_atomspace, ATOM_TYPE_CONCEPT, "task:42");
    TEST_ASSERT(task_node != ATOM_HANDLE_INVALID, "Task node should exist");
    
    /* Verify task is categorized */
    atom_handle_t task_category = atomspace_get_node(g_atomspace, ATOM_TYPE_CONCEPT, "CogPilot:Task");
    TEST_ASSERT(task_category != ATOM_HANDLE_INVALID, "Task category should exist");
    
    atom_handle_t inh_out[2] = {task_node, task_category};
    atom_handle_t inh_link = atomspace_get_link(g_atomspace, ATOM_TYPE_INHERITANCE, inh_out, 2);
    TEST_ASSERT(inh_link != ATOM_HANDLE_INVALID, "Task should inherit from Task category");
    
    /* Verify task truth value reflects success */
    truth_value_t tv;
    atomspace_get_tv(g_atomspace, task_node, &tv);
    TEST_ASSERT(tv.strength == 1.0, "Successful task should have strength 1.0");
    TEST_ASSERT(tv.confidence == 0.85, "Task confidence should match input");
    
    cogpilot_bridge_shutdown();
    cogpilot_shutdown(cogpilot);
    teardown_atomspace();
    return 1;
}

/**
 * Test: Agent status update modifies attention values
 */
static int test_cogpilot_agent_status_update(void) {
    setup_atomspace();
    
    cogpilot_config_t cp_config = {.max_concurrent_tasks = 2};
    cogpilot_context_t cogpilot = NULL;
    cogpilot_init(&cp_config, &cogpilot);
    cogpilot_bridge_init(g_atomspace, cogpilot);
    
    /* Sync agent */
    cogpilot_agent_t agent = {.name = "StatusAgent", .capabilities = 0x0001};
    atom_handle_t agent_atom;
    cogpilot_bridge_sync_agent(&agent, &agent_atom);
    
    /* Update status to "busy" with 30% availability */
    cog_result_t result = cogpilot_bridge_update_agent_status(
        agent_atom, "busy", 0.3);
    TEST_ASSERT(result == COG_OK, "Status update should succeed");
    
    /* Verify status node exists */
    atom_handle_t status_node = atomspace_get_node(g_atomspace, ATOM_TYPE_CONCEPT, "status:busy");
    TEST_ASSERT(status_node != ATOM_HANDLE_INVALID, "Status node should exist");
    
    /* Verify agent has positive attention (was stimulated) */
    attention_value_t av;
    atomspace_get_av(g_atomspace, agent_atom, &av);
    TEST_ASSERT(av.sti > 0, "Agent should have positive STI after status update");
    
    cogpilot_bridge_shutdown();
    cogpilot_shutdown(cogpilot);
    teardown_atomspace();
    return 1;
}

/**
 * Test: Success rate update modifies agent truth value
 */
static int test_cogpilot_success_rate_update(void) {
    setup_atomspace();
    
    cogpilot_config_t cp_config = {.max_concurrent_tasks = 2};
    cogpilot_context_t cogpilot = NULL;
    cogpilot_init(&cp_config, &cogpilot);
    cogpilot_bridge_init(g_atomspace, cogpilot);
    
    /* Sync agent */
    cogpilot_agent_t agent = {.name = "RateAgent", .capabilities = 0x0001};
    atom_handle_t agent_atom;
    cogpilot_bridge_sync_agent(&agent, &agent_atom);
    
    /* Update success rate: 85% success over 100 samples */
    cog_result_t result = cogpilot_bridge_update_success_rate(
        agent_atom, 0.85, 100);
    TEST_ASSERT(result == COG_OK, "Success rate update should succeed");
    
    /* Verify truth value was updated */
    truth_value_t tv;
    atomspace_get_tv(g_atomspace, agent_atom, &tv);
    TEST_ASSERT(tv.strength == 0.85, "Agent strength should reflect success rate");
    /* confidence = 100 / (100 + 800) = 0.111... */
    TEST_ASSERT(tv.confidence > 0.1 && tv.confidence < 0.12,
        "Confidence should reflect sample count / (count + k)");
    
    cogpilot_bridge_shutdown();
    cogpilot_shutdown(cogpilot);
    teardown_atomspace();
    return 1;
}

/*===========================================================================
 * End-to-End Integration Tests
 *===========================================================================*/

/**
 * Test: Full pipeline - Entity sync + PLN reasoning over synced entities
 */
static int test_full_pipeline_entity_to_pln(void) {
    setup_atomspace();
    
    /* Create entities representing an organizational hierarchy */
    msgraph_config_t config = {
        .tenant_id = "test-tenant",
        .client_id = "test-client",
        .client_secret = "test-secret"
    };
    
    msgraph_context_t ctx = NULL;
    msgraph_init(&config, g_atomspace, &ctx);
    
    /* Create user and group entities */
    msgraph_entity_t user = {.id = "u1", .display_name = "Engineer", .type = MSGRAPH_ENTITY_USER};
    msgraph_entity_t group = {.id = "g1", .display_name = "Engineering", .type = MSGRAPH_ENTITY_GROUP};
    
    atom_handle_t user_atom, group_atom;
    msgraph_entity_to_atom(ctx, &user, &user_atom);
    msgraph_entity_to_atom(ctx, &group, &group_atom);
    
    /* Manually create a membership relationship:
     * InheritanceLink(Engineer, Engineering) with TV <0.9, 0.8> */
    atom_handle_t member_out[2] = {user_atom, group_atom};
    truth_value_t member_tv = {.strength = 0.9, .confidence = 0.8};
    atom_handle_t membership = atomspace_add_link(
        g_atomspace, ATOM_TYPE_INHERITANCE, member_out, 2, &member_tv);
    TEST_ASSERT(membership != ATOM_HANDLE_INVALID, "Membership link should be created");
    
    /* Create another level: Engineering -> Organization */
    atom_handle_t org_atom = atomspace_add_node(
        g_atomspace, ATOM_TYPE_CONCEPT, "Organization", NULL);
    atom_handle_t org_out[2] = {group_atom, org_atom};
    truth_value_t org_tv = {.strength = 0.95, .confidence = 0.9};
    atom_handle_t org_link = atomspace_add_link(
        g_atomspace, ATOM_TYPE_INHERITANCE, org_out, 2, &org_tv);
    TEST_ASSERT(org_link != ATOM_HANDLE_INVALID, "Org link should be created");
    
    /* Initialize PLN and run deduction:
     * Engineer -> Engineering -> Organization
     * Should infer: Engineer -> Organization */
    pln_context_t pln;
    pln_config_t pln_config = {
        .k = 800.0,
        .max_chain_depth = 5,
        .confidence_threshold = 0.1,
        .enable_forward_chaining = true
    };
    
    cog_result_t result = pln_init(g_atomspace, &pln_config, &pln);
    TEST_ASSERT(result == COG_OK, "PLN init should succeed");
    
    /* Apply deduction rule */
    atom_handle_t premises[2] = {membership, org_link};
    atom_handle_t conclusion;
    truth_value_t conclusion_tv;
    
    result = pln_apply_rule(pln, PLN_RULE_DEDUCTION, premises, 2,
        &conclusion, &conclusion_tv);
    TEST_ASSERT(result == COG_OK, "PLN deduction should succeed");
    TEST_ASSERT(conclusion != ATOM_HANDLE_INVALID, "Should produce conclusion");
    TEST_ASSERT(conclusion_tv.strength > 0, "Conclusion should have positive strength");
    TEST_ASSERT(conclusion_tv.confidence > 0, "Conclusion should have positive confidence");
    
    /* The conclusion should be: Engineer -> Organization */
    /* Verify it's an InheritanceLink with user_atom and org_atom */
    atom_handle_t expected_out[2] = {user_atom, org_atom};
    atom_handle_t inferred = atomspace_get_link(
        g_atomspace, ATOM_TYPE_INHERITANCE, expected_out, 2);
    TEST_ASSERT(inferred != ATOM_HANDLE_INVALID,
        "PLN should have inferred Engineer -> Organization");
    
    pln_shutdown(pln);
    msgraph_shutdown(ctx);
    teardown_atomspace();
    return 1;
}

/**
 * Test: Stats are correctly tracked across operations
 */
static int test_bridge_stats_tracking(void) {
    setup_atomspace();
    
    msgraph_config_t config = {
        .tenant_id = "test-tenant",
        .client_id = "test-client",
        .client_secret = "test-secret"
    };
    
    msgraph_context_t ctx = NULL;
    msgraph_init(&config, g_atomspace, &ctx);
    
    /* Convert multiple entities */
    for (int i = 0; i < 5; i++) {
        char name[32];
        snprintf(name, sizeof(name), "StatsEntity%d", i);
        msgraph_entity_t entity = {
            .id = name,
            .display_name = name,
            .type = MSGRAPH_ENTITY_USER
        };
        atom_handle_t atom;
        msgraph_entity_to_atom(ctx, &entity, &atom);
    }
    
    /* Check stats */
    msgraph_stats_t stats;
    msgraph_get_stats(ctx, &stats);
    TEST_ASSERT(stats.atoms_created == 5, "Should have created 5 atoms");
    
    msgraph_shutdown(ctx);
    teardown_atomspace();
    return 1;
}

/*===========================================================================
 * Main Test Runner
 *===========================================================================*/

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    
    printf("========================================\n");
    printf("CoGWXP-OS9 Integration Bridge Tests\n");
    printf("========================================\n\n");
    
    /* Initialize CogUtil */
    cog_init();
    cog_log_set_level(COG_LOG_WARN);
    
    printf("--- MSHyperGraph -> AtomSpace Bridge Tests ---\n");
    RUN_TEST(test_entity_to_atom_creates_concept_node);
    RUN_TEST(test_entity_to_atom_creates_type_hierarchy);
    RUN_TEST(test_entity_to_atom_stores_id_property);
    RUN_TEST(test_entity_to_atom_idempotent);
    RUN_TEST(test_entity_to_atom_stimulates);
    RUN_TEST(test_multiple_entity_types);
    RUN_TEST(test_atom_to_entity_reverse_lookup);
    
    printf("\n--- CogPilot -> AtomSpace Bridge Tests ---\n");
    RUN_TEST(test_cogpilot_agent_sync);
    RUN_TEST(test_cogpilot_task_result_recording);
    RUN_TEST(test_cogpilot_agent_status_update);
    RUN_TEST(test_cogpilot_success_rate_update);
    
    printf("\n--- End-to-End Integration Tests ---\n");
    RUN_TEST(test_full_pipeline_entity_to_pln);
    RUN_TEST(test_bridge_stats_tracking);
    
    /* Cleanup */
    cog_shutdown();
    
    /* Print summary */
    printf("\n========================================\n");
    printf("Integration Test Summary\n");
    printf("========================================\n");
    printf("Total:  %d\n", tests_run);
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    printf("========================================\n");
    
    return tests_failed > 0 ? 1 : 0;
}
