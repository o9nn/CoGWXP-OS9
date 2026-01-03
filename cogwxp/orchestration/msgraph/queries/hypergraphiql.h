/**
 * @file hypergraphiql.h
 * @brief HyperGraphiQL Query Language for AtomSpace + MS Graph
 * 
 * HyperGraphiQL extends GraphQL with hypergraph-specific operations
 * for querying and manipulating the unified AtomSpace + MS Graph
 * knowledge base.
 * 
 * Features:
 * - Standard GraphQL query/mutation/subscription
 * - Hyperedge traversal operators
 * - Pattern matching on typed hypergraphs
 * - Attention-based filtering
 * - PLN inference integration
 * - MS Graph entity resolution
 * 
 * @copyright CoGWXP-OS9 Project
 */

#ifndef _COGWXP_HYPERGRAPHIQL_H_
#define _COGWXP_HYPERGRAPHIQL_H_

#include "../../opencog/atomspace/atomspace.h"
#include "../atomspace/ms_hypergraph.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * HyperGraphiQL Schema Types
 *===========================================================================*/

/**
 * Built-in scalar types
 */
typedef enum {
    HGQL_SCALAR_STRING,
    HGQL_SCALAR_INT,
    HGQL_SCALAR_FLOAT,
    HGQL_SCALAR_BOOLEAN,
    HGQL_SCALAR_ID,
    
    /* Extended scalars */
    HGQL_SCALAR_ATOM_HANDLE,
    HGQL_SCALAR_TRUTH_VALUE,
    HGQL_SCALAR_ATTENTION_VALUE,
    HGQL_SCALAR_ATOM_TYPE,
    HGQL_SCALAR_ENTITY_ID,
    HGQL_SCALAR_JSON,
    HGQL_SCALAR_DATETIME
} hgql_scalar_type_t;

/**
 * HyperGraphiQL type definition
 */
typedef struct hgql_type_def* hgql_type_def_t;

/**
 * HyperGraphiQL field definition
 */
typedef struct hgql_field_def* hgql_field_def_t;

/**
 * HyperGraphiQL schema
 */
typedef struct hgql_schema* hgql_schema_t;

/*===========================================================================
 * Query AST Types
 *===========================================================================*/

/**
 * Query operation types
 */
typedef enum {
    HGQL_OP_QUERY,
    HGQL_OP_MUTATION,
    HGQL_OP_SUBSCRIPTION
} hgql_operation_type_t;

/**
 * Selection types
 */
typedef enum {
    HGQL_SEL_FIELD,
    HGQL_SEL_FRAGMENT_SPREAD,
    HGQL_SEL_INLINE_FRAGMENT
} hgql_selection_type_t;

/**
 * Directive types (extended for hypergraph)
 */
typedef enum {
    HGQL_DIR_SKIP,
    HGQL_DIR_INCLUDE,
    HGQL_DIR_DEPRECATED,
    
    /* Hypergraph extensions */
    HGQL_DIR_ATOM,              /* @atom(handle: ID!) */
    HGQL_DIR_ENTITY,            /* @entity(id: ID!, type: EntityType!) */
    HGQL_DIR_TRAVERSE,          /* @traverse(depth: Int!, direction: Direction) */
    HGQL_DIR_PATTERN,           /* @pattern(template: String!) */
    HGQL_DIR_ATTENTION,         /* @attention(minSTI: Int, minLTI: Int) */
    HGQL_DIR_INFER,             /* @infer(rule: String!, maxSteps: Int) */
    HGQL_DIR_SIMILARITY,        /* @similarity(threshold: Float!) */
    HGQL_DIR_CACHE,             /* @cache(ttl: Int!) */
    HGQL_DIR_STREAM             /* @stream(initialCount: Int!) */
} hgql_directive_type_t;

/**
 * Parsed query document
 */
typedef struct hgql_document* hgql_document_t;

/**
 * Query operation
 */
typedef struct hgql_operation* hgql_operation_t;

/**
 * Selection set
 */
typedef struct hgql_selection_set* hgql_selection_set_t;

/**
 * Field selection
 */
typedef struct hgql_field* hgql_field_t;

/**
 * Directive
 */
typedef struct hgql_directive* hgql_directive_t;

/**
 * Variable definition
 */
typedef struct hgql_variable_def* hgql_variable_def_t;

/**
 * Argument
 */
typedef struct hgql_argument* hgql_argument_t;

/*===========================================================================
 * Query Execution Context
 *===========================================================================*/

/**
 * Execution context
 */
typedef struct {
    atomspace_t atomspace;
    msgraph_context_t msgraph;
    hgql_schema_t schema;
    
    /* Variables */
    const char* variables_json;
    
    /* Execution options */
    uint32_t max_depth;
    uint32_t max_results;
    uint32_t timeout_ms;
    bool enable_inference;
    bool enable_caching;
    
    /* Attention filtering */
    int16_t min_sti;
    int16_t min_lti;
    
    /* Error handling */
    bool stop_on_error;
    
    /* User context */
    void* user_data;
} hgql_context_t;

/**
 * Execution result
 */
typedef struct {
    char* data_json;
    size_t data_size;
    
    char* errors_json;
    size_t errors_size;
    
    char* extensions_json;
    size_t extensions_size;
    
    /* Atom results */
    atom_handle_t* atoms;
    size_t atom_count;
    
    /* Performance metrics */
    double parse_time_ms;
    double validate_time_ms;
    double execute_time_ms;
    uint64_t atoms_visited;
    uint64_t msgraph_calls;
} hgql_result_t;

/*===========================================================================
 * Schema API
 *===========================================================================*/

/**
 * Create default schema with AtomSpace + MS Graph types
 */
COGUTIL_API cog_result_t hgql_schema_create_default(hgql_schema_t* schema);

/**
 * Create schema from SDL (Schema Definition Language)
 */
COGUTIL_API cog_result_t hgql_schema_create_from_sdl(
    const char* sdl,
    hgql_schema_t* schema
);

/**
 * Extend schema with custom types
 */
COGUTIL_API cog_result_t hgql_schema_extend(
    hgql_schema_t schema,
    const char* extension_sdl
);

/**
 * Get schema as SDL
 */
COGUTIL_API cog_result_t hgql_schema_to_sdl(
    hgql_schema_t schema,
    char** sdl,
    size_t* sdl_size
);

/**
 * Destroy schema
 */
COGUTIL_API void hgql_schema_destroy(hgql_schema_t schema);

/*===========================================================================
 * Query Parsing
 *===========================================================================*/

/**
 * Parse HyperGraphiQL query
 */
COGUTIL_API cog_result_t hgql_parse(
    const char* query,
    hgql_document_t* document
);

/**
 * Validate document against schema
 */
COGUTIL_API cog_result_t hgql_validate(
    hgql_document_t document,
    hgql_schema_t schema,
    char** errors,
    size_t* error_count
);

/**
 * Free parsed document
 */
COGUTIL_API void hgql_document_free(hgql_document_t document);

/*===========================================================================
 * Query Execution
 *===========================================================================*/

/**
 * Execute HyperGraphiQL query
 */
COGUTIL_API cog_result_t hgql_execute(
    hgql_document_t document,
    const char* operation_name,
    hgql_context_t* context,
    hgql_result_t* result
);

/**
 * Execute query string directly
 */
COGUTIL_API cog_result_t hgql_execute_string(
    const char* query,
    const char* operation_name,
    hgql_context_t* context,
    hgql_result_t* result
);

/**
 * Free execution result
 */
COGUTIL_API void hgql_result_free(hgql_result_t* result);

/*===========================================================================
 * Subscription API
 *===========================================================================*/

/**
 * Subscription callback
 */
typedef void (*hgql_subscription_callback_t)(
    const hgql_result_t* result,
    void* user_data
);

/**
 * Subscription handle
 */
typedef struct hgql_subscription* hgql_subscription_t;

/**
 * Create subscription
 */
COGUTIL_API cog_result_t hgql_subscribe(
    hgql_document_t document,
    const char* operation_name,
    hgql_context_t* context,
    hgql_subscription_callback_t callback,
    void* user_data,
    hgql_subscription_t* subscription
);

/**
 * Cancel subscription
 */
COGUTIL_API cog_result_t hgql_unsubscribe(hgql_subscription_t subscription);

/*===========================================================================
 * Hypergraph-Specific Operations
 *===========================================================================*/

/**
 * Traverse hyperedges from atom
 * 
 * @traverse directive implementation
 */
COGUTIL_API cog_result_t hgql_traverse(
    hgql_context_t* context,
    atom_handle_t start,
    uint32_t depth,
    bool incoming,
    bool outgoing,
    atom_handle_t** results,
    size_t* count
);

/**
 * Pattern match on hypergraph
 * 
 * @pattern directive implementation
 */
COGUTIL_API cog_result_t hgql_pattern_match(
    hgql_context_t* context,
    const char* pattern,
    atom_handle_t** results,
    size_t* count
);

/**
 * Filter by attention value
 * 
 * @attention directive implementation
 */
COGUTIL_API cog_result_t hgql_attention_filter(
    hgql_context_t* context,
    atom_handle_t* atoms,
    size_t count,
    int16_t min_sti,
    int16_t min_lti,
    atom_handle_t** filtered,
    size_t* filtered_count
);

/**
 * Run PLN inference
 * 
 * @infer directive implementation
 */
COGUTIL_API cog_result_t hgql_infer(
    hgql_context_t* context,
    const char* rule,
    atom_handle_t* premises,
    size_t premise_count,
    uint32_t max_steps,
    atom_handle_t** conclusions,
    size_t* conclusion_count
);

/**
 * Find similar atoms
 * 
 * @similarity directive implementation
 */
COGUTIL_API cog_result_t hgql_similarity(
    hgql_context_t* context,
    atom_handle_t reference,
    float threshold,
    atom_handle_t** similar,
    float** scores,
    size_t* count
);

/*===========================================================================
 * Built-in Query Types
 *===========================================================================*/

/**
 * Default schema SDL (partial)
 * 
 * type Query {
 *   # AtomSpace queries
 *   atom(handle: AtomHandle!): Atom
 *   atoms(type: AtomType, limit: Int, offset: Int): [Atom!]!
 *   pattern(template: String!, variables: JSON): [Atom!]!
 *   
 *   # MS Graph queries
 *   me: User
 *   user(id: ID!): User
 *   users(filter: String, limit: Int): [User!]!
 *   group(id: ID!): Group
 *   groups(filter: String, limit: Int): [Group!]!
 *   
 *   # Unified queries
 *   search(query: String!, types: [String!]): [SearchResult!]!
 *   traverse(start: AtomHandle!, depth: Int!, direction: Direction): [Atom!]!
 *   infer(goal: String!, maxSteps: Int): InferenceResult!
 * }
 * 
 * type Mutation {
 *   # AtomSpace mutations
 *   createAtom(type: AtomType!, name: String, outgoing: [AtomHandle!]): Atom!
 *   updateAtom(handle: AtomHandle!, tv: TruthValueInput, av: AttentionValueInput): Atom!
 *   deleteAtom(handle: AtomHandle!): Boolean!
 *   
 *   # MS Graph mutations
 *   createUser(input: UserInput!): User!
 *   updateUser(id: ID!, input: UserInput!): User!
 *   deleteUser(id: ID!): Boolean!
 * }
 * 
 * type Subscription {
 *   atomChanged(type: AtomType): AtomChange!
 *   entityChanged(type: EntityType): EntityChange!
 *   inferenceProgress(goal: String!): InferenceProgress!
 * }
 */

/*===========================================================================
 * Introspection
 *===========================================================================*/

/**
 * Get schema introspection result
 */
COGUTIL_API cog_result_t hgql_introspect(
    hgql_schema_t schema,
    char** result_json,
    size_t* result_size
);

/**
 * Get type information
 */
COGUTIL_API cog_result_t hgql_get_type(
    hgql_schema_t schema,
    const char* type_name,
    char** type_json,
    size_t* type_size
);

/*===========================================================================
 * Query Analysis
 *===========================================================================*/

/**
 * Query complexity analysis result
 */
typedef struct {
    uint32_t depth;
    uint32_t breadth;
    uint32_t estimated_cost;
    bool has_inference;
    bool has_traversal;
    bool has_subscription;
    char** required_permissions;
    size_t permission_count;
} hgql_complexity_t;

/**
 * Analyze query complexity
 */
COGUTIL_API cog_result_t hgql_analyze_complexity(
    hgql_document_t document,
    hgql_schema_t schema,
    hgql_complexity_t* complexity
);

/**
 * Free complexity analysis
 */
COGUTIL_API void hgql_complexity_free(hgql_complexity_t* complexity);

#ifdef __cplusplus
}
#endif

#endif /* _COGWXP_HYPERGRAPHIQL_H_ */
