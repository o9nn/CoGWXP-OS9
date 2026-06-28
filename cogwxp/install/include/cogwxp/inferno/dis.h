/**
 * @file dis.h
 * @brief Dis Virtual Machine for CoGWXP-OS9
 * 
 * The Dis virtual machine executes Limbo bytecode, providing portable
 * distributed computing capabilities for the cognitive operating system.
 * 
 * @copyright MIT/GPL
 */

#ifndef _COGWXP_DIS_H_
#define _COGWXP_DIS_H_

#include "../../opencog/cogutil/cogutil.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Dis Constants
 *===========================================================================*/

#define DIS_MAGIC       0xDA0D1E00
#define DIS_VERSION     1
#define DIS_MAXSTACK    65536
#define DIS_MAXFRAME    4096

/*===========================================================================
 * Dis Types
 *===========================================================================*/

typedef enum {
    DIS_TYPE_NONE = 0,
    DIS_TYPE_INT,           /* 32-bit signed integer */
    DIS_TYPE_BIG,           /* Arbitrary precision integer */
    DIS_TYPE_REAL,          /* 64-bit floating point */
    DIS_TYPE_STRING,        /* UTF-8 string */
    DIS_TYPE_ARRAY,         /* Dynamic array */
    DIS_TYPE_LIST,          /* Linked list */
    DIS_TYPE_CHANNEL,       /* Communication channel */
    DIS_TYPE_REF,           /* Reference type (ADT) */
    DIS_TYPE_MODULE,        /* Module reference */
    DIS_TYPE_FN,            /* Function reference */
    DIS_TYPE_POLY,          /* Polymorphic type */
    
    /* CoGWXP-OS9 extensions */
    DIS_TYPE_ATOM,          /* AtomSpace atom handle */
    DIS_TYPE_TRUTHVALUE,    /* PLN truth value */
    DIS_TYPE_9PFID,         /* 9P file identifier */
} dis_type_t;

/*===========================================================================
 * Dis Instructions
 *===========================================================================*/

typedef enum {
    /* Arithmetic */
    DIS_OP_ADDI = 0x00,     /* Add integers */
    DIS_OP_SUBI,            /* Subtract integers */
    DIS_OP_MULI,            /* Multiply integers */
    DIS_OP_DIVI,            /* Divide integers */
    DIS_OP_MODI,            /* Modulo integers */
    DIS_OP_ADDR,            /* Add reals */
    DIS_OP_SUBR,            /* Subtract reals */
    DIS_OP_MULR,            /* Multiply reals */
    DIS_OP_DIVR,            /* Divide reals */
    
    /* Comparison */
    DIS_OP_EQI = 0x10,      /* Equal integers */
    DIS_OP_NEI,             /* Not equal integers */
    DIS_OP_LTI,             /* Less than integers */
    DIS_OP_LEI,             /* Less or equal integers */
    DIS_OP_GTI,             /* Greater than integers */
    DIS_OP_GEI,             /* Greater or equal integers */
    
    /* Logical */
    DIS_OP_ANDI = 0x20,     /* Bitwise AND */
    DIS_OP_ORI,             /* Bitwise OR */
    DIS_OP_XORI,            /* Bitwise XOR */
    DIS_OP_NOTI,            /* Bitwise NOT */
    DIS_OP_SHLI,            /* Shift left */
    DIS_OP_SHRI,            /* Shift right */
    
    /* Control flow */
    DIS_OP_JMP = 0x30,      /* Unconditional jump */
    DIS_OP_JZ,              /* Jump if zero */
    DIS_OP_JNZ,             /* Jump if not zero */
    DIS_OP_CALL,            /* Call function */
    DIS_OP_RET,             /* Return from function */
    DIS_OP_SPAWN,           /* Spawn new thread */
    
    /* Memory */
    DIS_OP_LOAD = 0x40,     /* Load from memory */
    DIS_OP_STORE,           /* Store to memory */
    DIS_OP_NEW,             /* Allocate new object */
    DIS_OP_FREE,            /* Free object */
    DIS_OP_COPY,            /* Copy object */
    
    /* Channel operations */
    DIS_OP_SEND = 0x50,     /* Send on channel */
    DIS_OP_RECV,            /* Receive from channel */
    DIS_OP_ALT,             /* Alternative (select) */
    
    /* Module operations */
    DIS_OP_MLOAD = 0x60,    /* Load module */
    DIS_OP_MCALL,           /* Call module function */
    
    /* CoGWXP-OS9 extensions */
    DIS_OP_ATOM_GET = 0x80, /* Get atom from AtomSpace */
    DIS_OP_ATOM_ADD,        /* Add atom to AtomSpace */
    DIS_OP_ATOM_TV,         /* Get/set truth value */
    DIS_OP_PLN_INFER,       /* PLN inference */
    DIS_OP_9P_OPEN,         /* 9P file open */
    DIS_OP_9P_READ,         /* 9P file read */
    DIS_OP_9P_WRITE,        /* 9P file write */
    DIS_OP_9P_CLOSE,        /* 9P file close */
    
    DIS_OP_HALT = 0xFF      /* Halt execution */
} dis_opcode_t;

/*===========================================================================
 * Dis Value
 *===========================================================================*/

typedef struct dis_value {
    dis_type_t type;
    union {
        int32_t i;          /* Integer */
        int64_t b;          /* Big integer (simplified) */
        double r;           /* Real */
        char* s;            /* String */
        struct dis_array* a; /* Array */
        struct dis_list* l;  /* List */
        struct dis_channel* c; /* Channel */
        void* ref;          /* Reference */
        uint64_t atom;      /* Atom handle */
        struct {
            double strength;
            double confidence;
        } tv;               /* Truth value */
        uint32_t fid;       /* 9P file ID */
    } v;
} dis_value_t;

/*===========================================================================
 * Dis Array
 *===========================================================================*/

typedef struct dis_array {
    dis_type_t elem_type;
    size_t length;
    size_t capacity;
    dis_value_t* elements;
} dis_array_t;

COGUTIL_API dis_array_t* dis_array_create(dis_type_t elem_type, size_t initial_capacity);
COGUTIL_API void         dis_array_destroy(dis_array_t* array);
COGUTIL_API cog_result_t dis_array_push(dis_array_t* array, dis_value_t* value);
COGUTIL_API dis_value_t* dis_array_get(dis_array_t* array, size_t index);
COGUTIL_API cog_result_t dis_array_set(dis_array_t* array, size_t index, dis_value_t* value);

/*===========================================================================
 * Dis Channel
 *===========================================================================*/

typedef struct dis_channel {
    dis_type_t elem_type;
    size_t buffer_size;
    cog_mutex_t mutex;
    cog_cond_t not_empty;
    cog_cond_t not_full;
    dis_value_t* buffer;
    size_t read_pos;
    size_t write_pos;
    size_t count;
    bool closed;
} dis_channel_t;

COGUTIL_API dis_channel_t* dis_channel_create(dis_type_t elem_type, size_t buffer_size);
COGUTIL_API void           dis_channel_destroy(dis_channel_t* channel);
COGUTIL_API cog_result_t   dis_channel_send(dis_channel_t* channel, dis_value_t* value);
COGUTIL_API cog_result_t   dis_channel_recv(dis_channel_t* channel, dis_value_t* value);
COGUTIL_API cog_result_t   dis_channel_close(dis_channel_t* channel);
COGUTIL_API bool           dis_channel_is_closed(dis_channel_t* channel);

/*===========================================================================
 * Dis Module
 *===========================================================================*/

typedef struct dis_module {
    char* name;
    char* path;
    uint32_t magic;
    uint32_t version;
    
    /* Code section */
    uint8_t* code;
    size_t code_size;
    
    /* Data section */
    uint8_t* data;
    size_t data_size;
    
    /* Type descriptors */
    struct dis_type_desc* types;
    size_t type_count;
    
    /* Exports */
    struct dis_export* exports;
    size_t export_count;
    
    /* Imports */
    struct dis_import* imports;
    size_t import_count;
    
    /* Entry point */
    size_t entry_pc;
} dis_module_t;

COGUTIL_API dis_module_t* dis_module_load(const char* path);
COGUTIL_API dis_module_t* dis_module_load_mem(const void* data, size_t size);
COGUTIL_API void          dis_module_unload(dis_module_t* module);
COGUTIL_API void*         dis_module_get_export(dis_module_t* module, const char* name);

/*===========================================================================
 * Dis Thread
 *===========================================================================*/

typedef struct dis_thread {
    uint64_t id;
    struct dis_vm* vm;
    dis_module_t* module;
    
    /* Execution state */
    size_t pc;              /* Program counter */
    dis_value_t* stack;     /* Operand stack */
    size_t sp;              /* Stack pointer */
    
    /* Frame stack */
    struct dis_frame* frames;
    size_t fp;              /* Frame pointer */
    
    /* Thread state */
    enum {
        DIS_THREAD_READY,
        DIS_THREAD_RUNNING,
        DIS_THREAD_BLOCKED,
        DIS_THREAD_TERMINATED
    } state;
    
    /* Blocking info */
    dis_channel_t* blocked_channel;
    bool blocked_send;      /* true = send, false = recv */
} dis_thread_t;

/*===========================================================================
 * Dis Frame
 *===========================================================================*/

typedef struct dis_frame {
    dis_module_t* module;
    size_t return_pc;
    size_t base_sp;
    dis_value_t* locals;
    size_t local_count;
} dis_frame_t;

/*===========================================================================
 * Dis Virtual Machine
 *===========================================================================*/

typedef struct dis_vm {
    /* Thread management */
    dis_thread_t** threads;
    size_t thread_count;
    size_t thread_capacity;
    uint64_t next_thread_id;
    
    /* Module cache */
    dis_module_t** modules;
    size_t module_count;
    
    /* Memory management */
    size_t heap_size;
    size_t heap_used;
    
    /* Integration hooks */
    void* atomspace;        /* AtomSpace reference */
    void* pln_engine;       /* PLN engine reference */
    void* p9_client;        /* 9P client reference */
    
    /* Configuration */
    size_t max_stack_size;
    size_t max_heap_size;
    bool gc_enabled;
    
    /* Statistics */
    uint64_t instructions_executed;
    uint64_t gc_collections;
} dis_vm_t;

/*===========================================================================
 * VM Lifecycle
 *===========================================================================*/

typedef struct dis_vm_config {
    size_t max_stack_size;
    size_t max_heap_size;
    size_t initial_threads;
    bool gc_enabled;
    void* atomspace;
    void* pln_engine;
    void* p9_client;
} dis_vm_config_t;

COGUTIL_API dis_vm_config_t dis_vm_config_default(void);
COGUTIL_API dis_vm_t*       dis_vm_create(dis_vm_config_t* config);
COGUTIL_API void            dis_vm_destroy(dis_vm_t* vm);

/*===========================================================================
 * VM Execution
 *===========================================================================*/

COGUTIL_API cog_result_t dis_vm_load_module(dis_vm_t* vm, const char* path);
COGUTIL_API cog_result_t dis_vm_run(dis_vm_t* vm, const char* module_name, const char* function);
COGUTIL_API cog_result_t dis_vm_run_with_args(dis_vm_t* vm, const char* module_name, const char* function, dis_value_t* args, size_t arg_count);
COGUTIL_API cog_result_t dis_vm_step(dis_vm_t* vm, dis_thread_t* thread);
COGUTIL_API void         dis_vm_stop(dis_vm_t* vm);

/*===========================================================================
 * Thread Management
 *===========================================================================*/

COGUTIL_API dis_thread_t* dis_vm_spawn(dis_vm_t* vm, dis_module_t* module, size_t entry_pc);
COGUTIL_API cog_result_t  dis_vm_join(dis_vm_t* vm, dis_thread_t* thread, dis_value_t* result);
COGUTIL_API void          dis_vm_yield(dis_vm_t* vm);

/*===========================================================================
 * Garbage Collection
 *===========================================================================*/

COGUTIL_API void dis_vm_gc(dis_vm_t* vm);
COGUTIL_API void dis_vm_gc_enable(dis_vm_t* vm, bool enable);

/*===========================================================================
 * Integration APIs
 *===========================================================================*/

/* AtomSpace integration */
COGUTIL_API void dis_vm_set_atomspace(dis_vm_t* vm, void* atomspace);
COGUTIL_API cog_result_t dis_vm_atom_get(dis_vm_t* vm, uint64_t handle, dis_value_t* result);
COGUTIL_API cog_result_t dis_vm_atom_add(dis_vm_t* vm, dis_type_t type, const char* name, uint64_t* handle);

/* PLN integration */
COGUTIL_API void dis_vm_set_pln(dis_vm_t* vm, void* pln_engine);
COGUTIL_API cog_result_t dis_vm_pln_infer(dis_vm_t* vm, uint64_t source, dis_value_t* result);

/* 9P integration */
COGUTIL_API void dis_vm_set_9p(dis_vm_t* vm, void* p9_client);
COGUTIL_API cog_result_t dis_vm_9p_open(dis_vm_t* vm, const char* path, uint8_t mode, uint32_t* fid);
COGUTIL_API cog_result_t dis_vm_9p_read(dis_vm_t* vm, uint32_t fid, void* buf, size_t count, size_t* nread);
COGUTIL_API cog_result_t dis_vm_9p_write(dis_vm_t* vm, uint32_t fid, const void* data, size_t count, size_t* nwritten);
COGUTIL_API cog_result_t dis_vm_9p_close(dis_vm_t* vm, uint32_t fid);

/*===========================================================================
 * Statistics
 *===========================================================================*/

typedef struct dis_vm_stats {
    uint64_t instructions_executed;
    uint64_t threads_created;
    uint64_t threads_terminated;
    uint64_t gc_collections;
    size_t heap_used;
    size_t heap_peak;
    size_t modules_loaded;
} dis_vm_stats_t;

COGUTIL_API void dis_vm_get_stats(dis_vm_t* vm, dis_vm_stats_t* stats);

/*===========================================================================
 * Debugging
 *===========================================================================*/

typedef void (*dis_debug_callback_t)(dis_vm_t* vm, dis_thread_t* thread, void* user_data);

COGUTIL_API void dis_vm_set_breakpoint(dis_vm_t* vm, dis_module_t* module, size_t pc);
COGUTIL_API void dis_vm_clear_breakpoint(dis_vm_t* vm, dis_module_t* module, size_t pc);
COGUTIL_API void dis_vm_set_debug_callback(dis_vm_t* vm, dis_debug_callback_t callback, void* user_data);
COGUTIL_API void dis_vm_dump_thread(dis_vm_t* vm, dis_thread_t* thread);

#ifdef __cplusplus
}
#endif

#endif /* _COGWXP_DIS_H_ */
