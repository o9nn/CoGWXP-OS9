/**
 * @file cogpilot.h
 * @brief CogPilot Integration Layer for AGI Toolchain Orchestration
 * 
 * Integrates components from the cogpilot ecosystem:
 * - HyperAgentQL: Hypergraph-based agent query language
 * - AzureCogML: Azure Cognitive Services + ML integration
 * - Planetary Neural Net: Distributed neural network architecture
 * 
 * @copyright CoGWXP-OS9 Project
 */

#ifndef _COGWXP_COGPILOT_H_
#define _COGWXP_COGPILOT_H_

#include "../opencog/atomspace/atomspace.h"
#include "../opencog/cogserver/cogserver.h"
#include "../msgraph/atomspace/ms_hypergraph.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * HyperAgentQL - Agent Query Language
 *===========================================================================*/

/**
 * HyperAgentQL enables querying and orchestrating cognitive agents
 * using a hypergraph-based query language.
 * 
 * Features:
 * - Agent discovery and selection
 * - Task delegation and coordination
 * - Multi-agent reasoning chains
 * - Attention-based agent scheduling
 */

/**
 * Agent capability types
 */
typedef enum {
    HAGENT_CAP_REASONING = 0x0001,
    HAGENT_CAP_LEARNING = 0x0002,
    HAGENT_CAP_PERCEPTION = 0x0004,
    HAGENT_CAP_ACTION = 0x0008,
    HAGENT_CAP_COMMUNICATION = 0x0010,
    HAGENT_CAP_PLANNING = 0x0020,
    HAGENT_CAP_MEMORY = 0x0040,
    HAGENT_CAP_INFERENCE = 0x0080,
    HAGENT_CAP_GENERATION = 0x0100,
    HAGENT_CAP_EMBEDDING = 0x0200,
    HAGENT_CAP_TOOL_USE = 0x0400,
    HAGENT_CAP_CODE_EXEC = 0x0800
} hagent_capability_t;

/**
 * Agent state
 */
typedef enum {
    HAGENT_STATE_IDLE,
    HAGENT_STATE_BUSY,
    HAGENT_STATE_WAITING,
    HAGENT_STATE_SUSPENDED,
    HAGENT_STATE_ERROR,
    HAGENT_STATE_TERMINATED
} hagent_state_t;

/**
 * Agent definition
 */
typedef struct {
    uint64_t id;
    const char* name;
    const char* description;
    uint32_t capabilities;
    hagent_state_t state;
    
    /* AtomSpace representation */
    atom_handle_t atom;
    atomspace_t workspace;
    
    /* Performance metrics */
    double avg_response_time_ms;
    uint64_t tasks_completed;
    float success_rate;
    
    /* Attention */
    attention_value_t attention;
} hagent_t;

/**
 * HyperAgentQL context
 */
typedef struct hagent_context* hagent_context_t;

/**
 * Initialize HyperAgentQL
 */
COGUTIL_API cog_result_t hagent_init(
    atomspace_t atomspace,
    cogserver_t cogserver,
    hagent_context_t* ctx
);

/**
 * Shutdown HyperAgentQL
 */
COGUTIL_API void hagent_shutdown(hagent_context_t ctx);

/**
 * Register agent
 */
COGUTIL_API cog_result_t hagent_register(
    hagent_context_t ctx,
    const hagent_t* agent,
    uint64_t* agent_id
);

/**
 * Unregister agent
 */
COGUTIL_API cog_result_t hagent_unregister(
    hagent_context_t ctx,
    uint64_t agent_id
);

/**
 * Query agents by capability
 */
COGUTIL_API cog_result_t hagent_query_by_capability(
    hagent_context_t ctx,
    uint32_t required_capabilities,
    hagent_t** agents,
    size_t* count
);

/**
 * Query agents using HyperAgentQL
 * 
 * Query syntax:
 *   SELECT agent WHERE capability HAS reasoning AND attention.sti > 100
 *   DELEGATE task TO agent WHERE capability HAS planning
 *   CHAIN agent1 -> agent2 -> agent3 FOR complex_task
 */
COGUTIL_API cog_result_t hagent_query(
    hagent_context_t ctx,
    const char* query,
    hagent_t** results,
    size_t* count
);

/**
 * Delegate task to agent
 */
COGUTIL_API cog_result_t hagent_delegate(
    hagent_context_t ctx,
    uint64_t agent_id,
    const char* task_description,
    const char* input_json,
    char** output_json,
    size_t* output_size
);

/**
 * Create agent chain for complex tasks
 */
COGUTIL_API cog_result_t hagent_chain(
    hagent_context_t ctx,
    uint64_t* agent_ids,
    size_t agent_count,
    const char* task_description,
    const char* input_json,
    char** output_json,
    size_t* output_size
);

/*===========================================================================
 * AzureCogML - Azure Cognitive + ML Integration
 *===========================================================================*/

/**
 * Azure Cognitive Services integration for the AGI OS
 * 
 * Provides:
 * - Vision (image analysis, OCR, face detection)
 * - Speech (speech-to-text, text-to-speech)
 * - Language (text analytics, translation, QnA)
 * - Decision (anomaly detection, personalizer)
 * - OpenAI (GPT, DALL-E, embeddings)
 */

/**
 * Azure service types
 */
typedef enum {
    AZURE_SERVICE_VISION,
    AZURE_SERVICE_SPEECH,
    AZURE_SERVICE_LANGUAGE,
    AZURE_SERVICE_DECISION,
    AZURE_SERVICE_OPENAI,
    AZURE_SERVICE_SEARCH,
    AZURE_SERVICE_FORM_RECOGNIZER,
    AZURE_SERVICE_CONTENT_SAFETY
} azure_service_type_t;

/**
 * Azure CogML configuration
 */
typedef struct {
    const char* subscription_key;
    const char* endpoint;
    const char* region;
    
    /* OpenAI specific */
    const char* openai_api_key;
    const char* openai_deployment;
    
    /* AtomSpace integration */
    atomspace_t atomspace;
    bool cache_results;
    uint32_t cache_ttl_seconds;
} azure_cogml_config_t;

/**
 * Azure CogML context
 */
typedef struct azure_cogml_context* azure_cogml_context_t;

/**
 * Initialize Azure CogML
 */
COGUTIL_API cog_result_t azure_cogml_init(
    const azure_cogml_config_t* config,
    azure_cogml_context_t* ctx
);

/**
 * Shutdown Azure CogML
 */
COGUTIL_API void azure_cogml_shutdown(azure_cogml_context_t ctx);

/**
 * Analyze image and store results in AtomSpace
 */
COGUTIL_API cog_result_t azure_cogml_analyze_image(
    azure_cogml_context_t ctx,
    const uint8_t* image_data,
    size_t image_size,
    const char* features,  /* comma-separated: "tags,description,objects,faces" */
    atom_handle_t* result_atom
);

/**
 * Speech to text with AtomSpace integration
 */
COGUTIL_API cog_result_t azure_cogml_speech_to_text(
    azure_cogml_context_t ctx,
    const uint8_t* audio_data,
    size_t audio_size,
    const char* language,
    char** text,
    atom_handle_t* result_atom
);

/**
 * Text to speech
 */
COGUTIL_API cog_result_t azure_cogml_text_to_speech(
    azure_cogml_context_t ctx,
    const char* text,
    const char* voice,
    uint8_t** audio_data,
    size_t* audio_size
);

/**
 * Analyze text sentiment and entities
 */
COGUTIL_API cog_result_t azure_cogml_analyze_text(
    azure_cogml_context_t ctx,
    const char* text,
    atom_handle_t* result_atom
);

/**
 * Generate embeddings and store in AtomSpace
 */
COGUTIL_API cog_result_t azure_cogml_embed(
    azure_cogml_context_t ctx,
    const char* text,
    float** embedding,
    size_t* embedding_dim,
    atom_handle_t* result_atom
);

/**
 * Chat completion with AtomSpace context
 */
COGUTIL_API cog_result_t azure_cogml_chat(
    azure_cogml_context_t ctx,
    const char* system_prompt,
    const char* user_message,
    atom_handle_t* context_atoms,
    size_t context_count,
    char** response,
    atom_handle_t* result_atom
);

/**
 * Generate image with DALL-E
 */
COGUTIL_API cog_result_t azure_cogml_generate_image(
    azure_cogml_context_t ctx,
    const char* prompt,
    uint32_t width,
    uint32_t height,
    uint8_t** image_data,
    size_t* image_size,
    atom_handle_t* result_atom
);

/*===========================================================================
 * Planetary Neural Net - Distributed Neural Architecture
 *===========================================================================*/

/**
 * Planetary Neural Net provides distributed neural network
 * capabilities across the AGI OS network.
 * 
 * Features:
 * - Distributed tensor computation
 * - Federated learning
 * - Neural network sharding
 * - Attention-based routing
 */

/**
 * Neural node types
 */
typedef enum {
    PNN_NODE_COMPUTE,       /* Computation node */
    PNN_NODE_STORAGE,       /* Model storage node */
    PNN_NODE_GATEWAY,       /* API gateway node */
    PNN_NODE_COORDINATOR    /* Coordination node */
} pnn_node_type_t;

/**
 * Neural node
 */
typedef struct {
    uint64_t id;
    const char* address;
    uint16_t port;
    pnn_node_type_t type;
    
    /* Capabilities */
    uint32_t gpu_count;
    uint64_t gpu_memory_mb;
    uint64_t cpu_cores;
    uint64_t ram_mb;
    
    /* Status */
    bool online;
    float load;
    uint64_t active_tasks;
    
    /* AtomSpace representation */
    atom_handle_t atom;
} pnn_node_t;

/**
 * Neural model
 */
typedef struct {
    uint64_t id;
    const char* name;
    const char* architecture;
    uint64_t parameter_count;
    
    /* Sharding info */
    uint32_t shard_count;
    uint64_t* shard_nodes;
    
    /* AtomSpace representation */
    atom_handle_t atom;
} pnn_model_t;

/**
 * Planetary Neural Net context
 */
typedef struct pnn_context* pnn_context_t;

/**
 * Initialize Planetary Neural Net
 */
COGUTIL_API cog_result_t pnn_init(
    atomspace_t atomspace,
    const char* coordinator_address,
    pnn_context_t* ctx
);

/**
 * Shutdown Planetary Neural Net
 */
COGUTIL_API void pnn_shutdown(pnn_context_t ctx);

/**
 * Register compute node
 */
COGUTIL_API cog_result_t pnn_register_node(
    pnn_context_t ctx,
    const pnn_node_t* node,
    uint64_t* node_id
);

/**
 * Discover available nodes
 */
COGUTIL_API cog_result_t pnn_discover_nodes(
    pnn_context_t ctx,
    pnn_node_type_t type,
    pnn_node_t** nodes,
    size_t* count
);

/**
 * Load model (distributed across nodes)
 */
COGUTIL_API cog_result_t pnn_load_model(
    pnn_context_t ctx,
    const char* model_path,
    uint32_t shard_count,
    pnn_model_t* model
);

/**
 * Run inference
 */
COGUTIL_API cog_result_t pnn_infer(
    pnn_context_t ctx,
    pnn_model_t* model,
    const float* input,
    size_t input_size,
    float** output,
    size_t* output_size
);

/**
 * Run distributed training step
 */
COGUTIL_API cog_result_t pnn_train_step(
    pnn_context_t ctx,
    pnn_model_t* model,
    const float* input,
    size_t input_size,
    const float* target,
    size_t target_size,
    float* loss
);

/**
 * Federated learning aggregation
 */
COGUTIL_API cog_result_t pnn_federated_aggregate(
    pnn_context_t ctx,
    pnn_model_t* model,
    uint64_t* node_ids,
    size_t node_count
);

/*===========================================================================
 * Unified CogPilot API
 *===========================================================================*/

/**
 * CogPilot configuration
 */
typedef struct {
    /* HyperAgentQL */
    bool enable_hagent;
    atomspace_t atomspace;
    cogserver_t cogserver;
    
    /* Azure CogML */
    bool enable_azure;
    azure_cogml_config_t azure_config;
    
    /* Planetary Neural Net */
    bool enable_pnn;
    const char* pnn_coordinator;
    
    /* MS Graph integration */
    bool enable_msgraph;
    msgraph_context_t msgraph;
} cogpilot_config_t;

/**
 * CogPilot unified context
 */
typedef struct cogpilot_context* cogpilot_context_t;

/**
 * Initialize CogPilot
 */
COGUTIL_API cog_result_t cogpilot_init(
    const cogpilot_config_t* config,
    cogpilot_context_t* ctx
);

/**
 * Shutdown CogPilot
 */
COGUTIL_API void cogpilot_shutdown(cogpilot_context_t ctx);

/**
 * Get HyperAgentQL context
 */
COGUTIL_API hagent_context_t cogpilot_get_hagent(cogpilot_context_t ctx);

/**
 * Get Azure CogML context
 */
COGUTIL_API azure_cogml_context_t cogpilot_get_azure(cogpilot_context_t ctx);

/**
 * Get Planetary Neural Net context
 */
COGUTIL_API pnn_context_t cogpilot_get_pnn(cogpilot_context_t ctx);

/**
 * Execute unified cognitive task
 * 
 * Automatically routes to appropriate subsystem based on task type
 */
COGUTIL_API cog_result_t cogpilot_execute(
    cogpilot_context_t ctx,
    const char* task_type,
    const char* input_json,
    char** output_json,
    size_t* output_size,
    atom_handle_t* result_atom
);

#ifdef __cplusplus
}
#endif

#endif /* _COGWXP_COGPILOT_H_ */
