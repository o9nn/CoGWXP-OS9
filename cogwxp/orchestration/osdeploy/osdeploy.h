/**
 * @file osdeploy.h
 * @brief OSDeploy Integration for AGI OS Deployment Orchestration
 * 
 * Integrates OSDeploy toolchain for automated deployment and
 * provisioning of the CoGWXP-OS9 AGI operating system.
 * 
 * Features:
 * - Automated OS deployment (OSDCloud)
 * - Driver pack management
 * - Azure integration for cloud deployments
 * - WinPE/WinRE customization
 * - Cognitive agent deployment
 * 
 * @copyright CoGWXP-OS9 Project
 */

#ifndef _COGWXP_OSDEPLOY_H_
#define _COGWXP_OSDEPLOY_H_

#include "../opencog/atomspace/atomspace.h"
#include "../cogpilot/cogpilot.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * OSDeploy Configuration
 *===========================================================================*/

/**
 * Deployment target types
 */
typedef enum {
    OSDEPLOY_TARGET_PHYSICAL,       /* Physical machine */
    OSDEPLOY_TARGET_VM_HYPERV,      /* Hyper-V VM */
    OSDEPLOY_TARGET_VM_VMWARE,      /* VMware VM */
    OSDEPLOY_TARGET_VM_VIRTUALBOX,  /* VirtualBox VM */
    OSDEPLOY_TARGET_AZURE_VM,       /* Azure VM */
    OSDEPLOY_TARGET_AWS_EC2,        /* AWS EC2 */
    OSDEPLOY_TARGET_CONTAINER,      /* Container (Docker/Podman) */
    OSDEPLOY_TARGET_WSL             /* Windows Subsystem for Linux */
} osdeploy_target_type_t;

/**
 * OS edition types
 */
typedef enum {
    OSDEPLOY_EDITION_COGWXP_CORE,       /* Core cognitive OS */
    OSDEPLOY_EDITION_COGWXP_FULL,       /* Full cognitive OS */
    OSDEPLOY_EDITION_COGWXP_SERVER,     /* Server edition */
    OSDEPLOY_EDITION_COGWXP_EMBEDDED,   /* Embedded/IoT edition */
    OSDEPLOY_EDITION_COGWXP_DEV         /* Developer edition */
} osdeploy_edition_t;

/**
 * Deployment configuration
 */
typedef struct {
    /* Target */
    osdeploy_target_type_t target_type;
    const char* target_name;
    const char* target_address;
    
    /* OS Edition */
    osdeploy_edition_t edition;
    const char* language;
    const char* timezone;
    
    /* Hardware */
    uint32_t cpu_cores;
    uint64_t ram_mb;
    uint64_t disk_gb;
    bool enable_gpu;
    
    /* Network */
    bool use_dhcp;
    const char* static_ip;
    const char* subnet_mask;
    const char* gateway;
    const char* dns_servers;
    
    /* Cognitive components */
    bool install_atomspace;
    bool install_cogserver;
    bool install_pln;
    bool install_msgraph;
    bool install_cogpilot;
    
    /* Azure integration */
    bool azure_join;
    const char* azure_tenant_id;
    const char* azure_subscription_id;
    
    /* Customization */
    const char* custom_script_path;
    const char* driver_pack_path;
    const char* app_pack_path;
    
    /* AtomSpace for deployment state */
    atomspace_t state_atomspace;
} osdeploy_config_t;

/**
 * OSDeploy context
 */
typedef struct osdeploy_context* osdeploy_context_t;

/*===========================================================================
 * OSDeploy Lifecycle
 *===========================================================================*/

/**
 * Initialize OSDeploy
 */
COGUTIL_API cog_result_t osdeploy_init(
    const osdeploy_config_t* config,
    osdeploy_context_t* ctx
);

/**
 * Shutdown OSDeploy
 */
COGUTIL_API void osdeploy_shutdown(osdeploy_context_t ctx);

/*===========================================================================
 * OSDCloud Integration
 *===========================================================================*/

/**
 * OSDCloud deployment options
 */
typedef struct {
    const char* os_version;         /* e.g., "Windows 11 23H2" */
    const char* os_build;
    const char* os_language;
    const char* os_license_key;
    
    bool skip_autopilot;
    bool skip_oobe;
    bool enable_bitlocker;
    
    /* WinPE customization */
    const char* winpe_drivers;
    const char* winpe_scripts;
    
    /* Post-install */
    const char* post_install_script;
    const char* app_packages;       /* JSON array of app packages */
} osdcloud_options_t;

/**
 * Start OSDCloud deployment
 */
COGUTIL_API cog_result_t osdcloud_deploy(
    osdeploy_context_t ctx,
    const osdcloud_options_t* options
);

/**
 * Create OSDCloud USB media
 */
COGUTIL_API cog_result_t osdcloud_create_usb(
    osdeploy_context_t ctx,
    const char* usb_drive,
    const osdcloud_options_t* options
);

/**
 * Create OSDCloud ISO
 */
COGUTIL_API cog_result_t osdcloud_create_iso(
    osdeploy_context_t ctx,
    const char* output_path,
    const osdcloud_options_t* options
);

/*===========================================================================
 * Driver Pack Management
 *===========================================================================*/

/**
 * Driver pack info
 */
typedef struct {
    const char* name;
    const char* version;
    const char* manufacturer;
    const char* model;
    const char* os_version;
    uint64_t size_bytes;
    const char* download_url;
    const char* hash_sha256;
} driver_pack_t;

/**
 * Get available driver packs
 */
COGUTIL_API cog_result_t osdeploy_get_driver_packs(
    osdeploy_context_t ctx,
    const char* manufacturer,
    const char* model,
    driver_pack_t** packs,
    size_t* count
);

/**
 * Download driver pack
 */
COGUTIL_API cog_result_t osdeploy_download_driver_pack(
    osdeploy_context_t ctx,
    const driver_pack_t* pack,
    const char* destination_path,
    void (*progress_callback)(uint64_t downloaded, uint64_t total, void* user_data),
    void* user_data
);

/**
 * Inject drivers into WIM/ISO
 */
COGUTIL_API cog_result_t osdeploy_inject_drivers(
    osdeploy_context_t ctx,
    const char* image_path,
    const char* driver_path
);

/*===========================================================================
 * Azure Deployment
 *===========================================================================*/

/**
 * Azure VM configuration
 */
typedef struct {
    const char* resource_group;
    const char* location;
    const char* vm_size;
    const char* vnet_name;
    const char* subnet_name;
    
    /* Storage */
    const char* storage_account;
    const char* os_disk_type;       /* "Standard_LRS", "Premium_LRS", etc. */
    uint32_t os_disk_size_gb;
    
    /* Networking */
    bool public_ip;
    const char* nsg_name;
    
    /* Identity */
    bool system_assigned_identity;
    const char** user_assigned_identities;
    size_t identity_count;
    
    /* Tags */
    const char** tag_keys;
    const char** tag_values;
    size_t tag_count;
} azure_vm_config_t;

/**
 * Deploy to Azure VM
 */
COGUTIL_API cog_result_t osdeploy_azure_deploy(
    osdeploy_context_t ctx,
    const azure_vm_config_t* vm_config,
    const osdcloud_options_t* os_options,
    char** vm_id
);

/**
 * Create Azure VM from gallery image
 */
COGUTIL_API cog_result_t osdeploy_azure_create_from_gallery(
    osdeploy_context_t ctx,
    const azure_vm_config_t* vm_config,
    const char* gallery_image_id,
    char** vm_id
);

/**
 * Upload custom VHD to Azure
 */
COGUTIL_API cog_result_t osdeploy_azure_upload_vhd(
    osdeploy_context_t ctx,
    const char* vhd_path,
    const char* storage_account,
    const char* container,
    const char* blob_name,
    void (*progress_callback)(uint64_t uploaded, uint64_t total, void* user_data),
    void* user_data
);

/*===========================================================================
 * Cognitive Agent Deployment
 *===========================================================================*/

/**
 * Cognitive agent package
 */
typedef struct {
    const char* name;
    const char* version;
    uint32_t capabilities;          /* hagent_capability_t flags */
    
    /* Dependencies */
    const char** dependencies;
    size_t dependency_count;
    
    /* Resources */
    uint32_t min_cpu_cores;
    uint64_t min_ram_mb;
    bool requires_gpu;
    
    /* Package location */
    const char* package_path;
    const char* package_hash;
} cognitive_agent_package_t;

/**
 * Deploy cognitive agent
 */
COGUTIL_API cog_result_t osdeploy_deploy_agent(
    osdeploy_context_t ctx,
    const cognitive_agent_package_t* package,
    const char* target_path
);

/**
 * Deploy agent fleet
 */
COGUTIL_API cog_result_t osdeploy_deploy_agent_fleet(
    osdeploy_context_t ctx,
    const cognitive_agent_package_t** packages,
    size_t package_count,
    const char** target_machines,
    size_t machine_count
);

/**
 * Update deployed agent
 */
COGUTIL_API cog_result_t osdeploy_update_agent(
    osdeploy_context_t ctx,
    const char* agent_name,
    const cognitive_agent_package_t* new_package
);

/*===========================================================================
 * Deployment State Management
 *===========================================================================*/

/**
 * Deployment state
 */
typedef enum {
    OSDEPLOY_STATE_PENDING,
    OSDEPLOY_STATE_PREPARING,
    OSDEPLOY_STATE_DEPLOYING,
    OSDEPLOY_STATE_CONFIGURING,
    OSDEPLOY_STATE_INSTALLING_AGENTS,
    OSDEPLOY_STATE_VERIFYING,
    OSDEPLOY_STATE_COMPLETED,
    OSDEPLOY_STATE_FAILED,
    OSDEPLOY_STATE_ROLLING_BACK
} osdeploy_state_t;

/**
 * Deployment status
 */
typedef struct {
    const char* deployment_id;
    osdeploy_state_t state;
    float progress_percent;
    const char* current_step;
    const char* error_message;
    
    /* Timing */
    uint64_t start_time;
    uint64_t end_time;
    uint64_t estimated_remaining_seconds;
    
    /* AtomSpace representation */
    atom_handle_t state_atom;
} osdeploy_status_t;

/**
 * Get deployment status
 */
COGUTIL_API cog_result_t osdeploy_get_status(
    osdeploy_context_t ctx,
    const char* deployment_id,
    osdeploy_status_t* status
);

/**
 * Cancel deployment
 */
COGUTIL_API cog_result_t osdeploy_cancel(
    osdeploy_context_t ctx,
    const char* deployment_id
);

/**
 * Rollback deployment
 */
COGUTIL_API cog_result_t osdeploy_rollback(
    osdeploy_context_t ctx,
    const char* deployment_id
);

/**
 * Subscribe to deployment events
 */
COGUTIL_API cog_result_t osdeploy_subscribe(
    osdeploy_context_t ctx,
    const char* deployment_id,
    void (*callback)(const osdeploy_status_t* status, void* user_data),
    void* user_data,
    uint64_t* subscription_id
);

/**
 * Unsubscribe from deployment events
 */
COGUTIL_API cog_result_t osdeploy_unsubscribe(
    osdeploy_context_t ctx,
    uint64_t subscription_id
);

/*===========================================================================
 * PowerShell Integration
 *===========================================================================*/

/**
 * Execute OSD PowerShell command
 */
COGUTIL_API cog_result_t osdeploy_ps_execute(
    osdeploy_context_t ctx,
    const char* command,
    char** output,
    size_t* output_size
);

/**
 * Execute OSD PowerShell script
 */
COGUTIL_API cog_result_t osdeploy_ps_script(
    osdeploy_context_t ctx,
    const char* script_path,
    const char* parameters_json,
    char** output,
    size_t* output_size
);

/**
 * Import OSD PowerShell module
 */
COGUTIL_API cog_result_t osdeploy_ps_import_module(
    osdeploy_context_t ctx,
    const char* module_name
);

/*===========================================================================
 * Inventory and Discovery
 *===========================================================================*/

/**
 * Hardware inventory
 */
typedef struct {
    const char* manufacturer;
    const char* model;
    const char* serial_number;
    const char* bios_version;
    
    uint32_t cpu_cores;
    uint64_t ram_mb;
    uint64_t disk_gb;
    
    bool has_tpm;
    bool has_secure_boot;
    bool has_uefi;
    
    /* Network adapters */
    const char** mac_addresses;
    size_t mac_count;
    
    /* GPU */
    const char* gpu_name;
    uint64_t gpu_memory_mb;
} hardware_inventory_t;

/**
 * Discover hardware inventory
 */
COGUTIL_API cog_result_t osdeploy_discover_hardware(
    osdeploy_context_t ctx,
    const char* target,
    hardware_inventory_t* inventory
);

/**
 * Store inventory in AtomSpace
 */
COGUTIL_API cog_result_t osdeploy_store_inventory(
    osdeploy_context_t ctx,
    const hardware_inventory_t* inventory,
    atom_handle_t* inventory_atom
);

#ifdef __cplusplus
}
#endif

#endif /* _COGWXP_OSDEPLOY_H_ */
