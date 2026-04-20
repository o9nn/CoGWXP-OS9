/**
 * @file test_frameworks.c
 * @brief Comprehensive Test Suite for ATenSpace, HyperMind, and CogZero
 *
 * Covers the core APIs introduced and expanded in the "next steps" phase:
 *  - ATenSpace: atom CRUD, TV/AV ops, ECAN step, embeddings, pattern matching,
 *               forward/backward chaining, persistence, traversal
 *  - HyperMind: actor system, reactive streams, sessions, commands, model
 *               training (including proper multi-layer forward pass), tensor ops
 *  - CogZero:   agent lifecycle, perception, planning, learning, memory recall,
 *               communication, tools, AtomSpace integration, distributed stubs
 *
 * @copyright CoGWXP-OS9 Project
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "../orchestration/atenspace/atenspace.h"
#include "../orchestration/hypermind/hypermind.h"
#include "../orchestration/cogzero/cogzero.h"

/*===========================================================================
 * Lightweight test framework
 *===========================================================================*/

static int g_tests_run    = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define ASSERT(cond, msg) do {                           \
    if (!(cond)) {                                       \
        printf("  FAIL [%s:%d]: %s\n", __FILE__, __LINE__, (msg)); \
        return 0;                                        \
    }                                                    \
} while (0)

#define ASSERT_OK(expr, msg)    ASSERT((expr) == COG_OK, msg)
#define ASSERT_EQ(a, b, msg)    ASSERT((a) == (b), msg)
#define ASSERT_NEQ(a, b, msg)   ASSERT((a) != (b), msg)
#define ASSERT_GT(a, b, msg)    ASSERT((a) > (b), msg)
#define ASSERT_GTE(a, b, msg)   ASSERT((a) >= (b), msg)
#define ASSERT_NOT_NULL(p, msg) ASSERT((p) != NULL, msg)
#define ASSERT_NULL(p, msg)     ASSERT((p) == NULL, msg)
#define ASSERT_NEAR(a, b, eps, msg) ASSERT(fabsf((float)(a) - (float)(b)) <= (float)(eps), msg)

#define RUN_TEST(fn) do {                                \
    printf("  %s ...\n", #fn);                           \
    g_tests_run++;                                       \
    if (fn()) { g_tests_passed++; printf("    PASS\n"); }\
    else       { g_tests_failed++;                       }\
} while (0)

/*===========================================================================
 * ATenSpace Tests
 *===========================================================================*/

static aten_context_t g_aten = NULL;

static aten_config_t aten_default_config(void) {
    aten_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.default_device  = ATEN_DEVICE_CPU;
    cfg.embedding_dim   = 32;
    cfg.embedding_dtype = ATEN_DTYPE_FLOAT32;
    cfg.enable_ecan     = true;
    cfg.ecan_params.stimulus_amount       = 10.0f;
    cfg.ecan_params.wage                  = 0.1f;
    cfg.ecan_params.rent                  = 0.05f;
    cfg.ecan_params.max_sti               = 1000;
    cfg.ecan_params.min_sti               = -1000;
    cfg.ecan_params.attention_focus_boundary = 100;
    cfg.max_inference_steps = 10;
    cfg.min_tv_confidence   = 0.0f;
    cfg.worker_threads      = 1;
    return cfg;
}

static int test_aten_lifecycle(void) {
    aten_config_t cfg = aten_default_config();
    aten_context_t ctx = NULL;
    ASSERT_OK(aten_init(&cfg, &ctx), "aten_init should succeed");
    ASSERT_NOT_NULL(ctx, "context should be non-NULL");
    aten_shutdown(ctx);
    return 1;
}

static int test_aten_create_nodes(void) {
    aten_truth_value_t tv = {ATEN_TV_SIMPLE, 0.9f, 0.8f, 1.0f, 0.9f};
    aten_handle_t h1, h2;

    ASSERT_OK(aten_create_node(g_aten, ATEN_NODE_CONCEPT, "dog", &tv, NULL, 0, &h1),
              "create node 'dog'");
    ASSERT_NEQ(h1, 0, "handle should be non-zero");

    ASSERT_OK(aten_create_node(g_aten, ATEN_NODE_CONCEPT, "animal", &tv, NULL, 0, &h2),
              "create node 'animal'");
    ASSERT_NEQ(h1, h2, "handles should differ");

    /* Idempotent: same name/type returns same handle */
    aten_handle_t h1b;
    ASSERT_OK(aten_create_node(g_aten, ATEN_NODE_CONCEPT, "dog", &tv, NULL, 0, &h1b),
              "re-create node 'dog'");
    ASSERT_EQ(h1, h1b, "duplicate node should return same handle");
    return 1;
}

static int test_aten_create_links(void) {
    aten_handle_t hdog, hanim, hlink;
    aten_truth_value_t tv = {ATEN_TV_SIMPLE, 1.0f, 0.9f, 1.0f, 1.0f};

    aten_create_node(g_aten, ATEN_NODE_CONCEPT, "dog2",    &tv, NULL, 0, &hdog);
    aten_create_node(g_aten, ATEN_NODE_CONCEPT, "animal2", &tv, NULL, 0, &hanim);

    aten_handle_t outgoing[2] = {hdog, hanim};
    ASSERT_OK(aten_create_link(g_aten, ATEN_LINK_INHERITANCE, outgoing, 2, &tv, &hlink),
              "create inheritance link");
    ASSERT_NEQ(hlink, 0, "link handle non-zero");
    return 1;
}

static int test_aten_get_atom(void) {
    aten_handle_t h;
    aten_truth_value_t tv = {ATEN_TV_SIMPLE, 0.7f, 0.6f, 1.0f, 0.7f};
    aten_create_node(g_aten, ATEN_NODE_CONCEPT, "cat", &tv, NULL, 0, &h);

    aten_atom_t atom;
    ASSERT_OK(aten_get_atom(g_aten, h, &atom), "get_atom should succeed");
    ASSERT_EQ(atom.type, ATEN_NODE_CONCEPT, "type should be CONCEPT");
    ASSERT_NOT_NULL(atom.name, "name should be non-NULL");
    ASSERT_EQ(strcmp(atom.name, "cat"), 0, "name should be 'cat'");
    return 1;
}

static int test_aten_tv_ops(void) {
    aten_handle_t h;
    aten_truth_value_t tv0 = {ATEN_TV_SIMPLE, 0.5f, 0.5f, 1.0f, 0.5f};
    aten_create_node(g_aten, ATEN_NODE_CONCEPT, "tv_test", &tv0, NULL, 0, &h);

    aten_truth_value_t tv_new = {ATEN_TV_SIMPLE, 0.9f, 0.95f, 2.0f, 1.8f};
    ASSERT_OK(aten_set_tv(g_aten, h, &tv_new), "set_tv should succeed");

    aten_truth_value_t tv_got;
    ASSERT_OK(aten_get_tv(g_aten, h, &tv_got), "get_tv should succeed");
    ASSERT_NEAR(tv_got.strength,    0.9f,  0.001f, "strength should match");
    ASSERT_NEAR(tv_got.confidence, 0.95f,  0.001f, "confidence should match");

    /* Merge */
    aten_truth_value_t tv1 = {ATEN_TV_SIMPLE, 0.8f, 0.6f, 1.0f, 0.8f};
    aten_truth_value_t tv2 = {ATEN_TV_SIMPLE, 0.4f, 0.4f, 1.0f, 0.4f};
    aten_truth_value_t merged;
    ASSERT_OK(aten_merge_tv(&tv1, &tv2, &merged), "merge_tv should succeed");
    ASSERT_GT(merged.confidence, 0.0f, "merged confidence should be positive");
    return 1;
}

static int test_aten_av_ops(void) {
    aten_handle_t h;
    aten_truth_value_t tv = {ATEN_TV_SIMPLE, 1.0f, 1.0f, 1.0f, 1.0f};
    aten_create_node(g_aten, ATEN_NODE_CONCEPT, "av_test", &tv, NULL, 0, &h);

    aten_attention_value_t av_new = {200, 50, true};
    ASSERT_OK(aten_set_av(g_aten, h, &av_new), "set_av should succeed");

    aten_attention_value_t av_got;
    ASSERT_OK(aten_get_av(g_aten, h, &av_got), "get_av should succeed");
    ASSERT_EQ(av_got.sti,  200, "STI should be 200");
    ASSERT_EQ(av_got.lti,   50, "LTI should be 50");
    ASSERT_EQ(av_got.vlti, true, "VLTI should be true");
    return 1;
}

static int test_aten_stimulate(void) {
    aten_handle_t h;
    aten_truth_value_t tv = {ATEN_TV_SIMPLE, 1.0f, 1.0f, 1.0f, 1.0f};
    aten_create_node(g_aten, ATEN_NODE_CONCEPT, "stimulate_me", &tv, NULL, 0, &h);

    ASSERT_OK(aten_stimulate(g_aten, h, 150.0f), "stimulate should succeed");

    aten_attention_value_t av;
    aten_get_av(g_aten, h, &av);
    ASSERT_GTE(av.sti, 100, "STI should be >= 100 after stimulation");
    return 1;
}

static int test_aten_ecan_step(void) {
    ASSERT_OK(aten_ecan_step(g_aten), "ecan_step should succeed");
    return 1;
}

static int test_aten_attention_focus(void) {
    aten_handle_t* focus = NULL;
    size_t count = 0;
    ASSERT_OK(aten_get_attention_focus(g_aten, &focus, &count),
              "get_attention_focus should succeed");
    free(focus);
    return 1;
}

static int test_aten_embeddings(void) {
    aten_handle_t h;
    aten_truth_value_t tv = {ATEN_TV_SIMPLE, 1.0f, 1.0f, 1.0f, 1.0f};
    aten_create_node(g_aten, ATEN_NODE_CONCEPT, "emb_node", &tv, NULL, 0, &h);

    float emb[32];
    for (int i = 0; i < 32; i++) emb[i] = (float)i / 32.0f;

    ASSERT_OK(aten_set_embedding(g_aten, h, emb, 32), "set_embedding");

    float* got_emb = NULL;
    size_t got_dim = 0;
    ASSERT_OK(aten_get_embedding(g_aten, h, &got_emb, &got_dim), "get_embedding");
    ASSERT_EQ(got_dim, 32u, "dim should be 32");
    ASSERT_NOT_NULL(got_emb, "embedding buffer non-NULL");
    ASSERT_NEAR(got_emb[0], 0.0f, 0.001f, "first component");
    ASSERT_NEAR(got_emb[16], 0.5f, 0.001f, "mid component");
    free(got_emb);
    return 1;
}

static int test_aten_tensor_similarity(void) {
    float emb_a[16], emb_b[16];
    for (int i = 0; i < 16; i++) { emb_a[i] = 1.0f; emb_b[i] = 1.0f; }

    aten_handle_t ha, hb;
    aten_truth_value_t tv = {ATEN_TV_SIMPLE, 1.0f, 1.0f, 1.0f, 1.0f};
    aten_create_node(g_aten, ATEN_NODE_CONCEPT, "sim_a", &tv, emb_a, 16, &ha);
    aten_create_node(g_aten, ATEN_NODE_CONCEPT, "sim_b", &tv, emb_b, 16, &hb);

    float sim = 0.0f;
    ASSERT_OK(aten_tensor_similarity(g_aten, ha, hb, &sim), "tensor_similarity");
    ASSERT_NEAR(sim, 1.0f, 0.001f, "identical embeddings should have similarity 1");
    return 1;
}

static int test_aten_batch_embed(void) {
    const char* texts[] = {"hello", "world", "cognitive"};
    aten_handle_t handles[3] = {0, 0, 0};
    ASSERT_OK(aten_batch_embed(g_aten, texts, 3, handles), "batch_embed");
    for (int i = 0; i < 3; i++) {
        ASSERT_NEQ(handles[i], 0u, "batch handle should be non-zero");
    }
    return 1;
}

static int test_aten_find_similar(void) {
    /* 'hello' was embedded in test_aten_batch_embed; find similar */
    aten_handle_t ref;
    aten_get_node(g_aten, ATEN_NODE_CONCEPT, "hello", &ref);
    if (ref == 0) return 1; /* skip if not found */

    aten_handle_t* results = NULL;
    float* sims = NULL;
    size_t count = 0;
    ASSERT_OK(aten_find_similar(g_aten, ref, 0.0f, 5, &results, &sims, &count),
              "find_similar");
    free(results);
    free(sims);
    return 1;
}

static int test_aten_get_incoming_outgoing(void) {
    aten_handle_t ha, hb, hlink;
    aten_truth_value_t tv = {ATEN_TV_SIMPLE, 1.0f, 1.0f, 1.0f, 1.0f};
    aten_create_node(g_aten, ATEN_NODE_CONCEPT, "inc_a", &tv, NULL, 0, &ha);
    aten_create_node(g_aten, ATEN_NODE_CONCEPT, "inc_b", &tv, NULL, 0, &hb);
    aten_handle_t out[2] = {ha, hb};
    aten_create_link(g_aten, ATEN_LINK_SIMILARITY, out, 2, &tv, &hlink);

    /* Outgoing of link */
    aten_handle_t* outgoing = NULL;
    size_t out_count = 0;
    ASSERT_OK(aten_get_outgoing(g_aten, hlink, &outgoing, &out_count), "get_outgoing");
    ASSERT_EQ(out_count, 2u, "link should have 2 outgoing atoms");
    free(outgoing);

    /* Incoming of ha */
    aten_handle_t* incoming = NULL;
    size_t in_count = 0;
    ASSERT_OK(aten_get_incoming(g_aten, ha, (aten_atom_type_t)-1, &incoming, &in_count),
              "get_incoming");
    ASSERT_GT(in_count, 0u, "ha should have at least one incoming link");
    free(incoming);
    return 1;
}

static int test_aten_get_by_type(void) {
    aten_handle_t* handles = NULL;
    size_t count = 0;
    ASSERT_OK(aten_get_by_type(g_aten, ATEN_NODE_CONCEPT, &handles, &count),
              "get_by_type");
    ASSERT_GT(count, 0u, "should find at least one CONCEPT node");
    free(handles);
    return 1;
}

static int test_aten_pattern_match(void) {
    aten_handle_t hpat;
    aten_truth_value_t tv = {ATEN_TV_SIMPLE, 1.0f, 0.9f, 1.0f, 1.0f};
    aten_create_node(g_aten, ATEN_NODE_CONCEPT, "pattern", &tv, NULL, 0, &hpat);

    aten_pattern_query_t q = {
        .pattern = hpat,
        .variables = NULL,
        .variable_count = 0,
        .min_confidence = 0.0f,
        .max_results = 10
    };

    aten_match_result_t* results = NULL;
    size_t count = 0;
    ASSERT_OK(aten_pattern_match(g_aten, &q, &results, &count), "pattern_match");
    /* Free bindings */
    for (size_t i = 0; i < count; i++) free(results[i].bindings);
    free(results);
    return 1;
}

static int test_aten_pln_deduction(void) {
    aten_handle_t ha, hb, hc, hab, hbc, hac;
    aten_truth_value_t tv = {ATEN_TV_SIMPLE, 1.0f, 0.9f, 1.0f, 1.0f};
    aten_truth_value_t tv_impl = {ATEN_TV_SIMPLE, 0.8f, 0.85f, 1.0f, 0.8f};

    aten_create_node(g_aten, ATEN_NODE_CONCEPT, "ded_A", &tv, NULL, 0, &ha);
    aten_create_node(g_aten, ATEN_NODE_CONCEPT, "ded_B", &tv, NULL, 0, &hb);
    aten_create_node(g_aten, ATEN_NODE_CONCEPT, "ded_C", &tv, NULL, 0, &hc);

    aten_handle_t ab_out[2] = {ha, hb};
    aten_handle_t bc_out[2] = {hb, hc};
    aten_create_link(g_aten, ATEN_LINK_IMPLICATION, ab_out, 2, &tv_impl, &hab);
    aten_create_link(g_aten, ATEN_LINK_IMPLICATION, bc_out, 2, &tv_impl, &hbc);

    aten_truth_value_t derived;
    ASSERT_OK(aten_pln_deduction(g_aten, hab, hbc, &hac, &derived),
              "pln_deduction should succeed");
    ASSERT_NEQ(hac, 0u, "derived link should be non-zero");
    ASSERT_GT(derived.strength, 0.0f, "derived strength should be positive");
    return 1;
}

static int test_aten_forward_chain(void) {
    aten_handle_t hsrc;
    aten_truth_value_t tv = {ATEN_TV_SIMPLE, 1.0f, 0.9f, 1.0f, 1.0f};
    aten_create_node(g_aten, ATEN_NODE_CONCEPT, "fwd_src", &tv, NULL, 0, &hsrc);

    aten_inference_rule_t rules[] = {ATEN_RULE_DEDUCTION};
    aten_inference_result_t res;
    memset(&res, 0, sizeof(res));

    ASSERT_OK(aten_forward_chain(g_aten, hsrc, rules, 1, 10, &res),
              "forward_chain should succeed");
    aten_inference_result_free(&res);
    return 1;
}

static int test_aten_backward_chain(void) {
    aten_handle_t htgt;
    aten_truth_value_t tv = {ATEN_TV_SIMPLE, 1.0f, 0.9f, 1.0f, 1.0f};
    aten_create_node(g_aten, ATEN_NODE_CONCEPT, "bwd_tgt", &tv, NULL, 0, &htgt);

    aten_inference_rule_t rules[] = {ATEN_RULE_MODUS_PONENS};
    aten_inference_result_t res;
    memset(&res, 0, sizeof(res));

    ASSERT_OK(aten_backward_chain(g_aten, htgt, rules, 1, 10, &res),
              "backward_chain should succeed");
    aten_inference_result_free(&res);
    return 1;
}

static int test_aten_remove_atom(void) {
    aten_handle_t h;
    aten_truth_value_t tv = {ATEN_TV_SIMPLE, 1.0f, 1.0f, 1.0f, 1.0f};
    aten_create_node(g_aten, ATEN_NODE_CONCEPT, "removeme", &tv, NULL, 0, &h);

    ASSERT_OK(aten_remove_atom(g_aten, h, false), "remove_atom should succeed");

    aten_atom_t atom;
    cog_result_t r = aten_get_atom(g_aten, h, &atom);
    ASSERT_EQ(r, COG_ERROR_NOT_FOUND, "removed atom should not be found");
    return 1;
}

static int test_aten_save_load(void) {
    const char* path = "/tmp/test_atenspace.bin";

    ASSERT_OK(aten_save(g_aten, path), "aten_save should succeed");

    /* Load into a fresh context */
    aten_config_t cfg = aten_default_config();
    aten_context_t ctx2 = NULL;
    ASSERT_OK(aten_init(&cfg, &ctx2), "aten_init for load");
    ASSERT_OK(aten_load(ctx2, path), "aten_load should succeed");

    aten_stats_t stats;
    aten_get_stats(ctx2, &stats);
    ASSERT_GT(stats.total_atoms, 0u, "loaded context should have atoms");

    aten_shutdown(ctx2);
    return 1;
}

static int test_aten_export_import_atomese(void) {
    aten_handle_t h;
    aten_truth_value_t tv = {ATEN_TV_SIMPLE, 0.7f, 0.8f, 1.0f, 0.7f};
    aten_create_node(g_aten, ATEN_NODE_CONCEPT, "atomese_node", &tv, NULL, 0, &h);

    char* atomese = NULL;
    ASSERT_OK(aten_export_atomese(g_aten, h, &atomese), "export_atomese");
    ASSERT_NOT_NULL(atomese, "atomese string non-NULL");
    ASSERT_GT(strlen(atomese), 0u, "atomese string non-empty");

    aten_handle_t imported;
    ASSERT_OK(aten_import_atomese(g_aten, atomese, &imported), "import_atomese");

    free(atomese);
    return 1;
}

static int test_aten_stats(void) {
    aten_stats_t stats;
    ASSERT_OK(aten_get_stats(g_aten, &stats), "get_stats");
    ASSERT_GT(stats.total_atoms, 0u, "should have atoms");
    ASSERT_GT(stats.node_count,  0u, "should have nodes");

    aten_reset_stats(g_aten);
    aten_get_stats(g_aten, &stats);
    ASSERT_EQ(stats.total_atoms, 0u, "after reset total_atoms should be 0");
    return 1;
}

/*===========================================================================
 * HyperMind Tests
 *===========================================================================*/

static hm_context_t g_hm = NULL;

static hm_config_t hm_default_config(void) {
    hm_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.max_actors        = 64;
    cfg.message_queue_size = 128;
    cfg.worker_threads    = 2;
    cfg.tensor_pool_mb    = 64;
    cfg.enable_cuda       = false;
    return cfg;
}

static int test_hm_lifecycle(void) {
    hm_config_t cfg = hm_default_config();
    hm_context_t ctx = NULL;
    ASSERT_OK(hm_init(&cfg, &ctx), "hm_init");
    ASSERT_NOT_NULL(ctx, "context non-NULL");
    hm_shutdown(ctx);
    return 1;
}

static void dummy_msg_handler(hm_actor_id_t actor,
                               const hm_message_t* msg,
                               void* user_data) {
    (void)actor; (void)msg; (void)user_data;
}

static int test_hm_actors(void) {
    hm_actor_id_t reactor_id, manager_id;

    ASSERT_OK(hm_create_actor(g_hm, HM_ACTOR_REACTOR, "reactor1",
                               dummy_msg_handler, NULL, &reactor_id),
              "create reactor actor");
    ASSERT_NEQ(reactor_id, 0u, "reactor id non-zero");

    ASSERT_OK(hm_create_actor(g_hm, HM_ACTOR_MANAGER, "manager1",
                               dummy_msg_handler, NULL, &manager_id),
              "create manager actor");
    ASSERT_NEQ(reactor_id, manager_id, "actor IDs should differ");

    ASSERT_OK(hm_destroy_actor(g_hm, reactor_id), "destroy reactor actor");
    return 1;
}

static int test_hm_messages(void) {
    hm_actor_id_t aid;
    hm_create_actor(g_hm, HM_ACTOR_REACTOR, "msg_reactor",
                    dummy_msg_handler, NULL, &aid);

    hm_message_t msg = {
        .type = HM_MSG_FORWARD,
        .sender = 0,
        .recipient = aid,
        .payload = NULL,
        .payload_size = 0,
        .priority = 1
    };
    ASSERT_OK(hm_send_message(g_hm, &msg), "send_message");

    ASSERT_OK(hm_broadcast_message(g_hm, HM_ACTOR_REACTOR, &msg),
              "broadcast_message");
    return 1;
}

static void dummy_stream_callback(hm_stream_id_t s,
                                   const hm_stream_event_t* e,
                                   void* u) {
    (void)s; (void)e; (void)u;
}

static int test_hm_streams(void) {
    hm_stream_id_t sid;
    ASSERT_OK(hm_create_stream(g_hm, HM_STREAM_CPU,
                                dummy_stream_callback,
                                NULL, &sid),
              "create stream");
    ASSERT_NEQ(sid, 0u, "stream id non-zero");

    float data[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    ASSERT_OK(hm_stream_push(g_hm, sid, data, sizeof(data)), "stream_push");
    ASSERT_OK(hm_stream_close(g_hm, sid), "stream_close");
    return 1;
}

static int test_hm_sessions(void) {
    hm_reactor_config_t rcfg = {
        .id = 1,
        .name = "session_reactor",
        .high_priority_queue_size = 32,
        .normal_priority_queue_size = 64,
        .state_cache_mb = 4,
        .gpu_device_id = -1,
        .enable_cuda = false,
        .enable_opencl = false
    };

    hm_session_config_t scfg = {
        .session_name = "test_session",
        .reactor_count = 1,
        .reactor_configs = &rcfg,
        .enable_load_balancing = false,
        .load_threshold = 0.8f,
        .topology = "star",
        .enable_checkpointing = false,
        .checkpoint_interval_sec = 60,
        .checkpoint_path = "/tmp"
    };

    hm_session_t session = NULL;
    ASSERT_OK(hm_create_session(g_hm, &scfg, &session), "create_session");
    ASSERT_NOT_NULL(session, "session non-NULL");

    ASSERT_OK(hm_start_session(session), "start_session");
    ASSERT_OK(hm_stop_session(session), "stop_session");

    hm_destroy_session(session);
    return 1;
}

static int test_hm_commands(void) {
    hm_reactor_config_t rcfg = {
        .id = 2, .name = "cmd_reactor",
        .high_priority_queue_size = 16, .normal_priority_queue_size = 32
    };
    hm_session_config_t scfg = {
        .session_name = "cmd_session",
        .reactor_count = 1, .reactor_configs = &rcfg,
        .topology = "ring"
    };

    hm_session_t session = NULL;
    hm_create_session(g_hm, &scfg, &session);
    hm_start_session(session);

    hm_command_id_t cmd_id;
    float input_val = 3.14f;
    ASSERT_OK(hm_submit_command(session, HM_CMD_PREDICT,
                                &input_val, sizeof(float),
                                NULL, NULL, &cmd_id),
              "submit_command");

    hm_command_t result;
    ASSERT_OK(hm_wait_command(session, cmd_id, 1000, &result),
              "wait_command");
    ASSERT_EQ(result.status, HM_CMD_COMPLETED, "command should complete");

    hm_destroy_session(session);
    return 1;
}

static int test_hm_model_forward_linear(void) {
    /* Build a simple 2-layer linear network: [4] -> [8] -> [2] */
    size_t in_shape[1]   = {4};
    size_t mid_shape[1]  = {8};
    size_t out_shape_dim[1] = {2};

    hm_layer_config_t layers[2];
    memset(layers, 0, sizeof(layers));

    layers[0].type = HM_LAYER_LINEAR;
    layers[0].name = "fc1";
    layers[0].input_shape  = in_shape;   layers[0].input_ndim  = 1;
    layers[0].output_shape = mid_shape;  layers[0].output_ndim = 1;
    layers[0].activation   = HM_ACTIVATION_RELU;

    layers[1].type = HM_LAYER_LINEAR;
    layers[1].name = "fc2";
    layers[1].input_shape  = mid_shape;     layers[1].input_ndim  = 1;
    layers[1].output_shape = out_shape_dim; layers[1].output_ndim = 1;
    layers[1].activation   = HM_ACTIVATION_SIGMOID;

    hm_model_config_t mcfg = {
        .name = "test_net",
        .layers = layers,
        .layer_count = 2,
        .learning_rate = 0.01f,
        .optimizer = "adam",
        .loss = "mse"
    };

    hm_model_t model = NULL;
    ASSERT_OK(hm_create_model(g_hm, &mcfg, &model), "create_model");

    /* Create input tensor [1, 4] */
    float data[4] = {1.0f, 0.5f, -0.3f, 0.8f};
    hm_tensor_t* input = NULL;
    size_t shape[1] = {4};
    ASSERT_OK(hm_tensor_from_data(g_hm, data, shape, 1, HM_DTYPE_FLOAT32, &input),
              "tensor_from_data");

    hm_tensor_t* output = NULL;
    ASSERT_OK(hm_model_forward(model, input, &output), "model_forward");
    ASSERT_NOT_NULL(output, "output tensor non-NULL");
    ASSERT_GT(output->size, 0u, "output size > 0");

    hm_tensor_free(input);
    hm_tensor_free(output);
    hm_destroy_model(model);
    return 1;
}

static int test_hm_model_train_batch(void) {
    size_t in_shape[1] = {4};
    size_t out_shape[1] = {2};

    hm_layer_config_t layer;
    memset(&layer, 0, sizeof(layer));
    layer.type = HM_LAYER_LINEAR;
    layer.name = "single";
    layer.input_shape  = in_shape;  layer.input_ndim  = 1;
    layer.output_shape = out_shape; layer.output_ndim = 1;
    layer.activation   = HM_ACTIVATION_RELU;

    hm_model_config_t mcfg = {
        .name = "train_net", .layers = &layer, .layer_count = 1,
        .learning_rate = 0.01f, .optimizer = "sgd", .loss = "mse"
    };

    hm_model_t model = NULL;
    hm_create_model(g_hm, &mcfg, &model);

    float inp_data[4]  = {1.0f, 2.0f, 3.0f, 4.0f};
    float tgt_data[2]  = {0.5f, 0.5f};

    hm_tensor_t* inp = NULL; hm_tensor_t* tgt = NULL;
    size_t s4[1] = {4};
    size_t s2[1] = {2};
    hm_tensor_from_data(g_hm, inp_data, s4, 1, HM_DTYPE_FLOAT32, &inp);
    hm_tensor_from_data(g_hm, tgt_data, s2, 1, HM_DTYPE_FLOAT32, &tgt);

    float loss = -1.0f;
    ASSERT_OK(hm_model_train_batch(model, inp, tgt, &loss), "train_batch");
    ASSERT_GTE(loss, 0.0f, "loss should be non-negative");

    hm_tensor_free(inp);
    hm_tensor_free(tgt);
    hm_destroy_model(model);
    return 1;
}

static int test_hm_model_save_load(void) {
    size_t in_shape[1] = {2};
    size_t out_shape[1] = {2};

    hm_layer_config_t layer;
    memset(&layer, 0, sizeof(layer));
    layer.type = HM_LAYER_LINEAR;
    layer.name = "sl_layer";
    layer.input_shape  = in_shape;  layer.input_ndim  = 1;
    layer.output_shape = out_shape; layer.output_ndim = 1;

    hm_model_config_t mcfg = {
        .name = "sl_model", .layers = &layer, .layer_count = 1,
        .learning_rate = 0.001f, .optimizer = "adam", .loss = "mse"
    };

    hm_model_t model = NULL;
    hm_create_model(g_hm, &mcfg, &model);

    const char* path = "/tmp/test_hypermind.bin";
    ASSERT_OK(hm_model_save(model, path), "model_save");

    hm_model_t loaded = NULL;
    ASSERT_OK(hm_model_load(g_hm, path, &loaded), "model_load");
    ASSERT_NOT_NULL(loaded, "loaded model non-NULL");
    ASSERT_EQ(loaded->layer_count, 1u, "loaded model should have 1 layer");

    hm_destroy_model(model);
    hm_destroy_model(loaded);
    return 1;
}

static int test_hm_tensor_ops(void) {
    size_t shape2[2] = {2, 3};
    hm_tensor_t *a = NULL, *b = NULL, *c = NULL;

    ASSERT_OK(hm_tensor_create(g_hm, shape2, 2, HM_DTYPE_FLOAT32, -1, &a), "create a");
    ASSERT_OK(hm_tensor_create(g_hm, shape2, 2, HM_DTYPE_FLOAT32, -1, &b), "create b");

    float* ad = (float*)a->data;
    float* bd = (float*)b->data;
    for (size_t i = 0; i < 6; i++) { ad[i] = (float)i; bd[i] = 1.0f; }

    ASSERT_OK(hm_tensor_add(g_hm, a, b, &c), "tensor_add");
    ASSERT_NOT_NULL(c, "add result non-NULL");
    ASSERT_NEAR(((float*)c->data)[0], 1.0f, 0.001f, "0+1=1");

    hm_tensor_free(a); hm_tensor_free(b); hm_tensor_free(c);

    /* Matrix multiply [2x3] * [3x2] = [2x2] */
    size_t sa[2] = {2, 3};
    size_t sb[2] = {3, 2};
    hm_tensor_t *ma = NULL, *mb = NULL, *mc = NULL;
    float ma_data[6] = {1,2,3,4,5,6};
    float mb_data[6] = {7,8,9,10,11,12};
    hm_tensor_from_data(g_hm, ma_data, sa, 2, HM_DTYPE_FLOAT32, &ma);
    hm_tensor_from_data(g_hm, mb_data, sb, 2, HM_DTYPE_FLOAT32, &mb);
    ASSERT_OK(hm_tensor_matmul(g_hm, ma, mb, &mc), "tensor_matmul");
    ASSERT_EQ(mc->size, 4u, "result [2x2] should have 4 elements");
    hm_tensor_free(ma); hm_tensor_free(mb); hm_tensor_free(mc);
    return 1;
}

static int test_hm_cluster(void) {
    ASSERT_OK(hm_cluster_join(g_hm, "127.0.0.1", 9090), "cluster_join");
    size_t shape[1] = {4};
    hm_tensor_t* t = NULL;
    hm_tensor_create(g_hm, shape, 1, HM_DTYPE_FLOAT32, -1, &t);
    ASSERT_OK(hm_distributed_all_reduce(g_hm, t), "all_reduce");
    ASSERT_OK(hm_distributed_broadcast(g_hm, t, 0), "broadcast");
    ASSERT_OK(hm_cluster_leave(g_hm), "cluster_leave");
    hm_tensor_free(t);
    return 1;
}

static int test_hm_stats(void) {
    hm_stats_t stats;
    ASSERT_OK(hm_get_stats(g_hm, &stats), "get_stats");
    hm_reset_stats(g_hm);
    ASSERT_OK(hm_get_stats(g_hm, &stats), "get_stats after reset");
    return 1;
}

/*===========================================================================
 * CogZero Tests
 *===========================================================================*/

static cz_context_t g_cz = NULL;

static cz_config_t cz_make_config(aten_context_t as) {
    cz_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.name = "test_agent";
    cfg.description = "CogZero test agent";
    cfg.atomspace = as;
    cfg.planning.max_plan_depth = 5;
    cfg.planning.max_plan_breadth = 10;
    cfg.planning.min_action_utility = 0.1f;
    cfg.planning.enable_hierarchical_planning = true;
    cfg.planning.enable_replanning = true;
    cfg.learning.default_type = CZ_LEARN_REINFORCEMENT;
    cfg.learning.replay_buffer_size = 64;
    cfg.learning.learning_rate = 0.01f;
    cfg.learning.discount_factor = 0.99f;
    cfg.learning.enable_moses = true;
    cfg.learning.moses_max_evals = 100;
    cfg.memory.working_memory_capacity = 32;
    cfg.memory.episodic_memory_capacity = 256;
    cfg.memory.importance_decay_rate = 0.9f;
    cfg.memory.recency_decay_rate = 0.95f;
    cfg.memory.enable_consolidation = true;
    cfg.cognitive_cycle_ms = 0;
    cfg.worker_threads = 1;
    cfg.enable_reflection = true;
    cfg.reflection_interval = 10;
    cfg.action_threshold = 0.3f;
    return cfg;
}

static int test_cz_lifecycle(void) {
    cz_context_t ctx = NULL;
    ASSERT_OK(cz_init(&ctx), "cz_init");
    ASSERT_NOT_NULL(ctx, "context non-NULL");
    cz_shutdown(ctx);
    return 1;
}

static int test_cz_create_agent(void) {
    cz_config_t cfg = cz_make_config(g_aten);
    cz_agent_t agent = NULL;
    ASSERT_OK(cz_create_agent(g_cz, &cfg, &agent), "create_agent");
    ASSERT_NOT_NULL(agent, "agent non-NULL");
    cz_destroy_agent(agent);
    return 1;
}

static int test_cz_perception(void) {
    cz_config_t cfg = cz_make_config(g_aten);
    cz_agent_t agent = NULL;
    cz_create_agent(g_cz, &cfg, &agent);

    ASSERT_OK(cz_perceive_text(agent, "The world is beautiful"), "perceive_text");
    ASSERT_OK(cz_perceive(agent, CZ_PERCEPT_API, "raw_data", 8, "sensor"), "perceive raw");

    cz_percept_t* percepts = NULL;
    size_t count = 0;
    ASSERT_OK(cz_get_percepts(agent, &percepts, &count), "get_percepts");
    ASSERT_GT(count, 0u, "should have percepts");

    cz_destroy_agent(agent);
    return 1;
}

static int test_cz_goals_and_planning(void) {
    cz_config_t cfg = cz_make_config(g_aten);

    /* Register a tool so planning can build actions */
    cz_tool_t tool = {
        .name = "search",
        .description = "Search the knowledge base",
        .type = CZ_TOOL_ATOMSPACE,
        .schema_json = "{}",
        .implementation = NULL,
        .requires_confirmation = false,
        .cost = 0.1f
    };
    cfg.tools = &tool;
    cfg.tool_count = 1;

    cz_agent_t agent = NULL;
    cz_create_agent(g_cz, &cfg, &agent);

    cz_goal_t* goal = NULL;
    ASSERT_OK(cz_add_goal(agent, "learn about dogs", 0.9f, &goal), "add_goal");
    ASSERT_NOT_NULL(goal, "goal non-NULL");
    ASSERT_EQ(goal->status, CZ_GOAL_ACTIVE, "goal should be active");

    cz_plan_t* plan = NULL;
    ASSERT_OK(cz_create_plan(agent, goal, &plan), "create_plan");
    ASSERT_NOT_NULL(plan, "plan non-NULL");

    ASSERT_OK(cz_execute_plan(agent, plan), "execute_plan");

    cz_goal_t* cur = NULL;
    cz_get_current_goal(agent, &cur);

    cz_destroy_agent(agent);
    return 1;
}

static int test_cz_learning(void) {
    cz_config_t cfg = cz_make_config(g_aten);
    cz_agent_t agent = NULL;
    cz_create_agent(g_cz, &cfg, &agent);

    cz_learning_sample_t sample = {
        .type = CZ_LEARN_REINFORCEMENT,
        .input = "action",
        .input_size = 6,
        .target = NULL,
        .target_size = 0,
        .reward = 1.0f,
        .terminal = false
    };
    ASSERT_OK(cz_learn(agent, &sample), "learn");
    ASSERT_OK(cz_learn_from_experience(agent), "learn_from_experience");

    char* solution = NULL;
    ASSERT_OK(cz_run_moses(agent, "learn to navigate", &solution), "run_moses");
    ASSERT_NOT_NULL(solution, "MOSES solution non-NULL");
    free(solution);

    cz_destroy_agent(agent);
    return 1;
}

static int test_cz_memory(void) {
    cz_config_t cfg = cz_make_config(g_aten);
    cz_agent_t agent = NULL;
    cz_create_agent(g_cz, &cfg, &agent);

    ASSERT_OK(cz_remember(agent, CZ_MEM_SEMANTIC,  "dogs are animals", 0.9f), "remember semantic");
    ASSERT_OK(cz_remember(agent, CZ_MEM_EPISODIC,  "saw a dog yesterday", 0.6f), "remember episodic");
    ASSERT_OK(cz_remember(agent, CZ_MEM_WORKING,   "current task: recall dogs", 0.8f), "remember working");

    cz_memory_t* memories = NULL;
    size_t count = 0;
    ASSERT_OK(cz_recall(agent, "dog", 10, &memories, &count), "recall");
    ASSERT_GT(count, 0u, "should recall memories about dogs");

    /* Consolidate */
    ASSERT_OK(cz_consolidate_memories(agent), "consolidate_memories");

    /* Forget first recalled memory */
    if (count > 0) {
        uint64_t mid = memories->id;
        cog_result_t res = cz_forget(agent, mid);
        /* Forget returns OK or NOT_FOUND depending on whether it survived consolidate */
        (void)res;
    }

    cz_destroy_agent(agent);
    return 1;
}

static int test_cz_communication(void) {
    cz_config_t cfg = cz_make_config(g_aten);
    cz_agent_t a1 = NULL, a2 = NULL;
    cz_create_agent(g_cz, &cfg, &a1);
    cz_create_agent(g_cz, &cfg, &a2);

    ASSERT_OK(cz_send_message(a1, a2, "hello from a1"), "send_message");

    cz_message_t* msgs = NULL;
    size_t count = 0;
    ASSERT_OK(cz_receive_messages(a2, &msgs, &count), "receive_messages");
    ASSERT_GT(count, 0u, "a2 should have messages");

    char* response = NULL;
    ASSERT_OK(cz_generate_response(a2, "What is 2+2?", &response),
              "generate_response");
    ASSERT_NOT_NULL(response, "response non-NULL");
    free(response);

    cz_destroy_agent(a1);
    cz_destroy_agent(a2);
    return 1;
}

static int test_cz_tools(void) {
    cz_config_t cfg = cz_make_config(g_aten);
    cz_agent_t agent = NULL;
    cz_create_agent(g_cz, &cfg, &agent);

    cz_tool_t tool = {
        .name = "calculator",
        .description = "Perform arithmetic",
        .type = CZ_TOOL_FUNCTION,
        .schema_json = "{\"op\":\"add\"}",
        .implementation = NULL,
        .requires_confirmation = false,
        .cost = 0.0f
    };
    ASSERT_OK(cz_register_tool(agent, &tool), "register_tool");

    cz_tool_result_t result;
    ASSERT_OK(cz_invoke_tool(agent, "calculator", "{\"a\":1,\"b\":2}", &result),
              "invoke_tool");
    ASSERT_EQ(result.success, true, "tool invocation should succeed");

    cz_tool_t* tools = NULL;
    size_t count = 0;
    ASSERT_OK(cz_list_tools(agent, &tools, &count), "list_tools");
    ASSERT_GT(count, 0u, "should list at least one tool");

    cz_destroy_agent(agent);
    return 1;
}

static int test_cz_atomspace_integration(void) {
    cz_config_t cfg = cz_make_config(g_aten);
    cz_agent_t agent = NULL;
    cz_create_agent(g_cz, &cfg, &agent);

    aten_context_t as = NULL;
    ASSERT_OK(cz_get_atomspace(agent, &as), "get_atomspace");
    ASSERT_EQ(as, g_aten, "should return configured AtomSpace");

    aten_handle_t* results = NULL;
    size_t count = 0;
    cz_query_atomspace(agent, "*", &results, &count);
    free(results);

    char* conclusion = NULL;
    cz_run_pln(agent, "ConceptNode \"dog\"", 5, &conclusion);
    ASSERT_NOT_NULL(conclusion, "PLN conclusion non-NULL");
    free(conclusion);

    ASSERT_OK(cz_sync_to_atomspace(agent), "sync_to_atomspace");

    cz_destroy_agent(agent);
    return 1;
}

static int test_cz_reflection(void) {
    cz_config_t cfg = cz_make_config(g_aten);
    cz_agent_t agent = NULL;
    cz_create_agent(g_cz, &cfg, &agent);

    char* reflection = NULL;
    ASSERT_OK(cz_reflect(agent, &reflection), "reflect");
    ASSERT_NOT_NULL(reflection, "reflection string non-NULL");
    ASSERT_GT(strlen(reflection), 0u, "reflection non-empty");
    free(reflection);

    ASSERT_OK(cz_self_improve(agent, "Be more concise"), "self_improve");

    cz_destroy_agent(agent);
    return 1;
}

static int test_cz_cognitive_cycle(void) {
    cz_config_t cfg = cz_make_config(g_aten);
    cz_agent_t agent = NULL;
    cz_create_agent(g_cz, &cfg, &agent);

    cz_perceive_text(agent, "observe something");
    cz_add_goal(agent, "process observation", 0.7f, NULL);

    ASSERT_OK(cz_cycle(agent), "cognitive cycle should succeed");

    cz_stats_t stats;
    ASSERT_OK(cz_get_stats(agent, &stats), "get_stats");
    ASSERT_GT(stats.cognitive_cycles, 0u, "should have run a cycle");

    cz_reset_stats(agent);
    cz_get_stats(agent, &stats);
    ASSERT_EQ(stats.cognitive_cycles, 0u, "after reset cycles should be 0");

    cz_destroy_agent(agent);
    return 1;
}

static int test_cz_distributed(void) {
    cz_config_t cfg = cz_make_config(g_aten);
    cz_agent_t agent = NULL;
    cz_create_agent(g_cz, &cfg, &agent);

    /* Discover (returns empty in non-distributed mode) */
    cz_remote_agent_t* remotes = NULL;
    size_t count = 0;
    ASSERT_OK(cz_discover_agents(g_cz, &remotes, &count), "discover_agents");

    /* Connect to a simulated remote */
    cz_remote_agent_t* remote = NULL;
    ASSERT_OK(cz_connect_agent(agent, "127.0.0.1", 7070, &remote),
              "connect_agent");
    ASSERT_NOT_NULL(remote, "remote descriptor non-NULL");

    /* Delegate a task */
    char* result = NULL;
    ASSERT_OK(cz_delegate_task(agent, remote, "summarize knowledge", &result),
              "delegate_task");
    ASSERT_NOT_NULL(result, "delegation result non-NULL");
    free(result);
    free(remote);

    cz_destroy_agent(agent);
    return 1;
}

static int test_cz_persistence(void) {
    cz_config_t cfg = cz_make_config(g_aten);
    cz_agent_t agent = NULL;
    cz_create_agent(g_cz, &cfg, &agent);

    cz_remember(agent, CZ_MEM_SEMANTIC, "persisted fact", 0.95f);

    const char* path = "/tmp/test_cogzero.bin";
    ASSERT_OK(cz_save(agent, path), "save agent");

    cz_agent_t loaded = NULL;
    ASSERT_OK(cz_load(g_cz, path, &loaded), "load agent");
    ASSERT_NOT_NULL(loaded, "loaded agent non-NULL");

    cz_destroy_agent(agent);
    cz_destroy_agent(loaded);
    return 1;
}

/*===========================================================================
 * Main
 *===========================================================================*/

int main(void) {
    printf("=================================================================\n");
    printf("CoGWXP-OS9 Framework Test Suite\n");
    printf("  ATenSpace | HyperMind | CogZero\n");
    printf("=================================================================\n\n");

    /* ---- ATenSpace ---- */
    printf("[ATenSpace]\n");

    /* Stand-alone lifecycle test (uses its own context) */
    RUN_TEST(test_aten_lifecycle);

    /* Create shared ATenSpace context for remaining tests */
    aten_config_t aten_cfg = aten_default_config();
    if (aten_init(&aten_cfg, &g_aten) != COG_OK) {
        fprintf(stderr, "FATAL: aten_init failed\n");
        return 1;
    }

    RUN_TEST(test_aten_create_nodes);
    RUN_TEST(test_aten_create_links);
    RUN_TEST(test_aten_get_atom);
    RUN_TEST(test_aten_tv_ops);
    RUN_TEST(test_aten_av_ops);
    RUN_TEST(test_aten_stimulate);
    RUN_TEST(test_aten_ecan_step);
    RUN_TEST(test_aten_attention_focus);
    RUN_TEST(test_aten_embeddings);
    RUN_TEST(test_aten_tensor_similarity);
    RUN_TEST(test_aten_batch_embed);
    RUN_TEST(test_aten_find_similar);
    RUN_TEST(test_aten_get_incoming_outgoing);
    RUN_TEST(test_aten_get_by_type);
    RUN_TEST(test_aten_pattern_match);
    RUN_TEST(test_aten_pln_deduction);
    RUN_TEST(test_aten_forward_chain);
    RUN_TEST(test_aten_backward_chain);
    RUN_TEST(test_aten_remove_atom);
    RUN_TEST(test_aten_save_load);
    RUN_TEST(test_aten_export_import_atomese);
    RUN_TEST(test_aten_stats);

    /* ---- HyperMind ---- */
    printf("\n[HyperMind]\n");

    RUN_TEST(test_hm_lifecycle);

    hm_config_t hm_cfg = hm_default_config();
    if (hm_init(&hm_cfg, &g_hm) != COG_OK) {
        fprintf(stderr, "FATAL: hm_init failed\n");
        aten_shutdown(g_aten);
        return 1;
    }

    RUN_TEST(test_hm_actors);
    RUN_TEST(test_hm_messages);
    RUN_TEST(test_hm_streams);
    RUN_TEST(test_hm_sessions);
    RUN_TEST(test_hm_commands);
    RUN_TEST(test_hm_model_forward_linear);
    RUN_TEST(test_hm_model_train_batch);
    RUN_TEST(test_hm_model_save_load);
    RUN_TEST(test_hm_tensor_ops);
    RUN_TEST(test_hm_cluster);
    RUN_TEST(test_hm_stats);

    /* ---- CogZero ---- */
    printf("\n[CogZero]\n");

    RUN_TEST(test_cz_lifecycle);

    if (cz_init(&g_cz) != COG_OK) {
        fprintf(stderr, "FATAL: cz_init failed\n");
        hm_shutdown(g_hm);
        aten_shutdown(g_aten);
        return 1;
    }

    RUN_TEST(test_cz_create_agent);
    RUN_TEST(test_cz_perception);
    RUN_TEST(test_cz_goals_and_planning);
    RUN_TEST(test_cz_learning);
    RUN_TEST(test_cz_memory);
    RUN_TEST(test_cz_communication);
    RUN_TEST(test_cz_tools);
    RUN_TEST(test_cz_atomspace_integration);
    RUN_TEST(test_cz_reflection);
    RUN_TEST(test_cz_cognitive_cycle);
    RUN_TEST(test_cz_distributed);
    RUN_TEST(test_cz_persistence);

    /* ---- Teardown ---- */
    cz_shutdown(g_cz);
    hm_shutdown(g_hm);
    aten_shutdown(g_aten);

    printf("\n=================================================================\n");
    printf("Results: %d/%d passed", g_tests_passed, g_tests_run);
    if (g_tests_failed > 0) {
        printf(", %d FAILED", g_tests_failed);
    }
    printf("\n=================================================================\n");

    return g_tests_failed > 0 ? 1 : 0;
}
