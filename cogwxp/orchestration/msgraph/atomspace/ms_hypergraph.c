/**
 * @file ms_hypergraph.c
 * @brief MSHyperGraph Implementation - MS Graph + AtomSpace Integration
 * 
 * @copyright CoGWXP-OS9 Project
 */

#include "ms_hypergraph.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <curl/curl.h>
#include <time.h>

/*===========================================================================
 * Internal Structures
 *===========================================================================*/

struct msgraph_context {
    msgraph_config_t config;
    atomspace_t atomspace;
    
    /* Authentication */
    char* access_token;
    time_t token_expiry;
    pthread_mutex_t token_mutex;
    
    /* HTTP client */
    CURL* curl;
    pthread_mutex_t curl_mutex;
    
    /* Entity cache */
    struct {
        msgraph_entity_t* entities;
        size_t count;
        size_t capacity;
        pthread_rwlock_t lock;
    } cache;
    
    /* Sync state */
    char* delta_link;
    pthread_t sync_thread;
    bool sync_running;
    
    /* Statistics */
    msgraph_stats_t stats;
    pthread_mutex_t stats_mutex;
};

/*===========================================================================
 * Memory Management Helpers
 *===========================================================================*/

static char* strdup_safe(const char* s) {
    return s ? strdup(s) : NULL;
}

static void free_safe(void* p) {
    if (p) free(p);
}

/*===========================================================================
 * HTTP Response Buffer
 *===========================================================================*/

typedef struct {
    char* data;
    size_t size;
} http_response_t;

static size_t http_write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    http_response_t* resp = (http_response_t*)userp;
    
    char* ptr = realloc(resp->data, resp->size + realsize + 1);
    if (!ptr) return 0;
    
    resp->data = ptr;
    memcpy(&(resp->data[resp->size]), contents, realsize);
    resp->size += realsize;
    resp->data[resp->size] = 0;
    
    return realsize;
}

/*===========================================================================
 * Authentication
 *===========================================================================*/

static cog_result_t refresh_access_token(msgraph_context_t ctx) {
    pthread_mutex_lock(&ctx->token_mutex);
    
    /* Check if token is still valid */
    if (ctx->access_token && time(NULL) < ctx->token_expiry - 60) {
        pthread_mutex_unlock(&ctx->token_mutex);
        return COG_OK;
    }
    
    /* Build token request */
    char post_data[2048];
    snprintf(post_data, sizeof(post_data),
        "client_id=%s&client_secret=%s&scope=https://graph.microsoft.com/.default&grant_type=client_credentials",
        ctx->config.client_id,
        ctx->config.client_secret);
    
    char url[512];
    snprintf(url, sizeof(url),
        "https://login.microsoftonline.com/%s/oauth2/v2.0/token",
        ctx->config.tenant_id);
    
    pthread_mutex_lock(&ctx->curl_mutex);
    
    http_response_t resp = {0};
    curl_easy_setopt(ctx->curl, CURLOPT_URL, url);
    curl_easy_setopt(ctx->curl, CURLOPT_POSTFIELDS, post_data);
    curl_easy_setopt(ctx->curl, CURLOPT_WRITEFUNCTION, http_write_callback);
    curl_easy_setopt(ctx->curl, CURLOPT_WRITEDATA, &resp);
    
    CURLcode res = curl_easy_perform(ctx->curl);
    
    pthread_mutex_unlock(&ctx->curl_mutex);
    
    if (res != CURLE_OK) {
        free_safe(resp.data);
        pthread_mutex_unlock(&ctx->token_mutex);
        return COG_ERROR_NETWORK;
    }
    
    /* Parse JSON response (simplified - would use proper JSON parser) */
    char* token_start = strstr(resp.data, "\"access_token\":\"");
    if (token_start) {
        token_start += 16;
        char* token_end = strchr(token_start, '"');
        if (token_end) {
            free_safe(ctx->access_token);
            size_t token_len = token_end - token_start;
            ctx->access_token = malloc(token_len + 1);
            strncpy(ctx->access_token, token_start, token_len);
            ctx->access_token[token_len] = 0;
            ctx->token_expiry = time(NULL) + 3600; /* Default 1 hour */
        }
    }
    
    free_safe(resp.data);
    pthread_mutex_unlock(&ctx->token_mutex);
    
    return ctx->access_token ? COG_OK : COG_ERROR_AUTH;
}

/*===========================================================================
 * Graph API Requests
 *===========================================================================*/

static cog_result_t graph_api_request(
    msgraph_context_t ctx,
    const char* method,
    const char* endpoint,
    const char* body,
    char** response,
    size_t* response_size
) {
    cog_result_t result = refresh_access_token(ctx);
    if (result != COG_OK) return result;
    
    char url[1024];
    snprintf(url, sizeof(url), "https://graph.microsoft.com/v1.0%s", endpoint);
    
    pthread_mutex_lock(&ctx->curl_mutex);
    
    /* Reset curl handle */
    curl_easy_reset(ctx->curl);
    
    /* Set URL */
    curl_easy_setopt(ctx->curl, CURLOPT_URL, url);
    
    /* Set headers */
    struct curl_slist* headers = NULL;
    char auth_header[2048];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", ctx->access_token);
    headers = curl_slist_append(headers, auth_header);
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(ctx->curl, CURLOPT_HTTPHEADER, headers);
    
    /* Set method */
    if (strcmp(method, "POST") == 0) {
        curl_easy_setopt(ctx->curl, CURLOPT_POST, 1L);
        if (body) curl_easy_setopt(ctx->curl, CURLOPT_POSTFIELDS, body);
    } else if (strcmp(method, "PATCH") == 0) {
        curl_easy_setopt(ctx->curl, CURLOPT_CUSTOMREQUEST, "PATCH");
        if (body) curl_easy_setopt(ctx->curl, CURLOPT_POSTFIELDS, body);
    } else if (strcmp(method, "DELETE") == 0) {
        curl_easy_setopt(ctx->curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    }
    
    /* Response buffer */
    http_response_t resp = {0};
    curl_easy_setopt(ctx->curl, CURLOPT_WRITEFUNCTION, http_write_callback);
    curl_easy_setopt(ctx->curl, CURLOPT_WRITEDATA, &resp);
    
    /* Execute request */
    CURLcode res = curl_easy_perform(ctx->curl);
    
    curl_slist_free_all(headers);
    pthread_mutex_unlock(&ctx->curl_mutex);
    
    /* Update stats */
    pthread_mutex_lock(&ctx->stats_mutex);
    ctx->stats.api_calls++;
    pthread_mutex_unlock(&ctx->stats_mutex);
    
    if (res != CURLE_OK) {
        free_safe(resp.data);
        return COG_ERROR_NETWORK;
    }
    
    *response = resp.data;
    *response_size = resp.size;
    
    return COG_OK;
}

/*===========================================================================
 * Context Lifecycle
 *===========================================================================*/

COGUTIL_API cog_result_t msgraph_init(
    const msgraph_config_t* config,
    msgraph_context_t* ctx
) {
    if (!config || !config->target_atomspace || !ctx) return COG_ERROR_INVALID_PARAM;
    
    msgraph_context_t c = calloc(1, sizeof(struct msgraph_context));
    if (!c) return COG_ERROR_MEMORY;
    
    /* Copy config */
    c->config.tenant_id = strdup_safe(config->tenant_id);
    c->config.client_id = strdup_safe(config->client_id);
    c->config.client_secret = strdup_safe(config->client_secret);
    c->config.redirect_uri = strdup_safe(config->redirect_uri);
    c->config.scope = strdup_safe(config->scope);
    c->config.enable_cache = config->enable_cache;
    c->config.cache_ttl_seconds = config->cache_ttl_seconds;
    
    c->atomspace = config->target_atomspace;
    
    /* Initialize mutexes */
    pthread_mutex_init(&c->token_mutex, NULL);
    pthread_mutex_init(&c->curl_mutex, NULL);
    pthread_mutex_init(&c->stats_mutex, NULL);
    pthread_rwlock_init(&c->cache.lock, NULL);
    
    /* Initialize curl */
    c->curl = curl_easy_init();
    if (!c->curl) {
        free(c);
        return COG_ERROR_INIT;
    }
    
    /* Initialize cache */
    c->cache.capacity = 1024;
    c->cache.entities = calloc(c->cache.capacity, sizeof(msgraph_entity_t));
    
    *ctx = c;
    return COG_OK;
}

COGUTIL_API void msgraph_shutdown(msgraph_context_t ctx) {
    if (!ctx) return;
    
    /* Stop sync thread */
    if (ctx->sync_running) {
        ctx->sync_running = false;
        pthread_join(ctx->sync_thread, NULL);
    }
    
    /* Cleanup curl */
    if (ctx->curl) curl_easy_cleanup(ctx->curl);
    
    /* Free cache */
    pthread_rwlock_wrlock(&ctx->cache.lock);
    for (size_t i = 0; i < ctx->cache.count; i++) {
        free_safe((void*)ctx->cache.entities[i].id);
        free_safe((void*)ctx->cache.entities[i].display_name);
        free_safe((void*)ctx->cache.entities[i].properties_json);
    }
    free_safe(ctx->cache.entities);
    pthread_rwlock_unlock(&ctx->cache.lock);
    
    /* Free config strings */
    free_safe((void*)ctx->config.tenant_id);
    free_safe((void*)ctx->config.client_id);
    free_safe((void*)ctx->config.client_secret);
    free_safe((void*)ctx->config.redirect_uri);
    free_safe((void*)ctx->config.scope);
    
    /* Free other resources */
    free_safe(ctx->access_token);
    free_safe(ctx->delta_link);
    
    /* Destroy mutexes */
    pthread_mutex_destroy(&ctx->token_mutex);
    pthread_mutex_destroy(&ctx->curl_mutex);
    pthread_mutex_destroy(&ctx->stats_mutex);
    pthread_rwlock_destroy(&ctx->cache.lock);
    
    free(ctx);
}

/*===========================================================================
 * Entity Operations
 *===========================================================================*/

COGUTIL_API cog_result_t msgraph_get_entity(
    msgraph_context_t ctx,
    const char* entity_id,
    msgraph_entity_type_t type,
    msgraph_entity_t* entity
) {
    if (!ctx || !entity_id || !entity) return COG_ERROR_INVALID_PARAM;
    
    /* Build endpoint based on type */
    char endpoint[256];
    switch (type) {
        case MSGRAPH_TYPE_USER:
            snprintf(endpoint, sizeof(endpoint), "/users/%s", entity_id);
            break;
        case MSGRAPH_TYPE_GROUP:
            snprintf(endpoint, sizeof(endpoint), "/groups/%s", entity_id);
            break;
        case MSGRAPH_TYPE_DRIVE_ITEM:
            snprintf(endpoint, sizeof(endpoint), "/me/drive/items/%s", entity_id);
            break;
        case MSGRAPH_TYPE_MESSAGE:
            snprintf(endpoint, sizeof(endpoint), "/me/messages/%s", entity_id);
            break;
        case MSGRAPH_TYPE_EVENT:
            snprintf(endpoint, sizeof(endpoint), "/me/events/%s", entity_id);
            break;
        case MSGRAPH_TYPE_SITE:
            snprintf(endpoint, sizeof(endpoint), "/sites/%s", entity_id);
            break;
        case MSGRAPH_TYPE_TEAM:
            snprintf(endpoint, sizeof(endpoint), "/teams/%s", entity_id);
            break;
        case MSGRAPH_TYPE_CHANNEL:
            snprintf(endpoint, sizeof(endpoint), "/teams/%s", entity_id);
            break;
        default:
            return COG_ERROR_INVALID_PARAM;
    }
    
    char* response = NULL;
    size_t response_size = 0;
    cog_result_t result = graph_api_request(ctx, "GET", endpoint, NULL, &response, &response_size);
    
    if (result != COG_OK) return result;
    
    /* Parse response and populate entity */
    entity->id = strdup_safe(entity_id);
    entity->type = type;
    entity->properties_json = response;
    
    /* Extract display name (simplified parsing) */
    char* name_start = strstr(response, "\"displayName\":\"");
    if (name_start) {
        name_start += 15;
        char* name_end = strchr(name_start, '"');
        if (name_end) {
            size_t name_len = name_end - name_start;
            entity->display_name = malloc(name_len + 1);
            strncpy((char*)entity->display_name, name_start, name_len);
            ((char*)entity->display_name)[name_len] = 0;
        }
    }
    
    entity->last_modified = time(NULL);
    
    /* Update stats */
    pthread_mutex_lock(&ctx->stats_mutex);
    ctx->stats.entities_fetched++;
    pthread_mutex_unlock(&ctx->stats_mutex);
    
    return COG_OK;
}

COGUTIL_API cog_result_t msgraph_list_entities(
    msgraph_context_t ctx,
    msgraph_entity_type_t type,
    const char* filter,
    const char* select,
    uint32_t top,
    msgraph_entity_t** entities,
    size_t* count
) {
    if (!ctx || !entities || !count) return COG_ERROR_INVALID_PARAM;
    
    /* Build endpoint */
    char endpoint[512];
    const char* base_endpoint;
    
    switch (type) {
        case MSGRAPH_TYPE_USER: base_endpoint = "/users"; break;
        case MSGRAPH_TYPE_GROUP: base_endpoint = "/groups"; break;
        case MSGRAPH_TYPE_DRIVE_ITEM: base_endpoint = "/me/drive/root/children"; break;
        case MSGRAPH_TYPE_MESSAGE: base_endpoint = "/me/messages"; break;
        case MSGRAPH_TYPE_EVENT: base_endpoint = "/me/events"; break;
        case MSGRAPH_TYPE_SITE: base_endpoint = "/sites"; break;
        case MSGRAPH_TYPE_TEAM: base_endpoint = "/me/joinedTeams"; break;
        default: return COG_ERROR_INVALID_PARAM;
    }
    
    /* Build query string */
    char query[256] = "";
    int offset = 0;
    
    if (filter) {
        offset += snprintf(query + offset, sizeof(query) - offset, "$filter=%s", filter);
    }
    if (select) {
        offset += snprintf(query + offset, sizeof(query) - offset, "%s$select=%s", 
            offset > 0 ? "&" : "", select);
    }
    if (top > 0) {
        offset += snprintf(query + offset, sizeof(query) - offset, "%s$top=%u",
            offset > 0 ? "&" : "", top);
    }
    
    if (offset > 0) {
        snprintf(endpoint, sizeof(endpoint), "%s?%s", base_endpoint, query);
    } else {
        snprintf(endpoint, sizeof(endpoint), "%s", base_endpoint);
    }
    
    char* response = NULL;
    size_t response_size = 0;
    cog_result_t result = graph_api_request(ctx, "GET", endpoint, NULL, &response, &response_size);
    
    if (result != COG_OK) return result;
    
    /* Parse response - count entities (simplified) */
    size_t entity_count = 0;
    char* ptr = response;
    while ((ptr = strstr(ptr, "\"id\":\"")) != NULL) {
        entity_count++;
        ptr++;
    }
    
    /* Allocate entity array */
    *entities = calloc(entity_count, sizeof(msgraph_entity_t));
    *count = entity_count;
    
    /* Parse each entity (simplified - would use proper JSON parser) */
    ptr = response;
    for (size_t i = 0; i < entity_count; i++) {
        char* id_start = strstr(ptr, "\"id\":\"");
        if (id_start) {
            id_start += 6;
            char* id_end = strchr(id_start, '"');
            if (id_end) {
                size_t id_len = id_end - id_start;
                (*entities)[i].id = malloc(id_len + 1);
                strncpy((char*)(*entities)[i].id, id_start, id_len);
                ((char*)(*entities)[i].id)[id_len] = 0;
                (*entities)[i].type = type;
                ptr = id_end;
            }
        }
    }
    
    free_safe(response);
    return COG_OK;
}

/*===========================================================================
 * AtomSpace Integration
 *===========================================================================*/

COGUTIL_API cog_result_t msgraph_entity_to_atom(
    msgraph_context_t ctx,
    const msgraph_entity_t* entity,
    atom_handle_t* atom
) {
    if (!ctx || !entity || !atom) return COG_ERROR_INVALID_PARAM;
    if (!ctx->atomspace) return COG_ERROR_INIT;
    
    atomspace_t as = ctx->atomspace;
    
    /* Determine the entity type category name */
    const char* type_name = NULL;
    switch (entity->type) {
        case MSGRAPH_TYPE_USER:    type_name = "MSGraph:User"; break;
        case MSGRAPH_TYPE_GROUP:   type_name = "MSGraph:Group"; break;
        case MSGRAPH_TYPE_TEAM:    type_name = "MSGraph:Team"; break;
        case MSGRAPH_TYPE_CHANNEL: type_name = "MSGraph:Channel"; break;
        case MSGRAPH_TYPE_DRIVE:   type_name = "MSGraph:Drive"; break;
        case MSGRAPH_TYPE_SITE:    type_name = "MSGraph:Site"; break;
        case MSGRAPH_TYPE_MESSAGE: type_name = "MSGraph:Message"; break;
        case MSGRAPH_TYPE_EVENT:   type_name = "MSGraph:Event"; break;
        default:                   type_name = "MSGraph:Entity"; break;
    }
    
    /* Create the entity node with truth value <1.0, 0.9> */
    const char* name = entity->display_name ? entity->display_name : entity->id;
    truth_value_t entity_tv = {.strength = 1.0, .confidence = 0.9};
    atom_handle_t entity_atom = atomspace_add_node_tv(as, ATOM_TYPE_CONCEPT_NODE, name, entity_tv);
    if (entity_atom == ATOM_HANDLE_INVALID) return COG_ERROR_MEMORY;
    
    /* Create the type category node */
    atom_handle_t category = atomspace_add_node(as, ATOM_TYPE_CONCEPT_NODE, type_name);
    if (category != ATOM_HANDLE_INVALID) {
        /* InheritanceLink: entity -> category  TV <1.0, 1.0> */
        atom_handle_t inh_out[2] = {entity_atom, category};
        truth_value_t inh_tv = {.strength = 1.0, .confidence = 1.0};
        atomspace_add_link_tv(as, ATOM_TYPE_INHERITANCE_LINK, inh_out, 2, inh_tv);
    }
    
    /* Store entity ID as an EvaluationLink property */
    if (entity->id) {
        atom_handle_t id_pred = atomspace_add_node(as, ATOM_TYPE_PREDICATE_NODE, "msgraph:id");
        char id_name[256];
        snprintf(id_name, sizeof(id_name), "id:%s", entity->id);
        atom_handle_t id_val = atomspace_add_node(as, ATOM_TYPE_CONCEPT_NODE, id_name);
        if (id_pred != ATOM_HANDLE_INVALID && id_val != ATOM_HANDLE_INVALID) {
            atom_handle_t list_out[2] = {entity_atom, id_val};
            atom_handle_t list_link = atomspace_add_link(as, ATOM_TYPE_LIST_LINK, list_out, 2);
            if (list_link != ATOM_HANDLE_INVALID) {
                atom_handle_t eval_out[2] = {id_pred, list_link};
                atomspace_add_link(as, ATOM_TYPE_EVALUATION_LINK, eval_out, 2);
            }
        }
    }
    
    /* Stimulate the atom to bring it into attentional focus */
    atomspace_stimulate(as, entity_atom, 5);
    
    *atom = entity_atom;
    
    /* Update stats */
    pthread_mutex_lock(&ctx->stats_mutex);
    ctx->stats.atoms_created++;
    pthread_mutex_unlock(&ctx->stats_mutex);
    
    return COG_OK;
}

COGUTIL_API cog_result_t msgraph_atom_to_entity(
    msgraph_context_t ctx,
    atom_handle_t atom,
    msgraph_entity_t* entity
) {
    if (!ctx || !entity) return COG_ERROR_INVALID_PARAM;
    if (!ctx->atomspace) return COG_ERROR_INIT;
    if (atom == ATOM_HANDLE_INVALID) return COG_ERROR_INVALID_PARAM;
    
    atomspace_t as = ctx->atomspace;
    
    /* Get the atom's name (display_name) */
    const char* node_name = atomspace_get_name(as, atom);
    if (!node_name) return COG_ERROR_NOT_FOUND;
    
    entity->display_name = strdup(node_name);
    
    /* Determine entity type by checking InheritanceLinks to category nodes */
    static const struct {
        const char* category;
        msgraph_entity_type_t type;
    } type_map[] = {
        {"MSGraph:User",    MSGRAPH_TYPE_USER},
        {"MSGraph:Group",   MSGRAPH_TYPE_GROUP},
        {"MSGraph:Team",    MSGRAPH_TYPE_TEAM},
        {"MSGraph:Channel", MSGRAPH_TYPE_CHANNEL},
        {"MSGraph:Drive",   MSGRAPH_TYPE_DRIVE},
        {"MSGraph:Site",    MSGRAPH_TYPE_SITE},
        {"MSGraph:Message", MSGRAPH_TYPE_MESSAGE},
        {"MSGraph:Event",   MSGRAPH_TYPE_EVENT},
    };
    
    entity->type = MSGRAPH_TYPE_USER; /* default */
    for (int i = 0; i < 8; i++) {
        atom_handle_t cat = atomspace_get_node(as, ATOM_TYPE_CONCEPT_NODE, type_map[i].category);
        if (cat != ATOM_HANDLE_INVALID) {
            atom_handle_t inh_out[2] = {atom, cat};
            atom_handle_t inh = atomspace_get_link(as, ATOM_TYPE_INHERITANCE_LINK, inh_out, 2);
            if (inh != ATOM_HANDLE_INVALID) {
                entity->type = type_map[i].type;
                break;
            }
        }
    }
    
    /* Recover entity ID from EvaluationLink(msgraph:id, ListLink(atom, id_val)) */
    /* For now, set id to NULL if not recoverable */
    entity->id = NULL;
    
    return COG_OK;
}

COGUTIL_API cog_result_t msgraph_sync_to_atomspace(
    msgraph_context_t ctx,
    msgraph_entity_type_t type,
    const char* filter,
    size_t* synced_count
) {
    if (!ctx || !synced_count) return COG_ERROR_INVALID_PARAM;
    
    msgraph_entity_t* entities = NULL;
    size_t count = 0;
    
    cog_result_t result = msgraph_list_entities(ctx, type, filter, NULL, 100, &entities, &count);
    if (result != COG_OK) return result;
    
    *synced_count = 0;
    for (size_t i = 0; i < count; i++) {
        atom_handle_t atom;
        if (msgraph_entity_to_atom(ctx, &entities[i], &atom) == COG_OK) {
            (*synced_count)++;
        }
    }
    
    /* Cleanup */
    for (size_t i = 0; i < count; i++) {
        free_safe((void*)entities[i].id);
        free_safe((void*)entities[i].display_name);
        free_safe((void*)entities[i].properties_json);
    }
    free_safe(entities);
    
    /* Update stats */
    pthread_mutex_lock(&ctx->stats_mutex);
    ctx->stats.sync_operations++;
    pthread_mutex_unlock(&ctx->stats_mutex);
    
    return COG_OK;
}

/*===========================================================================
 * Statistics
 *===========================================================================*/

COGUTIL_API cog_result_t msgraph_get_stats(
    msgraph_context_t ctx,
    msgraph_stats_t* stats
) {
    if (!ctx || !stats) return COG_ERROR_INVALID_PARAM;
    
    pthread_mutex_lock(&ctx->stats_mutex);
    memcpy(stats, &ctx->stats, sizeof(msgraph_stats_t));
    pthread_mutex_unlock(&ctx->stats_mutex);
    
    return COG_OK;
}

COGUTIL_API void msgraph_reset_stats(msgraph_context_t ctx) {
    if (!ctx) return;
    
    pthread_mutex_lock(&ctx->stats_mutex);
    memset(&ctx->stats, 0, sizeof(msgraph_stats_t));
    pthread_mutex_unlock(&ctx->stats_mutex);
}
