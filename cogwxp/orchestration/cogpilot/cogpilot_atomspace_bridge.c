/**
 * @file cogpilot_atomspace_bridge.c
 * @brief CogPilot <-> AtomSpace Bridge Implementation
 * 
 * Wires CogPilot agent registrations, task completions, and query results
 * into the shared AtomSpace, enabling PLN reasoning over agent state and
 * cognitive scheduling based on agent attention values.
 * 
 * Integration Pattern:
 *   Agent Registration -> ConceptNode + InheritanceLink(agent, "CogPilot:Agent")
 *   Task Completion   -> EvaluationLink(PredicateNode "task:result", ListLink(...))
 *   Agent Query       -> Pattern matching over AtomSpace agent subgraph
 * 
 * @copyright CoGWXP-OS9 Project
 */

#include "cogpilot.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

/*===========================================================================
 * AtomSpace Bridge Context
 *===========================================================================*/

typedef struct cogpilot_as_bridge {
    atomspace_t atomspace;
    cogpilot_context_t cogpilot;
    
    /* Category nodes (cached for performance) */
    atom_handle_t agent_category;       /* "CogPilot:Agent" */
    atom_handle_t task_category;        /* "CogPilot:Task" */
    atom_handle_t capability_category;  /* "CogPilot:Capability" */
    
    /* Predicate nodes (cached) */
    atom_handle_t pred_has_capability;  /* "cogpilot:hasCapability" */
    atom_handle_t pred_task_result;     /* "cogpilot:taskResult" */
    atom_handle_t pred_agent_status;    /* "cogpilot:agentStatus" */
    atom_handle_t pred_task_assigned;   /* "cogpilot:taskAssignedTo" */
    atom_handle_t pred_success_rate;    /* "cogpilot:successRate" */
    
    /* Statistics */
    struct {
        uint64_t agents_synced;
        uint64_t tasks_synced;
        uint64_t queries_executed;
    } stats;
    pthread_mutex_t stats_lock;
    
    bool initialized;
} cogpilot_as_bridge_t;

/* Global bridge instance (one per orchestration context) */
static cogpilot_as_bridge_t* g_bridge = NULL;

/*===========================================================================
 * Bridge Initialization
 *===========================================================================*/

/**
 * Initialize the CogPilot-AtomSpace bridge.
 * Creates the category and predicate nodes that form the schema
 * for representing agents and tasks in the hypergraph.
 */
cog_result_t cogpilot_bridge_init(
    atomspace_t atomspace,
    cogpilot_context_t cogpilot
) {
    if (!atomspace || !cogpilot) return COG_ERROR_INVALID_PARAM;
    
    cogpilot_as_bridge_t* bridge = calloc(1, sizeof(cogpilot_as_bridge_t));
    if (!bridge) return COG_ERROR_MEMORY;
    
    bridge->atomspace = atomspace;
    bridge->cogpilot = cogpilot;
    pthread_mutex_init(&bridge->stats_lock, NULL);
    
    /* Create category nodes - these form the type hierarchy */
    bridge->agent_category = atomspace_add_node(
        atomspace, ATOM_TYPE_CONCEPT, "CogPilot:Agent", NULL);
    bridge->task_category = atomspace_add_node(
        atomspace, ATOM_TYPE_CONCEPT, "CogPilot:Task", NULL);
    bridge->capability_category = atomspace_add_node(
        atomspace, ATOM_TYPE_CONCEPT, "CogPilot:Capability", NULL);
    
    /* Create predicate nodes - these define the relationship vocabulary */
    bridge->pred_has_capability = atomspace_add_node(
        atomspace, ATOM_TYPE_PREDICATE, "cogpilot:hasCapability", NULL);
    bridge->pred_task_result = atomspace_add_node(
        atomspace, ATOM_TYPE_PREDICATE, "cogpilot:taskResult", NULL);
    bridge->pred_agent_status = atomspace_add_node(
        atomspace, ATOM_TYPE_PREDICATE, "cogpilot:agentStatus", NULL);
    bridge->pred_task_assigned = atomspace_add_node(
        atomspace, ATOM_TYPE_PREDICATE, "cogpilot:taskAssignedTo", NULL);
    bridge->pred_success_rate = atomspace_add_node(
        atomspace, ATOM_TYPE_PREDICATE, "cogpilot:successRate", NULL);
    
    /* Create capability type nodes */
    static const char* capability_names[] = {
        "cap:reasoning", "cap:learning", "cap:perception", "cap:action",
        "cap:communication", "cap:planning", "cap:memory", "cap:inference",
        "cap:generation", "cap:embedding", "cap:tool_use", "cap:code_exec"
    };
    
    for (int i = 0; i < 12; i++) {
        atom_handle_t cap_node = atomspace_add_node(
            atomspace, ATOM_TYPE_CONCEPT, capability_names[i], NULL);
        if (cap_node != ATOM_HANDLE_INVALID) {
            /* InheritanceLink: cap_node -> capability_category */
            atom_handle_t out[2] = {cap_node, bridge->capability_category};
            truth_value_t tv = {.strength = 1.0, .confidence = 1.0};
            atomspace_add_link(atomspace, ATOM_TYPE_INHERITANCE, out, 2, &tv);
        }
    }
    
    bridge->initialized = true;
    g_bridge = bridge;
    
    return COG_OK;
}

/**
 * Shutdown the CogPilot-AtomSpace bridge.
 */
void cogpilot_bridge_shutdown(void) {
    if (!g_bridge) return;
    
    pthread_mutex_destroy(&g_bridge->stats_lock);
    free(g_bridge);
    g_bridge = NULL;
}

/*===========================================================================
 * Agent -> AtomSpace Sync
 *===========================================================================*/

/**
 * Sync a registered agent into the AtomSpace.
 * Creates:
 *   (ConceptNode "agent:<name>")
 *   (InheritanceLink (ConceptNode "agent:<name>") (ConceptNode "CogPilot:Agent"))
 *   (EvaluationLink (PredicateNode "cogpilot:hasCapability")
 *     (ListLink (ConceptNode "agent:<name>") (ConceptNode "cap:<capability>")))
 * 
 * @param agent The agent to sync
 * @param agent_atom Output: the atom handle for the agent node
 * @return COG_OK on success
 */
cog_result_t cogpilot_bridge_sync_agent(
    const cogpilot_agent_t* agent,
    atom_handle_t* agent_atom
) {
    if (!g_bridge || !g_bridge->initialized) return COG_ERROR_INIT;
    if (!agent || !agent->name || !agent_atom) return COG_ERROR_INVALID_PARAM;
    
    atomspace_t as = g_bridge->atomspace;
    
    /* Create agent node with truth value reflecting reliability */
    char agent_name[256];
    snprintf(agent_name, sizeof(agent_name), "agent:%s", agent->name);
    
    truth_value_t agent_tv = {
        .strength = 1.0,
        .confidence = 0.8
    };
    
    atom_handle_t a_atom = atomspace_add_node(
        as, ATOM_TYPE_CONCEPT, agent_name, &agent_tv);
    
    if (a_atom == ATOM_HANDLE_INVALID) return COG_ERROR_MEMORY;
    
    /* Create InheritanceLink: agent -> CogPilot:Agent */
    atom_handle_t inh_out[2] = {a_atom, g_bridge->agent_category};
    truth_value_t inh_tv = {.strength = 1.0, .confidence = 1.0};
    atomspace_add_link(as, ATOM_TYPE_INHERITANCE, inh_out, 2, &inh_tv);
    
    /* Create capability links */
    static const struct {
        uint32_t flag;
        const char* name;
    } cap_map[] = {
        {0x0001, "cap:reasoning"},
        {0x0002, "cap:learning"},
        {0x0004, "cap:perception"},
        {0x0008, "cap:action"},
        {0x0010, "cap:communication"},
        {0x0020, "cap:planning"},
        {0x0040, "cap:memory"},
        {0x0080, "cap:inference"},
        {0x0100, "cap:generation"},
        {0x0200, "cap:embedding"},
        {0x0400, "cap:tool_use"},
        {0x0800, "cap:code_exec"},
    };
    
    for (int i = 0; i < 12; i++) {
        if (agent->capabilities & cap_map[i].flag) {
            atom_handle_t cap_node = atomspace_add_node(
                as, ATOM_TYPE_CONCEPT, cap_map[i].name, NULL);
            
            if (cap_node != ATOM_HANDLE_INVALID) {
                /* EvaluationLink(hasCapability, ListLink(agent, capability)) */
                atom_handle_t list_out[2] = {a_atom, cap_node};
                atom_handle_t list_link = atomspace_add_link(
                    as, ATOM_TYPE_LIST, list_out, 2, NULL);
                
                if (list_link != ATOM_HANDLE_INVALID) {
                    atom_handle_t eval_out[2] = {g_bridge->pred_has_capability, list_link};
                    truth_value_t cap_tv = {.strength = 1.0, .confidence = 0.9};
                    atomspace_add_link(as, ATOM_TYPE_EVALUATION, eval_out, 2, &cap_tv);
                }
            }
        }
    }
    
    /* Stimulate the agent atom to bring it into attentional focus */
    atomspace_stimulate(as, a_atom, 10);
    
    /* Update stats */
    pthread_mutex_lock(&g_bridge->stats_lock);
    g_bridge->stats.agents_synced++;
    pthread_mutex_unlock(&g_bridge->stats_lock);
    
    *agent_atom = a_atom;
    return COG_OK;
}

/*===========================================================================
 * Task Results -> AtomSpace
 *===========================================================================*/

/**
 * Record a task completion in the AtomSpace.
 * Creates:
 *   (EvaluationLink (PredicateNode "cogpilot:taskResult")
 *     (ListLink
 *       (ConceptNode "task:<id>")
 *       (ConceptNode "agent:<agent_name>")
 *       (ConceptNode "<result_summary>")
 *     ))
 * 
 * The truth value of the EvaluationLink encodes task success/confidence.
 * 
 * @param task_id The completed task ID
 * @param agent_atom The agent that completed the task
 * @param result_summary Brief description of the result
 * @param success Whether the task succeeded
 * @param confidence Confidence in the result (0.0-1.0)
 * @return COG_OK on success
 */
cog_result_t cogpilot_bridge_record_task_result(
    uint64_t task_id,
    atom_handle_t agent_atom,
    const char* result_summary,
    bool success,
    double confidence
) {
    if (!g_bridge || !g_bridge->initialized) return COG_ERROR_INIT;
    if (!result_summary) return COG_ERROR_INVALID_PARAM;
    
    atomspace_t as = g_bridge->atomspace;
    
    /* Create task node */
    char task_name[64];
    snprintf(task_name, sizeof(task_name), "task:%lu", (unsigned long)task_id);
    
    truth_value_t task_tv = {
        .strength = success ? 1.0 : 0.0,
        .confidence = confidence
    };
    atom_handle_t task_atom = atomspace_add_node(
        as, ATOM_TYPE_CONCEPT, task_name, &task_tv);
    
    if (task_atom == ATOM_HANDLE_INVALID) return COG_ERROR_MEMORY;
    
    /* InheritanceLink: task -> CogPilot:Task */
    atom_handle_t inh_out[2] = {task_atom, g_bridge->task_category};
    truth_value_t inh_tv = {.strength = 1.0, .confidence = 1.0};
    atomspace_add_link(as, ATOM_TYPE_INHERITANCE, inh_out, 2, &inh_tv);
    
    /* Create result summary node */
    atom_handle_t result_atom = atomspace_add_node(
        as, ATOM_TYPE_CONCEPT, result_summary, NULL);
    
    if (result_atom == ATOM_HANDLE_INVALID) return COG_ERROR_MEMORY;
    
    /* Create EvaluationLink for the task result */
    atom_handle_t list_out[3] = {task_atom, agent_atom, result_atom};
    atom_handle_t list_link = atomspace_add_link(
        as, ATOM_TYPE_LIST, list_out, 3, NULL);
    
    if (list_link == ATOM_HANDLE_INVALID) return COG_ERROR_MEMORY;
    
    truth_value_t result_tv = {
        .strength = success ? 0.9 : 0.1,
        .confidence = confidence
    };
    atom_handle_t eval_out[2] = {g_bridge->pred_task_result, list_link};
    atomspace_add_link(as, ATOM_TYPE_EVALUATION, eval_out, 2, &result_tv);
    
    /* Record task assignment relationship */
    if (agent_atom != ATOM_HANDLE_INVALID) {
        atom_handle_t assign_list_out[2] = {task_atom, agent_atom};
        atom_handle_t assign_list = atomspace_add_link(
            as, ATOM_TYPE_LIST, assign_list_out, 2, NULL);
        
        if (assign_list != ATOM_HANDLE_INVALID) {
            atom_handle_t assign_eval_out[2] = {g_bridge->pred_task_assigned, assign_list};
            atomspace_add_link(as, ATOM_TYPE_EVALUATION, assign_eval_out, 2, NULL);
        }
    }
    
    /* Stimulate task and agent atoms */
    atomspace_stimulate(as, task_atom, 3);
    if (agent_atom != ATOM_HANDLE_INVALID) {
        atomspace_stimulate(as, agent_atom, success ? 5 : -2);
    }
    
    /* Update stats */
    pthread_mutex_lock(&g_bridge->stats_lock);
    g_bridge->stats.tasks_synced++;
    pthread_mutex_unlock(&g_bridge->stats_lock);
    
    return COG_OK;
}

/*===========================================================================
 * Agent Status -> AtomSpace
 *===========================================================================*/

/**
 * Update an agent's status in the AtomSpace.
 * Modifies the truth value of the agent's status EvaluationLink.
 * 
 * @param agent_atom The agent's atom handle
 * @param status_name Human-readable status (e.g., "idle", "busy", "error")
 * @param availability Availability score (0.0 = unavailable, 1.0 = fully available)
 * @return COG_OK on success
 */
cog_result_t cogpilot_bridge_update_agent_status(
    atom_handle_t agent_atom,
    const char* status_name,
    double availability
) {
    if (!g_bridge || !g_bridge->initialized) return COG_ERROR_INIT;
    if (agent_atom == ATOM_HANDLE_INVALID || !status_name) return COG_ERROR_INVALID_PARAM;
    
    atomspace_t as = g_bridge->atomspace;
    
    /* Create or get the status value node */
    char status_node_name[128];
    snprintf(status_node_name, sizeof(status_node_name), "status:%s", status_name);
    
    atom_handle_t status_node = atomspace_add_node(
        as, ATOM_TYPE_CONCEPT, status_node_name, NULL);
    
    if (status_node == ATOM_HANDLE_INVALID) return COG_ERROR_MEMORY;
    
    /* Create EvaluationLink(agentStatus, ListLink(agent, status)) */
    atom_handle_t list_out[2] = {agent_atom, status_node};
    atom_handle_t list_link = atomspace_add_link(
        as, ATOM_TYPE_LIST, list_out, 2, NULL);
    
    if (list_link == ATOM_HANDLE_INVALID) return COG_ERROR_MEMORY;
    
    /* Truth value encodes availability */
    truth_value_t status_tv = {
        .strength = availability,
        .confidence = 0.95
    };
    atom_handle_t eval_out[2] = {g_bridge->pred_agent_status, list_link};
    atomspace_add_link(as, ATOM_TYPE_EVALUATION, eval_out, 2, &status_tv);
    
    /* Adjust attention based on availability */
    int16_t sti_boost = (int16_t)(availability * 10.0);
    atomspace_stimulate(as, agent_atom, sti_boost);
    
    return COG_OK;
}

/*===========================================================================
 * Agent Success Rate -> AtomSpace
 *===========================================================================*/

/**
 * Update an agent's success rate in the AtomSpace.
 * This enables PLN to reason about agent reliability.
 * 
 * @param agent_atom The agent's atom handle
 * @param success_rate Success rate (0.0-1.0)
 * @param sample_count Number of tasks used to compute the rate
 * @return COG_OK on success
 */
cog_result_t cogpilot_bridge_update_success_rate(
    atom_handle_t agent_atom,
    double success_rate,
    uint64_t sample_count
) {
    if (!g_bridge || !g_bridge->initialized) return COG_ERROR_INIT;
    if (agent_atom == ATOM_HANDLE_INVALID) return COG_ERROR_INVALID_PARAM;
    
    atomspace_t as = g_bridge->atomspace;
    
    /* Compute confidence from sample count using the standard formula:
     * confidence = count / (count + k), where k is the PLN confidence parameter */
    double k = 800.0;  /* Standard PLN k-value */
    double confidence = (double)sample_count / ((double)sample_count + k);
    
    /* Update the agent node's truth value directly.
     * The strength represents the success rate, and confidence
     * represents how much evidence we have. */
    truth_value_t agent_tv = {
        .strength = success_rate,
        .confidence = confidence
    };
    atomspace_set_tv(as, agent_atom, &agent_tv);
    
    /* Also create an explicit success rate EvaluationLink for PLN reasoning */
    char rate_name[64];
    snprintf(rate_name, sizeof(rate_name), "rate:%.3f", success_rate);
    atom_handle_t rate_node = atomspace_add_node(
        as, ATOM_TYPE_CONCEPT, rate_name, NULL);
    
    if (rate_node != ATOM_HANDLE_INVALID) {
        atom_handle_t list_out[2] = {agent_atom, rate_node};
        atom_handle_t list_link = atomspace_add_link(
            as, ATOM_TYPE_LIST, list_out, 2, NULL);
        
        if (list_link != ATOM_HANDLE_INVALID) {
            atom_handle_t eval_out[2] = {g_bridge->pred_success_rate, list_link};
            atomspace_add_link(as, ATOM_TYPE_EVALUATION, eval_out, 2, &agent_tv);
        }
    }
    
    return COG_OK;
}

/*===========================================================================
 * AtomSpace -> Agent Selection (Cognitive Scheduling)
 *===========================================================================*/

/**
 * Select the best agent for a task based on AtomSpace attention values
 * and PLN-inferred reliability.
 * 
 * This function queries the AtomSpace for agents with the required
 * capabilities, then ranks them by:
 *   1. Attention value (STI) - how "active" the agent is
 *   2. Truth value strength - success rate
 *   3. Truth value confidence - evidence quality
 * 
 * @param required_capabilities Bitmask of required capabilities
 * @param best_agent Output: atom handle of the best agent
 * @return COG_OK on success, COG_ERROR_NOT_FOUND if no suitable agent
 */
cog_result_t cogpilot_bridge_select_best_agent(
    uint32_t required_capabilities,
    atom_handle_t* best_agent
) {
    if (!g_bridge || !g_bridge->initialized) return COG_ERROR_INIT;
    if (!best_agent) return COG_ERROR_INVALID_PARAM;
    
    atomspace_t as = g_bridge->atomspace;
    
    /* Query all agents from CogPilot */
    cogpilot_agent_t* agents = NULL;
    size_t agent_count = 0;
    
    cog_result_t result = cogpilot_query_agents(
        g_bridge->cogpilot, "*", &agents, &agent_count);
    
    if (result != COG_OK || agent_count == 0) {
        return COG_ERROR_NOT_FOUND;
    }
    
    /* Score each agent based on AtomSpace state */
    atom_handle_t best = ATOM_HANDLE_INVALID;
    double best_score = -1.0;
    
    for (size_t i = 0; i < agent_count; i++) {
        /* Check capability match */
        if ((agents[i].capabilities & required_capabilities) != required_capabilities) {
            continue;
        }
        
        /* Look up agent atom in AtomSpace */
        char agent_name[256];
        snprintf(agent_name, sizeof(agent_name), "agent:%s", agents[i].name);
        atom_handle_t agent_atom = atomspace_get_node(as, ATOM_TYPE_CONCEPT, agent_name);
        
        if (agent_atom == ATOM_HANDLE_INVALID) continue;
        
        /* Get truth value (success rate + confidence) */
        truth_value_t tv;
        atomspace_get_tv(as, agent_atom, &tv);
        
        /* Get attention value (activity level) */
        attention_value_t av;
        atomspace_get_av(as, agent_atom, &av);
        
        /* Composite score: weighted combination of STI, strength, confidence */
        double sti_normalized = (double)av.sti / 100.0;
        if (sti_normalized > 1.0) sti_normalized = 1.0;
        if (sti_normalized < 0.0) sti_normalized = 0.0;
        
        double score = (0.3 * sti_normalized) + 
                       (0.5 * tv.strength) + 
                       (0.2 * tv.confidence);
        
        if (score > best_score) {
            best_score = score;
            best = agent_atom;
        }
    }
    
    free(agents);
    
    if (best == ATOM_HANDLE_INVALID) return COG_ERROR_NOT_FOUND;
    
    *best_agent = best;
    
    /* Update stats */
    pthread_mutex_lock(&g_bridge->stats_lock);
    g_bridge->stats.queries_executed++;
    pthread_mutex_unlock(&g_bridge->stats_lock);
    
    return COG_OK;
}

/*===========================================================================
 * Bridge Statistics
 *===========================================================================*/

/**
 * Get bridge statistics.
 */
cog_result_t cogpilot_bridge_get_stats(
    uint64_t* agents_synced,
    uint64_t* tasks_synced,
    uint64_t* queries_executed
) {
    if (!g_bridge) return COG_ERROR_INIT;
    
    pthread_mutex_lock(&g_bridge->stats_lock);
    if (agents_synced) *agents_synced = g_bridge->stats.agents_synced;
    if (tasks_synced) *tasks_synced = g_bridge->stats.tasks_synced;
    if (queries_executed) *queries_executed = g_bridge->stats.queries_executed;
    pthread_mutex_unlock(&g_bridge->stats_lock);
    
    return COG_OK;
}
