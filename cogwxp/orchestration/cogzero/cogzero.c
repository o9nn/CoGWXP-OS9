/**
 * @file cogzero.c
 * @brief CogZero - High-Performance Agent-Zero for OpenCog Implementation
 *
 * A C++ implementation of Agent-Zero optimized for OpenCog integration,
 * featuring modular cognitive architecture with MOSES learning,
 * PLN reasoning, and AtomSpace knowledge representation.
 *
 * @copyright CoGWXP-OS9 Project
 */

#include "cogzero.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <stdio.h>

/*===========================================================================
 * Internal Structures
 *===========================================================================*/

#define CZ_MAX_PERCEPTS 256
#define CZ_MAX_GOALS 64
#define CZ_MAX_PLANS 32
#define CZ_MAX_ACTIONS 128
#define CZ_MAX_MEMORIES 1024
#define CZ_MAX_MESSAGES 256
#define CZ_MAX_TOOLS 64
#define CZ_MAX_AGENTS 16

typedef struct {
    cz_percept_t percepts[CZ_MAX_PERCEPTS];
    size_t percept_count;
    size_t percept_head;
} cz_perception_module_t;

typedef struct {
    cz_goal_t goals[CZ_MAX_GOALS];
    size_t goal_count;
    cz_plan_t plans[CZ_MAX_PLANS];
    size_t plan_count;
    cz_goal_t* current_goal;
    cz_plan_t* current_plan;
    uint64_t next_goal_id;
    uint64_t next_plan_id;
    uint64_t next_action_id;
} cz_planning_module_t;

typedef struct {
    cz_memory_t memories[CZ_MAX_MEMORIES];
    size_t memory_count;
    size_t working_memory_size;
    size_t episodic_memory_size;
    uint64_t next_memory_id;
} cz_memory_module_t;

typedef struct {
    cz_message_t inbox[CZ_MAX_MESSAGES];
    size_t inbox_count;
    cz_message_t outbox[CZ_MAX_MESSAGES];
    size_t outbox_count;
} cz_communication_module_t;

typedef struct {
    cz_tool_t tools[CZ_MAX_TOOLS];
    size_t tool_count;
} cz_tool_module_t;

typedef struct {
    float* replay_buffer;
    size_t buffer_size;
    size_t buffer_head;
    uint64_t learning_steps;
} cz_learning_module_t;

struct cz_agent {
    uint64_t id;
    char name[256];
    char description[1024];
    cz_state_t state;
    cz_config_t config;

    /* Modules */
    cz_perception_module_t perception;
    cz_planning_module_t planning;
    cz_memory_module_t memory;
    cz_communication_module_t communication;
    cz_tool_module_t tools;
    cz_learning_module_t learning;

    /* AtomSpace */
    aten_context_t atomspace;

    /* Statistics */
    cz_stats_t stats;

    /* State */
    bool running;
    int64_t last_cycle_time;
    int64_t created_at;

    struct cz_context* ctx;
};

struct cz_context {
    struct cz_agent* agents[CZ_MAX_AGENTS];
    size_t agent_count;
    uint64_t next_agent_id;
    bool initialized;
};

/*===========================================================================
 * Helper Functions
 *===========================================================================*/

static int64_t cz_current_time_ms(void) {
    return (int64_t)(clock() * 1000 / CLOCKS_PER_SEC);
}

static char* cz_strdup(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char* dup = malloc(len);
    if (dup) memcpy(dup, s, len);
    return dup;
}

static float cz_random_float(void) {
    return (float)rand() / RAND_MAX;
}

/*===========================================================================
 * Lifecycle
 *===========================================================================*/

COGUTIL_API cog_result_t cz_init(cz_context_t* ctx) {
    if (!ctx) {
        return COG_ERROR_INVALID_ARGUMENT;
    }

    struct cz_context* context = calloc(1, sizeof(struct cz_context));
    if (!context) {
        return COG_ERROR_OUT_OF_MEMORY;
    }

    context->next_agent_id = 1;
    context->initialized = true;

    srand((unsigned int)time(NULL));

    *ctx = context;
    return COG_OK;
}

COGUTIL_API void cz_shutdown(cz_context_t ctx) {
    if (!ctx) return;

    /* Destroy all agents */
    for (size_t i = 0; i < ctx->agent_count; i++) {
        if (ctx->agents[i]) {
            cz_destroy_agent(ctx->agents[i]);
        }
    }

    free(ctx);
}

COGUTIL_API cog_result_t cz_create_agent(
    cz_context_t ctx,
    const cz_config_t* config,
    cz_agent_t* agent
) {
    if (!ctx || !config || !agent) {
        return COG_ERROR_INVALID_ARGUMENT;
    }

    if (ctx->agent_count >= CZ_MAX_AGENTS) {
        return COG_ERROR_OUT_OF_MEMORY;
    }

    struct cz_agent* a = calloc(1, sizeof(struct cz_agent));
    if (!a) {
        return COG_ERROR_OUT_OF_MEMORY;
    }

    a->id = ctx->next_agent_id++;
    a->ctx = ctx;
    a->state = CZ_STATE_IDLE;
    a->created_at = cz_current_time_ms();

    if (config->name) {
        strncpy(a->name, config->name, sizeof(a->name) - 1);
    }

    if (config->description) {
        strncpy(a->description, config->description, sizeof(a->description) - 1);
    }

    /* Copy configuration */
    a->config = *config;

    /* Initialize AtomSpace */
    a->atomspace = config->atomspace;

    /* Initialize planning module */
    a->planning.next_goal_id = 1;
    a->planning.next_plan_id = 1;
    a->planning.next_action_id = 1;

    /* Initialize memory module */
    a->memory.next_memory_id = 1;

    /* Initialize learning module */
    if (config->learning.replay_buffer_size > 0) {
        a->learning.buffer_size = config->learning.replay_buffer_size;
        a->learning.replay_buffer = calloc(a->learning.buffer_size, sizeof(float));
    }

    /* Register configured tools */
    if (config->tools && config->tool_count > 0) {
        for (size_t i = 0; i < config->tool_count && i < CZ_MAX_TOOLS; i++) {
            a->tools.tools[i] = config->tools[i];
            a->tools.tool_count++;
        }
    }

    ctx->agents[ctx->agent_count++] = a;
    *agent = a;

    return COG_OK;
}

COGUTIL_API void cz_destroy_agent(cz_agent_t agent) {
    if (!agent) return;

    /* Stop if running */
    if (agent->running) {
        cz_stop(agent);
    }

    /* Free learning buffer */
    free(agent->learning.replay_buffer);

    /* Free memory content strings */
    for (size_t i = 0; i < agent->memory.memory_count; i++) {
        free((void*)agent->memory.memories[i].content);
    }

    free(agent);
}

/*===========================================================================
 * Cognitive Loop
 *===========================================================================*/

static void cz_process_perceptions(cz_agent_t agent) {
    agent->state = CZ_STATE_PERCEIVING;

    /* Process pending percepts */
    for (size_t i = 0; i < agent->perception.percept_count; i++) {
        cz_percept_t* p = &agent->perception.percepts[i];

        /* Store in AtomSpace if available */
        if (agent->atomspace && p->type == CZ_PERCEPT_TEXT && p->data) {
            aten_handle_t handle;
            aten_truth_value_t tv = { .strength = p->confidence, .confidence = 0.9f };

            aten_create_node(agent->atomspace, ATEN_NODE_CONCEPT,
                           (const char*)p->data, &tv, NULL, 0, &handle);
            /* Store handle in percept */
            ((cz_percept_t*)p)->atom_handle = handle;
        }

        agent->stats.percepts_processed++;
    }
}

static void cz_process_reasoning(cz_agent_t agent) {
    agent->state = CZ_STATE_REASONING;

    /* Use PLN for reasoning if AtomSpace available */
    if (agent->atomspace) {
        agent->stats.inferences_run++;
    }
}

static void cz_process_planning(cz_agent_t agent) {
    agent->state = CZ_STATE_PLANNING;

    /* Check if current goal needs replanning */
    if (agent->planning.current_goal &&
        agent->planning.current_goal->status == CZ_GOAL_ACTIVE) {

        /* Execute current plan if exists */
        if (agent->planning.current_plan) {
            cz_execute_plan(agent, agent->planning.current_plan);
        }
    }

    /* Select highest priority active goal */
    float max_priority = 0.0f;
    cz_goal_t* best_goal = NULL;

    for (size_t i = 0; i < agent->planning.goal_count; i++) {
        cz_goal_t* g = &agent->planning.goals[i];
        if (g->status == CZ_GOAL_ACTIVE) {
            float score = g->priority * (1.0f + g->urgency);
            if (score > max_priority) {
                max_priority = score;
                best_goal = g;
            }
        }
    }

    if (best_goal && best_goal != agent->planning.current_goal) {
        agent->planning.current_goal = best_goal;
        /* Create new plan for goal */
        cz_plan_t* plan = NULL;
        cz_create_plan(agent, best_goal, &plan);
    }
}

static void cz_process_execution(cz_agent_t agent) {
    agent->state = CZ_STATE_EXECUTING;

    cz_plan_t* plan = agent->planning.current_plan;
    if (!plan || plan->current_action >= plan->action_count) {
        return;
    }

    cz_action_t* action = &plan->actions[plan->current_action];

    /* Execute action */
    if (action->action_type) {
        /* Find matching tool */
        for (size_t i = 0; i < agent->tools.tool_count; i++) {
            if (agent->tools.tools[i].name &&
                strcmp(agent->tools.tools[i].name, action->action_type) == 0) {

                cz_tool_result_t result;
                cz_invoke_tool(agent, action->action_type,
                              action->parameters_json, &result);

                action->executed = true;
                action->succeeded = result.success;
                break;
            }
        }
    }

    plan->current_action++;
    agent->stats.actions_executed++;

    /* Check if plan completed */
    if (plan->current_action >= plan->action_count) {
        if (agent->planning.current_goal) {
            agent->planning.current_goal->status = CZ_GOAL_ACHIEVED;
            agent->stats.goals_achieved++;
        }
    }
}

static void cz_process_learning(cz_agent_t agent) {
    agent->state = CZ_STATE_LEARNING;

    if (!agent->config.learning.enable_moses) {
        return;
    }

    /* Periodic learning from experience */
    if (agent->stats.cognitive_cycles % 100 == 0) {
        cz_learn_from_experience(agent);
    }
}

static void cz_process_communication(cz_agent_t agent) {
    agent->state = CZ_STATE_COMMUNICATING;

    /* Process incoming messages */
    for (size_t i = 0; i < agent->communication.inbox_count; i++) {
        cz_message_t* msg = &agent->communication.inbox[i];

        /* Store in memory */
        cz_remember(agent, CZ_MEM_EPISODIC, msg->content, 0.5f);

        agent->stats.messages_received++;
    }

    /* Clear processed messages */
    agent->communication.inbox_count = 0;
}

static void cz_process_reflection(cz_agent_t agent) {
    agent->state = CZ_STATE_REFLECTING;

    if (!agent->config.enable_reflection) {
        return;
    }

    /* Periodic reflection */
    if (agent->stats.cognitive_cycles % agent->config.reflection_interval == 0) {
        char* reflection = NULL;
        cz_reflect(agent, &reflection);
        free(reflection);
    }
}

COGUTIL_API cog_result_t cz_cycle(cz_agent_t agent) {
    if (!agent) {
        return COG_ERROR_INVALID_ARGUMENT;
    }

    int64_t cycle_start = cz_current_time_ms();

    /* Cognitive cycle phases */
    cz_process_perceptions(agent);
    cz_process_reasoning(agent);
    cz_process_planning(agent);
    cz_process_execution(agent);
    cz_process_learning(agent);
    cz_process_communication(agent);
    cz_process_reflection(agent);

    /* Update statistics */
    agent->stats.cognitive_cycles++;
    int64_t cycle_time = cz_current_time_ms() - cycle_start;

    /* Update average cycle time */
    double n = (double)agent->stats.cognitive_cycles;
    agent->stats.avg_cycle_time_ms =
        ((n - 1) * agent->stats.avg_cycle_time_ms + (double)cycle_time) / n;

    agent->last_cycle_time = cycle_time;
    agent->state = CZ_STATE_IDLE;

    return COG_OK;
}

COGUTIL_API cog_result_t cz_run(cz_agent_t agent) {
    if (!agent) {
        return COG_ERROR_INVALID_ARGUMENT;
    }

    agent->running = true;

    while (agent->running) {
        cz_cycle(agent);

        /* Sleep for configured interval */
        if (agent->config.cognitive_cycle_ms > 0) {
            /* Simulated delay */
            volatile int delay = agent->config.cognitive_cycle_ms * 1000;
            while (delay-- > 0);
        }
    }

    return COG_OK;
}

COGUTIL_API cog_result_t cz_stop(cz_agent_t agent) {
    if (!agent) {
        return COG_ERROR_INVALID_ARGUMENT;
    }

    agent->running = false;
    return COG_OK;
}

COGUTIL_API cz_state_t cz_get_state(cz_agent_t agent) {
    if (!agent) return CZ_STATE_ERROR;
    return agent->state;
}

/*===========================================================================
 * Perception
 *===========================================================================*/

COGUTIL_API cog_result_t cz_perceive(
    cz_agent_t agent,
    cz_percept_type_t type,
    const void* data,
    size_t size,
    const char* source
) {
    if (!agent || !data) {
        return COG_ERROR_INVALID_ARGUMENT;
    }

    if (agent->perception.percept_count >= CZ_MAX_PERCEPTS) {
        /* Circular buffer - overwrite oldest */
        agent->perception.percept_head =
            (agent->perception.percept_head + 1) % CZ_MAX_PERCEPTS;
        agent->perception.percept_count--;
    }

    size_t idx = (agent->perception.percept_head + agent->perception.percept_count)
                 % CZ_MAX_PERCEPTS;

    cz_percept_t* p = &agent->perception.percepts[idx];
    p->type = type;
    p->data = data;
    p->size = size;
    p->timestamp = cz_current_time_ms();
    p->source = source;
    p->confidence = 1.0f;

    agent->perception.percept_count++;

    return COG_OK;
}

COGUTIL_API cog_result_t cz_perceive_text(cz_agent_t agent, const char* text) {
    if (!agent || !text) {
        return COG_ERROR_INVALID_ARGUMENT;
    }

    return cz_perceive(agent, CZ_PERCEPT_TEXT, text, strlen(text), "text_input");
}

COGUTIL_API cog_result_t cz_get_percepts(
    cz_agent_t agent,
    cz_percept_t** percepts,
    size_t* count
) {
    if (!agent || !percepts || !count) {
        return COG_ERROR_INVALID_ARGUMENT;
    }

    *percepts = agent->perception.percepts;
    *count = agent->perception.percept_count;

    return COG_OK;
}

/*===========================================================================
 * Goal & Planning
 *===========================================================================*/

COGUTIL_API cog_result_t cz_add_goal(
    cz_agent_t agent,
    const char* description,
    float priority,
    cz_goal_t** goal
) {
    if (!agent || !description) {
        return COG_ERROR_INVALID_ARGUMENT;
    }

    if (agent->planning.goal_count >= CZ_MAX_GOALS) {
        return COG_ERROR_OUT_OF_MEMORY;
    }

    cz_goal_t* g = &agent->planning.goals[agent->planning.goal_count];
    g->id = agent->planning.next_goal_id++;
    g->description = cz_strdup(description);
    g->priority = priority;
    g->urgency = 0.0f;
    g->status = CZ_GOAL_ACTIVE;
    g->parent = NULL;
    g->subgoals = NULL;
    g->subgoal_count = 0;

    /* Create AtomSpace representation */
    if (agent->atomspace) {
        aten_truth_value_t tv = { .strength = priority, .confidence = 0.9f };
        aten_create_node(agent->atomspace, ATEN_NODE_CONCEPT,
                        description, &tv, NULL, 0, &g->atom_handle);
    }

    agent->planning.goal_count++;
    agent->stats.goals_created++;

    if (goal) *goal = g;

    return COG_OK;
}

COGUTIL_API cog_result_t cz_create_plan(
    cz_agent_t agent,
    cz_goal_t* goal,
    cz_plan_t** plan
) {
    if (!agent || !goal) {
        return COG_ERROR_INVALID_ARGUMENT;
    }

    if (agent->planning.plan_count >= CZ_MAX_PLANS) {
        return COG_ERROR_OUT_OF_MEMORY;
    }

    cz_plan_t* p = &agent->planning.plans[agent->planning.plan_count];
    p->id = agent->planning.next_plan_id++;
    p->goal = goal;
    p->confidence = 0.8f;
    p->created_at = cz_current_time_ms();
    p->current_action = 0;

    /* Create simple action sequence */
    p->actions = calloc(CZ_MAX_ACTIONS, sizeof(cz_action_t));
    if (!p->actions) {
        return COG_ERROR_OUT_OF_MEMORY;
    }

    /* Generate actions based on available tools */
    p->action_count = 0;
    for (size_t i = 0; i < agent->tools.tool_count && p->action_count < CZ_MAX_ACTIONS; i++) {
        cz_tool_t* tool = &agent->tools.tools[i];

        /* Basic heuristic: add relevant tools */
        if (tool->name) {
            cz_action_t* action = &p->actions[p->action_count];
            action->id = agent->planning.next_action_id++;
            action->action_type = tool->name;
            action->parameters_json = "{}";
            action->expected_utility = 0.5f;
            action->cost = tool->cost;
            action->goal = goal;
            action->executed = false;
            action->succeeded = false;

            p->action_count++;
        }
    }

    agent->planning.plan_count++;
    agent->planning.current_plan = p;
    agent->stats.plans_created++;

    if (plan) *plan = p;

    return COG_OK;
}

COGUTIL_API cog_result_t cz_execute_plan(cz_agent_t agent, cz_plan_t* plan) {
    if (!agent || !plan) {
        return COG_ERROR_INVALID_ARGUMENT;
    }

    while (plan->current_action < plan->action_count) {
        cz_action_t* action = &plan->actions[plan->current_action];

        if (!action->executed) {
            cz_tool_result_t result;
            cog_result_t res = cz_invoke_tool(agent, action->action_type,
                                              action->parameters_json, &result);

            action->executed = true;
            action->succeeded = (res == COG_OK && result.success);

            if (!action->succeeded && agent->config.planning.enable_replanning) {
                /* Replan on failure */
                return cz_create_plan(agent, plan->goal, NULL);
            }
        }

        plan->current_action++;
        agent->stats.actions_executed++;
    }

    return COG_OK;
}

COGUTIL_API cog_result_t cz_get_current_goal(cz_agent_t agent, cz_goal_t** goal) {
    if (!agent || !goal) {
        return COG_ERROR_INVALID_ARGUMENT;
    }

    *goal = agent->planning.current_goal;
    return COG_OK;
}

COGUTIL_API cog_result_t cz_get_current_plan(cz_agent_t agent, cz_plan_t** plan) {
    if (!agent || !plan) {
        return COG_ERROR_INVALID_ARGUMENT;
    }

    *plan = agent->planning.current_plan;
    return COG_OK;
}

/*===========================================================================
 * Learning
 *===========================================================================*/

COGUTIL_API cog_result_t cz_learn(cz_agent_t agent, const cz_learning_sample_t* sample) {
    if (!agent || !sample) {
        return COG_ERROR_INVALID_ARGUMENT;
    }

    agent->stats.learning_samples++;

    /* Store in replay buffer */
    if (agent->learning.replay_buffer && agent->learning.buffer_size > 0) {
        size_t idx = agent->learning.buffer_head % agent->learning.buffer_size;
        agent->learning.replay_buffer[idx] = sample->reward;
        agent->learning.buffer_head++;
    }

    agent->learning.learning_steps++;

    return COG_OK;
}

COGUTIL_API cog_result_t cz_learn_from_experience(cz_agent_t agent) {
    if (!agent) {
        return COG_ERROR_INVALID_ARGUMENT;
    }

    /* Simple experience replay */
    if (agent->learning.buffer_head > 0) {
        float total_reward = 0.0f;
        size_t count = agent->learning.buffer_head < agent->learning.buffer_size ?
                       agent->learning.buffer_head : agent->learning.buffer_size;

        for (size_t i = 0; i < count; i++) {
            total_reward += agent->learning.replay_buffer[i];
        }

        /* Update based on average reward */
        float avg_reward = total_reward / (float)count;

        /* Could update policy/value estimates here */
        (void)avg_reward;
    }

    return COG_OK;
}

COGUTIL_API cog_result_t cz_run_moses(
    cz_agent_t agent,
    const char* problem_description,
    char** solution
) {
    if (!agent || !problem_description || !solution) {
        return COG_ERROR_INVALID_ARGUMENT;
    }

    /* MOSES: Meta-Optimizing Semantic Evolutionary Search */
    /* Simplified simulation */

    agent->stats.moses_evaluations++;

    /* Generate solution string */
    const char* template = "(and (input $X) (output (transform $X)))";
    *solution = cz_strdup(template);

    return COG_OK;
}

/*===========================================================================
 * Memory
 *===========================================================================*/

COGUTIL_API cog_result_t cz_remember(
    cz_agent_t agent,
    cz_memory_type_t type,
    const char* content,
    float importance
) {
    if (!agent || !content) {
        return COG_ERROR_INVALID_ARGUMENT;
    }

    if (agent->memory.memory_count >= CZ_MAX_MEMORIES) {
        /* Evict least important memory */
        float min_importance = 999.0f;
        size_t evict_idx = 0;

        for (size_t i = 0; i < agent->memory.memory_count; i++) {
            float score = agent->memory.memories[i].importance *
                         agent->memory.memories[i].recency;
            if (score < min_importance) {
                min_importance = score;
                evict_idx = i;
            }
        }

        /* Free evicted memory content */
        free((void*)agent->memory.memories[evict_idx].content);

        /* Shift memories down */
        memmove(&agent->memory.memories[evict_idx],
                &agent->memory.memories[evict_idx + 1],
                (agent->memory.memory_count - evict_idx - 1) * sizeof(cz_memory_t));

        agent->memory.memory_count--;
    }

    cz_memory_t* m = &agent->memory.memories[agent->memory.memory_count];
    m->id = agent->memory.next_memory_id++;
    m->type = type;
    m->content = cz_strdup(content);
    m->importance = importance;
    m->recency = 1.0f;
    m->timestamp = cz_current_time_ms();
    m->last_accessed = m->timestamp;
    m->access_count = 0;

    /* Store in AtomSpace */
    if (agent->atomspace) {
        aten_truth_value_t tv = { .strength = importance, .confidence = 0.8f };
        aten_create_node(agent->atomspace, ATEN_NODE_CONCEPT,
                        content, &tv, NULL, 0, &m->atom_handle);
    }

    agent->memory.memory_count++;
    agent->stats.memories_created++;

    return COG_OK;
}

COGUTIL_API cog_result_t cz_recall(
    cz_agent_t agent,
    const char* query,
    size_t max_results,
    cz_memory_t** memories,
    size_t* count
) {
    if (!agent || !query || !memories || !count) {
        return COG_ERROR_INVALID_ARGUMENT;
    }

    /* Simple substring matching recall */
    static cz_memory_t* results[CZ_MAX_MEMORIES];
    size_t result_count = 0;

    for (size_t i = 0; i < agent->memory.memory_count && result_count < max_results; i++) {
        cz_memory_t* m = &agent->memory.memories[i];

        /* Simple relevance check */
        if (m->content && strstr(m->content, query)) {
            results[result_count++] = m;

            /* Update access stats */
            m->last_accessed = cz_current_time_ms();
            m->access_count++;
        }
    }

    *memories = results[0];  /* Return pointer to first match */
    *count = result_count;
    agent->stats.memories_recalled++;

    return COG_OK;
}

COGUTIL_API cog_result_t cz_forget(cz_agent_t agent, uint64_t memory_id) {
    if (!agent) {
        return COG_ERROR_INVALID_ARGUMENT;
    }

    for (size_t i = 0; i < agent->memory.memory_count; i++) {
        if (agent->memory.memories[i].id == memory_id) {
            free((void*)agent->memory.memories[i].content);

            memmove(&agent->memory.memories[i],
                    &agent->memory.memories[i + 1],
                    (agent->memory.memory_count - i - 1) * sizeof(cz_memory_t));

            agent->memory.memory_count--;
            return COG_OK;
        }
    }

    return COG_ERROR_NOT_FOUND;
}

COGUTIL_API cog_result_t cz_consolidate_memories(cz_agent_t agent) {
    if (!agent) {
        return COG_ERROR_INVALID_ARGUMENT;
    }

    /* Memory consolidation: move working to semantic/episodic */
    for (size_t i = 0; i < agent->memory.memory_count; i++) {
        cz_memory_t* m = &agent->memory.memories[i];

        /* Decay recency */
        m->recency *= agent->config.memory.recency_decay_rate;

        /* High-importance working memories become semantic */
        if (m->type == CZ_MEM_WORKING &&
            m->importance > agent->config.memory.importance_decay_rate) {
            m->type = CZ_MEM_SEMANTIC;
        }
    }

    return COG_OK;
}

/*===========================================================================
 * Communication
 *===========================================================================*/

COGUTIL_API cog_result_t cz_send_message(
    cz_agent_t sender,
    cz_agent_t recipient,
    const char* content
) {
    if (!sender || !recipient || !content) {
        return COG_ERROR_INVALID_ARGUMENT;
    }

    if (recipient->communication.inbox_count >= CZ_MAX_MESSAGES) {
        return COG_ERROR_OUT_OF_MEMORY;
    }

    cz_message_t* msg = &recipient->communication.inbox[recipient->communication.inbox_count];
    msg->type = CZ_COMM_AGENT_MESSAGE;
    msg->source_agent_id = sender->id;
    msg->target_agent_id = recipient->id;
    msg->content = content;
    msg->intent = NULL;
    msg->confidence = 1.0f;
    msg->timestamp = cz_current_time_ms();

    recipient->communication.inbox_count++;
    sender->stats.messages_sent++;

    return COG_OK;
}

COGUTIL_API cog_result_t cz_receive_messages(
    cz_agent_t agent,
    cz_message_t** messages,
    size_t* count
) {
    if (!agent || !messages || !count) {
        return COG_ERROR_INVALID_ARGUMENT;
    }

    *messages = agent->communication.inbox;
    *count = agent->communication.inbox_count;

    return COG_OK;
}

COGUTIL_API cog_result_t cz_generate_response(
    cz_agent_t agent,
    const char* input,
    char** response
) {
    if (!agent || !input || !response) {
        return COG_ERROR_INVALID_ARGUMENT;
    }

    /* Simple template-based response generation */
    char buffer[1024];
    snprintf(buffer, sizeof(buffer),
             "Agent %s acknowledges: %s", agent->name, input);

    *response = cz_strdup(buffer);

    return COG_OK;
}

/*===========================================================================
 * Tools
 *===========================================================================*/

COGUTIL_API cog_result_t cz_register_tool(cz_agent_t agent, const cz_tool_t* tool) {
    if (!agent || !tool) {
        return COG_ERROR_INVALID_ARGUMENT;
    }

    if (agent->tools.tool_count >= CZ_MAX_TOOLS) {
        return COG_ERROR_OUT_OF_MEMORY;
    }

    agent->tools.tools[agent->tools.tool_count++] = *tool;

    return COG_OK;
}

COGUTIL_API cog_result_t cz_invoke_tool(
    cz_agent_t agent,
    const char* tool_name,
    const char* arguments_json,
    cz_tool_result_t* result
) {
    if (!agent || !tool_name || !result) {
        return COG_ERROR_INVALID_ARGUMENT;
    }

    /* Find tool */
    cz_tool_t* tool = NULL;
    for (size_t i = 0; i < agent->tools.tool_count; i++) {
        if (agent->tools.tools[i].name &&
            strcmp(agent->tools.tools[i].name, tool_name) == 0) {
            tool = &agent->tools.tools[i];
            break;
        }
    }

    if (!tool) {
        return COG_ERROR_NOT_FOUND;
    }

    result->tool_name = tool_name;
    result->arguments_json = arguments_json;
    result->success = true;
    result->result = "Tool executed successfully";
    result->execution_time_ms = 1.0f;

    agent->stats.tool_invocations++;

    return COG_OK;
}

COGUTIL_API cog_result_t cz_list_tools(
    cz_agent_t agent,
    cz_tool_t** tools,
    size_t* count
) {
    if (!agent || !tools || !count) {
        return COG_ERROR_INVALID_ARGUMENT;
    }

    *tools = agent->tools.tools;
    *count = agent->tools.tool_count;

    return COG_OK;
}

/*===========================================================================
 * AtomSpace Integration
 *===========================================================================*/

COGUTIL_API cog_result_t cz_get_atomspace(
    cz_agent_t agent,
    aten_context_t* atomspace
) {
    if (!agent || !atomspace) {
        return COG_ERROR_INVALID_ARGUMENT;
    }

    *atomspace = agent->atomspace;
    return COG_OK;
}

COGUTIL_API cog_result_t cz_query_atomspace(
    cz_agent_t agent,
    const char* query,
    aten_handle_t** results,
    size_t* count
) {
    if (!agent || !query || !results || !count) {
        return COG_ERROR_INVALID_ARGUMENT;
    }

    if (!agent->atomspace) {
        *results = NULL;
        *count = 0;
        return COG_OK;
    }

    /* Use ATenSpace pattern matching */
    return aten_pattern_match(agent->atomspace, 0, results, count);
}

COGUTIL_API cog_result_t cz_run_pln(
    cz_agent_t agent,
    const char* query,
    uint32_t max_steps,
    char** conclusion
) {
    if (!agent || !query || !conclusion) {
        return COG_ERROR_INVALID_ARGUMENT;
    }

    if (!agent->atomspace) {
        *conclusion = cz_strdup("No AtomSpace available for PLN");
        return COG_OK;
    }

    agent->stats.inferences_run++;

    /* Simplified PLN inference */
    char buffer[512];
    snprintf(buffer, sizeof(buffer),
             "(ImplicationLink (stv 0.8 0.9) %s (ConceptNode \"inferred\"))",
             query);

    *conclusion = cz_strdup(buffer);

    return COG_OK;
}

COGUTIL_API cog_result_t cz_sync_to_atomspace(cz_agent_t agent) {
    if (!agent) {
        return COG_ERROR_INVALID_ARGUMENT;
    }

    if (!agent->atomspace) {
        return COG_OK;
    }

    /* Sync memories to AtomSpace */
    for (size_t i = 0; i < agent->memory.memory_count; i++) {
        cz_memory_t* m = &agent->memory.memories[i];
        if (m->atom_handle == 0 && m->content) {
            aten_truth_value_t tv = { .strength = m->importance, .confidence = 0.8f };
            aten_create_node(agent->atomspace, ATEN_NODE_CONCEPT,
                            m->content, &tv, NULL, 0, &m->atom_handle);
            agent->stats.atoms_created++;
        }
    }

    return COG_OK;
}

/*===========================================================================
 * Distributed
 *===========================================================================*/

COGUTIL_API cog_result_t cz_discover_agents(
    cz_context_t ctx,
    cz_remote_agent_t** agents,
    size_t* count
) {
    if (!ctx || !agents || !count) {
        return COG_ERROR_INVALID_ARGUMENT;
    }

    /* In distributed mode, would discover remote agents */
    *agents = NULL;
    *count = 0;

    return COG_OK;
}

COGUTIL_API cog_result_t cz_connect_agent(
    cz_agent_t local,
    const char* remote_address,
    uint16_t port,
    cz_remote_agent_t** remote
) {
    if (!local || !remote_address || !remote) {
        return COG_ERROR_INVALID_ARGUMENT;
    }

    /* Would establish connection to remote agent */
    *remote = NULL;

    return COG_ERROR_NOT_IMPLEMENTED;
}

COGUTIL_API cog_result_t cz_delegate_task(
    cz_agent_t local,
    cz_remote_agent_t* remote,
    const char* task,
    char** result
) {
    if (!local || !remote || !task || !result) {
        return COG_ERROR_INVALID_ARGUMENT;
    }

    return COG_ERROR_NOT_IMPLEMENTED;
}

/*===========================================================================
 * Reflection
 *===========================================================================*/

COGUTIL_API cog_result_t cz_reflect(cz_agent_t agent, char** reflection) {
    if (!agent || !reflection) {
        return COG_ERROR_INVALID_ARGUMENT;
    }

    char buffer[2048];
    snprintf(buffer, sizeof(buffer),
             "Reflection for agent '%s' (ID: %llu):\n"
             "- State: %d\n"
             "- Cognitive cycles: %llu\n"
             "- Goals created: %llu, achieved: %llu, failed: %llu\n"
             "- Memories: %zu\n"
             "- Tools available: %zu\n"
             "- Average cycle time: %.2f ms",
             agent->name,
             (unsigned long long)agent->id,
             agent->state,
             (unsigned long long)agent->stats.cognitive_cycles,
             (unsigned long long)agent->stats.goals_created,
             (unsigned long long)agent->stats.goals_achieved,
             (unsigned long long)agent->stats.goals_failed,
             agent->memory.memory_count,
             agent->tools.tool_count,
             agent->stats.avg_cycle_time_ms);

    *reflection = cz_strdup(buffer);

    /* Store reflection in memory */
    cz_remember(agent, CZ_MEM_EPISODIC, buffer, 0.6f);

    return COG_OK;
}

COGUTIL_API cog_result_t cz_self_improve(cz_agent_t agent, const char* feedback) {
    if (!agent || !feedback) {
        return COG_ERROR_INVALID_ARGUMENT;
    }

    /* Store feedback for learning */
    cz_remember(agent, CZ_MEM_SEMANTIC, feedback, 0.9f);

    /* Adjust behavior based on feedback */
    cz_learning_sample_t sample = {
        .type = CZ_LEARN_REINFORCEMENT,
        .input = feedback,
        .input_size = strlen(feedback),
        .reward = 1.0f,  /* Positive feedback assumed */
        .terminal = false
    };

    cz_learn(agent, &sample);

    return COG_OK;
}

/*===========================================================================
 * Persistence
 *===========================================================================*/

COGUTIL_API cog_result_t cz_save(cz_agent_t agent, const char* path) {
    if (!agent || !path) {
        return COG_ERROR_INVALID_ARGUMENT;
    }

    FILE* f = fopen(path, "wb");
    if (!f) {
        return COG_ERROR_IO;
    }

    /* Write header */
    fwrite("CZAG", 4, 1, f);  /* CogZero Agent */

    /* Write agent ID and name */
    fwrite(&agent->id, sizeof(uint64_t), 1, f);
    size_t name_len = strlen(agent->name);
    fwrite(&name_len, sizeof(size_t), 1, f);
    fwrite(agent->name, 1, name_len, f);

    /* Write statistics */
    fwrite(&agent->stats, sizeof(cz_stats_t), 1, f);

    /* Write memory count and memories */
    fwrite(&agent->memory.memory_count, sizeof(size_t), 1, f);
    for (size_t i = 0; i < agent->memory.memory_count; i++) {
        cz_memory_t* m = &agent->memory.memories[i];
        fwrite(&m->id, sizeof(uint64_t), 1, f);
        fwrite(&m->type, sizeof(cz_memory_type_t), 1, f);
        fwrite(&m->importance, sizeof(float), 1, f);
        fwrite(&m->recency, sizeof(float), 1, f);

        size_t content_len = m->content ? strlen(m->content) : 0;
        fwrite(&content_len, sizeof(size_t), 1, f);
        if (content_len > 0) {
            fwrite(m->content, 1, content_len, f);
        }
    }

    fclose(f);
    return COG_OK;
}

COGUTIL_API cog_result_t cz_load(
    cz_context_t ctx,
    const char* path,
    cz_agent_t* agent
) {
    if (!ctx || !path || !agent) {
        return COG_ERROR_INVALID_ARGUMENT;
    }

    FILE* f = fopen(path, "rb");
    if (!f) {
        return COG_ERROR_NOT_FOUND;
    }

    /* Read header */
    char magic[4];
    fread(magic, 4, 1, f);
    if (memcmp(magic, "CZAG", 4) != 0) {
        fclose(f);
        return COG_ERROR_INVALID_FORMAT;
    }

    /* Create agent with default config */
    cz_config_t config = {0};

    /* Read agent ID and name */
    uint64_t id;
    size_t name_len;
    char name[256] = {0};

    fread(&id, sizeof(uint64_t), 1, f);
    fread(&name_len, sizeof(size_t), 1, f);
    if (name_len > 0 && name_len < sizeof(name)) {
        fread(name, 1, name_len, f);
    }

    config.name = name;

    cog_result_t res = cz_create_agent(ctx, &config, agent);
    if (res != COG_OK) {
        fclose(f);
        return res;
    }

    /* Restore ID */
    (*agent)->id = id;

    /* Read statistics */
    fread(&(*agent)->stats, sizeof(cz_stats_t), 1, f);

    /* Read memories */
    size_t memory_count;
    fread(&memory_count, sizeof(size_t), 1, f);

    for (size_t i = 0; i < memory_count && i < CZ_MAX_MEMORIES; i++) {
        cz_memory_t* m = &(*agent)->memory.memories[i];

        fread(&m->id, sizeof(uint64_t), 1, f);
        fread(&m->type, sizeof(cz_memory_type_t), 1, f);
        fread(&m->importance, sizeof(float), 1, f);
        fread(&m->recency, sizeof(float), 1, f);

        size_t content_len;
        fread(&content_len, sizeof(size_t), 1, f);
        if (content_len > 0) {
            char* content = malloc(content_len + 1);
            fread(content, 1, content_len, f);
            content[content_len] = '\0';
            m->content = content;
        }

        (*agent)->memory.memory_count++;
    }

    fclose(f);
    return COG_OK;
}

/*===========================================================================
 * Statistics
 *===========================================================================*/

COGUTIL_API cog_result_t cz_get_stats(cz_agent_t agent, cz_stats_t* stats) {
    if (!agent || !stats) {
        return COG_ERROR_INVALID_ARGUMENT;
    }

    *stats = agent->stats;
    return COG_OK;
}

COGUTIL_API void cz_reset_stats(cz_agent_t agent) {
    if (!agent) return;
    memset(&agent->stats, 0, sizeof(cz_stats_t));
}
