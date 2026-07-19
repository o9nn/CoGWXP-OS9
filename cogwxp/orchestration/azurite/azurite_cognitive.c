/**
 * @file azurite_cognitive.c
 * @brief Azurite Cognitive Architecture Implementation
 */

#include "azurite_cognitive.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

struct azurite_agent {
    azurite_agent_config_t config;
    azurite_memory_t* memories;
    size_t memory_count;
    size_t memory_capacity;
    azurite_emotion_t emotion;
    uint64_t next_memory_id;
    uint64_t next_plan_id;
};

static char* az_strdup(const char* s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char* out = malloc(n);
    if (out) memcpy(out, s, n);
    return out;
}

static cog_result_t ensure_memory_capacity(azurite_agent_t agent) {
    if (agent->memory_count < agent->memory_capacity) return COG_OK;
    size_t new_capacity = agent->memory_capacity ? agent->memory_capacity * 2 : 16;
    azurite_memory_t* memories = realloc(agent->memories, new_capacity * sizeof(*memories));
    if (!memories) return COG_ERROR_MEMORY;
    agent->memories = memories;
    agent->memory_capacity = new_capacity;
    return COG_OK;
}

static cog_result_t append_memory(azurite_agent_t agent, azurite_memory_type_t type, const char* content, float importance, azurite_memory_t* out) {
    if (!agent || !content) return COG_ERROR_INVALID_PARAM;
    cog_result_t r = ensure_memory_capacity(agent);
    if (r != COG_OK) return r;

    azurite_memory_t* mem = &agent->memories[agent->memory_count++];
    memset(mem, 0, sizeof(*mem));
    mem->id = agent->next_memory_id++;
    mem->type = type;
    mem->content = az_strdup(content);
    mem->creation_time = (uint64_t)time(NULL);
    mem->last_access_time = mem->creation_time;
    mem->importance = importance;
    mem->recency = 1.0f;
    mem->relevance = 1.0f;
    if (out) *out = *mem;
    return COG_OK;
}

COGUTIL_API cog_result_t azurite_agent_create(const azurite_agent_config_t* config, azurite_agent_t* agent) {
    if (!config || !agent) return COG_ERROR_INVALID_PARAM;
    azurite_agent_t a = calloc(1, sizeof(*a));
    if (!a) return COG_ERROR_MEMORY;

    a->config = *config;
    a->config.name = az_strdup(config->name);
    a->config.description = az_strdup(config->description);
    a->config.background = az_strdup(config->background);
    if (config->goal_count && config->initial_goals) {
        const char** goals = calloc(config->goal_count, sizeof(*goals));
        if (!goals) { azurite_agent_destroy(a); return COG_ERROR_MEMORY; }
        for (size_t i = 0; i < config->goal_count; ++i) goals[i] = az_strdup(config->initial_goals[i]);
        a->config.initial_goals = goals;
    }
    a->emotion.joy = 0.5f;
    a->emotion.trust = 0.5f;
    a->emotion.valence = 0.0f;
    a->emotion.arousal = 0.5f;
    a->next_memory_id = 1;
    a->next_plan_id = 1;

    if (config->background) append_memory(a, AZURITE_MEM_OBSERVATION, config->background, 0.8f, NULL);
    for (size_t i = 0; i < config->goal_count; ++i) append_memory(a, AZURITE_MEM_GOAL, config->initial_goals[i], 0.9f, NULL);

    *agent = a;
    return COG_OK;
}

COGUTIL_API void azurite_agent_destroy(azurite_agent_t agent) {
    if (!agent) return;
    free((void*)agent->config.name);
    free((void*)agent->config.description);
    free((void*)agent->config.background);
    for (size_t i = 0; i < agent->config.goal_count; ++i) free((void*)agent->config.initial_goals[i]);
    free((void*)agent->config.initial_goals);
    for (size_t i = 0; i < agent->memory_count; ++i) {
        free((void*)agent->memories[i].content);
        free(agent->memories[i].embedding);
        free(agent->memories[i].related_ids);
    }
    free(agent->memories);
    free(agent);
}

COGUTIL_API cog_result_t azurite_agent_save(azurite_agent_t agent, const char* path) {
    if (!agent || !path) return COG_ERROR_INVALID_PARAM;
    FILE* f = fopen(path, "w");
    if (!f) return COG_ERROR_IO;
    fprintf(f, "azurite:%s:%zu\n", agent->config.name ? agent->config.name : "", agent->memory_count);
    fclose(f);
    return COG_OK;
}

COGUTIL_API cog_result_t azurite_agent_load(azurite_agent_t agent, const char* path) {
    if (!agent || !path) return COG_ERROR_INVALID_PARAM;
    FILE* f = fopen(path, "r");
    if (!f) return COG_ERROR_IO;
    fclose(f);
    return COG_OK;
}

COGUTIL_API cog_result_t azurite_observe(azurite_agent_t agent, const char* observation, float importance, azurite_memory_t* memory) {
    return append_memory(agent, AZURITE_MEM_OBSERVATION, observation, importance, memory);
}

COGUTIL_API cog_result_t azurite_think(azurite_agent_t agent, const char* thought, azurite_memory_t* memory) {
    return append_memory(agent, AZURITE_MEM_THOUGHT, thought, 0.5f, memory);
}

COGUTIL_API cog_result_t azurite_retrieve(azurite_agent_t agent, const char* query, uint32_t max_results, azurite_memory_t** memories, size_t* count) {
    (void)query;
    if (!agent || !memories || !count) return COG_ERROR_INVALID_PARAM;
    size_t n = agent->memory_count < max_results ? agent->memory_count : max_results;
    *memories = n ? calloc(n, sizeof(**memories)) : NULL;
    if (n && !*memories) return COG_ERROR_MEMORY;
    for (size_t i = 0; i < n; ++i) (*memories)[i] = agent->memories[i];
    *count = n;
    return COG_OK;
}

COGUTIL_API cog_result_t azurite_retrieve_by_type(azurite_agent_t agent, azurite_memory_type_t type, uint32_t max_results, azurite_memory_t** memories, size_t* count) {
    if (!agent || !memories || !count) return COG_ERROR_INVALID_PARAM;
    *count = 0;
    *memories = max_results ? calloc(max_results, sizeof(**memories)) : NULL;
    if (max_results && !*memories) return COG_ERROR_MEMORY;
    for (size_t i = 0; i < agent->memory_count && *count < max_results; ++i) {
        if (agent->memories[i].type == type) (*memories)[(*count)++] = agent->memories[i];
    }
    return COG_OK;
}

COGUTIL_API cog_result_t azurite_get_recent(azurite_agent_t agent, uint32_t count, azurite_memory_t** memories, size_t* actual_count) {
    if (!agent || !memories || !actual_count) return COG_ERROR_INVALID_PARAM;
    size_t n = agent->memory_count < count ? agent->memory_count : count;
    *memories = n ? calloc(n, sizeof(**memories)) : NULL;
    if (n && !*memories) return COG_ERROR_MEMORY;
    size_t start = agent->memory_count - n;
    for (size_t i = 0; i < n; ++i) (*memories)[i] = agent->memories[start + i];
    *actual_count = n;
    return COG_OK;
}

COGUTIL_API void azurite_memories_free(azurite_memory_t* memories, size_t count) {
    (void)count;
    free(memories);
}

COGUTIL_API cog_result_t azurite_reflect(azurite_agent_t agent, azurite_memory_t** reflections, size_t* count) {
    if (!agent || !reflections || !count) return COG_ERROR_INVALID_PARAM;
    *reflections = calloc(1, sizeof(**reflections));
    if (!*reflections) return COG_ERROR_MEMORY;
    append_memory(agent, AZURITE_MEM_REFLECTION, "Generated reflection", 0.7f, *reflections);
    *count = 1;
    return COG_OK;
}

COGUTIL_API cog_result_t azurite_reflect_on(azurite_agent_t agent, const char* topic, azurite_memory_t* reflection) {
    char buf[256];
    snprintf(buf, sizeof(buf), "Reflection on %s", topic ? topic : "topic");
    return append_memory(agent, AZURITE_MEM_REFLECTION, buf, 0.7f, reflection);
}

COGUTIL_API cog_result_t azurite_self_summary(azurite_agent_t agent, char** summary, size_t* summary_size) {
    if (!agent || !summary || !summary_size) return COG_ERROR_INVALID_PARAM;
    const char* name = agent->config.name ? agent->config.name : "Azurite agent";
    size_t n = strlen(name) + 64;
    *summary = malloc(n);
    if (!*summary) return COG_ERROR_MEMORY;
    snprintf(*summary, n, "%s with %zu memories", name, agent->memory_count);
    *summary_size = strlen(*summary);
    return COG_OK;
}

COGUTIL_API cog_result_t azurite_plan(azurite_agent_t agent, const char* goal, azurite_plan_t* plan) {
    if (!agent || !goal || !plan) return COG_ERROR_INVALID_PARAM;
    memset(plan, 0, sizeof(*plan));
    plan->id = agent->next_plan_id++;
    plan->goal = az_strdup(goal);
    plan->active = true;
    plan->start_time = (uint64_t)time(NULL);
    plan->progress = 0.0f;
    return COG_OK;
}

COGUTIL_API cog_result_t azurite_execute_step(azurite_agent_t agent, azurite_plan_t* plan, char** action, size_t* action_size) {
    (void)agent;
    if (!plan || !action || !action_size) return COG_ERROR_INVALID_PARAM;
    *action = az_strdup(plan->goal ? plan->goal : "execute");
    if (!*action) return COG_ERROR_MEMORY;
    *action_size = strlen(*action);
    plan->completed = true;
    plan->active = false;
    plan->progress = 1.0f;
    return COG_OK;
}

COGUTIL_API cog_result_t azurite_replan(azurite_agent_t agent, azurite_plan_t* plan, const char* new_information) {
    (void)agent; (void)new_information;
    if (!plan) return COG_ERROR_INVALID_PARAM;
    plan->current_step = 0;
    plan->completed = false;
    plan->active = true;
    return COG_OK;
}

COGUTIL_API cog_result_t azurite_get_plans(azurite_agent_t agent, azurite_plan_t** plans, size_t* count) {
    if (!agent || !plans || !count) return COG_ERROR_INVALID_PARAM;
    *plans = NULL;
    *count = 0;
    return COG_OK;
}

COGUTIL_API void azurite_plan_free(azurite_plan_t* plan) {
    if (!plan) return;
    free((void*)plan->goal);
    for (size_t i = 0; i < plan->step_count; ++i) free((void*)plan->steps[i]);
    free((void*)plan->steps);
}

COGUTIL_API cog_result_t azurite_react(azurite_agent_t agent, const char* observation, char** reaction, size_t* reaction_size) {
    if (!agent || !observation || !reaction || !reaction_size) return COG_ERROR_INVALID_PARAM;
    append_memory(agent, AZURITE_MEM_OBSERVATION, observation, 0.5f, NULL);
    *reaction = az_strdup("Acknowledged.");
    if (!*reaction) return COG_ERROR_MEMORY;
    *reaction_size = strlen(*reaction);
    return COG_OK;
}

COGUTIL_API cog_result_t azurite_react_to_agent(azurite_agent_t agent, azurite_agent_t other_agent, const char* other_action, char** reaction, size_t* reaction_size) {
    (void)other_agent;
    return azurite_react(agent, other_action ? other_action : "agent action", reaction, reaction_size);
}

COGUTIL_API cog_result_t azurite_dialogue(azurite_agent_t agent, azurite_agent_t other_agent, const char* topic, uint32_t max_turns, char*** dialogue, size_t* turn_count) {
    (void)agent; (void)other_agent; (void)topic;
    if (!dialogue || !turn_count) return COG_ERROR_INVALID_PARAM;
    *turn_count = max_turns ? 1 : 0;
    *dialogue = *turn_count ? calloc(1, sizeof(char*)) : NULL;
    if (*turn_count && !*dialogue) return COG_ERROR_MEMORY;
    if (*turn_count) (*dialogue)[0] = az_strdup("Hello.");
    return COG_OK;
}

COGUTIL_API cog_result_t azurite_get_emotion(azurite_agent_t agent, azurite_emotion_t* emotion) {
    if (!agent || !emotion) return COG_ERROR_INVALID_PARAM;
    *emotion = agent->emotion;
    return COG_OK;
}

COGUTIL_API cog_result_t azurite_process_emotion(azurite_agent_t agent, const char* event, azurite_emotion_t* new_emotion) {
    (void)event;
    if (!agent || !new_emotion) return COG_ERROR_INVALID_PARAM;
    agent->emotion = *new_emotion;
    return COG_OK;
}

COGUTIL_API cog_result_t azurite_get_relationship(azurite_agent_t agent, uint64_t other_agent_id, azurite_relationship_t* relationship) {
    if (!agent || !relationship) return COG_ERROR_INVALID_PARAM;
    memset(relationship, 0, sizeof(*relationship));
    relationship->other_agent_id = other_agent_id;
    relationship->type = AZURITE_REL_STRANGER;
    return COG_OK;
}

COGUTIL_API cog_result_t azurite_update_relationship(azurite_agent_t agent, uint64_t other_agent_id, const char* interaction_description) {
    (void)other_agent_id; (void)interaction_description;
    return agent ? COG_OK : COG_ERROR_INVALID_PARAM;
}

COGUTIL_API cog_result_t azurite_get_relationships(azurite_agent_t agent, azurite_relationship_t** relationships, size_t* count) {
    if (!agent || !relationships || !count) return COG_ERROR_INVALID_PARAM;
    *relationships = NULL;
    *count = 0;
    return COG_OK;
}

COGUTIL_API cog_result_t azurite_register_hagent(azurite_agent_t agent, hagent_context_t hagent_ctx, uint64_t* hagent_id) {
    (void)hagent_ctx;
    if (!agent || !hagent_id) return COG_ERROR_INVALID_PARAM;
    *hagent_id = 0;
    return COG_OK;
}

COGUTIL_API cog_result_t azurite_from_hagent_query(hagent_context_t hagent_ctx, const char* query, azurite_agent_t* agent) {
    (void)hagent_ctx; (void)query;
    if (!agent) return COG_ERROR_INVALID_PARAM;
    azurite_agent_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.name = "hagent";
    return azurite_agent_create(&cfg, agent);
}
