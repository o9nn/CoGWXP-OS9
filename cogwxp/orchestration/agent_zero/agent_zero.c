/**
 * @file agent_zero.c
 * @brief Agent Zero Implementation
 *
 * @copyright CoGWXP-OS9 Project
 */

#include "agent_zero.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>

/*===========================================================================
 * Internal Structures
 *===========================================================================*/

/* Memory entry */
typedef struct memory_node {
    agent_zero_memory_t memory;
    struct memory_node* next;
} memory_node_t;

/* Tool registry entry */
typedef struct tool_node {
    agent_zero_tool_def_t tool;
    struct tool_node* next;
} tool_node_t;

/* Message queue entry */
typedef struct message_node {
    agent_zero_message_t message;
    struct message_node* next;
} message_node_t;

/* Agent instance */
struct agent_zero_instance {
    uint64_t id;
    agent_zero_config_t config;
    agent_zero_state_t state;

    /* Memory systems */
    memory_node_t* working_memory;
    memory_node_t* long_term_memory;
    size_t working_memory_count;
    size_t long_term_memory_count;
    pthread_rwlock_t memory_lock;

    /* Tools */
    tool_node_t* tools;
    size_t tool_count;
    pthread_mutex_t tools_lock;

    /* Current plan */
    agent_zero_plan_t* current_plan;
    pthread_mutex_t plan_lock;

    /* Message inbox */
    message_node_t* inbox;
    size_t inbox_count;
    pthread_mutex_t inbox_lock;

    /* Conversation history */
    message_node_t* history;
    size_t history_count;
    pthread_mutex_t history_lock;

    /* Execution state */
    bool interrupted;
    pthread_mutex_t exec_lock;

    /* Statistics */
    agent_zero_stats_t stats;
    pthread_mutex_t stats_lock;

    /* ID generator */
    uint64_t next_memory_id;
    pthread_mutex_t id_lock;
};

/* Framework context */
struct agent_zero_context {
    agent_zero_t* agents;
    size_t agent_count;
    size_t agent_capacity;
    pthread_rwlock_t agents_lock;

    uint64_t next_agent_id;
    pthread_mutex_t id_lock;
};

/*===========================================================================
 * Utility Functions
 *===========================================================================*/

static char* strdup_safe(const char* s) {
    return s ? strdup(s) : NULL;
}

static uint64_t generate_id(pthread_mutex_t* lock, uint64_t* counter) {
    pthread_mutex_lock(lock);
    uint64_t id = (*counter)++;
    pthread_mutex_unlock(lock);
    return id;
}

static float cosine_similarity(const float* a, const float* b, size_t dim) {
    if (!a || !b || dim == 0) return 0.0f;
    float dot = 0.0f, norm_a = 0.0f, norm_b = 0.0f;
    for (size_t i = 0; i < dim; i++) {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }
    if (norm_a == 0.0f || norm_b == 0.0f) return 0.0f;
    return dot / (sqrtf(norm_a) * sqrtf(norm_b));
}

/*===========================================================================
 * Embedding Generation
 *===========================================================================*/

static cog_result_t generate_embedding(
    const char* text,
    float** embedding,
    size_t* dim
) {
    size_t output_dim = 384;
    *embedding = calloc(output_dim, sizeof(float));
    *dim = output_dim;

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

static cog_result_t call_llm(
    agent_zero_t agent,
    const char* system_prompt,
    const char* user_message,
    char** response
) {
    /* In production, would call Claude/LLM API */
    pthread_mutex_lock(&agent->stats_lock);
    agent->stats.llm_calls++;
    pthread_mutex_unlock(&agent->stats_lock);

    /* Simulate response */
    *response = strdup("[Agent thinking and responding...]");
    return COG_OK;
}

/*===========================================================================
 * Framework Lifecycle
 *===========================================================================*/

COGUTIL_API cog_result_t agent_zero_init(agent_zero_context_t* ctx) {
    if (!ctx) return COG_ERROR_INVALID_PARAM;

    agent_zero_context_t c = calloc(1, sizeof(struct agent_zero_context));
    if (!c) return COG_ERROR_MEMORY;

    c->agent_capacity = 64;
    c->agents = calloc(c->agent_capacity, sizeof(agent_zero_t));

    pthread_rwlock_init(&c->agents_lock, NULL);
    pthread_mutex_init(&c->id_lock, NULL);
    c->next_agent_id = 1;

    *ctx = c;
    return COG_OK;
}

COGUTIL_API void agent_zero_shutdown(agent_zero_context_t ctx) {
    if (!ctx) return;

    pthread_rwlock_wrlock(&ctx->agents_lock);
    for (size_t i = 0; i < ctx->agent_count; i++) {
        agent_zero_destroy(ctx->agents[i]);
    }
    free(ctx->agents);
    pthread_rwlock_unlock(&ctx->agents_lock);

    pthread_rwlock_destroy(&ctx->agents_lock);
    pthread_mutex_destroy(&ctx->id_lock);
    free(ctx);
}

/*===========================================================================
 * Agent Lifecycle
 *===========================================================================*/

COGUTIL_API cog_result_t agent_zero_create(
    agent_zero_context_t ctx,
    const agent_zero_config_t* config,
    agent_zero_t* agent
) {
    if (!ctx || !config || !agent) return COG_ERROR_INVALID_PARAM;

    agent_zero_t a = calloc(1, sizeof(struct agent_zero_instance));
    if (!a) return COG_ERROR_MEMORY;

    a->id = generate_id(&ctx->id_lock, &ctx->next_agent_id);
    a->state = AGENT_ZERO_STATE_IDLE;

    /* Copy config */
    a->config.name = strdup_safe(config->name);
    a->config.description = strdup_safe(config->description);
    a->config.role = config->role;
    a->config.system_prompt = strdup_safe(config->system_prompt);
    a->config.personality = strdup_safe(config->personality);
    a->config.default_reasoning = config->default_reasoning;
    a->config.max_reasoning_steps = config->max_reasoning_steps > 0 ?
        config->max_reasoning_steps : 10;
    a->config.confidence_threshold = config->confidence_threshold > 0 ?
        config->confidence_threshold : 0.7f;
    a->config.auto_reflect = config->auto_reflect;
    a->config.verbose = config->verbose;
    a->config.atomspace = config->atomspace;
    a->config.graphrag = config->graphrag;

    /* Copy memory config */
    memcpy(&a->config.memory_config, &config->memory_config,
        sizeof(agent_zero_memory_config_t));
    if (a->config.memory_config.working_memory_limit == 0) {
        a->config.memory_config.working_memory_limit = 100;
    }

    /* Initialize locks */
    pthread_rwlock_init(&a->memory_lock, NULL);
    pthread_mutex_init(&a->tools_lock, NULL);
    pthread_mutex_init(&a->plan_lock, NULL);
    pthread_mutex_init(&a->inbox_lock, NULL);
    pthread_mutex_init(&a->history_lock, NULL);
    pthread_mutex_init(&a->exec_lock, NULL);
    pthread_mutex_init(&a->stats_lock, NULL);
    pthread_mutex_init(&a->id_lock, NULL);

    a->next_memory_id = 1;

    /* Register default tools */
    if (config->tools) {
        for (size_t i = 0; i < config->tool_count; i++) {
            agent_zero_register_tool(a, &config->tools[i]);
        }
    }

    /* Add to context */
    pthread_rwlock_wrlock(&ctx->agents_lock);
    if (ctx->agent_count >= ctx->agent_capacity) {
        size_t new_cap = ctx->agent_capacity * 2;
        agent_zero_t* new_agents = realloc(ctx->agents, new_cap * sizeof(agent_zero_t));
        if (!new_agents) {
            pthread_rwlock_unlock(&ctx->agents_lock);
            free(a);
            return COG_ERROR_MEMORY;
        }
        ctx->agents = new_agents;
        ctx->agent_capacity = new_cap;
    }
    ctx->agents[ctx->agent_count++] = a;
    pthread_rwlock_unlock(&ctx->agents_lock);

    *agent = a;
    return COG_OK;
}

COGUTIL_API void agent_zero_destroy(agent_zero_t agent) {
    if (!agent) return;

    /* Free memories */
    pthread_rwlock_wrlock(&agent->memory_lock);
    memory_node_t* mem = agent->working_memory;
    while (mem) {
        memory_node_t* next = mem->next;
        free((void*)mem->memory.content);
        free(mem->memory.embedding);
        free((void*)mem->memory.metadata_json);
        free(mem);
        mem = next;
    }
    mem = agent->long_term_memory;
    while (mem) {
        memory_node_t* next = mem->next;
        free((void*)mem->memory.content);
        free(mem->memory.embedding);
        free((void*)mem->memory.metadata_json);
        free(mem);
        mem = next;
    }
    pthread_rwlock_unlock(&agent->memory_lock);

    /* Free tools */
    pthread_mutex_lock(&agent->tools_lock);
    tool_node_t* tool = agent->tools;
    while (tool) {
        tool_node_t* next = tool->next;
        free((void*)tool->tool.name);
        free((void*)tool->tool.description);
        free((void*)tool->tool.parameters_schema);
        free((void*)tool->tool.implementation);
        free(tool);
        tool = next;
    }
    pthread_mutex_unlock(&agent->tools_lock);

    /* Free messages */
    pthread_mutex_lock(&agent->inbox_lock);
    message_node_t* msg = agent->inbox;
    while (msg) {
        message_node_t* next = msg->next;
        free((void*)msg->message.agent_name);
        free((void*)msg->message.message);
        free(msg);
        msg = next;
    }
    pthread_mutex_unlock(&agent->inbox_lock);

    /* Free history */
    pthread_mutex_lock(&agent->history_lock);
    msg = agent->history;
    while (msg) {
        message_node_t* next = msg->next;
        free((void*)msg->message.agent_name);
        free((void*)msg->message.message);
        free(msg);
        msg = next;
    }
    pthread_mutex_unlock(&agent->history_lock);

    /* Free config */
    free((void*)agent->config.name);
    free((void*)agent->config.description);
    free((void*)agent->config.system_prompt);
    free((void*)agent->config.personality);

    /* Destroy locks */
    pthread_rwlock_destroy(&agent->memory_lock);
    pthread_mutex_destroy(&agent->tools_lock);
    pthread_mutex_destroy(&agent->plan_lock);
    pthread_mutex_destroy(&agent->inbox_lock);
    pthread_mutex_destroy(&agent->history_lock);
    pthread_mutex_destroy(&agent->exec_lock);
    pthread_mutex_destroy(&agent->stats_lock);
    pthread_mutex_destroy(&agent->id_lock);

    free(agent);
}

COGUTIL_API agent_zero_state_t agent_zero_get_state(agent_zero_t agent) {
    return agent ? agent->state : AGENT_ZERO_STATE_ERROR;
}

COGUTIL_API uint64_t agent_zero_get_id(agent_zero_t agent) {
    return agent ? agent->id : 0;
}

/*===========================================================================
 * Task Execution
 *===========================================================================*/

COGUTIL_API cog_result_t agent_zero_execute(
    agent_zero_t agent,
    const char* task,
    char** result
) {
    if (!agent || !task || !result) return COG_ERROR_INVALID_PARAM;

    pthread_mutex_lock(&agent->exec_lock);
    agent->state = AGENT_ZERO_STATE_THINKING;
    agent->interrupted = false;
    pthread_mutex_unlock(&agent->exec_lock);

    clock_t start_time = clock();

    /* Add task to memory */
    agent_zero_remember(agent, MEMORY_TYPE_CONVERSATION, task, 0.8f);

    /* Build context from memories */
    agent_zero_memory_t* memories = NULL;
    size_t memory_count = 0;
    agent_zero_recall(agent, task, 5, &memories, &memory_count);

    /* Build prompt */
    char prompt[8192];
    int offset = snprintf(prompt, sizeof(prompt),
        "You are %s, %s.\n\n"
        "Your personality: %s\n\n",
        agent->config.name ? agent->config.name : "Agent Zero",
        agent->config.description ? agent->config.description : "an AI assistant",
        agent->config.personality ? agent->config.personality : "helpful and intelligent");

    if (memory_count > 0) {
        offset += snprintf(prompt + offset, sizeof(prompt) - offset,
            "Relevant memories:\n");
        for (size_t i = 0; i < memory_count; i++) {
            offset += snprintf(prompt + offset, sizeof(prompt) - offset,
                "- %s\n", memories[i].content);
        }
        offset += snprintf(prompt + offset, sizeof(prompt) - offset, "\n");
    }

    snprintf(prompt + offset, sizeof(prompt) - offset,
        "Task: %s\n\n"
        "Think step by step and complete the task.", task);

    /* ReAct loop */
    char* response = NULL;
    uint32_t steps = 0;
    bool done = false;

    while (!done && steps < agent->config.max_reasoning_steps && !agent->interrupted) {
        agent->state = AGENT_ZERO_STATE_THINKING;

        /* Call LLM */
        char* thought = NULL;
        cog_result_t llm_result = call_llm(agent, agent->config.system_prompt,
            prompt, &thought);

        if (llm_result != COG_OK) {
            agent->state = AGENT_ZERO_STATE_ERROR;
            pthread_mutex_lock(&agent->stats_lock);
            agent->stats.tasks_failed++;
            pthread_mutex_unlock(&agent->stats_lock);
            return llm_result;
        }

        /* Check for tool calls or final answer */
        if (strstr(thought, "FINAL ANSWER:") || strstr(thought, "Done.")) {
            response = thought;
            done = true;
        } else if (strstr(thought, "ACTION:")) {
            agent->state = AGENT_ZERO_STATE_ACTING;
            /* Would parse and execute tool */
            pthread_mutex_lock(&agent->stats_lock);
            agent->stats.tools_invoked++;
            pthread_mutex_unlock(&agent->stats_lock);
            free(thought);
        } else {
            response = thought;
            done = true;
        }

        steps++;
    }

    /* Update state */
    agent->state = AGENT_ZERO_STATE_IDLE;

    /* Record in history */
    agent_zero_remember(agent, MEMORY_TYPE_CONVERSATION, response ? response : "", 0.6f);

    /* Update stats */
    clock_t end_time = clock();
    double task_time = (double)(end_time - start_time) / CLOCKS_PER_SEC * 1000.0;

    pthread_mutex_lock(&agent->stats_lock);
    agent->stats.tasks_completed++;
    agent->stats.avg_task_time_ms =
        (agent->stats.avg_task_time_ms * (agent->stats.tasks_completed - 1) + task_time) /
        agent->stats.tasks_completed;
    pthread_mutex_unlock(&agent->stats_lock);

    /* Cleanup memories */
    for (size_t i = 0; i < memory_count; i++) {
        free((void*)memories[i].content);
    }
    free(memories);

    /* Auto-reflect if enabled */
    if (agent->config.auto_reflect && agent->stats.tasks_completed % 5 == 0) {
        char* reflection = NULL;
        agent_zero_reflect(agent, &reflection);
        free(reflection);
    }

    *result = response ? response : strdup("Task completed.");
    return COG_OK;
}

COGUTIL_API cog_result_t agent_zero_interrupt(agent_zero_t agent) {
    if (!agent) return COG_ERROR_INVALID_PARAM;

    pthread_mutex_lock(&agent->exec_lock);
    agent->interrupted = true;
    pthread_mutex_unlock(&agent->exec_lock);

    return COG_OK;
}

/*===========================================================================
 * Memory Operations
 *===========================================================================*/

COGUTIL_API cog_result_t agent_zero_remember(
    agent_zero_t agent,
    agent_zero_memory_type_t type,
    const char* content,
    float importance
) {
    if (!agent || !content) return COG_ERROR_INVALID_PARAM;

    memory_node_t* node = calloc(1, sizeof(memory_node_t));
    if (!node) return COG_ERROR_MEMORY;

    node->memory.id = generate_id(&agent->id_lock, &agent->next_memory_id);
    node->memory.type = type;
    node->memory.content = strdup(content);
    node->memory.importance = importance;
    node->memory.timestamp = time(NULL);
    node->memory.last_accessed = node->memory.timestamp;
    node->memory.access_count = 0;

    /* Generate embedding */
    generate_embedding(content, &node->memory.embedding, &node->memory.embedding_dim);

    pthread_rwlock_wrlock(&agent->memory_lock);

    /* Add to working memory */
    node->next = agent->working_memory;
    agent->working_memory = node;
    agent->working_memory_count++;

    /* Consolidate if over limit */
    if (agent->working_memory_count > agent->config.memory_config.working_memory_limit) {
        /* Move oldest to long-term */
        memory_node_t* prev = NULL;
        memory_node_t* curr = agent->working_memory;
        while (curr->next) {
            prev = curr;
            curr = curr->next;
        }
        if (prev) {
            prev->next = NULL;
            curr->next = agent->long_term_memory;
            agent->long_term_memory = curr;
            agent->working_memory_count--;
            agent->long_term_memory_count++;
        }
    }

    pthread_rwlock_unlock(&agent->memory_lock);

    pthread_mutex_lock(&agent->stats_lock);
    agent->stats.memories_created++;
    pthread_mutex_unlock(&agent->stats_lock);

    return COG_OK;
}

COGUTIL_API cog_result_t agent_zero_recall(
    agent_zero_t agent,
    const char* query,
    size_t max_results,
    agent_zero_memory_t** memories,
    size_t* count
) {
    if (!agent || !query || !memories || !count) return COG_ERROR_INVALID_PARAM;

    /* Generate query embedding */
    float* query_embedding = NULL;
    size_t query_dim = 0;
    generate_embedding(query, &query_embedding, &query_dim);

    pthread_rwlock_rdlock(&agent->memory_lock);

    /* Score all memories */
    size_t total = agent->working_memory_count + agent->long_term_memory_count;
    if (total == 0) {
        pthread_rwlock_unlock(&agent->memory_lock);
        free(query_embedding);
        *memories = NULL;
        *count = 0;
        return COG_OK;
    }

    typedef struct {
        memory_node_t* node;
        float score;
    } scored_memory_t;

    scored_memory_t* scored = calloc(total, sizeof(scored_memory_t));
    size_t idx = 0;

    time_t now = time(NULL);

    /* Score working memory */
    memory_node_t* node = agent->working_memory;
    while (node) {
        float recency = expf(-0.1f * (float)(now - node->memory.timestamp) / 3600.0f);
        float relevance = cosine_similarity(query_embedding, node->memory.embedding, query_dim);
        scored[idx].node = node;
        scored[idx].score = 0.3f * recency + 0.3f * node->memory.importance + 0.4f * relevance;
        idx++;
        node = node->next;
    }

    /* Score long-term memory */
    node = agent->long_term_memory;
    while (node) {
        float recency = expf(-0.1f * (float)(now - node->memory.timestamp) / 3600.0f);
        float relevance = cosine_similarity(query_embedding, node->memory.embedding, query_dim);
        scored[idx].node = node;
        scored[idx].score = 0.2f * recency + 0.3f * node->memory.importance + 0.5f * relevance;
        idx++;
        node = node->next;
    }

    /* Sort by score */
    for (size_t i = 0; i < idx - 1; i++) {
        for (size_t j = 0; j < idx - i - 1; j++) {
            if (scored[j].score < scored[j + 1].score) {
                scored_memory_t temp = scored[j];
                scored[j] = scored[j + 1];
                scored[j + 1] = temp;
            }
        }
    }

    /* Return top results */
    size_t result_count = (max_results < idx) ? max_results : idx;
    *memories = calloc(result_count, sizeof(agent_zero_memory_t));
    *count = result_count;

    for (size_t i = 0; i < result_count; i++) {
        memcpy(&(*memories)[i], &scored[i].node->memory, sizeof(agent_zero_memory_t));
        (*memories)[i].content = strdup(scored[i].node->memory.content);
        scored[i].node->memory.access_count++;
        scored[i].node->memory.last_accessed = now;
    }

    pthread_rwlock_unlock(&agent->memory_lock);

    free(scored);
    free(query_embedding);

    return COG_OK;
}

/*===========================================================================
 * Tool Operations
 *===========================================================================*/

COGUTIL_API cog_result_t agent_zero_register_tool(
    agent_zero_t agent,
    const agent_zero_tool_def_t* tool
) {
    if (!agent || !tool || !tool->name) return COG_ERROR_INVALID_PARAM;

    tool_node_t* node = calloc(1, sizeof(tool_node_t));
    if (!node) return COG_ERROR_MEMORY;

    node->tool.name = strdup(tool->name);
    node->tool.description = strdup_safe(tool->description);
    node->tool.parameters_schema = strdup_safe(tool->parameters_schema);
    node->tool.type = tool->type;
    node->tool.implementation = strdup_safe(tool->implementation);
    node->tool.requires_confirmation = tool->requires_confirmation;
    node->tool.is_dangerous = tool->is_dangerous;
    node->tool.timeout_ms = tool->timeout_ms > 0 ? tool->timeout_ms : 30000;

    pthread_mutex_lock(&agent->tools_lock);
    node->next = agent->tools;
    agent->tools = node;
    agent->tool_count++;
    pthread_mutex_unlock(&agent->tools_lock);

    return COG_OK;
}

COGUTIL_API cog_result_t agent_zero_invoke_tool(
    agent_zero_t agent,
    const char* tool_name,
    const char* arguments_json,
    agent_zero_tool_result_t* result
) {
    if (!agent || !tool_name || !result) return COG_ERROR_INVALID_PARAM;

    memset(result, 0, sizeof(agent_zero_tool_result_t));
    result->tool_name = tool_name;
    result->arguments_json = arguments_json;
    result->start_time = time(NULL);

    pthread_mutex_lock(&agent->tools_lock);

    /* Find tool */
    tool_node_t* node = agent->tools;
    while (node && strcmp(node->tool.name, tool_name) != 0) {
        node = node->next;
    }

    pthread_mutex_unlock(&agent->tools_lock);

    if (!node) {
        result->success = false;
        result->error_message = "Tool not found";
        result->end_time = time(NULL);
        return COG_ERROR_NOT_FOUND;
    }

    /* Execute based on type */
    switch (node->tool.type) {
        case TOOL_TYPE_BUILTIN:
            /* Would call built-in function */
            result->result = strdup("[Built-in tool result]");
            result->success = true;
            break;

        case TOOL_TYPE_PYTHON:
        case TOOL_TYPE_SHELL: {
            agent_zero_execution_t exec = {0};
            agent_zero_execute_code(agent,
                node->tool.type == TOOL_TYPE_PYTHON ? SANDBOX_PYTHON : SANDBOX_SHELL,
                node->tool.implementation,
                arguments_json,
                &exec);
            result->result = exec.stdout_data;
            result->success = (exec.exit_code == 0);
            if (!result->success) {
                result->error_message = exec.stderr_data;
            }
            break;
        }

        default:
            result->result = strdup("[Tool executed]");
            result->success = true;
            break;
    }

    result->end_time = time(NULL);

    pthread_mutex_lock(&agent->stats_lock);
    agent->stats.tools_invoked++;
    pthread_mutex_unlock(&agent->stats_lock);

    return COG_OK;
}

/*===========================================================================
 * Code Execution
 *===========================================================================*/

COGUTIL_API cog_result_t agent_zero_execute_code(
    agent_zero_t agent,
    agent_zero_sandbox_type_t type,
    const char* code,
    const char* stdin_data,
    agent_zero_execution_t* result
) {
    if (!agent || !code || !result) return COG_ERROR_INVALID_PARAM;

    memset(result, 0, sizeof(agent_zero_execution_t));
    result->type = type;
    result->code = code;
    result->stdin_data = stdin_data;

    clock_t start = clock();

    /* Create pipes for communication */
    int stdout_pipe[2], stderr_pipe[2];
    if (pipe(stdout_pipe) < 0 || pipe(stderr_pipe) < 0) {
        return COG_ERROR_IO;
    }

    pid_t pid = fork();
    if (pid < 0) {
        return COG_ERROR_IO;
    }

    if (pid == 0) {
        /* Child process */
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stderr_pipe[1], STDERR_FILENO);

        const char* interpreter = NULL;
        switch (type) {
            case SANDBOX_PYTHON: interpreter = "python3"; break;
            case SANDBOX_JAVASCRIPT: interpreter = "node"; break;
            case SANDBOX_SHELL: interpreter = "sh"; break;
            case SANDBOX_SCHEME: interpreter = "guile"; break;
            default: interpreter = "sh"; break;
        }

        execlp(interpreter, interpreter, "-c", code, NULL);
        _exit(127);
    }

    /* Parent process */
    close(stdout_pipe[1]);
    close(stderr_pipe[1]);

    /* Read output */
    char buffer[4096];
    size_t stdout_size = 0, stderr_size = 0;
    result->stdout_data = calloc(1, 1);
    result->stderr_data = calloc(1, 1);

    ssize_t n;
    while ((n = read(stdout_pipe[0], buffer, sizeof(buffer))) > 0) {
        result->stdout_data = realloc(result->stdout_data, stdout_size + n + 1);
        memcpy(result->stdout_data + stdout_size, buffer, n);
        stdout_size += n;
        result->stdout_data[stdout_size] = '\0';
    }

    while ((n = read(stderr_pipe[0], buffer, sizeof(buffer))) > 0) {
        result->stderr_data = realloc(result->stderr_data, stderr_size + n + 1);
        memcpy(result->stderr_data + stderr_size, buffer, n);
        stderr_size += n;
        result->stderr_data[stderr_size] = '\0';
    }

    close(stdout_pipe[0]);
    close(stderr_pipe[0]);

    /* Wait for child */
    int status;
    waitpid(pid, &status, 0);
    result->exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

    clock_t end = clock();
    result->execution_time_ms = (uint64_t)((end - start) * 1000 / CLOCKS_PER_SEC);

    pthread_mutex_lock(&agent->stats_lock);
    agent->stats.code_executions++;
    pthread_mutex_unlock(&agent->stats_lock);

    return COG_OK;
}

/*===========================================================================
 * Reflection & Learning
 *===========================================================================*/

COGUTIL_API cog_result_t agent_zero_reflect(
    agent_zero_t agent,
    char** reflection
) {
    if (!agent || !reflection) return COG_ERROR_INVALID_PARAM;

    agent->state = AGENT_ZERO_STATE_REFLECTING;

    /* Gather recent experiences */
    agent_zero_memory_t* memories = NULL;
    size_t count = 0;
    agent_zero_recall(agent, "recent experiences actions thoughts", 10, &memories, &count);

    /* Build reflection prompt */
    char prompt[4096];
    int offset = snprintf(prompt, sizeof(prompt),
        "Reflect on your recent experiences and generate insights.\n\n"
        "Recent memories:\n");

    for (size_t i = 0; i < count; i++) {
        offset += snprintf(prompt + offset, sizeof(prompt) - offset,
            "- %s\n", memories[i].content);
    }

    snprintf(prompt + offset, sizeof(prompt) - offset,
        "\nWhat patterns do you notice? What have you learned? "
        "What would you do differently?");

    /* Generate reflection */
    char* response = NULL;
    call_llm(agent, agent->config.system_prompt, prompt, &response);

    /* Store reflection */
    if (response) {
        agent_zero_remember(agent, MEMORY_TYPE_REFLECTION, response, 0.9f);
    }

    /* Cleanup */
    for (size_t i = 0; i < count; i++) {
        free((void*)memories[i].content);
    }
    free(memories);

    agent->state = AGENT_ZERO_STATE_IDLE;

    pthread_mutex_lock(&agent->stats_lock);
    agent->stats.reflections++;
    pthread_mutex_unlock(&agent->stats_lock);

    *reflection = response ? response : strdup("Reflection complete.");
    return COG_OK;
}

/*===========================================================================
 * Multi-Agent Communication
 *===========================================================================*/

COGUTIL_API cog_result_t agent_zero_send_message(
    agent_zero_t sender,
    agent_zero_t recipient,
    const char* message
) {
    if (!sender || !recipient || !message) return COG_ERROR_INVALID_PARAM;

    message_node_t* node = calloc(1, sizeof(message_node_t));
    if (!node) return COG_ERROR_MEMORY;

    node->message.agent_id = sender->id;
    node->message.agent_name = strdup_safe(sender->config.name);
    node->message.role = sender->config.role;
    node->message.message = strdup(message);
    node->message.timestamp = time(NULL);

    pthread_mutex_lock(&recipient->inbox_lock);
    node->next = recipient->inbox;
    recipient->inbox = node;
    recipient->inbox_count++;
    pthread_mutex_unlock(&recipient->inbox_lock);

    pthread_mutex_lock(&sender->stats_lock);
    sender->stats.messages_sent++;
    pthread_mutex_unlock(&sender->stats_lock);

    pthread_mutex_lock(&recipient->stats_lock);
    recipient->stats.messages_received++;
    pthread_mutex_unlock(&recipient->stats_lock);

    return COG_OK;
}

/*===========================================================================
 * Statistics
 *===========================================================================*/

COGUTIL_API cog_result_t agent_zero_get_stats(
    agent_zero_t agent,
    agent_zero_stats_t* stats
) {
    if (!agent || !stats) return COG_ERROR_INVALID_PARAM;

    pthread_mutex_lock(&agent->stats_lock);
    memcpy(stats, &agent->stats, sizeof(agent_zero_stats_t));
    pthread_mutex_unlock(&agent->stats_lock);

    return COG_OK;
}

COGUTIL_API void agent_zero_reset_stats(agent_zero_t agent) {
    if (!agent) return;

    pthread_mutex_lock(&agent->stats_lock);
    memset(&agent->stats, 0, sizeof(agent_zero_stats_t));
    pthread_mutex_unlock(&agent->stats_lock);
}
