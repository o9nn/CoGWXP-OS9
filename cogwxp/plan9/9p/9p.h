/**
 * @file 9p.h
 * @brief 9P Protocol Implementation for CoGWXP-OS9
 * 
 * The 9P protocol (also known as Styx in Inferno) provides a unified
 * interface for accessing all system resources as files.
 * 
 * @copyright MIT/Lucent Public License
 */

#ifndef _COGWXP_9P_H_
#define _COGWXP_9P_H_

#include "../../opencog/cogutil/cogutil.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * 9P Protocol Constants
 *===========================================================================*/

#define P9_VERSION      "9P2000"
#define P9_NOTAG        ((uint16_t)~0)
#define P9_NOFID        ((uint32_t)~0)
#define P9_MAXWELEM     16
#define P9_IOHDRSZ      24
#define P9_MAXMSG       8192

/*===========================================================================
 * 9P Message Types
 *===========================================================================*/

typedef enum {
    P9_TVERSION = 100,
    P9_RVERSION,
    P9_TAUTH = 102,
    P9_RAUTH,
    P9_TATTACH = 104,
    P9_RATTACH,
    P9_TERROR = 106,    /* Illegal */
    P9_RERROR,
    P9_TFLUSH = 108,
    P9_RFLUSH,
    P9_TWALK = 110,
    P9_RWALK,
    P9_TOPEN = 112,
    P9_ROPEN,
    P9_TCREATE = 114,
    P9_RCREATE,
    P9_TREAD = 116,
    P9_RREAD,
    P9_TWRITE = 118,
    P9_RWRITE,
    P9_TCLUNK = 120,
    P9_RCLUNK,
    P9_TREMOVE = 122,
    P9_RREMOVE,
    P9_TSTAT = 124,
    P9_RSTAT,
    P9_TWSTAT = 126,
    P9_RWSTAT
} p9_msg_type_t;

/*===========================================================================
 * 9P Open Modes
 *===========================================================================*/

#define P9_OREAD    0x00
#define P9_OWRITE   0x01
#define P9_ORDWR    0x02
#define P9_OEXEC    0x03
#define P9_OTRUNC   0x10
#define P9_ORCLOSE  0x40

/*===========================================================================
 * 9P File Modes (Permissions)
 *===========================================================================*/

#define P9_DMDIR    0x80000000
#define P9_DMAPPEND 0x40000000
#define P9_DMEXCL   0x20000000
#define P9_DMAUTH   0x08000000
#define P9_DMTMP    0x04000000

/* Permission bits */
#define P9_DMREAD   0x4
#define P9_DMWRITE  0x2
#define P9_DMEXEC   0x1

/*===========================================================================
 * 9P QID (Unique File Identifier)
 *===========================================================================*/

typedef struct p9_qid {
    uint8_t type;       /* Type of file (directory, etc.) */
    uint32_t version;   /* Version number for cache coherence */
    uint64_t path;      /* Unique path identifier */
} p9_qid_t;

/* QID types */
#define P9_QTDIR    0x80
#define P9_QTAPPEND 0x40
#define P9_QTEXCL   0x20
#define P9_QTAUTH   0x08
#define P9_QTTMP    0x04
#define P9_QTFILE   0x00

/*===========================================================================
 * 9P Stat Structure
 *===========================================================================*/

typedef struct p9_stat {
    uint16_t size;      /* Total size of stat structure */
    uint16_t type;      /* Server type */
    uint32_t dev;       /* Server subtype */
    p9_qid_t qid;       /* Unique identifier */
    uint32_t mode;      /* Permissions and flags */
    uint32_t atime;     /* Last access time */
    uint32_t mtime;     /* Last modification time */
    uint64_t length;    /* File length */
    char* name;         /* File name */
    char* uid;          /* Owner name */
    char* gid;          /* Group name */
    char* muid;         /* Last modifier name */
} p9_stat_t;

/*===========================================================================
 * 9P Connection
 *===========================================================================*/

typedef struct p9_conn* p9_conn_t;

/* Connection callbacks */
typedef struct p9_callbacks {
    cog_result_t (*on_attach)(p9_conn_t conn, uint32_t fid, const char* uname, const char* aname, void* user_data);
    cog_result_t (*on_walk)(p9_conn_t conn, uint32_t fid, uint32_t newfid, char** wnames, size_t nwnames, void* user_data);
    cog_result_t (*on_open)(p9_conn_t conn, uint32_t fid, uint8_t mode, void* user_data);
    cog_result_t (*on_create)(p9_conn_t conn, uint32_t fid, const char* name, uint32_t perm, uint8_t mode, void* user_data);
    cog_result_t (*on_read)(p9_conn_t conn, uint32_t fid, uint64_t offset, uint32_t count, void* user_data);
    cog_result_t (*on_write)(p9_conn_t conn, uint32_t fid, uint64_t offset, const void* data, uint32_t count, void* user_data);
    cog_result_t (*on_clunk)(p9_conn_t conn, uint32_t fid, void* user_data);
    cog_result_t (*on_remove)(p9_conn_t conn, uint32_t fid, void* user_data);
    cog_result_t (*on_stat)(p9_conn_t conn, uint32_t fid, void* user_data);
    cog_result_t (*on_wstat)(p9_conn_t conn, uint32_t fid, p9_stat_t* stat, void* user_data);
    void* user_data;
} p9_callbacks_t;

/*===========================================================================
 * 9P Server
 *===========================================================================*/

typedef struct p9_server* p9_server_t;

COGUTIL_API p9_server_t p9_server_create(p9_callbacks_t* callbacks);
COGUTIL_API void        p9_server_destroy(p9_server_t server);
COGUTIL_API cog_result_t p9_server_listen(p9_server_t server, const char* address, uint16_t port);
COGUTIL_API cog_result_t p9_server_listen_unix(p9_server_t server, const char* path);
COGUTIL_API void        p9_server_stop(p9_server_t server);

/* Response helpers */
COGUTIL_API cog_result_t p9_respond_error(p9_conn_t conn, uint16_t tag, const char* error);
COGUTIL_API cog_result_t p9_respond_read(p9_conn_t conn, uint16_t tag, const void* data, uint32_t count);
COGUTIL_API cog_result_t p9_respond_write(p9_conn_t conn, uint16_t tag, uint32_t count);
COGUTIL_API cog_result_t p9_respond_stat(p9_conn_t conn, uint16_t tag, p9_stat_t* stat);
COGUTIL_API cog_result_t p9_respond_walk(p9_conn_t conn, uint16_t tag, p9_qid_t* qids, size_t nqids);
COGUTIL_API cog_result_t p9_respond_open(p9_conn_t conn, uint16_t tag, p9_qid_t* qid, uint32_t iounit);
COGUTIL_API cog_result_t p9_respond_create(p9_conn_t conn, uint16_t tag, p9_qid_t* qid, uint32_t iounit);

/*===========================================================================
 * 9P Client
 *===========================================================================*/

typedef struct p9_client* p9_client_t;

COGUTIL_API p9_client_t p9_client_create(void);
COGUTIL_API void        p9_client_destroy(p9_client_t client);
COGUTIL_API cog_result_t p9_client_connect(p9_client_t client, const char* address, uint16_t port);
COGUTIL_API cog_result_t p9_client_connect_unix(p9_client_t client, const char* path);
COGUTIL_API void        p9_client_disconnect(p9_client_t client);

/* File operations */
COGUTIL_API cog_result_t p9_attach(p9_client_t client, const char* uname, const char* aname, uint32_t* fid);
COGUTIL_API cog_result_t p9_walk(p9_client_t client, uint32_t fid, const char* path, uint32_t* newfid);
COGUTIL_API cog_result_t p9_open(p9_client_t client, uint32_t fid, uint8_t mode);
COGUTIL_API cog_result_t p9_create(p9_client_t client, uint32_t fid, const char* name, uint32_t perm, uint8_t mode);
COGUTIL_API cog_result_t p9_read(p9_client_t client, uint32_t fid, uint64_t offset, void* buf, uint32_t count, uint32_t* nread);
COGUTIL_API cog_result_t p9_write(p9_client_t client, uint32_t fid, uint64_t offset, const void* data, uint32_t count, uint32_t* nwritten);
COGUTIL_API cog_result_t p9_clunk(p9_client_t client, uint32_t fid);
COGUTIL_API cog_result_t p9_remove(p9_client_t client, uint32_t fid);
COGUTIL_API cog_result_t p9_stat(p9_client_t client, uint32_t fid, p9_stat_t* stat);
COGUTIL_API cog_result_t p9_wstat(p9_client_t client, uint32_t fid, p9_stat_t* stat);

/*===========================================================================
 * 9P File Handle (High-Level API)
 *===========================================================================*/

typedef struct p9_file* p9_file_t;

COGUTIL_API p9_file_t   p9_fopen(p9_client_t client, const char* path, uint8_t mode);
COGUTIL_API void        p9_fclose(p9_file_t file);
COGUTIL_API ssize_t     p9_fread(p9_file_t file, void* buf, size_t count);
COGUTIL_API ssize_t     p9_fwrite(p9_file_t file, const void* data, size_t count);
COGUTIL_API cog_result_t p9_fseek(p9_file_t file, int64_t offset, int whence);
COGUTIL_API int64_t     p9_ftell(p9_file_t file);
COGUTIL_API cog_result_t p9_fstat(p9_file_t file, p9_stat_t* stat);

/*===========================================================================
 * 9P Directory Operations
 *===========================================================================*/

typedef struct p9_dir* p9_dir_t;

COGUTIL_API p9_dir_t    p9_opendir(p9_client_t client, const char* path);
COGUTIL_API void        p9_closedir(p9_dir_t dir);
COGUTIL_API p9_stat_t*  p9_readdir(p9_dir_t dir);
COGUTIL_API void        p9_rewinddir(p9_dir_t dir);

/*===========================================================================
 * 9P Namespace Operations
 *===========================================================================*/

typedef struct p9_namespace* p9_namespace_t;

COGUTIL_API p9_namespace_t p9_namespace_create(void);
COGUTIL_API void           p9_namespace_destroy(p9_namespace_t ns);
COGUTIL_API cog_result_t   p9_namespace_bind(p9_namespace_t ns, const char* name, const char* old, int flags);
COGUTIL_API cog_result_t   p9_namespace_mount(p9_namespace_t ns, p9_client_t client, const char* mountpoint);
COGUTIL_API cog_result_t   p9_namespace_unmount(p9_namespace_t ns, const char* mountpoint);

/* Bind flags */
#define P9_MREPL    0x0000  /* Replace */
#define P9_MBEFORE  0x0001  /* Mount before */
#define P9_MAFTER   0x0002  /* Mount after */
#define P9_MCREATE  0x0004  /* Create if doesn't exist */

/*===========================================================================
 * AtomSpace 9P File System
 *===========================================================================*/

/* Export AtomSpace as 9P server */
typedef struct atomspace_9p_server* atomspace_9p_server_t;

COGUTIL_API atomspace_9p_server_t atomspace_9p_create(void* atomspace);
COGUTIL_API void                  atomspace_9p_destroy(atomspace_9p_server_t server);
COGUTIL_API cog_result_t          atomspace_9p_listen(atomspace_9p_server_t server, const char* address, uint16_t port);
COGUTIL_API void                  atomspace_9p_stop(atomspace_9p_server_t server);

/*
 * AtomSpace 9P File System Layout:
 * 
 * /atoms/
 *   ├── by-type/
 *   │   ├── ConceptNode/
 *   │   │   ├── atom_name_1
 *   │   │   └── atom_name_2
 *   │   ├── PredicateNode/
 *   │   └── ...
 *   ├── by-handle/
 *   │   ├── 0x0001
 *   │   ├── 0x0002
 *   │   └── ...
 *   └── ctl              (control file)
 * 
 * /links/
 *   ├── InheritanceLink/
 *   ├── SimilarityLink/
 *   └── ...
 * 
 * /query                 (write query, read results)
 * /stats                 (read-only statistics)
 */

/*===========================================================================
 * Utility Functions
 *===========================================================================*/

COGUTIL_API void p9_stat_free(p9_stat_t* stat);
COGUTIL_API p9_stat_t* p9_stat_dup(p9_stat_t* stat);
COGUTIL_API void p9_qid_to_string(p9_qid_t* qid, char* buf, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* _COGWXP_9P_H_ */
