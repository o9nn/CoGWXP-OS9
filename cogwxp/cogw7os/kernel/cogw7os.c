/**
 * @file cogw7os.c
 * @brief CogW7OS Cognitive Kernel Implementation
 * 
 * Core implementation of the cognitive operating system kernel,
 * integrating OpenCog reasoning with NT-compatible system services.
 * 
 * @copyright CoGWXP-OS9 Project
 */

#define _COGW7_INTERNAL
#include "cogw7os.h"
#include "../../opencog/cogutil/cogutil.h"
#include "../../opencog/atomspace/atomspace.h"
#include "../../opencog/pln/pln.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>

/*===========================================================================
 * Constants
 *===========================================================================*/

#define COGW7_MAX_PROCESSES     1024
#define COGW7_MAX_THREADS       4096
#define COGW7_MAX_HANDLES       65536
#define COGW7_TICK_MS           10
#define COGW7_QUANTUM_MS        20

/*===========================================================================
 * Internal Structures
 *===========================================================================*/

/* Thread state */
typedef enum {
    THREAD_STATE_INIT,
    THREAD_STATE_READY,
    THREAD_STATE_RUNNING,
    THREAD_STATE_WAITING,
    THREAD_STATE_TERMINATED
} thread_state_t;

/* Thread structure */
typedef struct cogw7_thread {
    uint32_t tid;
    uint32_t pid;
    thread_state_t state;
    int priority;
    
    /* Cognitive context */
    atom_handle_t context_atom;
    double attention_level;
    
    /* Execution state */
    void* stack;
    size_t stack_size;
    void* entry_point;
    void* arg;
    
    /* Scheduling */
    uint64_t cpu_time;
    uint64_t wait_time;
    uint64_t last_run;
    
    /* Synchronization */
    pthread_t native_thread;
    pthread_mutex_t lock;
    pthread_cond_t wait_cond;
    
    struct cogw7_thread* next;
} cogw7_thread_t;

/* Process structure */
typedef struct cogw7_process {
    uint32_t pid;
    char* name;
    cogw7_process_state_t state;
    
    /* Cognitive context */
    atom_handle_t process_atom;
    atomspace_t local_atomspace;
    
    /* Memory */
    void* address_space;
    size_t memory_used;
    size_t memory_limit;
    
    /* Threads */
    cogw7_thread_t* threads;
    size_t thread_count;
    
    /* Handles */
    cogw7_handle_t* handles;
    size_t handle_count;
    size_t handle_capacity;
    
    /* Parent/child */
    uint32_t parent_pid;
    struct cogw7_process* children;
    struct cogw7_process* next_sibling;
    
    pthread_mutex_t lock;
} cogw7_process_t;

/* Cognitive service */
typedef struct {
    cogw7_service_type_t type;
    char* name;
    bool running;
    
    /* Service thread */
    pthread_t thread;
    
    /* Cognitive integration */
    atom_handle_t service_atom;
    pln_context_t pln_ctx;
    
    /* Callbacks */
    void (*start_callback)(void*);
    void (*stop_callback)(void*);
    void* callback_data;
} cogw7_service_t;

/* Kernel structure */
struct cogw7_kernel {
    /* Configuration */
    cogw7_config_t config;
    
    /* State */
    cogw7_kernel_state_t state;
    pthread_mutex_t state_lock;
    
    /* Cognitive core */
    atomspace_t atomspace;
    pln_context_t pln;
    
    /* Process management */
    cogw7_process_t* processes[COGW7_MAX_PROCESSES];
    uint32_t next_pid;
    pthread_rwlock_t process_lock;
    
    /* Thread management */
    cogw7_thread_t* ready_queue;
    cogw7_thread_t* current_thread;
    uint32_t next_tid;
    pthread_mutex_t scheduler_lock;
    
    /* Services */
    cogw7_service_t* services;
    size_t service_count;
    size_t service_capacity;
    pthread_rwlock_t service_lock;
    
    /* Cognitive agents */
    struct {
        atom_handle_t* agents;
        size_t count;
        size_t capacity;
        pthread_mutex_t lock;
    } agents;
    
    /* Scheduler thread */
    pthread_t scheduler_thread;
    bool scheduler_running;
    
    /* Cognitive reasoning thread */
    pthread_t reasoning_thread;
    bool reasoning_running;
    
    /* Statistics */
    cogw7_stats_t stats;
    pthread_mutex_t stats_lock;
    
    /* Boot time */
    uint64_t boot_time;
};

/*===========================================================================
 * Forward Declarations
 *===========================================================================*/

static void* scheduler_loop(void* arg);
static void* reasoning_loop(void* arg);
static void schedule_next(cogw7_kernel_t kernel);

/*===========================================================================
 * Kernel Lifecycle
 *===========================================================================*/

COGUTIL_API cog_result_t cogw7_kernel_create(
    const cogw7_config_t* config,
    cogw7_kernel_t* kernel
) {
    if (!kernel) return COG_ERROR_INVALID_PARAM;
    
    cogw7_kernel_t k = COG_CALLOC(1, sizeof(struct cogw7_kernel));
    if (!k) return COG_ERROR_MEMORY;
    
    /* Copy configuration */
    if (config) {
        memcpy(&k->config, config, sizeof(cogw7_config_t));
    } else {
        /* Default configuration */
        k->config.max_processes = 256;
        k->config.max_threads_per_process = 64;
        k->config.default_stack_size = 1024 * 1024;  /* 1MB */
        k->config.scheduler_quantum_ms = COGW7_QUANTUM_MS;
        k->config.enable_cognitive_scheduling = true;
        k->config.enable_attention_allocation = true;
        k->config.reasoning_interval_ms = 100;
    }
    
    /* Initialize state */
    k->state = COGW7_STATE_INIT;
    pthread_mutex_init(&k->state_lock, NULL);
    
    /* Create AtomSpace */
    k->atomspace = atomspace_create();
    cog_result_t result = k->atomspace ? COG_OK : COG_ERROR_MEMORY;
    if (result != COG_OK) {
        COG_FREE(k);
        return result;
    }
    
    /* Create PLN context */
    pln_config_t pln_config = pln_config_default();
    pln_config.max_inference_steps = 10;
    pln_config.min_confidence_threshold = 0.1;
    k->pln = pln_engine_create(k->atomspace, &pln_config);
    result = k->pln ? COG_OK : COG_ERROR_MEMORY;
    if (result != COG_OK) {
        atomspace_destroy(k->atomspace);
        COG_FREE(k);
        return result;
    }
    
    /* Initialize locks */
    pthread_rwlock_init(&k->process_lock, NULL);
    pthread_mutex_init(&k->scheduler_lock, NULL);
    pthread_rwlock_init(&k->service_lock, NULL);
    pthread_mutex_init(&k->agents.lock, NULL);
    pthread_mutex_init(&k->stats_lock, NULL);
    
    /* Initialize services array */
    k->service_capacity = 32;
    k->services = COG_CALLOC(k->service_capacity, sizeof(cogw7_service_t));
    
    /* Initialize agents array */
    k->agents.capacity = 64;
    k->agents.agents = COG_CALLOC(k->agents.capacity, sizeof(atom_handle_t));
    
    /* Set next IDs */
    k->next_pid = 1;
    k->next_tid = 1;
    
    /* Record boot time */
    k->boot_time = cog_time_now_ms();
    
    *kernel = k;
    
    COG_LOG_INFO("CogW7OS kernel created");
    return COG_OK;
}

COGUTIL_API void cogw7_kernel_destroy(cogw7_kernel_t kernel) {
    if (!kernel) return;
    
    /* Stop kernel if running */
    cogw7_kernel_shutdown(kernel);
    
    /* Destroy all processes */
    pthread_rwlock_wrlock(&kernel->process_lock);
    for (size_t i = 0; i < COGW7_MAX_PROCESSES; i++) {
        if (kernel->processes[i]) {
            cogw7_process_t* proc = kernel->processes[i];
            COG_FREE(proc->name);
            COG_FREE(proc->handles);
            /* TODO: Destroy threads */
            COG_FREE(proc);
        }
    }
    pthread_rwlock_unlock(&kernel->process_lock);
    
    /* Destroy services */
    COG_FREE(kernel->services);
    
    /* Destroy agents */
    COG_FREE(kernel->agents.agents);
    
    /* Destroy PLN */
    pln_engine_destroy(kernel->pln);
    
    /* Destroy AtomSpace */
    atomspace_destroy(kernel->atomspace);
    
    /* Destroy locks */
    pthread_mutex_destroy(&kernel->state_lock);
    pthread_rwlock_destroy(&kernel->process_lock);
    pthread_mutex_destroy(&kernel->scheduler_lock);
    pthread_rwlock_destroy(&kernel->service_lock);
    pthread_mutex_destroy(&kernel->agents.lock);
    pthread_mutex_destroy(&kernel->stats_lock);
    
    COG_FREE(kernel);
    
    COG_LOG_INFO("CogW7OS kernel destroyed");
}

/*===========================================================================
 * Kernel Control
 *===========================================================================*/

COGUTIL_API cog_result_t cogw7_kernel_boot(cogw7_kernel_t kernel) {
    if (!kernel) return COG_ERROR_INVALID_PARAM;
    
    pthread_mutex_lock(&kernel->state_lock);
    
    if (kernel->state != COGW7_STATE_INIT) {
        pthread_mutex_unlock(&kernel->state_lock);
        return COG_ERROR_STATE;
    }
    
    kernel->state = COGW7_STATE_BOOTING;
    pthread_mutex_unlock(&kernel->state_lock);
    
    COG_LOG_INFO("CogW7OS kernel booting...");
    
    /* Create kernel process (PID 0) */
    cogw7_process_t* kernel_proc = COG_CALLOC(1, sizeof(cogw7_process_t));
    kernel_proc->pid = 0;
    kernel_proc->name = COG_STRDUP("System");
    kernel_proc->state = COGW7_PROC_RUNNING;
    pthread_mutex_init(&kernel_proc->lock, NULL);
    
    /* Create kernel process atom */
    kernel_proc->process_atom = atomspace_add_node(kernel->atomspace,
        ATOM_TYPE_CONCEPT_NODE, "System");
    
    pthread_rwlock_wrlock(&kernel->process_lock);
    kernel->processes[0] = kernel_proc;
    pthread_rwlock_unlock(&kernel->process_lock);
    
    /* Start scheduler thread */
    kernel->scheduler_running = true;
    pthread_create(&kernel->scheduler_thread, NULL, scheduler_loop, kernel);
    
    /* Start reasoning thread */
    kernel->reasoning_running = true;
    pthread_create(&kernel->reasoning_thread, NULL, reasoning_loop, kernel);
    
    /* Start built-in services */
    cogw7_service_start(kernel, COGW7_SVC_MEMORY_MANAGER);
    cogw7_service_start(kernel, COGW7_SVC_PROCESS_MANAGER);
    cogw7_service_start(kernel, COGW7_SVC_COGNITIVE_ENGINE);
    cogw7_service_start(kernel, COGW7_SVC_ATTENTION_ALLOCATOR);
    
    pthread_mutex_lock(&kernel->state_lock);
    kernel->state = COGW7_STATE_RUNNING;
    pthread_mutex_unlock(&kernel->state_lock);
    
    COG_LOG_INFO("CogW7OS kernel boot complete");
    return COG_OK;
}

COGUTIL_API cog_result_t cogw7_kernel_shutdown(cogw7_kernel_t kernel) {
    if (!kernel) return COG_ERROR_INVALID_PARAM;
    
    pthread_mutex_lock(&kernel->state_lock);
    
    if (kernel->state != COGW7_STATE_RUNNING) {
        pthread_mutex_unlock(&kernel->state_lock);
        return COG_OK;
    }
    
    kernel->state = COGW7_STATE_SHUTTING_DOWN;
    pthread_mutex_unlock(&kernel->state_lock);
    
    COG_LOG_INFO("CogW7OS kernel shutting down...");
    
    /* Stop reasoning thread */
    kernel->reasoning_running = false;
    pthread_join(kernel->reasoning_thread, NULL);
    
    /* Stop scheduler thread */
    kernel->scheduler_running = false;
    pthread_join(kernel->scheduler_thread, NULL);
    
    /* Stop all services */
    pthread_rwlock_rdlock(&kernel->service_lock);
    for (size_t i = 0; i < kernel->service_count; i++) {
        if (kernel->services[i].running) {
            kernel->services[i].running = false;
            if (kernel->services[i].stop_callback) {
                kernel->services[i].stop_callback(kernel->services[i].callback_data);
            }
        }
    }
    pthread_rwlock_unlock(&kernel->service_lock);
    
    /* Terminate all processes */
    pthread_rwlock_wrlock(&kernel->process_lock);
    for (size_t i = 1; i < COGW7_MAX_PROCESSES; i++) {
        if (kernel->processes[i]) {
            kernel->processes[i]->state = COGW7_PROC_TERMINATED;
        }
    }
    pthread_rwlock_unlock(&kernel->process_lock);
    
    pthread_mutex_lock(&kernel->state_lock);
    kernel->state = COGW7_STATE_HALTED;
    pthread_mutex_unlock(&kernel->state_lock);
    
    COG_LOG_INFO("CogW7OS kernel shutdown complete");
    return COG_OK;
}

COGUTIL_API cogw7_kernel_state_t cogw7_kernel_get_state(cogw7_kernel_t kernel) {
    if (!kernel) return COGW7_STATE_HALTED;
    
    pthread_mutex_lock(&kernel->state_lock);
    cogw7_kernel_state_t state = kernel->state;
    pthread_mutex_unlock(&kernel->state_lock);
    
    return state;
}

/*===========================================================================
 * Process Management
 *===========================================================================*/

COGUTIL_API cog_result_t cogw7_process_create(
    cogw7_kernel_t kernel,
    const char* name,
    uint32_t parent_pid,
    uint32_t* pid
) {
    if (!kernel || !name || !pid) return COG_ERROR_INVALID_PARAM;
    
    pthread_rwlock_wrlock(&kernel->process_lock);
    
    /* Find free slot */
    uint32_t new_pid = kernel->next_pid;
    while (new_pid < COGW7_MAX_PROCESSES && kernel->processes[new_pid]) {
        new_pid++;
    }
    
    if (new_pid >= COGW7_MAX_PROCESSES) {
        pthread_rwlock_unlock(&kernel->process_lock);
        return COG_ERROR_MEMORY;
    }
    
    /* Create process */
    cogw7_process_t* proc = COG_CALLOC(1, sizeof(cogw7_process_t));
    if (!proc) {
        pthread_rwlock_unlock(&kernel->process_lock);
        return COG_ERROR_MEMORY;
    }
    
    proc->pid = new_pid;
    proc->name = COG_STRDUP(name);
    proc->state = COGW7_PROC_CREATED;
    proc->parent_pid = parent_pid;
    proc->handle_capacity = 64;
    proc->handles = COG_CALLOC(proc->handle_capacity, sizeof(cogw7_handle_t));
    pthread_mutex_init(&proc->lock, NULL);
    
    /* Create process atom in AtomSpace */
    proc->process_atom = atomspace_add_node(kernel->atomspace,
        ATOM_TYPE_CONCEPT_NODE, name);
    
    /* Link to parent */
    if (parent_pid > 0 && parent_pid < COGW7_MAX_PROCESSES && 
        kernel->processes[parent_pid]) {
        atom_handle_t parent_link[] = {
            kernel->processes[parent_pid]->process_atom,
            proc->process_atom
        };
        atomspace_add_link(kernel->atomspace, ATOM_TYPE_INHERITANCE_LINK,
            parent_link, 2);
    }
    
    kernel->processes[new_pid] = proc;
    kernel->next_pid = new_pid + 1;
    
    pthread_rwlock_unlock(&kernel->process_lock);
    
    /* Update stats */
    pthread_mutex_lock(&kernel->stats_lock);
    kernel->stats.process_count++;
    kernel->stats.total_processes_created++;
    pthread_mutex_unlock(&kernel->stats_lock);
    
    *pid = new_pid;
    
    COG_LOG_INFO("Process created: %s (PID %u)", name, new_pid);
    return COG_OK;
}

COGUTIL_API cog_result_t cogw7_process_terminate(
    cogw7_kernel_t kernel,
    uint32_t pid,
    int exit_code
) {
    if (!kernel || pid >= COGW7_MAX_PROCESSES) return COG_ERROR_INVALID_PARAM;
    
    pthread_rwlock_wrlock(&kernel->process_lock);
    
    cogw7_process_t* proc = kernel->processes[pid];
    if (!proc) {
        pthread_rwlock_unlock(&kernel->process_lock);
        return COG_ERROR_NOT_FOUND;
    }
    
    proc->state = COGW7_PROC_TERMINATED;
    
    /* Terminate all threads */
    cogw7_thread_t* thread = proc->threads;
    while (thread) {
        thread->state = THREAD_STATE_TERMINATED;
        pthread_cond_signal(&thread->wait_cond);
        thread = thread->next;
    }
    
    pthread_rwlock_unlock(&kernel->process_lock);
    
    /* Update stats */
    pthread_mutex_lock(&kernel->stats_lock);
    kernel->stats.process_count--;
    pthread_mutex_unlock(&kernel->stats_lock);
    
    COG_LOG_INFO("Process terminated: PID %u (exit code %d)", pid, exit_code);
    return COG_OK;
}

COGUTIL_API cog_result_t cogw7_process_get_info(
    cogw7_kernel_t kernel,
    uint32_t pid,
    cogw7_process_info_t* info
) {
    if (!kernel || !info || pid >= COGW7_MAX_PROCESSES) return COG_ERROR_INVALID_PARAM;
    
    pthread_rwlock_rdlock(&kernel->process_lock);
    
    cogw7_process_t* proc = kernel->processes[pid];
    if (!proc) {
        pthread_rwlock_unlock(&kernel->process_lock);
        return COG_ERROR_NOT_FOUND;
    }
    
    info->pid = proc->pid;
    strncpy(info->name, proc->name, sizeof(info->name) - 1);
    info->state = proc->state;
    info->parent_pid = proc->parent_pid;
    info->thread_count = proc->thread_count;
    info->handle_count = proc->handle_count;
    info->memory_used = proc->memory_used;
    info->process_atom = proc->process_atom;
    
    pthread_rwlock_unlock(&kernel->process_lock);
    
    return COG_OK;
}

/*===========================================================================
 * Thread Management
 *===========================================================================*/

static void* thread_wrapper(void* arg) {
    cogw7_thread_t* thread = (cogw7_thread_t*)arg;
    
    /* Call entry point */
    if (thread->entry_point) {
        ((void (*)(void*))thread->entry_point)(thread->arg);
    }
    
    thread->state = THREAD_STATE_TERMINATED;
    return NULL;
}

COGUTIL_API cog_result_t cogw7_thread_create(
    cogw7_kernel_t kernel,
    uint32_t pid,
    void* entry_point,
    void* arg,
    uint32_t* tid
) {
    if (!kernel || !entry_point || !tid || pid >= COGW7_MAX_PROCESSES) {
        return COG_ERROR_INVALID_PARAM;
    }
    
    pthread_rwlock_rdlock(&kernel->process_lock);
    cogw7_process_t* proc = kernel->processes[pid];
    if (!proc) {
        pthread_rwlock_unlock(&kernel->process_lock);
        return COG_ERROR_NOT_FOUND;
    }
    pthread_rwlock_unlock(&kernel->process_lock);
    
    /* Create thread */
    cogw7_thread_t* thread = COG_CALLOC(1, sizeof(cogw7_thread_t));
    if (!thread) return COG_ERROR_MEMORY;
    
    pthread_mutex_lock(&kernel->scheduler_lock);
    thread->tid = kernel->next_tid++;
    pthread_mutex_unlock(&kernel->scheduler_lock);
    
    thread->pid = pid;
    thread->state = THREAD_STATE_INIT;
    thread->priority = 8;  /* Normal priority */
    thread->entry_point = entry_point;
    thread->arg = arg;
    thread->stack_size = kernel->config.default_stack_size;
    thread->attention_level = 0.5;
    
    pthread_mutex_init(&thread->lock, NULL);
    pthread_cond_init(&thread->wait_cond, NULL);
    
    /* Create thread atom */
    char thread_name[64];
    snprintf(thread_name, sizeof(thread_name), "Thread_%u_%u", pid, thread->tid);
    thread->context_atom = atomspace_add_node(kernel->atomspace,
        ATOM_TYPE_CONCEPT_NODE, thread_name);
    
    /* Add to process thread list */
    pthread_mutex_lock(&proc->lock);
    thread->next = proc->threads;
    proc->threads = thread;
    proc->thread_count++;
    pthread_mutex_unlock(&proc->lock);
    
    /* Create native thread */
    pthread_create(&thread->native_thread, NULL, thread_wrapper, thread);
    
    /* Add to ready queue */
    pthread_mutex_lock(&kernel->scheduler_lock);
    thread->state = THREAD_STATE_READY;
    thread->next = kernel->ready_queue;
    kernel->ready_queue = thread;
    pthread_mutex_unlock(&kernel->scheduler_lock);
    
    /* Update stats */
    pthread_mutex_lock(&kernel->stats_lock);
    kernel->stats.thread_count++;
    kernel->stats.total_threads_created++;
    pthread_mutex_unlock(&kernel->stats_lock);
    
    *tid = thread->tid;
    
    COG_LOG_DEBUG("Thread created: TID %u in PID %u", thread->tid, pid);
    return COG_OK;
}

/*===========================================================================
 * Scheduler
 *===========================================================================*/

static void schedule_next(cogw7_kernel_t kernel) {
    pthread_mutex_lock(&kernel->scheduler_lock);
    
    if (!kernel->ready_queue) {
        pthread_mutex_unlock(&kernel->scheduler_lock);
        return;
    }
    
    /* Simple priority-based scheduling with cognitive attention */
    cogw7_thread_t* best = NULL;
    cogw7_thread_t* prev_best = NULL;
    cogw7_thread_t* prev = NULL;
    cogw7_thread_t* curr = kernel->ready_queue;
    double best_score = -1.0;
    
    while (curr) {
        if (curr->state == THREAD_STATE_READY) {
            double score = curr->priority;
            
            /* Factor in cognitive attention if enabled */
            if (kernel->config.enable_cognitive_scheduling) {
                score *= (1.0 + curr->attention_level);
            }
            
            /* Factor in wait time to prevent starvation */
            uint64_t now = cog_time_now_ms();
            if (curr->last_run > 0) {
                uint64_t wait = now - curr->last_run;
                score += wait / 1000.0;  /* Boost by seconds waited */
            }
            
            if (score > best_score) {
                best_score = score;
                best = curr;
                prev_best = prev;
            }
        }
        prev = curr;
        curr = curr->next;
    }
    
    if (best) {
        /* Remove from ready queue */
        if (prev_best) {
            prev_best->next = best->next;
        } else {
            kernel->ready_queue = best->next;
        }
        
        best->state = THREAD_STATE_RUNNING;
        best->last_run = cog_time_now_ms();
        kernel->current_thread = best;
    }
    
    pthread_mutex_unlock(&kernel->scheduler_lock);
}

static void* scheduler_loop(void* arg) {
    cogw7_kernel_t kernel = (cogw7_kernel_t)arg;
    
    while (kernel->scheduler_running) {
        schedule_next(kernel);
        
        /* Sleep for tick interval */
        cog_time_sleep_ms(COGW7_TICK_MS);
        
        /* Update stats */
        pthread_mutex_lock(&kernel->stats_lock);
        kernel->stats.scheduler_ticks++;
        pthread_mutex_unlock(&kernel->stats_lock);
    }
    
    return NULL;
}

/*===========================================================================
 * Cognitive Reasoning Loop
 *===========================================================================*/

static void* reasoning_loop(void* arg) {
    cogw7_kernel_t kernel = (cogw7_kernel_t)arg;
    
    while (kernel->reasoning_running) {
        /* Perform cognitive reasoning */
        
        /* 1. Update attention values based on system state */
        if (kernel->config.enable_attention_allocation) {
            pthread_rwlock_rdlock(&kernel->process_lock);
            
            for (size_t i = 0; i < COGW7_MAX_PROCESSES; i++) {
                cogw7_process_t* proc = kernel->processes[i];
                if (!proc || proc->state != COGW7_PROC_RUNNING) continue;
                
                /* Stimulate active process atoms */
                atomspace_stimulate(kernel->atomspace, proc->process_atom, 1);
            }
            
            pthread_rwlock_unlock(&kernel->process_lock);
        }
        
        /* 2. Run forward chaining on active atoms */
        size_t conclusion_count = 0;
        
        /* Get atoms in attentional focus */
        atom_handle_t* focus_atoms = NULL;
        size_t focus_count = 0;
        atom_query_t q = atomspace_query_create(kernel->atomspace);
        if (q) {
            atomspace_query_type(q, ATOM_TYPE_CONCEPT_NODE);
            atomspace_query_av_min(q, 1);
            focus_atoms = atomspace_query_execute(q, &focus_count);
            atomspace_query_destroy(q);
        }
        
        for (size_t i = 0; i < focus_count && i < 10; i++) {
            forward_chain_result_t* fc_result = pln_forward_chain(
                kernel->pln, focus_atoms[i], 5);
            if (fc_result) {
                conclusion_count += fc_result->derived_count;
                pln_forward_chain_result_free(fc_result);
            }
        }
        
        if (focus_atoms) atomspace_query_results_free(focus_atoms);
        
        /* Update stats */
        pthread_mutex_lock(&kernel->stats_lock);
        kernel->stats.reasoning_cycles++;
        kernel->stats.inferences_made += conclusion_count;
        pthread_mutex_unlock(&kernel->stats_lock);
        
        /* Sleep for reasoning interval */
        cog_time_sleep_ms(kernel->config.reasoning_interval_ms);
    }
    
    return NULL;
}

/*===========================================================================
 * Service Management
 *===========================================================================*/

COGUTIL_API cog_result_t cogw7_service_start(
    cogw7_kernel_t kernel,
    cogw7_service_type_t type
) {
    if (!kernel) return COG_ERROR_INVALID_PARAM;
    
    pthread_rwlock_wrlock(&kernel->service_lock);
    
    /* Check if service already exists */
    for (size_t i = 0; i < kernel->service_count; i++) {
        if (kernel->services[i].type == type) {
            if (kernel->services[i].running) {
                pthread_rwlock_unlock(&kernel->service_lock);
                return COG_OK;
            }
            kernel->services[i].running = true;
            pthread_rwlock_unlock(&kernel->service_lock);
            return COG_OK;
        }
    }
    
    /* Create new service */
    if (kernel->service_count >= kernel->service_capacity) {
        size_t new_capacity = kernel->service_capacity * 2;
        cogw7_service_t* new_services = COG_REALLOC(kernel->services,
            new_capacity * sizeof(cogw7_service_t));
        if (!new_services) {
            pthread_rwlock_unlock(&kernel->service_lock);
            return COG_ERROR_MEMORY;
        }
        kernel->services = new_services;
        kernel->service_capacity = new_capacity;
    }
    
    cogw7_service_t* svc = &kernel->services[kernel->service_count++];
    memset(svc, 0, sizeof(cogw7_service_t));
    svc->type = type;
    svc->running = true;
    
    /* Set service name */
    switch (type) {
        case COGW7_SVC_MEMORY_MANAGER:
            svc->name = COG_STRDUP("MemoryManager");
            break;
        case COGW7_SVC_PROCESS_MANAGER:
            svc->name = COG_STRDUP("ProcessManager");
            break;
        case COGW7_SVC_THREAD_SCHEDULER:
            svc->name = COG_STRDUP("ThreadScheduler");
            break;
        case COGW7_SVC_COGNITIVE_ENGINE:
            svc->name = COG_STRDUP("CognitiveEngine");
            break;
        case COGW7_SVC_ATTENTION_ALLOCATOR:
            svc->name = COG_STRDUP("AttentionAllocator");
            break;
        case COGW7_SVC_REASONING_ENGINE:
            svc->name = COG_STRDUP("ReasoningEngine");
            break;
        default:
            svc->name = COG_STRDUP("UnknownService");
            break;
    }
    
    /* Create service atom */
    svc->service_atom = atomspace_add_node(kernel->atomspace,
        ATOM_TYPE_CONCEPT_NODE, svc->name);
    
    pthread_rwlock_unlock(&kernel->service_lock);
    
    COG_LOG_INFO("Service started: %s", svc->name);
    return COG_OK;
}

COGUTIL_API cog_result_t cogw7_service_stop(
    cogw7_kernel_t kernel,
    cogw7_service_type_t type
) {
    if (!kernel) return COG_ERROR_INVALID_PARAM;
    
    pthread_rwlock_wrlock(&kernel->service_lock);
    
    for (size_t i = 0; i < kernel->service_count; i++) {
        if (kernel->services[i].type == type) {
            kernel->services[i].running = false;
            COG_LOG_INFO("Service stopped: %s", kernel->services[i].name);
            pthread_rwlock_unlock(&kernel->service_lock);
            return COG_OK;
        }
    }
    
    pthread_rwlock_unlock(&kernel->service_lock);
    return COG_ERROR_NOT_FOUND;
}

/*===========================================================================
 * Cognitive Agent Management
 *===========================================================================*/

COGUTIL_API cog_result_t cogw7_agent_register(
    cogw7_kernel_t kernel,
    atom_handle_t agent_atom
) {
    if (!kernel || agent_atom == ATOM_HANDLE_INVALID) return COG_ERROR_INVALID_PARAM;
    
    pthread_mutex_lock(&kernel->agents.lock);
    
    /* Grow array if needed */
    if (kernel->agents.count >= kernel->agents.capacity) {
        size_t new_capacity = kernel->agents.capacity * 2;
        atom_handle_t* new_agents = COG_REALLOC(kernel->agents.agents,
            new_capacity * sizeof(atom_handle_t));
        if (!new_agents) {
            pthread_mutex_unlock(&kernel->agents.lock);
            return COG_ERROR_MEMORY;
        }
        kernel->agents.agents = new_agents;
        kernel->agents.capacity = new_capacity;
    }
    
    kernel->agents.agents[kernel->agents.count++] = agent_atom;
    
    pthread_mutex_unlock(&kernel->agents.lock);
    
    /* Update stats */
    pthread_mutex_lock(&kernel->stats_lock);
    kernel->stats.agent_count++;
    pthread_mutex_unlock(&kernel->stats_lock);
    
    return COG_OK;
}

/*===========================================================================
 * Statistics
 *===========================================================================*/

COGUTIL_API cog_result_t cogw7_get_stats(cogw7_kernel_t kernel, cogw7_stats_t* stats) {
    if (!kernel || !stats) return COG_ERROR_INVALID_PARAM;
    
    pthread_mutex_lock(&kernel->stats_lock);
    memcpy(stats, &kernel->stats, sizeof(cogw7_stats_t));
    stats->uptime_ms = cog_time_now_ms() - kernel->boot_time;
    pthread_mutex_unlock(&kernel->stats_lock);
    
    return COG_OK;
}

COGUTIL_API atomspace_t cogw7_get_atomspace(cogw7_kernel_t kernel) {
    return kernel ? kernel->atomspace : NULL;
}

COGUTIL_API pln_context_t cogw7_get_pln(cogw7_kernel_t kernel) {
    return kernel ? kernel->pln : NULL;
}
