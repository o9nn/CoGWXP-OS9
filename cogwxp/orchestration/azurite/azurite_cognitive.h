/**
 * @file azurite_cognitive.h
 * @brief Azurite Cognitive Architecture for Generative Agents
 * 
 * Integrates the Azurite cognitive architecture for creating
 * generative agents with memory, planning, and reflection capabilities.
 * 
 * Based on: Azurite-Cognitive-Architecture-v0-for-Generative-Agents
 * 
 * Features:
 * - Memory stream (observations, reflections, plans)
 * - Retrieval system with recency, importance, relevance
 * - Reflection and higher-level abstractions
 * - Planning and reaction systems
 * - AtomSpace integration for persistent memory
 * 
 * @copyright CoGWXP-OS9 Project
 */

#ifndef _COGWXP_AZURITE_COGNITIVE_H_
#define _COGWXP_AZURITE_COGNITIVE_H_

#include "../opencog/atomspace/atomspace.h"
#include "../cogpilot/cogpilot.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Memory Types
 *===========================================================================*/

/**
 * Memory node types
 */
typedef enum {
    AZURITE_MEM_OBSERVATION,    /* Direct observation of environment */
    AZURITE_MEM_REFLECTION,     /* Higher-level abstraction */
    AZURITE_MEM_PLAN,           /* Future action plan */
    AZURITE_MEM_THOUGHT,        /* Internal reasoning */
    AZURITE_MEM_EMOTION,        /* Emotional state */
    AZURITE_MEM_BELIEF,         /* Belief about world */
    AZURITE_MEM_GOAL,           /* Active goal */
    AZURITE_MEM_RELATIONSHIP    /* Relationship with other agent */
} azurite_memory_type_t;

/**
 * Memory node
 */
typedef struct {
    uint64_t id;
    azurite_memory_type_t type;
    const char* content;
    
    /* Temporal */
    uint64_t creation_time;
    uint64_t last_access_time;
    uint32_t access_count;
    
    /* Importance scoring */
    float importance;           /* 1-10 scale */
    float recency;              /* Decay over time */
    float relevance;            /* Context-dependent */
    
    /* Embedding for similarity search */
    float* embedding;
    size_t embedding_dim;
    
    /* AtomSpace representation */
    atom_handle_t atom;
    
    /* Links to other memories */
    uint64_t* related_ids;
    size_t related_count;
} azurite_memory_t;

/*===========================================================================
 * Agent Configuration
 *===========================================================================*/

/**
 * Agent personality traits (Big Five)
 */
typedef struct {
    float openness;             /* 0-1: Curiosity, creativity */
    float conscientiousness;    /* 0-1: Organization, dependability */
    float extraversion;         /* 0-1: Sociability, assertiveness */
    float agreeableness;        /* 0-1: Cooperation, trust */
    float neuroticism;          /* 0-1: Emotional instability */
} azurite_personality_t;

/**
 * Agent configuration
 */
typedef struct {
    const char* name;
    const char* description;
    const char* background;
    
    /* Personality */
    azurite_personality_t personality;
    
    /* Goals and motivations */
    const char** initial_goals;
    size_t goal_count;
    
    /* Memory settings */
    size_t max_memory_nodes;
    float memory_decay_rate;
    uint32_t reflection_threshold;  /* Memories before reflection */
    
    /* LLM settings */
    const char* llm_model;
    float temperature;
    uint32_t max_tokens;
    
    /* AtomSpace */
    atomspace_t atomspace;
    
    /* Azure CogML for embeddings/LLM */
    azure_cogml_context_t azure_ctx;
} azurite_agent_config_t;

/**
 * Azurite agent context
 */
typedef struct azurite_agent* azurite_agent_t;

/*===========================================================================
 * Agent Lifecycle
 *===========================================================================*/

/**
 * Create Azurite agent
 */
COGUTIL_API cog_result_t azurite_agent_create(
    const azurite_agent_config_t* config,
    azurite_agent_t* agent
);

/**
 * Destroy Azurite agent
 */
COGUTIL_API void azurite_agent_destroy(azurite_agent_t agent);

/**
 * Save agent state
 */
COGUTIL_API cog_result_t azurite_agent_save(
    azurite_agent_t agent,
    const char* path
);

/**
 * Load agent state
 */
COGUTIL_API cog_result_t azurite_agent_load(
    azurite_agent_t agent,
    const char* path
);

/*===========================================================================
 * Memory Stream
 *===========================================================================*/

/**
 * Add observation to memory
 */
COGUTIL_API cog_result_t azurite_observe(
    azurite_agent_t agent,
    const char* observation,
    float importance,
    azurite_memory_t* memory
);

/**
 * Add thought to memory
 */
COGUTIL_API cog_result_t azurite_think(
    azurite_agent_t agent,
    const char* thought,
    azurite_memory_t* memory
);

/**
 * Retrieve relevant memories
 */
COGUTIL_API cog_result_t azurite_retrieve(
    azurite_agent_t agent,
    const char* query,
    uint32_t max_results,
    azurite_memory_t** memories,
    size_t* count
);

/**
 * Retrieve memories by type
 */
COGUTIL_API cog_result_t azurite_retrieve_by_type(
    azurite_agent_t agent,
    azurite_memory_type_t type,
    uint32_t max_results,
    azurite_memory_t** memories,
    size_t* count
);

/**
 * Get recent memories
 */
COGUTIL_API cog_result_t azurite_get_recent(
    azurite_agent_t agent,
    uint32_t count,
    azurite_memory_t** memories,
    size_t* actual_count
);

/**
 * Free memory array
 */
COGUTIL_API void azurite_memories_free(
    azurite_memory_t* memories,
    size_t count
);

/*===========================================================================
 * Reflection System
 *===========================================================================*/

/**
 * Trigger reflection
 * 
 * Generates higher-level abstractions from recent memories
 */
COGUTIL_API cog_result_t azurite_reflect(
    azurite_agent_t agent,
    azurite_memory_t** reflections,
    size_t* count
);

/**
 * Generate reflection on specific topic
 */
COGUTIL_API cog_result_t azurite_reflect_on(
    azurite_agent_t agent,
    const char* topic,
    azurite_memory_t* reflection
);

/**
 * Get agent's self-summary
 */
COGUTIL_API cog_result_t azurite_self_summary(
    azurite_agent_t agent,
    char** summary,
    size_t* summary_size
);

/*===========================================================================
 * Planning System
 *===========================================================================*/

/**
 * Plan structure
 */
typedef struct {
    uint64_t id;
    const char* goal;
    
    /* Plan steps */
    const char** steps;
    size_t step_count;
    size_t current_step;
    
    /* Timing */
    uint64_t start_time;
    uint64_t deadline;
    
    /* Status */
    bool active;
    bool completed;
    float progress;
    
    /* AtomSpace representation */
    atom_handle_t atom;
} azurite_plan_t;

/**
 * Create plan for goal
 */
COGUTIL_API cog_result_t azurite_plan(
    azurite_agent_t agent,
    const char* goal,
    azurite_plan_t* plan
);

/**
 * Execute next plan step
 */
COGUTIL_API cog_result_t azurite_execute_step(
    azurite_agent_t agent,
    azurite_plan_t* plan,
    char** action,
    size_t* action_size
);

/**
 * Replan based on new information
 */
COGUTIL_API cog_result_t azurite_replan(
    azurite_agent_t agent,
    azurite_plan_t* plan,
    const char* new_information
);

/**
 * Get active plans
 */
COGUTIL_API cog_result_t azurite_get_plans(
    azurite_agent_t agent,
    azurite_plan_t** plans,
    size_t* count
);

/**
 * Free plan
 */
COGUTIL_API void azurite_plan_free(azurite_plan_t* plan);

/*===========================================================================
 * Reaction System
 *===========================================================================*/

/**
 * React to observation
 * 
 * Generates immediate response based on personality and context
 */
COGUTIL_API cog_result_t azurite_react(
    azurite_agent_t agent,
    const char* observation,
    char** reaction,
    size_t* reaction_size
);

/**
 * React to another agent
 */
COGUTIL_API cog_result_t azurite_react_to_agent(
    azurite_agent_t agent,
    azurite_agent_t other_agent,
    const char* other_action,
    char** reaction,
    size_t* reaction_size
);

/**
 * Dialogue with another agent
 */
COGUTIL_API cog_result_t azurite_dialogue(
    azurite_agent_t agent,
    azurite_agent_t other_agent,
    const char* topic,
    uint32_t max_turns,
    char*** dialogue,
    size_t* turn_count
);

/*===========================================================================
 * Emotional State
 *===========================================================================*/

/**
 * Emotional state
 */
typedef struct {
    float joy;          /* 0-1 */
    float sadness;      /* 0-1 */
    float anger;        /* 0-1 */
    float fear;         /* 0-1 */
    float surprise;     /* 0-1 */
    float disgust;      /* 0-1 */
    float trust;        /* 0-1 */
    float anticipation; /* 0-1 */
    
    /* Derived */
    float valence;      /* -1 to 1: negative to positive */
    float arousal;      /* 0-1: calm to excited */
} azurite_emotion_t;

/**
 * Get current emotional state
 */
COGUTIL_API cog_result_t azurite_get_emotion(
    azurite_agent_t agent,
    azurite_emotion_t* emotion
);

/**
 * Update emotional state based on event
 */
COGUTIL_API cog_result_t azurite_process_emotion(
    azurite_agent_t agent,
    const char* event,
    azurite_emotion_t* new_emotion
);

/*===========================================================================
 * Social Relationships
 *===========================================================================*/

/**
 * Relationship type
 */
typedef enum {
    AZURITE_REL_STRANGER,
    AZURITE_REL_ACQUAINTANCE,
    AZURITE_REL_FRIEND,
    AZURITE_REL_CLOSE_FRIEND,
    AZURITE_REL_FAMILY,
    AZURITE_REL_ROMANTIC,
    AZURITE_REL_RIVAL,
    AZURITE_REL_ENEMY
} azurite_relationship_type_t;

/**
 * Relationship
 */
typedef struct {
    uint64_t other_agent_id;
    const char* other_agent_name;
    azurite_relationship_type_t type;
    
    float closeness;        /* 0-1 */
    float trust;            /* 0-1 */
    float respect;          /* 0-1 */
    
    /* Interaction history */
    uint32_t interaction_count;
    uint64_t last_interaction;
    
    /* AtomSpace representation */
    atom_handle_t atom;
} azurite_relationship_t;

/**
 * Get relationship with agent
 */
COGUTIL_API cog_result_t azurite_get_relationship(
    azurite_agent_t agent,
    uint64_t other_agent_id,
    azurite_relationship_t* relationship
);

/**
 * Update relationship
 */
COGUTIL_API cog_result_t azurite_update_relationship(
    azurite_agent_t agent,
    uint64_t other_agent_id,
    const char* interaction_description
);

/**
 * Get all relationships
 */
COGUTIL_API cog_result_t azurite_get_relationships(
    azurite_agent_t agent,
    azurite_relationship_t** relationships,
    size_t* count
);

/*===========================================================================
 * Integration with CogPilot
 *===========================================================================*/

/**
 * Register Azurite agent with HyperAgentQL
 */
COGUTIL_API cog_result_t azurite_register_hagent(
    azurite_agent_t agent,
    hagent_context_t hagent_ctx,
    uint64_t* hagent_id
);

/**
 * Create Azurite agent from HyperAgentQL query
 */
COGUTIL_API cog_result_t azurite_from_hagent_query(
    hagent_context_t hagent_ctx,
    const char* query,
    azurite_agent_t* agent
);

#ifdef __cplusplus
}
#endif

#endif /* _COGWXP_AZURITE_COGNITIVE_H_ */
