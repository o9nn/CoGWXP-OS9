/**
 * @file 9p.c
 * @brief 9P Protocol Implementation for Cognitive Channels
 * 
 * Implements the 9P2000 protocol for distributed cognitive computing,
 * enabling AtomSpace access through file-like interfaces.
 * 
 * @copyright CoGWXP-OS9 Project
 */

#define _P9_INTERNAL
#include "9p.h"
#include "../../opencog/atomspace/atomspace.h"
#include "../../opencog/cogutil/cogutil.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

/*===========================================================================
 * Constants
 *===========================================================================*/

#define P9_NOTAG    ((uint16_t)~0)
#define P9_NOFID    ((uint32_t)~0)
#define P9_IOHDRSZ  24
#define P9_MAXWELEM 16

/* QID types */
#define P9_QTDIR    0x80
#define P9_QTAPPEND 0x40
#define P9_QTEXCL   0x20
#define P9_QTMOUNT  0x10
#define P9_QTAUTH   0x08
#define P9_QTTMP    0x04
#define P9_QTFILE   0x00

/*===========================================================================
 * Internal Structures
 *===========================================================================*/

/* File information */
typedef struct p9_file {
    char* name;
    p9_qid_t qid;
    uint32_t mode;
    uint32_t atime;
    uint32_t mtime;
    uint64_t length;
    char* uid;
    char* gid;
    char* muid;
    
    /* Cognitive extensions */
    atom_handle_t atom_handle;
    p9_cog_file_type_t cog_type;
    
    /* File tree */
    struct p9_file* parent;
    struct p9_file* children;
    struct p9_file* next;
    
    /* Reference counting */
    uint32_t ref_count;
    pthread_mutex_t lock;
} p9_file_t;

/* FID - file identifier in a session */
typedef struct {
    uint32_t fid;
    p9_file_t* file;
    uint32_t mode;      /* Open mode */
    uint64_t offset;    /* Current read/write offset */
    bool open;
} p9_fid_t;

/* Client connection */
typedef struct p9_client {
    int fd;
    uint32_t msize;
    char* version;
    
    /* FID table */
    p9_fid_t* fids;
    size_t fid_count;
    size_t fid_capacity;
    pthread_mutex_t fid_lock;
    
    /* Message buffer */
    uint8_t* buf;
    size_t buf_size;
    
    /* State */
    bool connected;
    struct p9_client* next;
} p9_client_t;

/* 9P Server */
struct p9_server {
    int listen_fd;
    uint16_t port;
    uint32_t msize;
    
    /* Root filesystem */
    p9_file_t* root;
    pthread_rwlock_t fs_lock;
    
    /* Connected clients */
    p9_client_t* clients;
    pthread_mutex_t clients_lock;
    
    /* Cognitive integration */
    atomspace_t atomspace;
    p9_cog_handler_t cog_handler;
    void* cog_handler_data;
    
    /* Server thread */
    pthread_t accept_thread;
    bool running;
    pthread_mutex_t control_lock;
    
    /* Statistics */
    struct {
        uint64_t messages_received;
        uint64_t messages_sent;
        uint64_t bytes_read;
        uint64_t bytes_written;
        uint64_t errors;
    } stats;
    pthread_mutex_t stats_lock;
};
typedef struct p9_server p9_server_t;

/*===========================================================================
 * Message Encoding/Decoding
 *===========================================================================*/

static void put8(uint8_t** p, uint8_t v) {
    *(*p)++ = v;
}

static void put16(uint8_t** p, uint16_t v) {
    *(*p)++ = v & 0xFF;
    *(*p)++ = (v >> 8) & 0xFF;
}

static void put32(uint8_t** p, uint32_t v) {
    *(*p)++ = v & 0xFF;
    *(*p)++ = (v >> 8) & 0xFF;
    *(*p)++ = (v >> 16) & 0xFF;
    *(*p)++ = (v >> 24) & 0xFF;
}

static void put64(uint8_t** p, uint64_t v) {
    put32(p, v & 0xFFFFFFFF);
    put32(p, (v >> 32) & 0xFFFFFFFF);
}

static void putstr(uint8_t** p, const char* s) {
    uint16_t len = s ? strlen(s) : 0;
    put16(p, len);
    if (len > 0) {
        memcpy(*p, s, len);
        *p += len;
    }
}

static void putqid(uint8_t** p, const p9_qid_t* qid) {
    put8(p, qid->type);
    put32(p, qid->version);
    put64(p, qid->path);
}

static uint8_t get8(uint8_t** p) {
    return *(*p)++;
}

static uint16_t get16(uint8_t** p) {
    uint16_t v = (*p)[0] | ((*p)[1] << 8);
    *p += 2;
    return v;
}

static uint32_t get32(uint8_t** p) {
    uint32_t v = (*p)[0] | ((*p)[1] << 8) | ((*p)[2] << 16) | ((*p)[3] << 24);
    *p += 4;
    return v;
}

static uint64_t get64(uint8_t** p) {
    uint64_t lo = get32(p);
    uint64_t hi = get32(p);
    return lo | (hi << 32);
}

static char* getstr(uint8_t** p) {
    uint16_t len = get16(p);
    if (len == 0) return NULL;
    
    char* s = COG_CALLOC(len + 1, 1);
    memcpy(s, *p, len);
    *p += len;
    return s;
}

static void getqid(uint8_t** p, p9_qid_t* qid) {
    qid->type = get8(p);
    qid->version = get32(p);
    qid->path = get64(p);
}

/*===========================================================================
 * File Operations
 *===========================================================================*/

static p9_file_t* file_create(const char* name, uint32_t mode) {
    p9_file_t* f = COG_CALLOC(1, sizeof(p9_file_t));
    if (!f) return NULL;
    
    f->name = COG_STRDUP(name);
    f->mode = mode;
    f->qid.type = (mode & P9_DMDIR) ? P9_QTDIR : P9_QTFILE;
    f->qid.version = 0;
    f->qid.path = (uint64_t)(uintptr_t)f;  /* Use address as unique path */
    f->uid = COG_STRDUP("cogwxp");
    f->gid = COG_STRDUP("cogwxp");
    f->muid = COG_STRDUP("cogwxp");
    f->ref_count = 1;
    
    pthread_mutex_init(&f->lock, NULL);
    
    return f;
}

static void file_destroy(p9_file_t* f) {
    if (!f) return;
    
    COG_FREE(f->name);
    COG_FREE(f->uid);
    COG_FREE(f->gid);
    COG_FREE(f->muid);
    pthread_mutex_destroy(&f->lock);
    COG_FREE(f);
}

static p9_file_t* file_lookup(p9_file_t* dir, const char* name) {
    if (!dir || !name) return NULL;
    
    if (strcmp(name, ".") == 0) return dir;
    if (strcmp(name, "..") == 0) return dir->parent ? dir->parent : dir;
    
    p9_file_t* child = dir->children;
    while (child) {
        if (strcmp(child->name, name) == 0) {
            return child;
        }
        child = child->next;
    }
    
    return NULL;
}

static void file_add_child(p9_file_t* parent, p9_file_t* child) {
    if (!parent || !child) return;
    
    child->parent = parent;
    child->next = parent->children;
    parent->children = child;
}

/*===========================================================================
 * FID Management
 *===========================================================================*/

static p9_fid_t* fid_get(p9_client_t* client, uint32_t fid) {
    pthread_mutex_lock(&client->fid_lock);
    
    for (size_t i = 0; i < client->fid_count; i++) {
        if (client->fids[i].fid == fid) {
            pthread_mutex_unlock(&client->fid_lock);
            return &client->fids[i];
        }
    }
    
    pthread_mutex_unlock(&client->fid_lock);
    return NULL;
}

static p9_fid_t* fid_create(p9_client_t* client, uint32_t fid, p9_file_t* file) {
    pthread_mutex_lock(&client->fid_lock);
    
    /* Check if FID already exists */
    for (size_t i = 0; i < client->fid_count; i++) {
        if (client->fids[i].fid == fid) {
            pthread_mutex_unlock(&client->fid_lock);
            return NULL;
        }
    }
    
    /* Grow array if needed */
    if (client->fid_count >= client->fid_capacity) {
        size_t new_capacity = client->fid_capacity == 0 ? 16 : client->fid_capacity * 2;
        p9_fid_t* new_fids = COG_REALLOC(client->fids, new_capacity * sizeof(p9_fid_t));
        if (!new_fids) {
            pthread_mutex_unlock(&client->fid_lock);
            return NULL;
        }
        client->fids = new_fids;
        client->fid_capacity = new_capacity;
    }
    
    p9_fid_t* f = &client->fids[client->fid_count++];
    f->fid = fid;
    f->file = file;
    f->mode = 0;
    f->offset = 0;
    f->open = false;
    
    if (file) {
        pthread_mutex_lock(&file->lock);
        file->ref_count++;
        pthread_mutex_unlock(&file->lock);
    }
    
    pthread_mutex_unlock(&client->fid_lock);
    return f;
}

static void fid_destroy(p9_client_t* client, uint32_t fid) {
    pthread_mutex_lock(&client->fid_lock);
    
    for (size_t i = 0; i < client->fid_count; i++) {
        if (client->fids[i].fid == fid) {
            p9_file_t* file = client->fids[i].file;
            if (file) {
                pthread_mutex_lock(&file->lock);
                file->ref_count--;
                pthread_mutex_unlock(&file->lock);
            }
            
            /* Remove by swapping with last */
            client->fids[i] = client->fids[--client->fid_count];
            break;
        }
    }
    
    pthread_mutex_unlock(&client->fid_lock);
}

/*===========================================================================
 * Message Handlers
 *===========================================================================*/

static size_t handle_version(p9_server_t* srv, p9_client_t* client,
    uint8_t* in, size_t in_len, uint8_t* out) {
    
    uint8_t* p = in;
    uint32_t msize = get32(&p);
    char* version = getstr(&p);
    
    /* Negotiate version */
    client->msize = (msize < srv->msize) ? msize : srv->msize;
    COG_FREE(client->version);
    
    if (strncmp(version, "9P2000", 6) == 0) {
        client->version = COG_STRDUP("9P2000");
    } else {
        client->version = COG_STRDUP("unknown");
    }
    
    COG_FREE(version);
    
    /* Build response */
    uint8_t* op = out + 4;  /* Skip size field */
    put8(&op, P9_RVERSION);
    put16(&op, P9_NOTAG);
    put32(&op, client->msize);
    putstr(&op, client->version);
    
    size_t len = op - out;
    op = out;
    put32(&op, len);
    
    return len;
}

static size_t handle_attach(p9_server_t* srv, p9_client_t* client,
    uint16_t tag, uint8_t* in, size_t in_len, uint8_t* out) {
    
    uint8_t* p = in;
    uint32_t fid = get32(&p);
    uint32_t afid = get32(&p);
    char* uname = getstr(&p);
    char* aname = getstr(&p);
    
    (void)afid;  /* Auth not implemented */
    
    /* Create FID for root */
    p9_fid_t* f = fid_create(client, fid, srv->root);
    if (!f) {
        COG_FREE(uname);
        COG_FREE(aname);
        /* Return error */
        uint8_t* op = out + 4;
        put8(&op, P9_RERROR);
        put16(&op, tag);
        putstr(&op, "fid in use");
        size_t len = op - out;
        op = out;
        put32(&op, len);
        return len;
    }
    
    COG_FREE(uname);
    COG_FREE(aname);
    
    /* Build response */
    uint8_t* op = out + 4;
    put8(&op, P9_RATTACH);
    put16(&op, tag);
    putqid(&op, &srv->root->qid);
    
    size_t len = op - out;
    op = out;
    put32(&op, len);
    
    return len;
}

static size_t handle_walk(p9_server_t* srv, p9_client_t* client,
    uint16_t tag, uint8_t* in, size_t in_len, uint8_t* out) {
    
    uint8_t* p = in;
    uint32_t fid = get32(&p);
    uint32_t newfid = get32(&p);
    uint16_t nwname = get16(&p);
    
    p9_fid_t* f = fid_get(client, fid);
    if (!f || !f->file) {
        uint8_t* op = out + 4;
        put8(&op, P9_RERROR);
        put16(&op, tag);
        putstr(&op, "bad fid");
        size_t len = op - out;
        op = out;
        put32(&op, len);
        return len;
    }
    
    p9_file_t* current = f->file;
    p9_qid_t qids[P9_MAXWELEM];
    uint16_t nwqid = 0;
    
    for (uint16_t i = 0; i < nwname && i < P9_MAXWELEM; i++) {
        char* name = getstr(&p);
        p9_file_t* next = file_lookup(current, name);
        COG_FREE(name);
        
        if (!next) break;
        
        qids[nwqid++] = next->qid;
        current = next;
    }
    
    if (nwqid == nwname || nwname == 0) {
        /* Create new FID */
        if (fid != newfid) {
            fid_create(client, newfid, current);
        }
    }
    
    /* Build response */
    uint8_t* op = out + 4;
    put8(&op, P9_RWALK);
    put16(&op, tag);
    put16(&op, nwqid);
    for (uint16_t i = 0; i < nwqid; i++) {
        putqid(&op, &qids[i]);
    }
    
    size_t len = op - out;
    op = out;
    put32(&op, len);
    
    return len;
}

static size_t handle_open(p9_server_t* srv, p9_client_t* client,
    uint16_t tag, uint8_t* in, size_t in_len, uint8_t* out) {
    
    uint8_t* p = in;
    uint32_t fid = get32(&p);
    uint8_t mode = get8(&p);
    
    p9_fid_t* f = fid_get(client, fid);
    if (!f || !f->file) {
        uint8_t* op = out + 4;
        put8(&op, P9_RERROR);
        put16(&op, tag);
        putstr(&op, "bad fid");
        size_t len = op - out;
        op = out;
        put32(&op, len);
        return len;
    }
    
    f->mode = mode;
    f->open = true;
    f->offset = 0;
    
    /* Build response */
    uint8_t* op = out + 4;
    put8(&op, P9_ROPEN);
    put16(&op, tag);
    putqid(&op, &f->file->qid);
    put32(&op, client->msize - P9_IOHDRSZ);  /* iounit */
    
    size_t len = op - out;
    op = out;
    put32(&op, len);
    
    return len;
}

static size_t handle_read(p9_server_t* srv, p9_client_t* client,
    uint16_t tag, uint8_t* in, size_t in_len, uint8_t* out) {
    
    uint8_t* p = in;
    uint32_t fid = get32(&p);
    uint64_t offset = get64(&p);
    uint32_t count = get32(&p);
    
    p9_fid_t* f = fid_get(client, fid);
    if (!f || !f->file || !f->open) {
        uint8_t* op = out + 4;
        put8(&op, P9_RERROR);
        put16(&op, tag);
        putstr(&op, "bad fid");
        size_t len = op - out;
        op = out;
        put32(&op, len);
        return len;
    }
    
    /* Prepare response header */
    uint8_t* op = out + 4;
    put8(&op, P9_RREAD);
    put16(&op, tag);
    
    uint8_t* count_pos = op;
    op += 4;  /* Skip count field */
    
    uint32_t actual_count = 0;
    
    if (f->file->qid.type & P9_QTDIR) {
        /* Read directory entries */
        p9_file_t* child = f->file->children;
        uint64_t pos = 0;
        
        while (child && actual_count < count) {
            /* Build stat entry */
            uint8_t stat_buf[256];
            uint8_t* sp = stat_buf + 2;  /* Skip size */
            
            put16(&sp, 0);  /* type */
            put32(&sp, 0);  /* dev */
            putqid(&sp, &child->qid);
            put32(&sp, child->mode);
            put32(&sp, child->atime);
            put32(&sp, child->mtime);
            put64(&sp, child->length);
            putstr(&sp, child->name);
            putstr(&sp, child->uid);
            putstr(&sp, child->gid);
            putstr(&sp, child->muid);
            
            uint16_t stat_size = sp - stat_buf - 2;
            sp = stat_buf;
            put16(&sp, stat_size);
            
            uint16_t entry_size = stat_size + 2;
            
            if (pos >= offset && actual_count + entry_size <= count) {
                memcpy(op, stat_buf, entry_size);
                op += entry_size;
                actual_count += entry_size;
            }
            
            pos += entry_size;
            child = child->next;
        }
    } else {
        /* Read file - invoke cognitive handler if set */
        if (srv->cog_handler && f->file->cog_type != P9_COG_NONE) {
            /* Call cognitive handler for file read */
            /* This would interact with AtomSpace */
        }
    }
    
    /* Fill in count */
    uint8_t* cp = count_pos;
    put32(&cp, actual_count);
    
    size_t len = op - out;
    op = out;
    put32(&op, len);
    
    return len;
}

static size_t handle_clunk(p9_server_t* srv, p9_client_t* client,
    uint16_t tag, uint8_t* in, size_t in_len, uint8_t* out) {
    
    uint8_t* p = in;
    uint32_t fid = get32(&p);
    
    fid_destroy(client, fid);
    
    /* Build response */
    uint8_t* op = out + 4;
    put8(&op, P9_RCLUNK);
    put16(&op, tag);
    
    size_t len = op - out;
    op = out;
    put32(&op, len);
    
    return len;
}

/*===========================================================================
 * Message Processing
 *===========================================================================*/

static void process_message(p9_server_t* srv, p9_client_t* client) {
    /* Read message size */
    uint8_t size_buf[4];
    if (recv(client->fd, size_buf, 4, MSG_WAITALL) != 4) {
        client->connected = false;
        return;
    }
    
    uint8_t* p = size_buf;
    uint32_t size = get32(&p);
    
    if (size < 7 || size > client->msize) {
        client->connected = false;
        return;
    }
    
    /* Read rest of message */
    if (recv(client->fd, client->buf, size - 4, MSG_WAITALL) != (ssize_t)(size - 4)) {
        client->connected = false;
        return;
    }
    
    pthread_mutex_lock(&srv->stats_lock);
    srv->stats.messages_received++;
    srv->stats.bytes_read += size;
    pthread_mutex_unlock(&srv->stats_lock);
    
    p = client->buf;
    uint8_t type = get8(&p);
    uint16_t tag = get16(&p);
    
    uint8_t* out = COG_CALLOC(client->msize, 1);
    size_t out_len = 0;
    
    switch (type) {
        case P9_TVERSION:
            out_len = handle_version(srv, client, p, size - 7, out);
            break;
        case P9_TATTACH:
            out_len = handle_attach(srv, client, tag, p, size - 7, out);
            break;
        case P9_TWALK:
            out_len = handle_walk(srv, client, tag, p, size - 7, out);
            break;
        case P9_TOPEN:
            out_len = handle_open(srv, client, tag, p, size - 7, out);
            break;
        case P9_TREAD:
            out_len = handle_read(srv, client, tag, p, size - 7, out);
            break;
        case P9_TCLUNK:
            out_len = handle_clunk(srv, client, tag, p, size - 7, out);
            break;
        default:
            /* Unknown message - send error */
            {
                uint8_t* op = out + 4;
                put8(&op, P9_RERROR);
                put16(&op, tag);
                putstr(&op, "not implemented");
                out_len = op - out;
                op = out;
                put32(&op, out_len);
            }
            break;
    }
    
    if (out_len > 0) {
        send(client->fd, out, out_len, 0);
        
        pthread_mutex_lock(&srv->stats_lock);
        srv->stats.messages_sent++;
        srv->stats.bytes_written += out_len;
        pthread_mutex_unlock(&srv->stats_lock);
    }
    
    COG_FREE(out);
}

/*===========================================================================
 * Client Handler Thread
 *===========================================================================*/

static void* client_handler(void* arg) {
    struct {
        p9_server_t* srv;
        p9_client_t* client;
    }* ctx = arg;
    
    p9_server_t* srv = ctx->srv;
    p9_client_t* client = ctx->client;
    COG_FREE(ctx);
    
    while (client->connected && srv->running) {
        process_message(srv, client);
    }
    
    /* Cleanup */
    close(client->fd);
    
    pthread_mutex_lock(&srv->clients_lock);
    /* Remove from client list */
    p9_client_t** pp = &srv->clients;
    while (*pp) {
        if (*pp == client) {
            *pp = client->next;
            break;
        }
        pp = &(*pp)->next;
    }
    pthread_mutex_unlock(&srv->clients_lock);
    
    COG_FREE(client->fids);
    COG_FREE(client->buf);
    COG_FREE(client->version);
    COG_FREE(client);
    
    return NULL;
}

/*===========================================================================
 * Accept Thread
 *===========================================================================*/

static void* accept_handler(void* arg) {
    p9_server_t* srv = (p9_server_t*)arg;
    
    while (srv->running) {
        struct sockaddr_in addr;
        socklen_t addr_len = sizeof(addr);
        
        int fd = accept(srv->listen_fd, (struct sockaddr*)&addr, &addr_len);
        if (fd < 0) {
            if (errno == EINTR) continue;
            break;
        }
        
        /* Create client */
        p9_client_t* client = COG_CALLOC(1, sizeof(p9_client_t));
        client->fd = fd;
        client->msize = srv->msize;
        client->buf_size = srv->msize;
        client->buf = COG_CALLOC(client->buf_size, 1);
        client->connected = true;
        pthread_mutex_init(&client->fid_lock, NULL);
        
        /* Add to client list */
        pthread_mutex_lock(&srv->clients_lock);
        client->next = srv->clients;
        srv->clients = client;
        pthread_mutex_unlock(&srv->clients_lock);
        
        /* Start handler thread */
        struct {
            p9_server_t* srv;
            p9_client_t* client;
        }* ctx = COG_CALLOC(1, sizeof(*ctx));
        ctx->srv = srv;
        ctx->client = client;
        
        pthread_t thread;
        pthread_create(&thread, NULL, client_handler, ctx);
        pthread_detach(thread);
        
        COG_LOG_INFO("9P client connected from %s:%d", 
            inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
    }
    
    return NULL;
}

/*===========================================================================
 * Server Lifecycle
 *===========================================================================*/

COGUTIL_API cog_result_t p9_server_create(
    const p9_server_config_t* config,
    p9_server_t** server
) {
    if (!server) return COG_ERROR_INVALID_PARAM;
    
    p9_server_t* srv = COG_CALLOC(1, sizeof(p9_server_t));
    if (!srv) return COG_ERROR_MEMORY;
    
    srv->port = config ? config->port : 564;
    srv->msize = config ? config->msize : 8192;
    
    /* Initialize locks */
    pthread_rwlock_init(&srv->fs_lock, NULL);
    pthread_mutex_init(&srv->clients_lock, NULL);
    pthread_mutex_init(&srv->control_lock, NULL);
    pthread_mutex_init(&srv->stats_lock, NULL);
    
    /* Create root directory */
    srv->root = file_create("/", P9_DMDIR | 0755);
    
    /* Create cognitive filesystem structure */
    p9_file_t* atoms = file_create("atoms", P9_DMDIR | 0755);
    p9_file_t* links = file_create("links", P9_DMDIR | 0755);
    p9_file_t* types = file_create("types", P9_DMDIR | 0755);
    p9_file_t* query = file_create("query", 0644);
    p9_file_t* ctl = file_create("ctl", 0644);
    
    atoms->cog_type = P9_COG_ATOMSPACE;
    links->cog_type = P9_COG_ATOMSPACE;
    types->cog_type = P9_COG_TYPES;
    query->cog_type = P9_COG_QUERY;
    ctl->cog_type = P9_COG_CTL;
    
    file_add_child(srv->root, atoms);
    file_add_child(srv->root, links);
    file_add_child(srv->root, types);
    file_add_child(srv->root, query);
    file_add_child(srv->root, ctl);
    
    *server = srv;
    
    COG_LOG_INFO("9P server created on port %d", srv->port);
    return COG_OK;
}

COGUTIL_API void p9_server_destroy(p9_server_t* server) {
    if (!server) return;
    
    p9_server_stop(server);
    
    /* Destroy filesystem */
    /* TODO: Recursive destroy */
    file_destroy(server->root);
    
    /* Destroy locks */
    pthread_rwlock_destroy(&server->fs_lock);
    pthread_mutex_destroy(&server->clients_lock);
    pthread_mutex_destroy(&server->control_lock);
    pthread_mutex_destroy(&server->stats_lock);
    
    COG_FREE(server);
    
    COG_LOG_INFO("9P server destroyed");
}

COGUTIL_API cog_result_t p9_server_start(p9_server_t* server) {
    if (!server) return COG_ERROR_INVALID_PARAM;
    
    pthread_mutex_lock(&server->control_lock);
    
    if (server->running) {
        pthread_mutex_unlock(&server->control_lock);
        return COG_ERROR_STATE;
    }
    
    /* Create socket */
    server->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server->listen_fd < 0) {
        pthread_mutex_unlock(&server->control_lock);
        return COG_ERROR_IO;
    }
    
    int opt = 1;
    setsockopt(server->listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(server->port)
    };
    
    if (bind(server->listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(server->listen_fd);
        pthread_mutex_unlock(&server->control_lock);
        return COG_ERROR_IO;
    }
    
    if (listen(server->listen_fd, 10) < 0) {
        close(server->listen_fd);
        pthread_mutex_unlock(&server->control_lock);
        return COG_ERROR_IO;
    }
    
    server->running = true;
    pthread_create(&server->accept_thread, NULL, accept_handler, server);
    
    pthread_mutex_unlock(&server->control_lock);
    
    COG_LOG_INFO("9P server started on port %d", server->port);
    return COG_OK;
}

COGUTIL_API cog_result_t p9_server_stop(p9_server_t* server) {
    if (!server) return COG_ERROR_INVALID_PARAM;
    
    pthread_mutex_lock(&server->control_lock);
    
    if (!server->running) {
        pthread_mutex_unlock(&server->control_lock);
        return COG_OK;
    }
    
    server->running = false;
    close(server->listen_fd);
    
    pthread_join(server->accept_thread, NULL);
    
    /* Close all clients */
    pthread_mutex_lock(&server->clients_lock);
    p9_client_t* client = server->clients;
    while (client) {
        client->connected = false;
        close(client->fd);
        client = client->next;
    }
    pthread_mutex_unlock(&server->clients_lock);
    
    pthread_mutex_unlock(&server->control_lock);
    
    COG_LOG_INFO("9P server stopped");
    return COG_OK;
}

COGUTIL_API cog_result_t p9_server_set_atomspace(p9_server_t* server, atomspace_t as) {
    if (!server) return COG_ERROR_INVALID_PARAM;
    server->atomspace = as;
    return COG_OK;
}

COGUTIL_API cog_result_t p9_server_set_handler(
    p9_server_t* server,
    p9_cog_handler_t handler,
    void* data
) {
    if (!server) return COG_ERROR_INVALID_PARAM;
    server->cog_handler = handler;
    server->cog_handler_data = data;
    return COG_OK;
}
