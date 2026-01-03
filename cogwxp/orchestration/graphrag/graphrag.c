/**
 * @file graphrag.c
 * @brief GraphRAG Implementation
 *
 * @copyright CoGWXP-OS9 Project
 */

#include "graphrag.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <math.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>

/*===========================================================================
 * Internal Structures
 *===========================================================================*/

/* Entity node in graph */
typedef struct entity_node {
    graphrag_entity_t entity;
    struct entity_node* next;
} entity_node_t;

/* Relationship node */
typedef struct rel_node {
    graphrag_relationship_t rel;
    struct rel_node* next;
} rel_node_t;

/* Chunk node */
typedef struct chunk_node {
    graphrag_chunk_t chunk;
    struct chunk_node* next;
} chunk_node_t;

/* Community node */
typedef struct community_node {
    graphrag_community_t community;
    struct community_node* next;
} community_node_t;

/* GraphRAG context */
struct graphrag_context {
    graphrag_config_t config;

    /* Entities */
    entity_node_t* entities_head;
    size_t entity_count;
    pthread_rwlock_t entities_lock;

    /* Relationships */
    rel_node_t* rels_head;
    size_t rel_count;
    pthread_rwlock_t rels_lock;

    /* Chunks */
    chunk_node_t* chunks_head;
    size_t chunk_count;
    pthread_rwlock_t chunks_lock;

    /* Communities */
    community_node_t* communities_head;
    size_t community_count;
    pthread_rwlock_t communities_lock;

    /* Statistics */
    graphrag_stats_t stats;
    pthread_mutex_t stats_lock;

    /* ID generators */
    uint64_t next_entity_id;
    uint64_t next_rel_id;
    uint64_t next_chunk_id;
    uint64_t next_community_id;
    pthread_mutex_t id_lock;
};

/*===========================================================================
 * Utility Functions
 *===========================================================================*/

static char* strdup_safe(const char* s) {
    return s ? strdup(s) : NULL;
}

static uint64_t generate_id(graphrag_context_t ctx, uint64_t* counter) {
    pthread_mutex_lock(&ctx->id_lock);
    uint64_t id = (*counter)++;
    pthread_mutex_unlock(&ctx->id_lock);
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
 * LLM Interface
 *===========================================================================*/

static cog_result_t call_llm(
    graphrag_context_t ctx,
    const char* prompt,
    char** response
) {
    /* In production, would call actual LLM API */
    pthread_mutex_lock(&ctx->stats_lock);
    ctx->stats.llm_calls++;
    pthread_mutex_unlock(&ctx->stats_lock);

    *response = strdup("[LLM Response - Entity extraction result]");
    return COG_OK;
}

static cog_result_t generate_embedding(
    graphrag_context_t ctx,
    const char* text,
    float** embedding,
    size_t* dim
) {
    size_t output_dim = ctx->config.embedding_dim > 0 ? ctx->config.embedding_dim : 384;

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
 * Entity Extraction
 *===========================================================================*/

static cog_result_t extract_entities_from_chunk(
    graphrag_context_t ctx,
    const char* content,
    uint64_t chunk_id,
    size_t* entity_count,
    size_t* rel_count
) {
    /* Build extraction prompt */
    char prompt[4096];
    snprintf(prompt, sizeof(prompt),
        "Extract entities and relationships from the following text.\n"
        "For each entity, identify: name, type (person/org/location/concept), description.\n"
        "For each relationship, identify: source, target, type, description.\n\n"
        "Text:\n%s\n\n"
        "Output as JSON.",
        content);

    /* Call LLM for extraction */
    char* response = NULL;
    cog_result_t result = call_llm(ctx, prompt, &response);
    if (result != COG_OK) return result;

    /* Parse response and create entities */
    /* Simplified - would parse JSON response */

    /* Create sample entity */
    entity_node_t* entity = calloc(1, sizeof(entity_node_t));
    entity->entity.id = generate_id(ctx, &ctx->next_entity_id);
    entity->entity.type = GRAPHRAG_ENTITY_CONCEPT;
    entity->entity.name = strdup("Extracted Entity");
    entity->entity.description = strdup("Entity extracted from text");

    /* Generate embedding */
    generate_embedding(ctx, entity->entity.name,
        (float**)&entity->entity.embedding,
        (size_t*)&entity->entity.embedding_dim);

    /* Add to graph */
    pthread_rwlock_wrlock(&ctx->entities_lock);
    entity->next = ctx->entities_head;
    ctx->entities_head = entity;
    ctx->entity_count++;
    pthread_rwlock_unlock(&ctx->entities_lock);

    *entity_count = 1;
    *rel_count = 0;

    free(response);

    pthread_mutex_lock(&ctx->stats_lock);
    ctx->stats.entities_extracted++;
    pthread_mutex_unlock(&ctx->stats_lock);

    return COG_OK;
}

/*===========================================================================
 * Lifecycle
 *===========================================================================*/

COGUTIL_API cog_result_t graphrag_init(
    const graphrag_config_t* config,
    graphrag_context_t* ctx
) {
    if (!config || !ctx) return COG_ERROR_INVALID_PARAM;

    graphrag_context_t c = calloc(1, sizeof(struct graphrag_context));
    if (!c) return COG_ERROR_MEMORY;

    /* Copy config */
    memcpy(&c->config, config, sizeof(graphrag_config_t));
    c->config.llm_endpoint = strdup_safe(config->llm_endpoint);
    c->config.llm_api_key = strdup_safe(config->llm_api_key);
    c->config.llm_model = strdup_safe(config->llm_model);
    c->config.embedding_endpoint = strdup_safe(config->embedding_endpoint);
    c->config.embedding_model = strdup_safe(config->embedding_model);

    /* Initialize locks */
    pthread_rwlock_init(&c->entities_lock, NULL);
    pthread_rwlock_init(&c->rels_lock, NULL);
    pthread_rwlock_init(&c->chunks_lock, NULL);
    pthread_rwlock_init(&c->communities_lock, NULL);
    pthread_mutex_init(&c->stats_lock, NULL);
    pthread_mutex_init(&c->id_lock, NULL);

    /* Initialize IDs */
    c->next_entity_id = 1;
    c->next_rel_id = 1;
    c->next_chunk_id = 1;
    c->next_community_id = 1;

    *ctx = c;
    return COG_OK;
}

COGUTIL_API void graphrag_shutdown(graphrag_context_t ctx) {
    if (!ctx) return;

    /* Free entities */
    entity_node_t* entity = ctx->entities_head;
    while (entity) {
        entity_node_t* next = entity->next;
        free((void*)entity->entity.name);
        free((void*)entity->entity.description);
        free((void*)entity->entity.embedding);
        free((void*)entity->entity.metadata_json);
        free(entity);
        entity = next;
    }

    /* Free relationships */
    rel_node_t* rel = ctx->rels_head;
    while (rel) {
        rel_node_t* next = rel->next;
        free((void*)rel->rel.description);
        free(rel);
        rel = next;
    }

    /* Free chunks */
    chunk_node_t* chunk = ctx->chunks_head;
    while (chunk) {
        chunk_node_t* next = chunk->next;
        free((void*)chunk->chunk.content);
        free((void*)chunk->chunk.source_file);
        free((void*)chunk->chunk.embedding);
        free((void*)chunk->chunk.entity_ids);
        free(chunk);
        chunk = next;
    }

    /* Free communities */
    community_node_t* comm = ctx->communities_head;
    while (comm) {
        community_node_t* next = comm->next;
        free((void*)comm->community.name);
        free((void*)comm->community.summary);
        free((void*)comm->community.entity_ids);
        free(comm);
        comm = next;
    }

    /* Free config */
    free((void*)ctx->config.llm_endpoint);
    free((void*)ctx->config.llm_api_key);
    free((void*)ctx->config.llm_model);
    free((void*)ctx->config.embedding_endpoint);
    free((void*)ctx->config.embedding_model);

    /* Destroy locks */
    pthread_rwlock_destroy(&ctx->entities_lock);
    pthread_rwlock_destroy(&ctx->rels_lock);
    pthread_rwlock_destroy(&ctx->chunks_lock);
    pthread_rwlock_destroy(&ctx->communities_lock);
    pthread_mutex_destroy(&ctx->stats_lock);
    pthread_mutex_destroy(&ctx->id_lock);

    free(ctx);
}

/*===========================================================================
 * Document Processing
 *===========================================================================*/

COGUTIL_API cog_result_t graphrag_ingest_document(
    graphrag_context_t ctx,
    const char* content,
    const char* source_name,
    size_t* entities_extracted,
    size_t* relationships_created
) {
    if (!ctx || !content || !entities_extracted || !relationships_created) {
        return COG_ERROR_INVALID_PARAM;
    }

    size_t chunk_size = ctx->config.chunk_size > 0 ? ctx->config.chunk_size : 1000;
    size_t overlap = ctx->config.chunk_overlap > 0 ? ctx->config.chunk_overlap : 200;

    size_t content_len = strlen(content);
    size_t total_entities = 0;
    size_t total_rels = 0;

    /* Chunk the document */
    size_t offset = 0;
    while (offset < content_len) {
        size_t chunk_end = offset + chunk_size;
        if (chunk_end > content_len) chunk_end = content_len;

        /* Create chunk */
        chunk_node_t* chunk = calloc(1, sizeof(chunk_node_t));
        chunk->chunk.id = generate_id(ctx, &ctx->next_chunk_id);
        chunk->chunk.source_file = strdup_safe(source_name);
        chunk->chunk.start_offset = (uint32_t)offset;
        chunk->chunk.end_offset = (uint32_t)chunk_end;

        /* Extract chunk content */
        size_t chunk_len = chunk_end - offset;
        char* chunk_content = malloc(chunk_len + 1);
        strncpy(chunk_content, content + offset, chunk_len);
        chunk_content[chunk_len] = '\0';
        chunk->chunk.content = chunk_content;

        /* Generate embedding */
        generate_embedding(ctx, chunk_content,
            (float**)&chunk->chunk.embedding,
            (size_t*)&chunk->chunk.embedding_dim);

        /* Add chunk to list */
        pthread_rwlock_wrlock(&ctx->chunks_lock);
        chunk->next = ctx->chunks_head;
        ctx->chunks_head = chunk;
        ctx->chunk_count++;
        pthread_rwlock_unlock(&ctx->chunks_lock);

        /* Extract entities from chunk */
        size_t chunk_entities = 0, chunk_rels = 0;
        extract_entities_from_chunk(ctx, chunk_content, chunk->chunk.id,
            &chunk_entities, &chunk_rels);

        total_entities += chunk_entities;
        total_rels += chunk_rels;

        /* Advance with overlap */
        offset = chunk_end - overlap;
        if (offset >= chunk_end) break;
    }

    *entities_extracted = total_entities;
    *relationships_created = total_rels;

    pthread_mutex_lock(&ctx->stats_lock);
    ctx->stats.chunks_processed++;
    pthread_mutex_unlock(&ctx->stats_lock);

    return COG_OK;
}

COGUTIL_API cog_result_t graphrag_ingest_file(
    graphrag_context_t ctx,
    const char* file_path,
    size_t* entities_extracted,
    size_t* relationships_created
) {
    if (!ctx || !file_path) return COG_ERROR_INVALID_PARAM;

    FILE* f = fopen(file_path, "r");
    if (!f) return COG_ERROR_IO;

    /* Get file size */
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    /* Read content */
    char* content = malloc(size + 1);
    size_t read = fread(content, 1, size, f);
    content[read] = '\0';
    fclose(f);

    /* Process */
    cog_result_t result = graphrag_ingest_document(ctx, content, file_path,
        entities_extracted, relationships_created);

    free(content);
    return result;
}

COGUTIL_API cog_result_t graphrag_ingest_directory(
    graphrag_context_t ctx,
    const char* dir_path,
    const char* pattern,
    size_t* files_processed,
    size_t* entities_extracted
) {
    if (!ctx || !dir_path) return COG_ERROR_INVALID_PARAM;

    DIR* dir = opendir(dir_path);
    if (!dir) return COG_ERROR_IO;

    *files_processed = 0;
    *entities_extracted = 0;

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;

        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) != 0) continue;

        if (S_ISREG(st.st_mode)) {
            /* Check pattern if specified */
            if (pattern && !strstr(entry->d_name, pattern)) continue;

            size_t entities = 0, rels = 0;
            if (graphrag_ingest_file(ctx, full_path, &entities, &rels) == COG_OK) {
                (*files_processed)++;
                *entities_extracted += entities;
            }
        } else if (S_ISDIR(st.st_mode)) {
            /* Recurse into subdirectory */
            size_t sub_files = 0, sub_entities = 0;
            graphrag_ingest_directory(ctx, full_path, pattern, &sub_files, &sub_entities);
            *files_processed += sub_files;
            *entities_extracted += sub_entities;
        }
    }

    closedir(dir);
    return COG_OK;
}

/*===========================================================================
 * Entity Operations
 *===========================================================================*/

COGUTIL_API cog_result_t graphrag_get_entity(
    graphrag_context_t ctx,
    uint64_t entity_id,
    graphrag_entity_t* entity
) {
    if (!ctx || !entity) return COG_ERROR_INVALID_PARAM;

    pthread_rwlock_rdlock(&ctx->entities_lock);

    entity_node_t* node = ctx->entities_head;
    while (node) {
        if (node->entity.id == entity_id) {
            memcpy(entity, &node->entity, sizeof(graphrag_entity_t));
            pthread_rwlock_unlock(&ctx->entities_lock);
            return COG_OK;
        }
        node = node->next;
    }

    pthread_rwlock_unlock(&ctx->entities_lock);
    return COG_ERROR_NOT_FOUND;
}

COGUTIL_API cog_result_t graphrag_search_entities(
    graphrag_context_t ctx,
    const char* query,
    size_t max_results,
    graphrag_entity_t** entities,
    size_t* count
) {
    if (!ctx || !query || !entities || !count) return COG_ERROR_INVALID_PARAM;

    /* Generate query embedding */
    float* query_embedding = NULL;
    size_t query_dim = 0;
    generate_embedding(ctx, query, &query_embedding, &query_dim);

    pthread_rwlock_rdlock(&ctx->entities_lock);

    /* Score all entities */
    typedef struct {
        entity_node_t* node;
        float score;
    } scored_entity_t;

    scored_entity_t* scored = calloc(ctx->entity_count, sizeof(scored_entity_t));
    size_t scored_count = 0;

    entity_node_t* node = ctx->entities_head;
    while (node) {
        scored[scored_count].node = node;
        scored[scored_count].score = cosine_similarity(
            query_embedding, node->entity.embedding, query_dim);
        scored_count++;
        node = node->next;
    }

    /* Sort by score */
    for (size_t i = 0; i < scored_count - 1; i++) {
        for (size_t j = 0; j < scored_count - i - 1; j++) {
            if (scored[j].score < scored[j + 1].score) {
                scored_entity_t temp = scored[j];
                scored[j] = scored[j + 1];
                scored[j + 1] = temp;
            }
        }
    }

    /* Return top results */
    size_t result_count = (max_results < scored_count) ? max_results : scored_count;
    *entities = calloc(result_count, sizeof(graphrag_entity_t));
    *count = result_count;

    for (size_t i = 0; i < result_count; i++) {
        memcpy(&(*entities)[i], &scored[i].node->entity, sizeof(graphrag_entity_t));
        (*entities)[i].name = strdup(scored[i].node->entity.name);
        (*entities)[i].description = strdup_safe(scored[i].node->entity.description);
    }

    pthread_rwlock_unlock(&ctx->entities_lock);

    free(scored);
    free(query_embedding);

    return COG_OK;
}

COGUTIL_API cog_result_t graphrag_get_relationships(
    graphrag_context_t ctx,
    uint64_t entity_id,
    bool incoming,
    bool outgoing,
    graphrag_relationship_t** relationships,
    size_t* count
) {
    if (!ctx || !relationships || !count) return COG_ERROR_INVALID_PARAM;

    pthread_rwlock_rdlock(&ctx->rels_lock);

    /* Count matching relationships */
    size_t match_count = 0;
    rel_node_t* node = ctx->rels_head;
    while (node) {
        bool matches = false;
        if (outgoing && node->rel.source_id == entity_id) matches = true;
        if (incoming && node->rel.target_id == entity_id) matches = true;
        if (matches) match_count++;
        node = node->next;
    }

    /* Collect results */
    *relationships = calloc(match_count, sizeof(graphrag_relationship_t));
    *count = match_count;

    size_t idx = 0;
    node = ctx->rels_head;
    while (node && idx < match_count) {
        bool matches = false;
        if (outgoing && node->rel.source_id == entity_id) matches = true;
        if (incoming && node->rel.target_id == entity_id) matches = true;
        if (matches) {
            memcpy(&(*relationships)[idx], &node->rel, sizeof(graphrag_relationship_t));
            idx++;
        }
        node = node->next;
    }

    pthread_rwlock_unlock(&ctx->rels_lock);
    return COG_OK;
}

/*===========================================================================
 * Community Detection
 *===========================================================================*/

COGUTIL_API cog_result_t graphrag_detect_communities(
    graphrag_context_t ctx,
    size_t* community_count
) {
    if (!ctx || !community_count) return COG_ERROR_INVALID_PARAM;

    /* Simple community detection - assign all entities to one community */
    /* In production, would use Leiden algorithm */

    pthread_rwlock_rdlock(&ctx->entities_lock);

    community_node_t* comm = calloc(1, sizeof(community_node_t));
    comm->community.id = generate_id(ctx, &ctx->next_community_id);
    comm->community.name = strdup("Default Community");
    comm->community.level = 0;

    /* Collect entity IDs */
    comm->community.entity_count = ctx->entity_count;
    comm->community.entity_ids = calloc(ctx->entity_count, sizeof(uint64_t));

    entity_node_t* node = ctx->entities_head;
    size_t idx = 0;
    while (node) {
        comm->community.entity_ids[idx++] = node->entity.id;
        node = node->next;
    }

    pthread_rwlock_unlock(&ctx->entities_lock);

    /* Add community */
    pthread_rwlock_wrlock(&ctx->communities_lock);
    comm->next = ctx->communities_head;
    ctx->communities_head = comm;
    ctx->community_count++;
    pthread_rwlock_unlock(&ctx->communities_lock);

    *community_count = 1;

    pthread_mutex_lock(&ctx->stats_lock);
    ctx->stats.communities_detected++;
    pthread_mutex_unlock(&ctx->stats_lock);

    return COG_OK;
}

COGUTIL_API cog_result_t graphrag_summarize_communities(
    graphrag_context_t ctx,
    uint32_t level
) {
    if (!ctx) return COG_ERROR_INVALID_PARAM;

    pthread_rwlock_wrlock(&ctx->communities_lock);

    community_node_t* comm = ctx->communities_head;
    while (comm) {
        if (comm->community.level == level && !comm->community.summary) {
            /* Generate summary using LLM */
            char prompt[2048];
            snprintf(prompt, sizeof(prompt),
                "Summarize the following community of %zu entities in 2-3 sentences.",
                comm->community.entity_count);

            char* summary = NULL;
            if (call_llm(ctx, prompt, &summary) == COG_OK) {
                comm->community.summary = summary;
            }
        }
        comm = comm->next;
    }

    pthread_rwlock_unlock(&ctx->communities_lock);
    return COG_OK;
}

/*===========================================================================
 * Query Interface
 *===========================================================================*/

COGUTIL_API cog_result_t graphrag_local_search(
    graphrag_context_t ctx,
    const char* query,
    size_t max_results,
    graphrag_search_result_t** results,
    size_t* count
) {
    if (!ctx || !query || !results || !count) return COG_ERROR_INVALID_PARAM;

    /* Search for relevant entities */
    graphrag_entity_t* entities = NULL;
    size_t entity_count = 0;
    graphrag_search_entities(ctx, query, max_results, &entities, &entity_count);

    /* Convert to search results */
    *results = calloc(entity_count, sizeof(graphrag_search_result_t));
    *count = entity_count;

    for (size_t i = 0; i < entity_count; i++) {
        (*results)[i].entity_id = entities[i].id;
        (*results)[i].name = strdup(entities[i].name);
        (*results)[i].context = strdup_safe(entities[i].description);
        (*results)[i].relevance_score = 1.0f - (float)i / (float)entity_count;
    }

    /* Cleanup */
    for (size_t i = 0; i < entity_count; i++) {
        free((void*)entities[i].name);
        free((void*)entities[i].description);
    }
    free(entities);

    return COG_OK;
}

COGUTIL_API cog_result_t graphrag_global_search(
    graphrag_context_t ctx,
    const char* query,
    graphrag_response_t* response
) {
    if (!ctx || !query || !response) return COG_ERROR_INVALID_PARAM;

    memset(response, 0, sizeof(graphrag_response_t));

    /* Get community summaries for context */
    pthread_rwlock_rdlock(&ctx->communities_lock);

    char context[4096] = "";
    int offset = 0;

    community_node_t* comm = ctx->communities_head;
    while (comm && offset < 3800) {
        if (comm->community.summary) {
            offset += snprintf(context + offset, sizeof(context) - offset,
                "Community '%s': %s\n",
                comm->community.name, comm->community.summary);
        }
        comm = comm->next;
    }

    pthread_rwlock_unlock(&ctx->communities_lock);

    response->community_context = strdup(context);

    /* Generate answer using LLM with community context */
    char prompt[8192];
    snprintf(prompt, sizeof(prompt),
        "Based on the following community summaries, answer the question.\n\n"
        "Community Context:\n%s\n\n"
        "Question: %s\n\n"
        "Answer:",
        context, query);

    char* answer = NULL;
    cog_result_t result = call_llm(ctx, prompt, &answer);

    if (result == COG_OK) {
        response->answer = answer;
        response->confidence = 0.8f;
    }

    pthread_mutex_lock(&ctx->stats_lock);
    ctx->stats.queries_processed++;
    pthread_mutex_unlock(&ctx->stats_lock);

    return result;
}

COGUTIL_API cog_result_t graphrag_query(
    graphrag_context_t ctx,
    const char* query,
    bool use_local,
    bool use_global,
    graphrag_response_t* response
) {
    if (!ctx || !query || !response) return COG_ERROR_INVALID_PARAM;

    memset(response, 0, sizeof(graphrag_response_t));

    graphrag_search_result_t* local_results = NULL;
    size_t local_count = 0;

    if (use_local) {
        graphrag_local_search(ctx, query, 5, &local_results, &local_count);
    }

    graphrag_response_t global_response = {0};
    if (use_global) {
        graphrag_global_search(ctx, query, &global_response);
    }

    /* Combine results */
    if (local_count > 0) {
        response->sources = local_results;
        response->source_count = local_count;
    }

    if (global_response.answer) {
        response->answer = global_response.answer;
        response->community_context = global_response.community_context;
        response->confidence = global_response.confidence;
    } else if (local_count > 0) {
        /* Synthesize answer from local results */
        char answer[2048];
        snprintf(answer, sizeof(answer), "Based on %zu relevant entities found.", local_count);
        response->answer = strdup(answer);
        response->confidence = 0.6f;
    }

    return COG_OK;
}

COGUTIL_API void graphrag_response_free(graphrag_response_t* response) {
    if (!response) return;

    free((void*)response->answer);
    free((void*)response->community_context);

    if (response->sources) {
        for (size_t i = 0; i < response->source_count; i++) {
            free((void*)response->sources[i].name);
            free((void*)response->sources[i].context);
            free(response->sources[i].related_chunk_ids);
        }
        free(response->sources);
    }

    memset(response, 0, sizeof(graphrag_response_t));
}

/*===========================================================================
 * AtomSpace Integration
 *===========================================================================*/

COGUTIL_API cog_result_t graphrag_sync_to_atomspace(
    graphrag_context_t ctx,
    size_t* atoms_created
) {
    if (!ctx || !atoms_created) return COG_ERROR_INVALID_PARAM;
    if (!ctx->config.atomspace) return COG_ERROR_INVALID_STATE;

    *atoms_created = 0;

    pthread_rwlock_rdlock(&ctx->entities_lock);

    /* Create atoms for each entity */
    entity_node_t* node = ctx->entities_head;
    while (node) {
        /* In production, would use AtomSpace API */
        node->entity.atom_handle = node->entity.id + 1000000;
        (*atoms_created)++;
        node = node->next;
    }

    pthread_rwlock_unlock(&ctx->entities_lock);

    return COG_OK;
}

/*===========================================================================
 * Statistics
 *===========================================================================*/

COGUTIL_API cog_result_t graphrag_get_stats(
    graphrag_context_t ctx,
    graphrag_stats_t* stats
) {
    if (!ctx || !stats) return COG_ERROR_INVALID_PARAM;

    pthread_mutex_lock(&ctx->stats_lock);
    memcpy(stats, &ctx->stats, sizeof(graphrag_stats_t));
    pthread_mutex_unlock(&ctx->stats_lock);

    return COG_OK;
}

COGUTIL_API void graphrag_reset_stats(graphrag_context_t ctx) {
    if (!ctx) return;

    pthread_mutex_lock(&ctx->stats_lock);
    memset(&ctx->stats, 0, sizeof(graphrag_stats_t));
    pthread_mutex_unlock(&ctx->stats_lock);
}
