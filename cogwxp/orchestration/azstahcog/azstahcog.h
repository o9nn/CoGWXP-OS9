/**
 * @file azstahcog.h
 * @brief Azure Stack HCI Cognitive Architecture Integration
 *
 * Provides deployment and management of cognitive workloads
 * on Azure Stack HCI infrastructure with OpenCog integration.
 *
 * Based on: https://github.com/o9nn/AzStaHCog
 *
 * @copyright CoGWXP-OS9 Project
 */

#ifndef COGWXP_AZSTAHCOG_H
#define COGWXP_AZSTAHCOG_H

#include "../orchestration.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Types and Structures
 *===========================================================================*/

/* Cluster node info */
typedef struct {
    const char* name;
    const char* ip_address;
    const char* status;
    uint32_t cpu_cores;
    uint64_t memory_gb;
    uint64_t storage_tb;
    float cpu_usage;
    float memory_usage;
} azstahcog_node_t;

/* VM size */
typedef enum {
    AZSTAHCOG_VM_SMALL,      /* 2 cores, 4GB RAM */
    AZSTAHCOG_VM_MEDIUM,     /* 4 cores, 16GB RAM */
    AZSTAHCOG_VM_LARGE,      /* 8 cores, 32GB RAM */
    AZSTAHCOG_VM_XLARGE,     /* 16 cores, 64GB RAM */
    AZSTAHCOG_VM_GPU_SMALL,  /* 4 cores, 16GB RAM, 1 GPU */
    AZSTAHCOG_VM_GPU_LARGE   /* 8 cores, 64GB RAM, 2 GPU */
} azstahcog_vm_size_t;

/* VM info */
typedef struct {
    const char* id;
    const char* name;
    const char* status;
    azstahcog_vm_size_t size;
    const char* ip_address;
    const char* os_type;
    uint32_t cpu_cores;
    uint64_t memory_gb;
    int64_t created_at;
} azstahcog_vm_t;

/* Cognitive service types */
typedef enum {
    AZSTAHCOG_SERVICE_ATOMSPACE,
    AZSTAHCOG_SERVICE_COGSERVER,
    AZSTAHCOG_SERVICE_PLN,
    AZSTAHCOG_SERVICE_MOSES,
    AZSTAHCOG_SERVICE_RELEX,
    AZSTAHCOG_SERVICE_LINK_GRAMMAR,
    AZSTAHCOG_SERVICE_GHOST,
    AZSTAHCOG_SERVICE_OPENCOG_UNITY,
    AZSTAHCOG_SERVICE_CUSTOM
} azstahcog_service_type_t;

/* Cognitive service */
typedef struct {
    const char* id;
    const char* name;
    azstahcog_service_type_t type;
    const char* status;
    const char* endpoint;
    uint32_t replicas;
    const char* version;
} azstahcog_service_t;

/* Cognitive cluster */
typedef struct {
    const char* name;
    azstahcog_service_t* services;
    size_t service_count;
    const char* atomspace_endpoint;
    const char* cogserver_endpoint;
} azstahcog_cognitive_cluster_t;

/* Storage volume */
typedef struct {
    const char* name;
    uint64_t size_gb;
    const char* type;  /* "SSD", "HDD", "NVMe" */
    bool replicated;
    uint32_t replica_count;
} azstahcog_volume_t;

/* Network configuration */
typedef struct {
    const char* name;
    const char* subnet;
    const char* gateway;
    bool enable_sdn;
    bool enable_vxlan;
} azstahcog_network_t;

/* Deployment configuration */
typedef struct {
    const char* name;
    azstahcog_vm_size_t vm_size;
    uint32_t replica_count;

    /* Cognitive services to deploy */
    azstahcog_service_type_t* services;
    size_t service_count;

    /* Storage */
    azstahcog_volume_t* volumes;
    size_t volume_count;

    /* Networking */
    const char* network_name;
    bool public_ip;

    /* High availability */
    bool enable_ha;
    bool enable_backup;
} azstahcog_deployment_config_t;

/* Configuration */
typedef struct {
    /* Azure Stack HCI connection */
    const char* cluster_name;
    const char* management_url;
    const char* tenant_id;
    const char* client_id;
    const char* client_secret;

    /* Default settings */
    azstahcog_vm_size_t default_vm_size;
    const char* default_network;
    const char* storage_pool;

    /* AtomSpace */
    atomspace_t atomspace;
} azstahcog_config_t;

/* Statistics */
typedef struct {
    uint32_t total_nodes;
    uint32_t active_nodes;
    uint32_t total_vms;
    uint32_t running_vms;
    uint64_t total_storage_tb;
    uint64_t used_storage_tb;
    float cluster_cpu_usage;
    float cluster_memory_usage;
} azstahcog_stats_t;

/* Context handle */
typedef struct azstahcog_context* azstahcog_context_t;

/*===========================================================================
 * Lifecycle
 *===========================================================================*/

COGUTIL_API cog_result_t azstahcog_init(
    const azstahcog_config_t* config,
    azstahcog_context_t* ctx
);

COGUTIL_API void azstahcog_shutdown(azstahcog_context_t ctx);

/*===========================================================================
 * Cluster Management
 *===========================================================================*/

/**
 * Get cluster nodes
 */
COGUTIL_API cog_result_t azstahcog_list_nodes(
    azstahcog_context_t ctx,
    azstahcog_node_t** nodes,
    size_t* count
);

/**
 * Get cluster health
 */
COGUTIL_API cog_result_t azstahcog_get_health(
    azstahcog_context_t ctx,
    char** health_json
);

/*===========================================================================
 * VM Management
 *===========================================================================*/

/**
 * Create VM
 */
COGUTIL_API cog_result_t azstahcog_create_vm(
    azstahcog_context_t ctx,
    const char* name,
    azstahcog_vm_size_t size,
    const char* image,
    char** vm_id
);

/**
 * Start VM
 */
COGUTIL_API cog_result_t azstahcog_start_vm(
    azstahcog_context_t ctx,
    const char* vm_id
);

/**
 * Stop VM
 */
COGUTIL_API cog_result_t azstahcog_stop_vm(
    azstahcog_context_t ctx,
    const char* vm_id
);

/**
 * Delete VM
 */
COGUTIL_API cog_result_t azstahcog_delete_vm(
    azstahcog_context_t ctx,
    const char* vm_id
);

/**
 * List VMs
 */
COGUTIL_API cog_result_t azstahcog_list_vms(
    azstahcog_context_t ctx,
    azstahcog_vm_t** vms,
    size_t* count
);

/*===========================================================================
 * Cognitive Cluster Management
 *===========================================================================*/

/**
 * Deploy cognitive cluster
 */
COGUTIL_API cog_result_t azstahcog_deploy_cognitive_cluster(
    azstahcog_context_t ctx,
    const azstahcog_deployment_config_t* config,
    char** cluster_id
);

/**
 * Get cognitive cluster
 */
COGUTIL_API cog_result_t azstahcog_get_cognitive_cluster(
    azstahcog_context_t ctx,
    const char* cluster_id,
    azstahcog_cognitive_cluster_t* cluster
);

/**
 * Scale cognitive service
 */
COGUTIL_API cog_result_t azstahcog_scale_service(
    azstahcog_context_t ctx,
    const char* cluster_id,
    azstahcog_service_type_t service,
    uint32_t replicas
);

/**
 * Connect to AtomSpace
 */
COGUTIL_API cog_result_t azstahcog_connect_atomspace(
    azstahcog_context_t ctx,
    const char* cluster_id,
    atomspace_t* atomspace
);

/**
 * Run PLN inference
 */
COGUTIL_API cog_result_t azstahcog_run_pln(
    azstahcog_context_t ctx,
    const char* cluster_id,
    const char* query,
    char** result
);

/**
 * Run MOSES learning
 */
COGUTIL_API cog_result_t azstahcog_run_moses(
    azstahcog_context_t ctx,
    const char* cluster_id,
    const char* problem,
    const char* parameters,
    char** result
);

/*===========================================================================
 * Storage Management
 *===========================================================================*/

/**
 * Create volume
 */
COGUTIL_API cog_result_t azstahcog_create_volume(
    azstahcog_context_t ctx,
    const azstahcog_volume_t* config,
    char** volume_id
);

/**
 * List volumes
 */
COGUTIL_API cog_result_t azstahcog_list_volumes(
    azstahcog_context_t ctx,
    azstahcog_volume_t** volumes,
    size_t* count
);

/*===========================================================================
 * Network Management
 *===========================================================================*/

/**
 * Create network
 */
COGUTIL_API cog_result_t azstahcog_create_network(
    azstahcog_context_t ctx,
    const azstahcog_network_t* config,
    char** network_id
);

/**
 * List networks
 */
COGUTIL_API cog_result_t azstahcog_list_networks(
    azstahcog_context_t ctx,
    azstahcog_network_t** networks,
    size_t* count
);

/*===========================================================================
 * Statistics
 *===========================================================================*/

COGUTIL_API cog_result_t azstahcog_get_stats(
    azstahcog_context_t ctx,
    azstahcog_stats_t* stats
);

#ifdef __cplusplus
}
#endif

#endif /* COGWXP_AZSTAHCOG_H */
