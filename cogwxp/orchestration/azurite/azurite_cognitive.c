/**
 * @file azurite_cognitive.c
 * @brief Azurite Cognitive Architecture Implementation
 * 
 * Implements generative agent architecture with memory streams,
 * reflection, planning, and emotional/social modeling.
 * 
 * @copyright CoGWXP-OS9 Project
 */

#include "azurite_cognitive.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <math.h>
#include <time.h>

/*===========================================================================
 * Internal Structures
 *===========================================================================*/

/* Memory entry */
typedef struct memory_entry {
    uint64_t id;
    azurite_memory_type_t type;
    char* content;
    time_t timestamp;
    float importance;
    float* embedding;
    size_t embedding_dim;
    uint32_t access_count;
    time_t last_accessed;
    struct memory_entry* next;
} memory_entry_t;

/* Plan step */
typedef struct plan_step {
    uint64_t id;
    char* description;
    time_t scheduled_time;
    uint32_t duration_minutes;
    bool completed;
    struct plan_step* next;
} plan_step_t;

/* Relationship entry */
typedef struct relationship_entry {
    uint64_t agent_id;
    char* agent_name;
    float affinity;
    char* relationship_type;
    char** shared_memories;
    size_t shared_memory_count;
    struct relationship_entry* next;
} relationship_entry_t;

/* Agent state */
struct azurite_agent {
    uint64_t id;
    azurite_agent_config_t config;
    
    /* Memory stream */
    memory_entry_t* memory_head;
    memory_entry_t* memory_tail;
    size_t memory_count;
    pthread_rwlock_t memory_lock;
    
    /* Current plan */
    plan_step_t* plan_head;
    plan_step_t* plan_tail;
    pthread_mutex_t plan_lock;
    
    /* Emotional state */
    azurite_emotional_state_t emotional_state;
    pthread_mutex_t emotion_lock;
    
    /* Relationships */
    relationship_entry_t* relationships_head;
    pthread_rwlock_t relationships_lock;
    
    /* Current activity */
    char* current_activity;
    char* current_location;
    pthread_mutex_t activity_lock;
    
    /* Statistics */
    struct {
        uint64_t memories_added;
        uint64_t reflections_generated;
        uint64_t plans_created;
        uint64_t reactions_processed;
    } stats;
    pthread_mutex_t stats_lock;
    
    /* Next IDs */
    uint64_t next_memory_id;
    uint64_t next_plan_id;
    pthread_mutex_t id_lock;
};

/* Azurite context */
struct azurite_context {
    azurite_config_t config;
    
    /* Agents */
    azurite_agent_t** agents;
    size_t agent_count;
    size_t agent_capacity;
    pthread_rwlock_t agents_lock;
    
    /* Embedding model */
    struct {
        float* weights;
        size_t input_dim;
        size_t output_dim;
    } embedding_model;
    
    /* LLM interface */
    struct {
        char* endpoint;
        char* api_key;
    } llm;
    
    /* Statistics */
    azurite_stats_t stats;
    pthread_mutex_t stats_lock;
    
    /* Next agent ID */
    uint64_t next_agent_id;
    pthread_mutex_t id_lock;
};

/*===========================================================================
 * Utility Functions
 *===========================================================================*/

static uint64_t generate_id(pthread_mutex_t* lock, uint64_t* counter) {
    pthread_mutex_lock(lock);
    uint64_t id = (*counter)++;
    pthread_mutex_unlock(lock);
    return id;
}

static float compute_recency(time_t timestamp, time_t now) {
    double hours_ago = difftime(now, timestamp) / 3600.0;
    return (float)exp(-0.99 * hours_ago);
}

static float compute_relevance(float* embedding1, float* embedding2, size_t dim) {
    if (!embedding1 || !embedding2 || dim == 0) return 0.0f;
    
    float dot = 0.0f, norm1 = 0.0f, norm2 = 0.0f;
    for (size_t i = 0; i < dim; i++) {
        dot += embedding1[i] * embedding2[i];
        norm1 += embedding1[i] * embedding1[i];
        norm2 += embedding2[i] * embedding2[i];
    }
    
    if (norm1 == 0.0f || norm2 == 0.0f) return 0.0f;
    return dot / (sqrtf(norm1) * sqrtf(norm2));
}

static float compute_retrieval_score(
    memory_entry_t* memory,
    float* query_embedding,
    size_t embedding_dim,
    time_t now,
    float recency_weight,
    float importance_weight,
    float relevance_weight
) {
    float recency = compute_recency(memory->timestamp, now);
    float relevance = compute_relevance(memory->embedding, query_embedding, embedding_dim);
    
    return recency_weight * recency +
           importance_weight * memory->importance +
           relevance_weight * relevance;
}

/*===========================================================================
 * Embedding Generation
 *===========================================================================*/

static cog_result_t generate_embedding(
    azurite_context_t ctx,
    const char* text,
    float** embedding,
    size_t* dim
) {
    /* In a real implementation, this would call an embedding API */
    /* For now, generate a simple hash-based embedding */
    
    size_t output_dim = ctx->embedding_model.output_dim > 0 ? 
                        ctx->embedding_model.output_dim : 384;
    
    *embedding = calloc(output_dim, sizeof(float));
    *dim = output_dim;
    
    /* Simple hash-based embedding for demonstration */
    size_t text_len = strlen(text);
    for (size_t i = 0; i < output_dim; i++) {
        float sum = 0.0f;
        for (size_t j = 0; j < text_len; j++) {
            sum += (float)text[j] * sinf((float)(i + j + 1));
        }
        (*embedding)[i] = tanhf(sum / (float)(text_len + 1));
    }
    
    return COG_OK;
}

/*===========================================================================
 * LLM Interface
 *===========================================================================*/

static cog_result_t llm_generate(
    azurite_context_t ctx,
    const char* prompt,
    char** response
) {
    /* In a real implementation, this would call an LLM API */
    /* For now, return a placeholder response */
    
    *response = strdup("[Generated response based on prompt]");
    return COG_OK;
}

/*===========================================================================
 * Memory Operations
 *===========================================================================*/

static memory_entry_t* create_memory_entry(
    azurite_agent_t agent,
    azurite_memory_type_t type,
    const char* content,
    float importance
) {
    memory_entry_t* entry = calloc(1, sizeof(memory_entry_t));
    if (!entry) return NULL;
    
    entry->id = generate_id(&agent->id_lock, &agent->next_memory_id);
    entry->type = type;
    entry->content = strdup(content);
    entry->timestamp = time(NULL);
    entry->importance = importance;
    entry->access_count = 0;
    entry->last_accessed = entry->timestamp;
    
    return entry;
}

static void destroy_memory_entry(memory_entry_t* entry) {
    if (!entry) return;
    free(entry->content);
    free(entry->embedding);
    free(entry);
}

/*===========================================================================
 * Context Lifecycle
 *===========================================================================*/

COGUTIL_API cog_result_t azurite_init(
    const azurite_config_t* config,
    azurite_context_t* ctx
) {
    if (!config || !ctx) return COG_ERROR_INVALID_PARAM;
    
    azurite_context_t c = calloc(1, sizeof(struct azurite_context));
    if (!c) return COG_ERROR_MEMORY;
    
    /* Copy config */
    memcpy(&c->config, config, sizeof(azurite_config_t));
    if (config->llm_endpoint) c->config.llm_endpoint = strdup(config->llm_endpoint);
    if (config->llm_api_key) c->config.llm_api_key = strdup(config->llm_api_key);
    if (config->embedding_model) c->config.embedding_model = strdup(config->embedding_model);
    
    /* Initialize locks */
    pthread_rwlock_init(&c->agents_lock, NULL);
    pthread_mutex_init(&c->stats_lock, NULL);
    pthread_mutex_init(&c->id_lock, NULL);
    
    /* Initialize agents array */
    c->agent_capacity = 64;
    c->agents = calloc(c->agent_capacity, sizeof(azurite_agent_t));
    
    /* Initialize embedding model */
    c->embedding_model.output_dim = config->embedding_dim > 0 ? config->embedding_dim : 384;
    
    /* Initialize LLM interface */
    if (config->llm_endpoint) c->llm.endpoint = strdup(config->llm_endpoint);
    if (config->llm_api_key) c->llm.api_key = strdup(config->llm_api_key);
    
    /* Initialize next agent ID */
    c->next_agent_id = 1;
    
    *ctx = c;
    return COG_OK;
}

COGUTIL_API void azurite_shutdown(azurite_context_t ctx) {
    if (!ctx) return;
    
    /* Destroy all agents */
    pthread_rwlock_wrlock(&ctx->agents_lock);
    for (size_t i = 0; i < ctx->agent_count; i++) {
        azurite_destroy_agent(ctx->agents[i]);
    }
    free(ctx->agents);
    pthread_rwlock_unlock(&ctx->agents_lock);
    
    /* Free config strings */
    free((void*)ctx->config.llm_endpoint);
    free((void*)ctx->config.llm_api_key);
    free((void*)ctx->config.embedding_model);
    
    /* Free LLM interface */
    free(ctx->llm.endpoint);
    free(ctx->llm.api_key);
    
    /* Free embedding model */
    free(ctx->embedding_model.weights);
    
    /* Destroy locks */
    pthread_rwlock_destroy(&ctx->agents_lock);
    pthread_mutex_destroy(&ctx->stats_lock);
    pthread_mutex_destroy(&ctx->id_lock);
    
    free(ctx);
}

/*===========================================================================
 * Agent Lifecycle
 *===========================================================================*/

COGUTIL_API cog_result_t azurite_create_agent(
    azurite_context_t ctx,
    const azurite_agent_config_t* config,
    azurite_agent_t* agent
) {
    if (!ctx || !config || !agent) return COG_ERROR_INVALID_PARAM;
    
    azurite_agent_t a = calloc(1, sizeof(struct azurite_agent));
    if (!a) return COG_ERROR_MEMORY;
    
    a->id = generate_id(&ctx->id_lock, &ctx->next_agent_id);
    
    /* Copy config */
    memcpy(&a->config, config, sizeof(azurite_agent_config_t));
    if (config->name) a->config.name = strdup(config->name);
    if (config->description) a->config.description = strdup(config->description);
    if (config->personality) a->config.personality = strdup(config->personality);
    if (config->backstory) a->config.backstory = strdup(config->backstory);
    if (config->goals) a->config.goals = strdup(config->goals);
    
    /* Initialize locks */
    pthread_rwlock_init(&a->memory_lock, NULL);
    pthread_mutex_init(&a->plan_lock, NULL);
    pthread_mutex_init(&a->emotion_lock, NULL);
    pthread_rwlock_init(&a->relationships_lock, NULL);
    pthread_mutex_init(&a->activity_lock, NULL);
    pthread_mutex_init(&a->stats_lock, NULL);
    pthread_mutex_init(&a->id_lock, NULL);
    
    /* Initialize emotional state */
    a->emotional_state.valence = 0.5f;
    a->emotional_state.arousal = 0.5f;
    a->emotional_state.dominance = 0.5f;
    
    /* Initialize IDs */
    a->next_memory_id = 1;
    a->next_plan_id = 1;
    
    /* Add initial memories from backstory */
    if (config->backstory) {
        azurite_add_memory(ctx, a, AZURITE_MEMORY_OBSERVATION, config->backstory, 0.8f);
    }
    
    /* Add to context */
    pthread_rwlock_wrlock(&ctx->agents_lock);
    
    if (ctx->agent_count >= ctx->agent_capacity) {
        size_t new_capacity = ctx->agent_capacity * 2;
        azurite_agent_t* new_agents = realloc(ctx->agents, new_capacity * sizeof(azurite_agent_t));
        if (!new_agents) {
            pthread_rwlock_unlock(&ctx->agents_lock);
            free(a);
            return COG_ERROR_MEMORY;
        }
        ctx->agents = new_agents;
        ctx->agent_capacity = new_capacity;
    }
    
    ctx->agents[ctx->agent_count++] = a;
    pthread_rwlock_unlock(&ctx->agents_lock);
    
    *agent = a;
    
    /* Update stats */
    pthread_mutex_lock(&ctx->stats_lock);
    ctx->stats.agents_created++;
    pthread_mutex_unlock(&ctx->stats_lock);
    
    return COG_OK;
}

COGUTIL_API void azurite_destroy_agent(azurite_agent_t agent) {
    if (!agent) return;
    
    /* Free memories */
    memory_entry_t* mem = agent->memory_head;
    while (mem) {
        memory_entry_t* next = mem->next;
        destroy_memory_entry(mem);
        mem = next;
    }
    
    /* Free plan */
    plan_step_t* step = agent->plan_head;
    while (step) {
        plan_step_t* next = step->next;
        free(step->description);
        free(step);
        step = next;
    }
    
    /* Free relationships */
    relationship_entry_t* rel = agent->relationships_head;
    while (rel) {
        relationship_entry_t* next = rel->next;
        free(rel->agent_name);
        free(rel->relationship_type);
        for (size_t i = 0; i < rel->shared_memory_count; i++) {
            free(rel->shared_memories[i]);
        }
        free(rel->shared_memories);
        free(rel);
        rel = next;
    }
    
    /* Free config strings */
    free((void*)agent->config.name);
    free((void*)agent->config.description);
    free((void*)agent->config.personality);
    free((void*)agent->config.backstory);
    free((void*)agent->config.goals);
    
    /* Free current state */
    free(agent->current_activity);
    free(agent->current_location);
    
    /* Destroy locks */
    pthread_rwlock_destroy(&agent->memory_lock);
    pthread_mutex_destroy(&agent->plan_lock);
    pthread_mutex_destroy(&agent->emotion_lock);
    pthread_rwlock_destroy(&agent->relationships_lock);
    pthread_mutex_destroy(&agent->activity_lock);
    pthread_mutex_destroy(&agent->stats_lock);
    pthread_mutex_destroy(&agent->id_lock);
    
    free(agent);
}

/*===========================================================================
 * Memory Stream Operations
 *===========================================================================*/

COGUTIL_API cog_result_t azurite_add_memory(
    azurite_context_t ctx,
    azurite_agent_t agent,
    azurite_memory_type_t type,
    const char* content,
    float importance
) {
    if (!ctx || !agent || !content) return COG_ERROR_INVALID_PARAM;
    
    memory_entry_t* entry = create_memory_entry(agent, type, content, importance);
    if (!entry) return COG_ERROR_MEMORY;
    
    /* Generate embedding */
    generate_embedding(ctx, content, &entry->embedding, &entry->embedding_dim);
    
    /* Add to memory stream */
    pthread_rwlock_wrlock(&agent->memory_lock);
    
    if (agent->memory_tail) {
        agent->memory_tail->next = entry;
        agent->memory_tail = entry;
    } else {
        agent->memory_head = agent->memory_tail = entry;
    }
    agent->memory_count++;
    
    pthread_rwlock_unlock(&agent->memory_lock);
    
    /* Update stats */
    pthread_mutex_lock(&agent->stats_lock);
    agent->stats.memories_added++;
    pthread_mutex_unlock(&agent->stats_lock);
    
    return COG_OK;
}

COGUTIL_API cog_result_t azurite_retrieve_memories(
    azurite_context_t ctx,
    azurite_agent_t agent,
    const char* query,
    size_t max_results,
    azurite_memory_t** memories,
    size_t* count
) {
    if (!ctx || !agent || !query || !memories || !count) return COG_ERROR_INVALID_PARAM;
    
    /* Generate query embedding */
    float* query_embedding = NULL;
    size_t embedding_dim = 0;
    generate_embedding(ctx, query, &query_embedding, &embedding_dim);
    
    time_t now = time(NULL);
    
    /* Score all memories */
    pthread_rwlock_rdlock(&agent->memory_lock);
    
    typedef struct {
        memory_entry_t* entry;
        float score;
    } scored_memory_t;
    
    scored_memory_t* scored = calloc(agent->memory_count, sizeof(scored_memory_t));
    size_t scored_count = 0;
    
    memory_entry_t* mem = agent->memory_head;
    while (mem) {
        scored[scored_count].entry = mem;
        scored[scored_count].score = compute_retrieval_score(
            mem, query_embedding, embedding_dim, now,
            ctx->config.recency_weight,
            ctx->config.importance_weight,
            ctx->config.relevance_weight);
        scored_count++;
        mem = mem->next;
    }
    
    /* Sort by score (simple bubble sort for now) */
    for (size_t i = 0; i < scored_count - 1; i++) {
        for (size_t j = 0; j < scored_count - i - 1; j++) {
            if (scored[j].score < scored[j + 1].score) {
                scored_memory_t temp = scored[j];
                scored[j] = scored[j + 1];
                scored[j + 1] = temp;
            }
        }
    }
    
    /* Return top results */
    size_t result_count = (max_results < scored_count) ? max_results : scored_count;
    *memories = calloc(result_count, sizeof(azurite_memory_t));
    *count = result_count;
    
    for (size_t i = 0; i < result_count; i++) {
        (*memories)[i].id = scored[i].entry->id;
        (*memories)[i].type = scored[i].entry->type;
        (*memories)[i].content = strdup(scored[i].entry->content);
        (*memories)[i].timestamp = scored[i].entry->timestamp;
        (*memories)[i].importance = scored[i].entry->importance;
        
        /* Update access count */
        scored[i].entry->access_count++;
        scored[i].entry->last_accessed = now;
    }
    
    pthread_rwlock_unlock(&agent->memory_lock);
    
    free(scored);
    free(query_embedding);
    
    return COG_OK;
}

/*===========================================================================
 * Reflection Engine
 *===========================================================================*/

COGUTIL_API cog_result_t azurite_reflect(
    azurite_context_t ctx,
    azurite_agent_t agent,
    size_t recent_memory_count,
    char** reflection
) {
    if (!ctx || !agent || !reflection) return COG_ERROR_INVALID_PARAM;
    
    /* Retrieve recent memories */
    azurite_memory_t* memories = NULL;
    size_t count = 0;
    azurite_retrieve_memories(ctx, agent, "recent events and observations", 
        recent_memory_count, &memories, &count);
    
    /* Build reflection prompt */
    char prompt[4096];
    int offset = snprintf(prompt, sizeof(prompt),
        "You are %s. Based on your recent memories, generate a higher-level insight or reflection.\n\n"
        "Your personality: %s\n\n"
        "Recent memories:\n",
        agent->config.name,
        agent->config.personality);
    
    for (size_t i = 0; i < count && offset < (int)sizeof(prompt) - 256; i++) {
        offset += snprintf(prompt + offset, sizeof(prompt) - offset,
            "- %s\n", memories[i].content);
    }
    
    snprintf(prompt + offset, sizeof(prompt) - offset,
        "\nWhat is a key insight or reflection based on these memories?");
    
    /* Generate reflection */
    char* generated = NULL;
    cog_result_t result = llm_generate(ctx, prompt, &generated);
    
    if (result == COG_OK && generated) {
        /* Add reflection as new memory */
        azurite_add_memory(ctx, agent, AZURITE_MEMORY_REFLECTION, generated, 0.9f);
        *reflection = generated;
        
        /* Update stats */
        pthread_mutex_lock(&agent->stats_lock);
        agent->stats.reflections_generated++;
        pthread_mutex_unlock(&agent->stats_lock);
    }
    
    /* Cleanup */
    for (size_t i = 0; i < count; i++) {
        free((void*)memories[i].content);
    }
    free(memories);
    
    return result;
}

/*===========================================================================
 * Planning System
 *===========================================================================*/

COGUTIL_API cog_result_t azurite_create_plan(
    azurite_context_t ctx,
    azurite_agent_t agent,
    const char* goal,
    uint32_t time_horizon_hours,
    azurite_plan_t* plan
) {
    if (!ctx || !agent || !goal || !plan) return COG_ERROR_INVALID_PARAM;
    
    /* Retrieve relevant memories */
    azurite_memory_t* memories = NULL;
    size_t count = 0;
    azurite_retrieve_memories(ctx, agent, goal, 10, &memories, &count);
    
    /* Build planning prompt */
    char prompt[4096];
    int offset = snprintf(prompt, sizeof(prompt),
        "You are %s. Create a detailed plan to achieve the following goal:\n\n"
        "Goal: %s\n"
        "Time horizon: %u hours\n\n"
        "Your personality: %s\n"
        "Your goals: %s\n\n"
        "Relevant memories:\n",
        agent->config.name,
        goal,
        time_horizon_hours,
        agent->config.personality,
        agent->config.goals);
    
    for (size_t i = 0; i < count && offset < (int)sizeof(prompt) - 256; i++) {
        offset += snprintf(prompt + offset, sizeof(prompt) - offset,
            "- %s\n", memories[i].content);
    }
    
    snprintf(prompt + offset, sizeof(prompt) - offset,
        "\nCreate a step-by-step plan with specific actions and times.");
    
    /* Generate plan */
    char* generated = NULL;
    cog_result_t result = llm_generate(ctx, prompt, &generated);
    
    if (result == COG_OK && generated) {
        /* Parse and store plan */
        pthread_mutex_lock(&agent->plan_lock);
        
        /* Clear existing plan */
        plan_step_t* step = agent->plan_head;
        while (step) {
            plan_step_t* next = step->next;
            free(step->description);
            free(step);
            step = next;
        }
        agent->plan_head = agent->plan_tail = NULL;
        
        /* Create new plan step (simplified - would parse generated text) */
        plan_step_t* new_step = calloc(1, sizeof(plan_step_t));
        new_step->id = generate_id(&agent->id_lock, &agent->next_plan_id);
        new_step->description = generated;
        new_step->scheduled_time = time(NULL);
        new_step->duration_minutes = 60;
        new_step->completed = false;
        
        agent->plan_head = agent->plan_tail = new_step;
        
        pthread_mutex_unlock(&agent->plan_lock);
        
        /* Return plan info */
        plan->id = new_step->id;
        plan->goal = strdup(goal);
        plan->step_count = 1;
        plan->steps = calloc(1, sizeof(azurite_plan_step_t));
        plan->steps[0].description = strdup(new_step->description);
        plan->steps[0].scheduled_time = new_step->scheduled_time;
        plan->steps[0].duration_minutes = new_step->duration_minutes;
        
        /* Add plan as memory */
        char plan_memory[512];
        snprintf(plan_memory, sizeof(plan_memory), "Created plan to: %s", goal);
        azurite_add_memory(ctx, agent, AZURITE_MEMORY_PLAN, plan_memory, 0.7f);
        
        /* Update stats */
        pthread_mutex_lock(&agent->stats_lock);
        agent->stats.plans_created++;
        pthread_mutex_unlock(&agent->stats_lock);
    }
    
    /* Cleanup */
    for (size_t i = 0; i < count; i++) {
        free((void*)memories[i].content);
    }
    free(memories);
    
    return result;
}

/*===========================================================================
 * Reaction System
 *===========================================================================*/

COGUTIL_API cog_result_t azurite_react(
    azurite_context_t ctx,
    azurite_agent_t agent,
    const char* stimulus,
    char** reaction
) {
    if (!ctx || !agent || !stimulus || !reaction) return COG_ERROR_INVALID_PARAM;
    
    /* Add stimulus as observation */
    azurite_add_memory(ctx, agent, AZURITE_MEMORY_OBSERVATION, stimulus, 0.6f);
    
    /* Retrieve relevant memories */
    azurite_memory_t* memories = NULL;
    size_t count = 0;
    azurite_retrieve_memories(ctx, agent, stimulus, 5, &memories, &count);
    
    /* Build reaction prompt */
    char prompt[4096];
    int offset = snprintf(prompt, sizeof(prompt),
        "You are %s.\n"
        "Personality: %s\n"
        "Current emotional state: valence=%.2f, arousal=%.2f\n\n"
        "You observe: %s\n\n"
        "Relevant memories:\n",
        agent->config.name,
        agent->config.personality,
        agent->emotional_state.valence,
        agent->emotional_state.arousal,
        stimulus);
    
    for (size_t i = 0; i < count && offset < (int)sizeof(prompt) - 256; i++) {
        offset += snprintf(prompt + offset, sizeof(prompt) - offset,
            "- %s\n", memories[i].content);
    }
    
    snprintf(prompt + offset, sizeof(prompt) - offset,
        "\nHow do you react? Describe your thoughts, feelings, and actions.");
    
    /* Generate reaction */
    char* generated = NULL;
    cog_result_t result = llm_generate(ctx, prompt, &generated);
    
    if (result == COG_OK && generated) {
        *reaction = generated;
        
        /* Update stats */
        pthread_mutex_lock(&agent->stats_lock);
        agent->stats.reactions_processed++;
        pthread_mutex_unlock(&agent->stats_lock);
    }
    
    /* Cleanup */
    for (size_t i = 0; i < count; i++) {
        free((void*)memories[i].content);
    }
    free(memories);
    
    return result;
}

/*===========================================================================
 * Emotional State
 *===========================================================================*/

COGUTIL_API cog_result_t azurite_get_emotional_state(
    azurite_agent_t agent,
    azurite_emotional_state_t* state
) {
    if (!agent || !state) return COG_ERROR_INVALID_PARAM;
    
    pthread_mutex_lock(&agent->emotion_lock);
    memcpy(state, &agent->emotional_state, sizeof(azurite_emotional_state_t));
    pthread_mutex_unlock(&agent->emotion_lock);
    
    return COG_OK;
}

COGUTIL_API cog_result_t azurite_update_emotional_state(
    azurite_agent_t agent,
    float valence_delta,
    float arousal_delta,
    float dominance_delta
) {
    if (!agent) return COG_ERROR_INVALID_PARAM;
    
    pthread_mutex_lock(&agent->emotion_lock);
    
    agent->emotional_state.valence = fmaxf(0.0f, fminf(1.0f, 
        agent->emotional_state.valence + valence_delta));
    agent->emotional_state.arousal = fmaxf(0.0f, fminf(1.0f,
        agent->emotional_state.arousal + arousal_delta));
    agent->emotional_state.dominance = fmaxf(0.0f, fminf(1.0f,
        agent->emotional_state.dominance + dominance_delta));
    
    pthread_mutex_unlock(&agent->emotion_lock);
    
    return COG_OK;
}

/*===========================================================================
 * Statistics
 *===========================================================================*/

COGUTIL_API cog_result_t azurite_get_stats(
    azurite_context_t ctx,
    azurite_stats_t* stats
) {
    if (!ctx || !stats) return COG_ERROR_INVALID_PARAM;
    
    pthread_mutex_lock(&ctx->stats_lock);
    memcpy(stats, &ctx->stats, sizeof(azurite_stats_t));
    pthread_mutex_unlock(&ctx->stats_lock);
    
    return COG_OK;
}

COGUTIL_API void azurite_reset_stats(azurite_context_t ctx) {
    if (!ctx) return;
    
    pthread_mutex_lock(&ctx->stats_lock);
    memset(&ctx->stats, 0, sizeof(azurite_stats_t));
    pthread_mutex_unlock(&ctx->stats_lock);
}
