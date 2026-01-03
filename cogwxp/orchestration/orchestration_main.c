/**
 * @file orchestration_main.c
 * @brief Unified AGI Orchestration Main Entry Point and CLI
 * 
 * Provides unified initialization, management, and CLI interface for all
 * AGI toolchain orchestration components.
 * 
 * @copyright CoGWXP-OS9 Project
 */

#include "orchestration.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>

/*===========================================================================
 * Global State
 *===========================================================================*/

static struct {
    cogwxp_orchestration_t* orchestration;
    bool running;
    pthread_mutex_t lock;
} g_state = {
    .orchestration = NULL,
    .running = false,
    .lock = PTHREAD_MUTEX_INITIALIZER
};

/*===========================================================================
 * Signal Handling
 *===========================================================================*/

static void signal_handler(int sig) {
    (void)sig;
    pthread_mutex_lock(&g_state.lock);
    g_state.running = false;
    pthread_mutex_unlock(&g_state.lock);
}

static void setup_signal_handlers(void) {
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

/*===========================================================================
 * Unified Orchestration Context
 *===========================================================================*/

struct cogwxp_orchestration {
    /* Component contexts */
    msgraph_context_t msgraph;
    mshypergraph_fs_t mshypergraph_fs;
    cogpilot_context_t cogpilot;
    osdeploy_context_t osdeploy;
    azurite_context_t azurite;
    torch7u_context_t torch7u;
    dynav_context_t dynav;
    hypermind_context_t hypermind;
    
    /* AtomSpace */
    atomspace_t atomspace;
    
    /* Configuration */
    cogwxp_orchestration_config_t config;
    
    /* State */
    bool initialized;
    pthread_mutex_t lock;
};

/*===========================================================================
 * Orchestration Lifecycle
 *===========================================================================*/

COGUTIL_API cog_result_t cogwxp_orchestration_init(
    const cogwxp_orchestration_config_t* config,
    cogwxp_orchestration_t** orchestration
) {
    if (!config || !orchestration) return COG_ERROR_INVALID_PARAM;
    
    cogwxp_orchestration_t* o = calloc(1, sizeof(cogwxp_orchestration_t));
    if (!o) return COG_ERROR_MEMORY;
    
    pthread_mutex_init(&o->lock, NULL);
    memcpy(&o->config, config, sizeof(cogwxp_orchestration_config_t));
    
    cog_result_t result = COG_OK;
    
    printf("Initializing CoGWXP-OS9 Orchestration Framework...\n");
    
    /* Initialize AtomSpace */
    printf("  [1/8] Initializing AtomSpace...\n");
    /* Would initialize AtomSpace here */
    o->atomspace = (atomspace_t)1; /* Placeholder */
    
    /* Initialize MSHyperGraph */
    if (config->enable_msgraph) {
        printf("  [2/8] Initializing MSHyperGraph...\n");
        msgraph_config_t msgraph_config = {
            .tenant_id = config->azure_tenant_id,
            .client_id = config->azure_client_id,
            .client_secret = config->azure_client_secret,
            .enable_caching = true,
            .cache_ttl_seconds = 300
        };
        result = msgraph_init(&msgraph_config, o->atomspace, &o->msgraph);
        if (result != COG_OK) {
            printf("    Warning: MSHyperGraph initialization failed\n");
        }
    }
    
    /* Initialize MSHyperGraph Filesystem */
    if (config->enable_msgraph_fs) {
        printf("  [3/8] Initializing MSHyperGraph Filesystem...\n");
        mshypergraph_fs_config_t fs_config = {
            .mount_point = "/mshypergraph",
            .port = 564,
            .max_message_size = 65536
        };
        result = mshypergraph_fs_create(&fs_config, &o->mshypergraph_fs);
        if (result == COG_OK) {
            mshypergraph_fs_start(o->mshypergraph_fs);
        }
    }
    
    /* Initialize CogPilot */
    if (config->enable_cogpilot) {
        printf("  [4/8] Initializing CogPilot...\n");
        cogpilot_config_t cogpilot_config = {
            .azure_endpoint = config->azure_cognitive_endpoint,
            .azure_api_key = config->azure_cognitive_key,
            .openai_endpoint = config->openai_endpoint,
            .openai_api_key = config->openai_api_key,
            .max_concurrent_tasks = 8
        };
        result = cogpilot_init(&cogpilot_config, &o->cogpilot);
        if (result != COG_OK) {
            printf("    Warning: CogPilot initialization failed\n");
        }
    }
    
    /* Initialize OSDeploy */
    if (config->enable_osdeploy) {
        printf("  [5/8] Initializing OSDeploy...\n");
        osdeploy_config_t osdeploy_config = {
            .staging_path = "/tmp/osdeploy",
            .azure_subscription_id = config->azure_subscription_id,
            .azure_resource_group = config->azure_resource_group
        };
        result = osdeploy_init(&osdeploy_config, &o->osdeploy);
        if (result != COG_OK) {
            printf("    Warning: OSDeploy initialization failed\n");
        }
    }
    
    /* Initialize Azurite */
    if (config->enable_azurite) {
        printf("  [6/8] Initializing Azurite Cognitive Architecture...\n");
        azurite_config_t azurite_config = {
            .llm_endpoint = config->openai_endpoint,
            .llm_api_key = config->openai_api_key,
            .embedding_dim = 384,
            .recency_weight = 1.0f,
            .importance_weight = 1.0f,
            .relevance_weight = 1.0f
        };
        result = azurite_init(&azurite_config, &o->azurite);
        if (result != COG_OK) {
            printf("    Warning: Azurite initialization failed\n");
        }
    }
    
    /* Initialize Torch7u */
    if (config->enable_torch7u) {
        printf("  [7/8] Initializing Torch7u Neural Network Framework...\n");
        torch7u_config_t torch7u_config = {
            .seed = 42,
            .use_cuda = false
        };
        result = torch7u_init(&torch7u_config, &o->torch7u);
        if (result != COG_OK) {
            printf("    Warning: Torch7u initialization failed\n");
        }
    }
    
    /* Initialize DynaV and HyperMind */
    printf("  [8/8] Initializing Visualization and Reasoning...\n");
    
    dynav_config_t dynav_config = {0};
    dynav_init(&dynav_config, &o->dynav);
    
    hypermind_config_t hypermind_config = {
        .embedding_dim = 128
    };
    hypermind_init(&hypermind_config, &o->hypermind);
    
    o->initialized = true;
    *orchestration = o;
    
    printf("CoGWXP-OS9 Orchestration Framework initialized successfully.\n\n");
    
    return COG_OK;
}

COGUTIL_API void cogwxp_orchestration_shutdown(cogwxp_orchestration_t* orchestration) {
    if (!orchestration) return;
    
    printf("Shutting down CoGWXP-OS9 Orchestration Framework...\n");
    
    pthread_mutex_lock(&orchestration->lock);
    
    if (orchestration->mshypergraph_fs) {
        mshypergraph_fs_stop(orchestration->mshypergraph_fs);
        mshypergraph_fs_destroy(orchestration->mshypergraph_fs);
    }
    
    if (orchestration->msgraph) msgraph_shutdown(orchestration->msgraph);
    if (orchestration->cogpilot) cogpilot_shutdown(orchestration->cogpilot);
    if (orchestration->osdeploy) osdeploy_shutdown(orchestration->osdeploy);
    if (orchestration->azurite) azurite_shutdown(orchestration->azurite);
    if (orchestration->torch7u) torch7u_shutdown(orchestration->torch7u);
    if (orchestration->dynav) dynav_shutdown(orchestration->dynav);
    if (orchestration->hypermind) hypermind_shutdown(orchestration->hypermind);
    
    pthread_mutex_unlock(&orchestration->lock);
    pthread_mutex_destroy(&orchestration->lock);
    
    free(orchestration);
    
    printf("Shutdown complete.\n");
}

/*===========================================================================
 * Component Access
 *===========================================================================*/

COGUTIL_API msgraph_context_t cogwxp_get_msgraph(cogwxp_orchestration_t* o) {
    return o ? o->msgraph : NULL;
}

COGUTIL_API cogpilot_context_t cogwxp_get_cogpilot(cogwxp_orchestration_t* o) {
    return o ? o->cogpilot : NULL;
}

COGUTIL_API osdeploy_context_t cogwxp_get_osdeploy(cogwxp_orchestration_t* o) {
    return o ? o->osdeploy : NULL;
}

COGUTIL_API azurite_context_t cogwxp_get_azurite(cogwxp_orchestration_t* o) {
    return o ? o->azurite : NULL;
}

COGUTIL_API torch7u_context_t cogwxp_get_torch7u(cogwxp_orchestration_t* o) {
    return o ? o->torch7u : NULL;
}

COGUTIL_API dynav_context_t cogwxp_get_dynav(cogwxp_orchestration_t* o) {
    return o ? o->dynav : NULL;
}

COGUTIL_API hypermind_context_t cogwxp_get_hypermind(cogwxp_orchestration_t* o) {
    return o ? o->hypermind : NULL;
}

/*===========================================================================
 * CLI Commands
 *===========================================================================*/

static void print_usage(const char* program) {
    printf("CoGWXP-OS9 Orchestration CLI\n\n");
    printf("Usage: %s [options] <command> [args...]\n\n", program);
    printf("Options:\n");
    printf("  -c, --config <file>    Configuration file path\n");
    printf("  -v, --verbose          Enable verbose output\n");
    printf("  -h, --help             Show this help message\n");
    printf("\n");
    printf("Commands:\n");
    printf("  start                  Start the orchestration framework\n");
    printf("  stop                   Stop the orchestration framework\n");
    printf("  status                 Show status of all components\n");
    printf("  query <hgql>           Execute HyperGraphiQL query\n");
    printf("  agent <subcommand>     Manage cognitive agents\n");
    printf("  deploy <subcommand>    Manage deployments\n");
    printf("  visualize <file>       Generate visualization\n");
    printf("\n");
    printf("Agent Subcommands:\n");
    printf("  agent list             List registered agents\n");
    printf("  agent create <name>    Create new agent\n");
    printf("  agent task <id> <json> Submit task to agent\n");
    printf("\n");
    printf("Deploy Subcommands:\n");
    printf("  deploy create <json>   Create new deployment\n");
    printf("  deploy status <id>     Get deployment status\n");
    printf("  deploy log <id>        Get deployment log\n");
    printf("\n");
}

static void cmd_status(cogwxp_orchestration_t* o) {
    printf("CoGWXP-OS9 Orchestration Status\n");
    printf("================================\n\n");
    
    printf("Components:\n");
    printf("  MSHyperGraph:      %s\n", o->msgraph ? "Active" : "Disabled");
    printf("  MSHyperGraph FS:   %s\n", o->mshypergraph_fs ? "Active" : "Disabled");
    printf("  CogPilot:          %s\n", o->cogpilot ? "Active" : "Disabled");
    printf("  OSDeploy:          %s\n", o->osdeploy ? "Active" : "Disabled");
    printf("  Azurite:           %s\n", o->azurite ? "Active" : "Disabled");
    printf("  Torch7u:           %s\n", o->torch7u ? "Active" : "Disabled");
    printf("  DynaV:             %s\n", o->dynav ? "Active" : "Disabled");
    printf("  HyperMind:         %s\n", o->hypermind ? "Active" : "Disabled");
    printf("\n");
    
    if (o->cogpilot) {
        cogpilot_stats_t stats;
        cogpilot_get_stats(o->cogpilot, &stats);
        printf("CogPilot Statistics:\n");
        printf("  Agents Registered: %lu\n", stats.agents_registered);
        printf("  Tasks Submitted:   %lu\n", stats.tasks_submitted);
        printf("  Tasks Completed:   %lu\n", stats.tasks_completed);
        printf("  Azure API Calls:   %lu\n", stats.azure_calls);
        printf("  OpenAI API Calls:  %lu\n", stats.openai_calls);
        printf("\n");
    }
    
    if (o->osdeploy) {
        osdeploy_stats_t stats;
        osdeploy_get_stats(o->osdeploy, &stats);
        printf("OSDeploy Statistics:\n");
        printf("  Total Deployments:      %lu\n", stats.total_deployments);
        printf("  Successful Deployments: %lu\n", stats.successful_deployments);
        printf("  Failed Deployments:     %lu\n", stats.failed_deployments);
        printf("  Agents Deployed:        %lu\n", stats.agents_deployed);
        printf("\n");
    }
    
    if (o->azurite) {
        azurite_stats_t stats;
        azurite_get_stats(o->azurite, &stats);
        printf("Azurite Statistics:\n");
        printf("  Agents Created: %lu\n", stats.agents_created);
        printf("\n");
    }
}

static void cmd_query(cogwxp_orchestration_t* o, const char* query) {
    if (!o->mshypergraph_fs) {
        printf("Error: MSHyperGraph filesystem not initialized\n");
        return;
    }
    
    char* result = NULL;
    size_t result_size = 0;
    
    cog_result_t res = mshypergraph_fs_query(o->mshypergraph_fs, query, NULL, &result, &result_size);
    
    if (res == COG_OK && result) {
        printf("%s\n", result);
        free(result);
    } else {
        printf("Error: Query execution failed\n");
    }
}

static void cmd_agent_list(cogwxp_orchestration_t* o) {
    if (!o->cogpilot) {
        printf("Error: CogPilot not initialized\n");
        return;
    }
    
    cogpilot_agent_t* agents = NULL;
    size_t count = 0;
    
    cog_result_t res = cogpilot_query_agents(o->cogpilot, "*", &agents, &count);
    
    if (res == COG_OK) {
        printf("Registered Agents (%zu):\n", count);
        printf("%-8s %-20s %-10s %-10s\n", "ID", "Name", "Type", "Status");
        printf("----------------------------------------------------\n");
        
        for (size_t i = 0; i < count; i++) {
            const char* type_str = "Unknown";
            switch (agents[i].type) {
                case COGPILOT_AGENT_REASONING: type_str = "Reasoning"; break;
                case COGPILOT_AGENT_PERCEPTION: type_str = "Perception"; break;
                case COGPILOT_AGENT_ACTION: type_str = "Action"; break;
                case COGPILOT_AGENT_LEARNING: type_str = "Learning"; break;
                case COGPILOT_AGENT_MEMORY: type_str = "Memory"; break;
            }
            
            const char* status_str = "Unknown";
            switch (agents[i].status) {
                case COGPILOT_AGENT_IDLE: status_str = "Idle"; break;
                case COGPILOT_AGENT_BUSY: status_str = "Busy"; break;
                case COGPILOT_AGENT_OFFLINE: status_str = "Offline"; break;
            }
            
            printf("%-8lu %-20s %-10s %-10s\n",
                agents[i].id,
                agents[i].name ? agents[i].name : "(unnamed)",
                type_str,
                status_str);
        }
        
        free(agents);
    } else {
        printf("Error: Failed to list agents\n");
    }
}

static void cmd_visualize(cogwxp_orchestration_t* o, const char* filename) {
    if (!o->dynav) {
        printf("Error: DynaV not initialized\n");
        return;
    }
    
    /* Add sample nodes and edges for demonstration */
    uint64_t node1, node2, node3, node4;
    dynav_add_node(o->dynav, "AtomSpace", 0, 0, 0, &node1);
    dynav_add_node(o->dynav, "MSGraph", 100, 50, 0, &node2);
    dynav_add_node(o->dynav, "CogPilot", -50, 100, 0, &node3);
    dynav_add_node(o->dynav, "Azurite", 50, -100, 0, &node4);
    
    dynav_add_edge(o->dynav, node1, node2, 2.0f);
    dynav_add_edge(o->dynav, node1, node3, 2.0f);
    dynav_add_edge(o->dynav, node1, node4, 2.0f);
    dynav_add_edge(o->dynav, node2, node3, 1.0f);
    
    /* Apply layout */
    dynav_layout_force_directed(o->dynav, 100);
    
    /* Export */
    cog_result_t res = dynav_export_svg(o->dynav, filename);
    
    if (res == COG_OK) {
        printf("Visualization exported to: %s\n", filename);
    } else {
        printf("Error: Failed to export visualization\n");
    }
}

/*===========================================================================
 * Main Entry Point
 *===========================================================================*/

int main(int argc, char* argv[]) {
    static struct option long_options[] = {
        {"config",  required_argument, 0, 'c'},
        {"verbose", no_argument,       0, 'v'},
        {"help",    no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };
    
    const char* config_file = NULL;
    bool verbose = false;
    
    int opt;
    while ((opt = getopt_long(argc, argv, "c:vh", long_options, NULL)) != -1) {
        switch (opt) {
            case 'c':
                config_file = optarg;
                break;
            case 'v':
                verbose = true;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    if (optind >= argc) {
        print_usage(argv[0]);
        return 1;
    }
    
    const char* command = argv[optind];
    
    /* Setup signal handlers */
    setup_signal_handlers();
    
    /* Initialize orchestration */
    cogwxp_orchestration_config_t config = {
        .enable_msgraph = true,
        .enable_msgraph_fs = true,
        .enable_cogpilot = true,
        .enable_osdeploy = true,
        .enable_azurite = true,
        .enable_torch7u = true
    };
    
    /* Load config file if specified */
    if (config_file) {
        /* Would load configuration from file */
        if (verbose) {
            printf("Loading configuration from: %s\n", config_file);
        }
    }
    
    cogwxp_orchestration_t* orchestration = NULL;
    cog_result_t result = cogwxp_orchestration_init(&config, &orchestration);
    
    if (result != COG_OK) {
        fprintf(stderr, "Error: Failed to initialize orchestration framework\n");
        return 1;
    }
    
    g_state.orchestration = orchestration;
    g_state.running = true;
    
    /* Execute command */
    if (strcmp(command, "start") == 0) {
        printf("CoGWXP-OS9 Orchestration Framework running.\n");
        printf("Press Ctrl+C to stop.\n\n");
        
        while (g_state.running) {
            sleep(1);
        }
        
    } else if (strcmp(command, "stop") == 0) {
        printf("Stopping orchestration framework...\n");
        
    } else if (strcmp(command, "status") == 0) {
        cmd_status(orchestration);
        
    } else if (strcmp(command, "query") == 0) {
        if (optind + 1 >= argc) {
            fprintf(stderr, "Error: Query string required\n");
        } else {
            cmd_query(orchestration, argv[optind + 1]);
        }
        
    } else if (strcmp(command, "agent") == 0) {
        if (optind + 1 >= argc) {
            fprintf(stderr, "Error: Agent subcommand required\n");
        } else if (strcmp(argv[optind + 1], "list") == 0) {
            cmd_agent_list(orchestration);
        } else {
            fprintf(stderr, "Error: Unknown agent subcommand: %s\n", argv[optind + 1]);
        }
        
    } else if (strcmp(command, "visualize") == 0) {
        const char* filename = optind + 1 < argc ? argv[optind + 1] : "cogwxp_graph.svg";
        cmd_visualize(orchestration, filename);
        
    } else {
        fprintf(stderr, "Error: Unknown command: %s\n", command);
        print_usage(argv[0]);
    }
    
    /* Cleanup */
    cogwxp_orchestration_shutdown(orchestration);
    
    return 0;
}
