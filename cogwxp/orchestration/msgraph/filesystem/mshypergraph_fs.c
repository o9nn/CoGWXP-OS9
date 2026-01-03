/**
 * @file mshypergraph_fs.c
 * @brief MSHyperGraph Filesystem Implementation - 9P Interface
 * 
 * @copyright CoGWXP-OS9 Project
 */

#include "mshypergraph_fs.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>

/*===========================================================================
 * Internal Structures
 *===========================================================================*/

/* Virtual file node */
typedef struct vnode {
    uint64_t qid_path;
    uint32_t qid_version;
    uint8_t qid_type;
    
    mshypergraph_file_type_t type;
    char* name;
    char* full_path;
    
    /* Parent and children */
    struct vnode* parent;
    struct vnode** children;
    size_t child_count;
    size_t child_capacity;
    
    /* Type-specific data */
    union {
        struct {
            char* entity_id;
            msgraph_entity_type_t entity_type;
        } entity;
        struct {
            atom_handle_t handle;
        } atom;
    } data;
    
    /* Metadata */
    uint32_t mode;
    uint32_t atime;
    uint32_t mtime;
    uint64_t length;
    
    pthread_rwlock_t lock;
} vnode_t;

/* Client connection */
typedef struct client {
    int fd;
    uint32_t msize;
    char* uname;
    char* aname;
    
    /* Fid table */
    struct {
        uint32_t fid;
        vnode_t* node;
        uint64_t offset;
        bool open;
        uint8_t mode;
    }* fids;
    size_t fid_count;
    size_t fid_capacity;
    
    pthread_mutex_t lock;
} client_t;

/* Filesystem context */
struct mshypergraph_fs {
    mshypergraph_fs_config_t config;
    
    /* Root vnode */
    vnode_t* root;
    
    /* Server socket */
    int server_fd;
    bool running;
    pthread_t accept_thread;
    
    /* Clients */
    client_t** clients;
    size_t client_count;
    size_t client_capacity;
    pthread_mutex_t clients_lock;
    
    /* Statistics */
    mshypergraph_fs_stats_t stats;
    pthread_mutex_t stats_lock;
    
    /* Next QID path */
    uint64_t next_qid_path;
    pthread_mutex_t qid_lock;
};

/*===========================================================================
 * VNode Management
 *===========================================================================*/

static vnode_t* vnode_create(mshypergraph_fs_t fs, const char* name, mshypergraph_file_type_t type) {
    vnode_t* node = calloc(1, sizeof(vnode_t));
    if (!node) return NULL;
    
    pthread_mutex_lock(&fs->qid_lock);
    node->qid_path = fs->next_qid_path++;
    pthread_mutex_unlock(&fs->qid_lock);
    
    node->qid_version = 0;
    node->qid_type = (type == MSHG_FILE_DIRECTORY) ? 0x80 : 0x00;
    node->type = type;
    node->name = strdup(name);
    node->mode = (type == MSHG_FILE_DIRECTORY) ? 0755 : 0644;
    node->atime = node->mtime = (uint32_t)time(NULL);
    
    node->child_capacity = 16;
    node->children = calloc(node->child_capacity, sizeof(vnode_t*));
    
    pthread_rwlock_init(&node->lock, NULL);
    
    return node;
}

static void vnode_destroy(vnode_t* node) {
    if (!node) return;
    
    /* Destroy children first */
    for (size_t i = 0; i < node->child_count; i++) {
        vnode_destroy(node->children[i]);
    }
    
    free(node->name);
    free(node->full_path);
    free(node->children);
    
    if (node->type == MSHG_FILE_ENTITY) {
        free(node->data.entity.entity_id);
    }
    
    pthread_rwlock_destroy(&node->lock);
    free(node);
}

static cog_result_t vnode_add_child(vnode_t* parent, vnode_t* child) {
    if (!parent || !child) return COG_ERROR_INVALID_PARAM;
    
    pthread_rwlock_wrlock(&parent->lock);
    
    if (parent->child_count >= parent->child_capacity) {
        size_t new_capacity = parent->child_capacity * 2;
        vnode_t** new_children = realloc(parent->children, new_capacity * sizeof(vnode_t*));
        if (!new_children) {
            pthread_rwlock_unlock(&parent->lock);
            return COG_ERROR_MEMORY;
        }
        parent->children = new_children;
        parent->child_capacity = new_capacity;
    }
    
    parent->children[parent->child_count++] = child;
    child->parent = parent;
    
    /* Build full path */
    if (parent->full_path) {
        size_t path_len = strlen(parent->full_path) + strlen(child->name) + 2;
        child->full_path = malloc(path_len);
        snprintf(child->full_path, path_len, "%s/%s", parent->full_path, child->name);
    } else {
        child->full_path = strdup(child->name);
    }
    
    pthread_rwlock_unlock(&parent->lock);
    return COG_OK;
}

static vnode_t* vnode_lookup(vnode_t* parent, const char* name) {
    if (!parent || !name) return NULL;
    
    pthread_rwlock_rdlock(&parent->lock);
    
    for (size_t i = 0; i < parent->child_count; i++) {
        if (strcmp(parent->children[i]->name, name) == 0) {
            vnode_t* result = parent->children[i];
            pthread_rwlock_unlock(&parent->lock);
            return result;
        }
    }
    
    pthread_rwlock_unlock(&parent->lock);
    return NULL;
}

static vnode_t* vnode_walk(vnode_t* start, const char* path) {
    if (!start || !path) return NULL;
    if (path[0] == '/') path++;
    if (path[0] == '\0') return start;
    
    char* path_copy = strdup(path);
    char* saveptr;
    char* token = strtok_r(path_copy, "/", &saveptr);
    vnode_t* current = start;
    
    while (token && current) {
        if (strcmp(token, ".") == 0) {
            /* Stay at current */
        } else if (strcmp(token, "..") == 0) {
            current = current->parent ? current->parent : current;
        } else {
            current = vnode_lookup(current, token);
        }
        token = strtok_r(NULL, "/", &saveptr);
    }
    
    free(path_copy);
    return current;
}

/*===========================================================================
 * Filesystem Tree Construction
 *===========================================================================*/

static cog_result_t build_filesystem_tree(mshypergraph_fs_t fs) {
    /* Create root */
    fs->root = vnode_create(fs, "", MSHG_FILE_DIRECTORY);
    if (!fs->root) return COG_ERROR_MEMORY;
    fs->root->full_path = strdup("/mshypergraph");
    
    /* Create /me directory */
    vnode_t* me = vnode_create(fs, "me", MSHG_FILE_DIRECTORY);
    vnode_add_child(fs->root, me);
    
    /* Create /me subdirectories */
    vnode_add_child(me, vnode_create(fs, "profile", MSHG_FILE_ENTITY));
    vnode_add_child(me, vnode_create(fs, "drive", MSHG_FILE_DIRECTORY));
    vnode_add_child(me, vnode_create(fs, "mail", MSHG_FILE_DIRECTORY));
    vnode_add_child(me, vnode_create(fs, "calendar", MSHG_FILE_DIRECTORY));
    vnode_add_child(me, vnode_create(fs, "teams", MSHG_FILE_DIRECTORY));
    
    /* Create /users directory */
    vnode_t* users = vnode_create(fs, "users", MSHG_FILE_DIRECTORY);
    vnode_add_child(fs->root, users);
    
    /* Create /groups directory */
    vnode_t* groups = vnode_create(fs, "groups", MSHG_FILE_DIRECTORY);
    vnode_add_child(fs->root, groups);
    
    /* Create /sites directory */
    vnode_t* sites = vnode_create(fs, "sites", MSHG_FILE_DIRECTORY);
    vnode_add_child(fs->root, sites);
    
    /* Create /atoms directory */
    vnode_t* atoms = vnode_create(fs, "atoms", MSHG_FILE_DIRECTORY);
    vnode_add_child(fs->root, atoms);
    
    /* Create special files */
    vnode_t* query = vnode_create(fs, "query", MSHG_FILE_QUERY);
    vnode_add_child(fs->root, query);
    
    vnode_t* inference = vnode_create(fs, "inference", MSHG_FILE_INFERENCE);
    vnode_add_child(fs->root, inference);
    
    vnode_t* ctl = vnode_create(fs, "ctl", MSHG_FILE_CONTROL);
    vnode_add_child(fs->root, ctl);
    
    return COG_OK;
}

/*===========================================================================
 * 9P Protocol Handling
 *===========================================================================*/

/* 9P message types */
#define P9_TVERSION 100
#define P9_RVERSION 101
#define P9_TAUTH    102
#define P9_RAUTH    103
#define P9_TATTACH  104
#define P9_RATTACH  105
#define P9_TWALK    110
#define P9_RWALK    111
#define P9_TOPEN    112
#define P9_ROPEN    113
#define P9_TCREATE  114
#define P9_RCREATE  115
#define P9_TREAD    116
#define P9_RREAD    117
#define P9_TWRITE   118
#define P9_RWRITE   119
#define P9_TCLUNK   120
#define P9_RCLUNK   121
#define P9_TREMOVE  122
#define P9_RREMOVE  123
#define P9_TSTAT    124
#define P9_RSTAT    125
#define P9_TWSTAT   126
#define P9_RWSTAT   127
#define P9_RERROR   107

static void send_error(client_t* client, uint16_t tag, const char* error) {
    size_t elen = strlen(error);
    uint32_t size = 4 + 1 + 2 + 2 + elen;
    uint8_t buf[256];
    
    /* Size */
    buf[0] = size & 0xff;
    buf[1] = (size >> 8) & 0xff;
    buf[2] = (size >> 16) & 0xff;
    buf[3] = (size >> 24) & 0xff;
    
    /* Type */
    buf[4] = P9_RERROR;
    
    /* Tag */
    buf[5] = tag & 0xff;
    buf[6] = (tag >> 8) & 0xff;
    
    /* Error string */
    buf[7] = elen & 0xff;
    buf[8] = (elen >> 8) & 0xff;
    memcpy(buf + 9, error, elen);
    
    write(client->fd, buf, size);
}

static void handle_version(mshypergraph_fs_t fs, client_t* client, uint8_t* msg, size_t len, uint16_t tag) {
    /* Parse msize and version */
    uint32_t msize = msg[0] | (msg[1] << 8) | (msg[2] << 16) | (msg[3] << 24);
    uint16_t vlen = msg[4] | (msg[5] << 8);
    
    /* Set client msize */
    client->msize = (msize < fs->config.max_message_size) ? msize : fs->config.max_message_size;
    
    /* Send Rversion */
    const char* version = "9P2000";
    size_t version_len = strlen(version);
    uint32_t size = 4 + 1 + 2 + 4 + 2 + version_len;
    uint8_t buf[64];
    
    buf[0] = size & 0xff;
    buf[1] = (size >> 8) & 0xff;
    buf[2] = (size >> 16) & 0xff;
    buf[3] = (size >> 24) & 0xff;
    buf[4] = P9_RVERSION;
    buf[5] = tag & 0xff;
    buf[6] = (tag >> 8) & 0xff;
    buf[7] = client->msize & 0xff;
    buf[8] = (client->msize >> 8) & 0xff;
    buf[9] = (client->msize >> 16) & 0xff;
    buf[10] = (client->msize >> 24) & 0xff;
    buf[11] = version_len & 0xff;
    buf[12] = (version_len >> 8) & 0xff;
    memcpy(buf + 13, version, version_len);
    
    write(client->fd, buf, size);
}

static void handle_attach(mshypergraph_fs_t fs, client_t* client, uint8_t* msg, size_t len, uint16_t tag) {
    uint32_t fid = msg[0] | (msg[1] << 8) | (msg[2] << 16) | (msg[3] << 24);
    
    /* Add fid pointing to root */
    pthread_mutex_lock(&client->lock);
    
    if (client->fid_count >= client->fid_capacity) {
        size_t new_cap = client->fid_capacity * 2;
        void* new_fids = realloc(client->fids, new_cap * sizeof(client->fids[0]));
        if (!new_fids) {
            pthread_mutex_unlock(&client->lock);
            send_error(client, tag, "out of memory");
            return;
        }
        client->fids = new_fids;
        client->fid_capacity = new_cap;
    }
    
    client->fids[client->fid_count].fid = fid;
    client->fids[client->fid_count].node = fs->root;
    client->fids[client->fid_count].offset = 0;
    client->fids[client->fid_count].open = false;
    client->fid_count++;
    
    pthread_mutex_unlock(&client->lock);
    
    /* Send Rattach with root qid */
    uint32_t size = 4 + 1 + 2 + 13;
    uint8_t buf[32];
    
    buf[0] = size & 0xff;
    buf[1] = (size >> 8) & 0xff;
    buf[2] = (size >> 16) & 0xff;
    buf[3] = (size >> 24) & 0xff;
    buf[4] = P9_RATTACH;
    buf[5] = tag & 0xff;
    buf[6] = (tag >> 8) & 0xff;
    
    /* QID: type(1) + version(4) + path(8) */
    buf[7] = fs->root->qid_type;
    buf[8] = fs->root->qid_version & 0xff;
    buf[9] = (fs->root->qid_version >> 8) & 0xff;
    buf[10] = (fs->root->qid_version >> 16) & 0xff;
    buf[11] = (fs->root->qid_version >> 24) & 0xff;
    memcpy(buf + 12, &fs->root->qid_path, 8);
    
    write(client->fd, buf, size);
}

static void* client_handler(void* arg) {
    struct {
        mshypergraph_fs_t fs;
        client_t* client;
    }* ctx = arg;
    
    mshypergraph_fs_t fs = ctx->fs;
    client_t* client = ctx->client;
    free(ctx);
    
    uint8_t header[7];
    
    while (fs->running) {
        /* Read message header */
        ssize_t n = read(client->fd, header, 7);
        if (n <= 0) break;
        
        uint32_t size = header[0] | (header[1] << 8) | (header[2] << 16) | (header[3] << 24);
        uint8_t type = header[4];
        uint16_t tag = header[5] | (header[6] << 8);
        
        /* Read message body */
        uint8_t* body = NULL;
        size_t body_len = size - 7;
        if (body_len > 0) {
            body = malloc(body_len);
            n = read(client->fd, body, body_len);
            if (n != (ssize_t)body_len) {
                free(body);
                break;
            }
        }
        
        /* Handle message */
        switch (type) {
            case P9_TVERSION:
                handle_version(fs, client, body, body_len, tag);
                break;
            case P9_TATTACH:
                handle_attach(fs, client, body, body_len, tag);
                break;
            /* Additional message handlers would go here */
            default:
                send_error(client, tag, "not implemented");
                break;
        }
        
        free(body);
        
        /* Update stats */
        pthread_mutex_lock(&fs->stats_lock);
        fs->stats.total_requests++;
        pthread_mutex_unlock(&fs->stats_lock);
    }
    
    close(client->fd);
    free(client->uname);
    free(client->aname);
    free(client->fids);
    pthread_mutex_destroy(&client->lock);
    free(client);
    
    return NULL;
}

static void* accept_handler(void* arg) {
    mshypergraph_fs_t fs = arg;
    
    while (fs->running) {
        struct sockaddr_in addr;
        socklen_t addr_len = sizeof(addr);
        
        int client_fd = accept(fs->server_fd, (struct sockaddr*)&addr, &addr_len);
        if (client_fd < 0) {
            if (errno == EINTR) continue;
            break;
        }
        
        /* Create client */
        client_t* client = calloc(1, sizeof(client_t));
        client->fd = client_fd;
        client->msize = 8192;
        client->fid_capacity = 64;
        client->fids = calloc(client->fid_capacity, sizeof(client->fids[0]));
        pthread_mutex_init(&client->lock, NULL);
        
        /* Start client handler thread */
        struct {
            mshypergraph_fs_t fs;
            client_t* client;
        }* ctx = malloc(sizeof(*ctx));
        ctx->fs = fs;
        ctx->client = client;
        
        pthread_t thread;
        pthread_create(&thread, NULL, client_handler, ctx);
        pthread_detach(thread);
        
        /* Update stats */
        pthread_mutex_lock(&fs->stats_lock);
        fs->stats.active_connections++;
        pthread_mutex_unlock(&fs->stats_lock);
    }
    
    return NULL;
}

/*===========================================================================
 * Filesystem Lifecycle
 *===========================================================================*/

COGUTIL_API cog_result_t mshypergraph_fs_create(
    const mshypergraph_fs_config_t* config,
    mshypergraph_fs_t* fs
) {
    if (!config || !fs) return COG_ERROR_INVALID_PARAM;
    
    mshypergraph_fs_t f = calloc(1, sizeof(struct mshypergraph_fs));
    if (!f) return COG_ERROR_MEMORY;
    
    /* Copy config */
    memcpy(&f->config, config, sizeof(mshypergraph_fs_config_t));
    if (config->mount_point) {
        f->config.mount_point = strdup(config->mount_point);
    }
    if (config->announce) {
        f->config.announce = strdup(config->announce);
    }
    if (config->auth_domain) {
        f->config.auth_domain = strdup(config->auth_domain);
    }
    
    /* Initialize mutexes */
    pthread_mutex_init(&f->clients_lock, NULL);
    pthread_mutex_init(&f->stats_lock, NULL);
    pthread_mutex_init(&f->qid_lock, NULL);
    
    /* Initialize client array */
    f->client_capacity = 64;
    f->clients = calloc(f->client_capacity, sizeof(client_t*));
    
    /* Build filesystem tree */
    cog_result_t result = build_filesystem_tree(f);
    if (result != COG_OK) {
        free(f);
        return result;
    }
    
    *fs = f;
    return COG_OK;
}

COGUTIL_API cog_result_t mshypergraph_fs_start(mshypergraph_fs_t fs) {
    if (!fs) return COG_ERROR_INVALID_PARAM;
    
    /* Create server socket */
    fs->server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fs->server_fd < 0) return COG_ERROR_NETWORK;
    
    int opt = 1;
    setsockopt(fs->server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(fs->config.port);
    
    if (bind(fs->server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(fs->server_fd);
        return COG_ERROR_NETWORK;
    }
    
    if (listen(fs->server_fd, 128) < 0) {
        close(fs->server_fd);
        return COG_ERROR_NETWORK;
    }
    
    /* Start accept thread */
    fs->running = true;
    pthread_create(&fs->accept_thread, NULL, accept_handler, fs);
    
    return COG_OK;
}

COGUTIL_API cog_result_t mshypergraph_fs_stop(mshypergraph_fs_t fs) {
    if (!fs) return COG_ERROR_INVALID_PARAM;
    
    fs->running = false;
    
    /* Close server socket to unblock accept */
    if (fs->server_fd >= 0) {
        close(fs->server_fd);
        fs->server_fd = -1;
    }
    
    /* Wait for accept thread */
    pthread_join(fs->accept_thread, NULL);
    
    return COG_OK;
}

COGUTIL_API void mshypergraph_fs_destroy(mshypergraph_fs_t fs) {
    if (!fs) return;
    
    /* Stop if running */
    if (fs->running) {
        mshypergraph_fs_stop(fs);
    }
    
    /* Destroy filesystem tree */
    vnode_destroy(fs->root);
    
    /* Free clients */
    free(fs->clients);
    
    /* Free config strings */
    free((void*)fs->config.mount_point);
    free((void*)fs->config.announce);
    free((void*)fs->config.auth_domain);
    
    /* Destroy mutexes */
    pthread_mutex_destroy(&fs->clients_lock);
    pthread_mutex_destroy(&fs->stats_lock);
    pthread_mutex_destroy(&fs->qid_lock);
    
    free(fs);
}

/*===========================================================================
 * Query and Control Operations
 *===========================================================================*/

COGUTIL_API cog_result_t mshypergraph_fs_query(
    mshypergraph_fs_t fs,
    const char* query,
    const char* variables,
    char** result,
    size_t* result_size
) {
    if (!fs || !query || !result || !result_size) return COG_ERROR_INVALID_PARAM;
    
    /* This would delegate to the HyperGraphiQL executor */
    /* For now, return a placeholder response */
    
    const char* response = "{\"data\": {}, \"errors\": null}";
    *result = strdup(response);
    *result_size = strlen(response);
    
    /* Update stats */
    pthread_mutex_lock(&fs->stats_lock);
    fs->stats.query_requests++;
    pthread_mutex_unlock(&fs->stats_lock);
    
    return COG_OK;
}

COGUTIL_API cog_result_t mshypergraph_fs_infer(
    mshypergraph_fs_t fs,
    const char* request,
    char** result,
    size_t* result_size
) {
    if (!fs || !request || !result || !result_size) return COG_ERROR_INVALID_PARAM;
    
    /* This would delegate to PLN inference engine */
    /* For now, return a placeholder response */
    
    const char* response = "{\"conclusions\": [], \"steps\": 0}";
    *result = strdup(response);
    *result_size = strlen(response);
    
    /* Update stats */
    pthread_mutex_lock(&fs->stats_lock);
    fs->stats.inference_requests++;
    pthread_mutex_unlock(&fs->stats_lock);
    
    return COG_OK;
}

COGUTIL_API cog_result_t mshypergraph_fs_control(
    mshypergraph_fs_t fs,
    const char* command,
    char** response,
    size_t* response_size
) {
    if (!fs || !command || !response || !response_size) return COG_ERROR_INVALID_PARAM;
    
    char buf[256];
    
    if (strcmp(command, "stats") == 0) {
        pthread_mutex_lock(&fs->stats_lock);
        snprintf(buf, sizeof(buf),
            "{\"total_requests\": %lu, \"query_requests\": %lu, \"active_connections\": %u}",
            fs->stats.total_requests,
            fs->stats.query_requests,
            fs->stats.active_connections);
        pthread_mutex_unlock(&fs->stats_lock);
    } else if (strncmp(command, "cache clear", 11) == 0) {
        snprintf(buf, sizeof(buf), "{\"status\": \"cache cleared\"}");
    } else {
        snprintf(buf, sizeof(buf), "{\"error\": \"unknown command\"}");
    }
    
    *response = strdup(buf);
    *response_size = strlen(buf);
    
    return COG_OK;
}

/*===========================================================================
 * Statistics
 *===========================================================================*/

COGUTIL_API cog_result_t mshypergraph_fs_get_stats(
    mshypergraph_fs_t fs,
    mshypergraph_fs_stats_t* stats
) {
    if (!fs || !stats) return COG_ERROR_INVALID_PARAM;
    
    pthread_mutex_lock(&fs->stats_lock);
    memcpy(stats, &fs->stats, sizeof(mshypergraph_fs_stats_t));
    pthread_mutex_unlock(&fs->stats_lock);
    
    return COG_OK;
}

COGUTIL_API void mshypergraph_fs_reset_stats(mshypergraph_fs_t fs) {
    if (!fs) return;
    
    pthread_mutex_lock(&fs->stats_lock);
    memset(&fs->stats, 0, sizeof(mshypergraph_fs_stats_t));
    pthread_mutex_unlock(&fs->stats_lock);
}
