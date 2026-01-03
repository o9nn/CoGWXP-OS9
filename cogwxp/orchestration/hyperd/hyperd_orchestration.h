/**
 * @file hyperd_orchestration.h
 * @brief Hyperd Container Orchestration
 *
 * Provides container orchestration using Hyperd/HyperContainer
 * for running cognitive workloads in isolated VM-based containers.
 *
 * Based on: https://github.com/hyperhq/hyperd
 *
 * @copyright CoGWXP-OS9 Project
 */

#ifndef COGWXP_HYPERD_ORCHESTRATION_H
#define COGWXP_HYPERD_ORCHESTRATION_H

#include "../orchestration.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Types and Structures
 *===========================================================================*/

/* Container runtime types */
typedef enum {
    HYPERD_RUNTIME_RUNV,      /* HyperContainer VM-based */
    HYPERD_RUNTIME_RUNC,      /* Standard OCI runtime */
    HYPERD_RUNTIME_KATA,      /* Kata Containers */
    HYPERD_RUNTIME_GVISOR,    /* gVisor sandbox */
    HYPERD_RUNTIME_FIRECRACKER /* Firecracker microVM */
} hyperd_runtime_t;

/* Container state */
typedef enum {
    HYPERD_STATE_CREATED,
    HYPERD_STATE_RUNNING,
    HYPERD_STATE_PAUSED,
    HYPERD_STATE_STOPPED,
    HYPERD_STATE_FAILED
} hyperd_state_t;

/* Resource limits */
typedef struct {
    uint64_t memory_mb;
    uint32_t cpu_shares;
    uint32_t cpu_count;
    uint64_t disk_mb;
    uint32_t network_bandwidth_mbps;
} hyperd_resources_t;

/* Volume mount */
typedef struct {
    const char* host_path;
    const char* container_path;
    bool read_only;
} hyperd_volume_t;

/* Port mapping */
typedef struct {
    uint16_t host_port;
    uint16_t container_port;
    const char* protocol;  /* "tcp" or "udp" */
} hyperd_port_t;

/* Environment variable */
typedef struct {
    const char* name;
    const char* value;
} hyperd_env_t;

/* Container configuration */
typedef struct {
    const char* name;
    const char* image;
    const char** command;
    size_t command_count;

    hyperd_runtime_t runtime;
    hyperd_resources_t resources;

    hyperd_volume_t* volumes;
    size_t volume_count;

    hyperd_port_t* ports;
    size_t port_count;

    hyperd_env_t* environment;
    size_t env_count;

    const char* network;
    const char* hostname;

    bool privileged;
    bool auto_remove;
    uint32_t restart_policy;  /* 0=never, 1=on-failure, 2=always */
} hyperd_container_config_t;

/* Container info */
typedef struct {
    const char* id;
    const char* name;
    const char* image;
    hyperd_state_t state;
    int64_t created_at;
    int64_t started_at;
    uint64_t pid;
    const char* ip_address;
} hyperd_container_info_t;

/* Pod configuration (group of containers) */
typedef struct {
    const char* name;
    hyperd_container_config_t* containers;
    size_t container_count;
    const char* network_mode;
    bool share_network;
    bool share_pid;
} hyperd_pod_config_t;

/* Pod info */
typedef struct {
    const char* id;
    const char* name;
    hyperd_state_t state;
    hyperd_container_info_t* containers;
    size_t container_count;
} hyperd_pod_info_t;

/* Cognitive workload definition */
typedef struct {
    const char* name;
    const char* type;  /* "atomspace", "pln", "moses", "relex", etc. */

    /* Container config */
    const char* image;
    hyperd_resources_t resources;

    /* OpenCog-specific config */
    const char* atomspace_config;
    const char* cogserver_port;
    bool enable_rest_api;

    /* Scaling */
    uint32_t replicas;
    bool auto_scale;
    float cpu_threshold;
} hyperd_cognitive_workload_t;

/* Configuration */
typedef struct {
    const char* socket_path;
    const char* data_dir;
    hyperd_runtime_t default_runtime;

    /* Network */
    const char* bridge_name;
    const char* subnet;

    /* Registry */
    const char* registry_url;
    const char* registry_user;
    const char* registry_password;

    /* Resource defaults */
    hyperd_resources_t default_resources;

    /* AtomSpace */
    atomspace_t atomspace;
} hyperd_config_t;

/* Statistics */
typedef struct {
    uint32_t running_containers;
    uint32_t total_containers;
    uint64_t memory_used_mb;
    uint64_t memory_total_mb;
    float cpu_usage_percent;
    uint64_t network_rx_bytes;
    uint64_t network_tx_bytes;
} hyperd_stats_t;

/* Context handle */
typedef struct hyperd_context* hyperd_context_t;

/*===========================================================================
 * Lifecycle
 *===========================================================================*/

/**
 * Initialize Hyperd context
 */
COGUTIL_API cog_result_t hyperd_init(
    const hyperd_config_t* config,
    hyperd_context_t* ctx
);

/**
 * Shutdown Hyperd context
 */
COGUTIL_API void hyperd_shutdown(hyperd_context_t ctx);

/*===========================================================================
 * Container Operations
 *===========================================================================*/

/**
 * Create a container
 */
COGUTIL_API cog_result_t hyperd_create_container(
    hyperd_context_t ctx,
    const hyperd_container_config_t* config,
    char** container_id
);

/**
 * Start a container
 */
COGUTIL_API cog_result_t hyperd_start_container(
    hyperd_context_t ctx,
    const char* container_id
);

/**
 * Stop a container
 */
COGUTIL_API cog_result_t hyperd_stop_container(
    hyperd_context_t ctx,
    const char* container_id,
    uint32_t timeout_seconds
);

/**
 * Remove a container
 */
COGUTIL_API cog_result_t hyperd_remove_container(
    hyperd_context_t ctx,
    const char* container_id,
    bool force
);

/**
 * Get container info
 */
COGUTIL_API cog_result_t hyperd_get_container(
    hyperd_context_t ctx,
    const char* container_id,
    hyperd_container_info_t* info
);

/**
 * List containers
 */
COGUTIL_API cog_result_t hyperd_list_containers(
    hyperd_context_t ctx,
    bool all,
    hyperd_container_info_t** containers,
    size_t* count
);

/**
 * Execute command in container
 */
COGUTIL_API cog_result_t hyperd_exec(
    hyperd_context_t ctx,
    const char* container_id,
    const char** command,
    size_t command_count,
    char** output
);

/**
 * Get container logs
 */
COGUTIL_API cog_result_t hyperd_logs(
    hyperd_context_t ctx,
    const char* container_id,
    bool follow,
    uint32_t tail_lines,
    char** logs
);

/*===========================================================================
 * Pod Operations
 *===========================================================================*/

/**
 * Create a pod
 */
COGUTIL_API cog_result_t hyperd_create_pod(
    hyperd_context_t ctx,
    const hyperd_pod_config_t* config,
    char** pod_id
);

/**
 * Start a pod
 */
COGUTIL_API cog_result_t hyperd_start_pod(
    hyperd_context_t ctx,
    const char* pod_id
);

/**
 * Stop a pod
 */
COGUTIL_API cog_result_t hyperd_stop_pod(
    hyperd_context_t ctx,
    const char* pod_id
);

/**
 * Get pod info
 */
COGUTIL_API cog_result_t hyperd_get_pod(
    hyperd_context_t ctx,
    const char* pod_id,
    hyperd_pod_info_t* info
);

/*===========================================================================
 * Cognitive Workload Operations
 *===========================================================================*/

/**
 * Deploy a cognitive workload
 */
COGUTIL_API cog_result_t hyperd_deploy_cognitive(
    hyperd_context_t ctx,
    const hyperd_cognitive_workload_t* workload,
    char** deployment_id
);

/**
 * Scale a cognitive workload
 */
COGUTIL_API cog_result_t hyperd_scale_cognitive(
    hyperd_context_t ctx,
    const char* deployment_id,
    uint32_t replicas
);

/**
 * Connect AtomSpace to cognitive container
 */
COGUTIL_API cog_result_t hyperd_connect_atomspace(
    hyperd_context_t ctx,
    const char* container_id,
    atomspace_t atomspace
);

/**
 * Run PLN inference in container
 */
COGUTIL_API cog_result_t hyperd_run_pln(
    hyperd_context_t ctx,
    const char* container_id,
    const char* query,
    char** result
);

/*===========================================================================
 * Image Operations
 *===========================================================================*/

/**
 * Pull an image
 */
COGUTIL_API cog_result_t hyperd_pull_image(
    hyperd_context_t ctx,
    const char* image_name
);

/**
 * List images
 */
COGUTIL_API cog_result_t hyperd_list_images(
    hyperd_context_t ctx,
    char*** images,
    size_t* count
);

/*===========================================================================
 * Statistics
 *===========================================================================*/

/**
 * Get overall statistics
 */
COGUTIL_API cog_result_t hyperd_get_stats(
    hyperd_context_t ctx,
    hyperd_stats_t* stats
);

/**
 * Get container statistics
 */
COGUTIL_API cog_result_t hyperd_get_container_stats(
    hyperd_context_t ctx,
    const char* container_id,
    hyperd_stats_t* stats
);

#ifdef __cplusplus
}
#endif

#endif /* COGWXP_HYPERD_ORCHESTRATION_H */
