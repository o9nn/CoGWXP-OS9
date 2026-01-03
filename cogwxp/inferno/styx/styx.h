/**
 * @file styx.h
 * @brief Styx Protocol - 9P over Dis Channels for CoGWXP-OS9
 * 
 * Styx is the Inferno implementation of the 9P protocol, providing
 * distributed file system access through Dis VM channels.
 * 
 * @copyright MIT/Lucent Public License
 */

#ifndef _COGWXP_STYX_H_
#define _COGWXP_STYX_H_

#include "../dis/dis.h"
#include "../../plan9/9p/9p.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Styx Constants
 *===========================================================================*/

#define STYX_VERSION        "9P2000"
#define STYX_MAX_MSG_SIZE   8192
#define STYX_NOTAG          ((uint16_t)~0)
#define STYX_NOFID          ((uint32_t)~0)

/*===========================================================================
 * Styx Message Types (same as 9P)
 *===========================================================================*/

typedef enum {
    STYX_TVERSION = 100,
    STYX_RVERSION,
    STYX_TAUTH = 102,
    STYX_RAUTH,
    STYX_TATTACH = 104,
    STYX_RATTACH,
    STYX_TERROR = 106,  /* Illegal */
    STYX_RERROR,
    STYX_TFLUSH = 108,
    STYX_RFLUSH,
    STYX_TWALK = 110,
    STYX_RWALK,
    STYX_TOPEN = 112,
    STYX_ROPEN,
    STYX_TCREATE = 114,
    STYX_RCREATE,
    STYX_TREAD = 116,
    STYX_RREAD,
    STYX_TWRITE = 118,
    STYX_RWRITE,
    STYX_TCLUNK = 120,
    STYX_RCLUNK,
    STYX_TREMOVE = 122,
    STYX_RREMOVE,
    STYX_TSTAT = 124,
    STYX_RSTAT,
    STYX_TWSTAT = 126,
    STYX_RWSTAT
} styx_msg_type_t;

/*===========================================================================
 * Styx Connection
 *===========================================================================*/

typedef struct styx_conn {
    dis_channel_t* request_chan;    /* Client -> Server requests */
    dis_channel_t* response_chan;   /* Server -> Client responses */
    
    uint32_t msize;                 /* Maximum message size */
    char* version;                  /* Protocol version */
    
    /* Authentication */
    uint32_t afid;
    char* uname;
    char* aname;
    
    /* State */
    bool connected;
    cog_mutex_t mutex;
    
    /* Tag management */
    uint16_t next_tag;
    
    /* FID table */
    struct styx_fid** fids;
    size_t fid_count;
    size_t fid_capacity;
} styx_conn_t;

/*===========================================================================
 * Styx FID
 *===========================================================================*/

typedef struct styx_fid {
    uint32_t fid;
    char* path;
    uint8_t mode;
    uint64_t offset;
    p9_qid_t qid;
    bool open;
    void* aux;                      /* Server-side auxiliary data */
} styx_fid_t;

/*===========================================================================
 * Styx Server
 *===========================================================================*/

typedef struct styx_server styx_server_t;

/* Server callbacks */
typedef struct styx_ops {
    /* File operations */
    cog_result_t (*attach)(styx_server_t* srv, styx_fid_t* fid, const char* uname, const char* aname);
    cog_result_t (*walk)(styx_server_t* srv, styx_fid_t* fid, styx_fid_t* newfid, const char** names, size_t nnames, p9_qid_t* qids, size_t* nqids);
    cog_result_t (*open)(styx_server_t* srv, styx_fid_t* fid, uint8_t mode);
    cog_result_t (*create)(styx_server_t* srv, styx_fid_t* fid, const char* name, uint32_t perm, uint8_t mode);
    cog_result_t (*read)(styx_server_t* srv, styx_fid_t* fid, uint64_t offset, void* buf, size_t count, size_t* nread);
    cog_result_t (*write)(styx_server_t* srv, styx_fid_t* fid, uint64_t offset, const void* data, size_t count, size_t* nwritten);
    cog_result_t (*clunk)(styx_server_t* srv, styx_fid_t* fid);
    cog_result_t (*remove)(styx_server_t* srv, styx_fid_t* fid);
    cog_result_t (*stat)(styx_server_t* srv, styx_fid_t* fid, p9_stat_t* stat);
    cog_result_t (*wstat)(styx_server_t* srv, styx_fid_t* fid, p9_stat_t* stat);
    
    /* Authentication */
    cog_result_t (*auth)(styx_server_t* srv, styx_fid_t* afid, const char* uname, const char* aname);
    
    /* User data */
    void* user_data;
} styx_ops_t;

struct styx_server {
    dis_vm_t* vm;
    dis_channel_t* listen_chan;     /* Channel for accepting connections */
    
    styx_ops_t ops;
    
    /* Active connections */
    styx_conn_t** connections;
    size_t conn_count;
    size_t conn_capacity;
    
    /* Configuration */
    uint32_t msize;
    
    /* State */
    bool running;
    dis_thread_t* accept_thread;
    cog_mutex_t mutex;
};

/*===========================================================================
 * Styx Client
 *===========================================================================*/

typedef struct styx_client {
    dis_vm_t* vm;
    styx_conn_t* conn;
    
    /* Root fid */
    uint32_t root_fid;
    
    /* FID allocation */
    uint32_t next_fid;
} styx_client_t;

/*===========================================================================
 * Server Lifecycle
 *===========================================================================*/

COGUTIL_API styx_server_t* styx_server_create(dis_vm_t* vm, styx_ops_t* ops);
COGUTIL_API void           styx_server_destroy(styx_server_t* server);
COGUTIL_API cog_result_t   styx_server_listen(styx_server_t* server, dis_channel_t* listen_chan);
COGUTIL_API void           styx_server_stop(styx_server_t* server);

/*===========================================================================
 * Client Lifecycle
 *===========================================================================*/

COGUTIL_API styx_client_t* styx_client_create(dis_vm_t* vm);
COGUTIL_API void           styx_client_destroy(styx_client_t* client);
COGUTIL_API cog_result_t   styx_client_connect(styx_client_t* client, dis_channel_t* request_chan, dis_channel_t* response_chan);
COGUTIL_API cog_result_t   styx_client_attach(styx_client_t* client, const char* uname, const char* aname);
COGUTIL_API void           styx_client_disconnect(styx_client_t* client);

/*===========================================================================
 * Client Operations
 *===========================================================================*/

COGUTIL_API cog_result_t styx_open(styx_client_t* client, const char* path, uint8_t mode, uint32_t* fid);
COGUTIL_API cog_result_t styx_create(styx_client_t* client, const char* path, uint32_t perm, uint8_t mode, uint32_t* fid);
COGUTIL_API cog_result_t styx_read(styx_client_t* client, uint32_t fid, void* buf, size_t count, size_t* nread);
COGUTIL_API cog_result_t styx_write(styx_client_t* client, uint32_t fid, const void* data, size_t count, size_t* nwritten);
COGUTIL_API cog_result_t styx_close(styx_client_t* client, uint32_t fid);
COGUTIL_API cog_result_t styx_remove(styx_client_t* client, const char* path);
COGUTIL_API cog_result_t styx_stat(styx_client_t* client, const char* path, p9_stat_t* stat);
COGUTIL_API cog_result_t styx_wstat(styx_client_t* client, const char* path, p9_stat_t* stat);

/* Directory operations */
COGUTIL_API cog_result_t styx_readdir(styx_client_t* client, const char* path, p9_stat_t** entries, size_t* count);
COGUTIL_API cog_result_t styx_mkdir(styx_client_t* client, const char* path, uint32_t perm);

/*===========================================================================
 * AtomSpace Styx Server
 *===========================================================================*/

/**
 * Create a Styx server that exports an AtomSpace
 * 
 * File system layout:
 * /
 * ├── atoms/
 * │   ├── by-type/
 * │   │   ├── ConceptNode/
 * │   │   │   ├── <name1>
 * │   │   │   └── <name2>
 * │   │   └── ...
 * │   └── by-handle/
 * │       ├── 0x0001
 * │       └── ...
 * ├── links/
 * │   └── ...
 * ├── query
 * └── ctl
 */
COGUTIL_API styx_server_t* styx_atomspace_server_create(dis_vm_t* vm, void* atomspace);

/*===========================================================================
 * Agent Styx Server
 *===========================================================================*/

/**
 * Create a Styx server that exports cognitive agents
 * 
 * File system layout:
 * /
 * ├── <agent_id>/
 * │   ├── ctl
 * │   ├── status
 * │   ├── atomspace/
 * │   ├── inbox
 * │   └── outbox
 * └── ...
 */
COGUTIL_API styx_server_t* styx_agent_server_create(dis_vm_t* vm, void* cogserver);

/*===========================================================================
 * Utility Functions
 *===========================================================================*/

/* Message encoding/decoding */
COGUTIL_API size_t styx_msg_size(void* msg);
COGUTIL_API cog_result_t styx_msg_pack(void* msg, uint8_t* buf, size_t bufsize, size_t* packed_size);
COGUTIL_API cog_result_t styx_msg_unpack(uint8_t* buf, size_t bufsize, void** msg);
COGUTIL_API void styx_msg_free(void* msg);

/* Path manipulation */
COGUTIL_API char** styx_path_split(const char* path, size_t* count);
COGUTIL_API char* styx_path_join(const char** components, size_t count);
COGUTIL_API void styx_path_free(char** components, size_t count);

/* QID utilities */
COGUTIL_API p9_qid_t styx_qid_file(uint64_t path, uint32_t version);
COGUTIL_API p9_qid_t styx_qid_dir(uint64_t path, uint32_t version);

#ifdef __cplusplus
}
#endif

#endif /* _COGWXP_STYX_H_ */
