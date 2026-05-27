# Phase 1: Core Fractal Infrastructure

## Overview

Establish the foundational infrastructure for the Fractal AtomSpace extension, enabling self-similar recursive hypergraph structures within the CoGWXP-OS9 cognitive operating system.

## Objectives

- Implement core fractal atom data structures
- Create fractal atomspace lifecycle management
- Establish fractal type system (nodes, links, meta-types)
- Build memory management foundations

## Actionable Steps

### Step 1.1: Define Core Data Structures

**Task:** Implement the fundamental fractal atom structures

**Functions to Implement:**
```c
// In fractal_atoms.c
struct fractal_atomspace {
    atomspace_t* base;
    fractal_config_t config;
    fractal_stats_t stats;
    fractal_atom_t** atoms;
    size_t atom_count;
    size_t atom_capacity;
    bool owns_base;
};
```

**Actions:**
1. Define `fractal_atom_t` structure with hierarchy pointers
2. Define `fractal_properties_t` for scale/dimension metadata
3. Define `fractal_config_t` for configuration options
4. Define `fractal_stats_t` for runtime statistics

**Deliverables:**
- [ ] `fractal_atom_t` struct complete with all fields
- [ ] `fractal_properties_t` struct with scale/dimension fields
- [ ] `fractal_config_t` struct with all configuration options
- [ ] `fractal_stats_t` struct with counters and metrics

---

### Step 1.2: Implement Fractal Type System

**Task:** Create extended atom types for fractal structures

**Functions to Implement:**
```c
typedef enum {
    FRACTAL_TYPE_NODE = 800,
    FRACTAL_TYPE_RECURSIVE_NODE,
    FRACTAL_TYPE_SCALE_NODE,
    FRACTAL_TYPE_CONTAINER_NODE,
    FRACTAL_TYPE_MIRROR_NODE,
    FRACTAL_TYPE_GENERATOR_NODE,
    FRACTAL_TYPE_LINK = 850,
    // ... link types
    FRACTAL_TYPE_META = 900,
    // ... meta types
} fractal_type_t;
```

**Actions:**
1. Define node types starting at 800
2. Define link types starting at 850
3. Define meta-fractal types starting at 900
4. Create type validation helper functions

**Deliverables:**
- [ ] Complete `fractal_type_t` enumeration
- [ ] Type validation helper: `fractal_type_is_valid()`
- [ ] Type category helpers: `fractal_type_is_node()`, `fractal_type_is_link()`

---

### Step 1.3: Implement Lifecycle Functions

**Task:** Create atomspace creation and destruction

**Functions to Implement:**
```c
COGUTIL_API cog_result_t fractal_atomspace_create(
    const fractal_config_t* config,
    fractal_atomspace_t* fas
);

COGUTIL_API void fractal_atomspace_destroy(
    fractal_atomspace_t fas,
    bool preserve_base
);

COGUTIL_API atomspace_t fractal_atomspace_get_base(
    fractal_atomspace_t fas
);
```

**Actions:**
1. Implement `fractal_atomspace_create()`:
   - Allocate fractal_atomspace struct
   - Initialize with default or provided config
   - Create or attach to base atomspace
   - Initialize internal atom array
2. Implement `fractal_atomspace_destroy()`:
   - Free all fractal atoms
   - Optionally preserve base atomspace
   - Clean up internal structures
3. Implement `fractal_atomspace_get_base()`:
   - Return underlying atomspace handle

**Deliverables:**
- [ ] `fractal_atomspace_create()` with full error handling
- [ ] `fractal_atomspace_destroy()` with optional base preservation
- [ ] `fractal_atomspace_get_base()` accessor

---

### Step 1.4: Implement Memory Allocation Helpers

**Task:** Create internal memory management utilities

**Functions to Implement:**
```c
static fractal_atom_t* alloc_fractal_atom(fractal_atomspace_t* fas);
static void free_fractal_atom(fractal_atom_t* atom);
static bool ensure_capacity(fractal_atomspace_t* fas, size_t needed);
```

**Actions:**
1. Implement atom allocation with capacity growth
2. Implement atom deallocation with child cleanup
3. Implement dynamic array resizing

**Deliverables:**
- [ ] `alloc_fractal_atom()` with automatic capacity management
- [ ] `free_fractal_atom()` with proper cleanup
- [ ] `ensure_capacity()` for dynamic array growth

---

### Step 1.5: Implement Basic Atom Creation

**Task:** Create fractal nodes and links

**Functions to Implement:**
```c
COGUTIL_API cog_result_t fractal_create_node(
    fractal_atomspace_t fas,
    fractal_type_t type,
    const char* name,
    atom_handle_t parent,
    const fractal_properties_t* properties,
    fractal_atom_t** atom
);

COGUTIL_API cog_result_t fractal_create_link(
    fractal_atomspace_t fas,
    fractal_type_t type,
    const atom_handle_t* outgoing,
    size_t outgoing_count,
    atom_handle_t parent,
    const fractal_properties_t* properties,
    fractal_atom_t** atom
);
```

**Actions:**
1. Validate input parameters
2. Allocate fractal atom structure
3. Create underlying atom in base atomspace
4. Set fractal properties (depth, scale, dimension)
5. Register with parent if provided
6. Update statistics

**Deliverables:**
- [ ] `fractal_create_node()` for fractal nodes
- [ ] `fractal_create_link()` for fractal links
- [ ] Default property initialization

---

## Constants to Define

```c
#define FRACTAL_MAX_DEPTH           32
#define FRACTAL_DEFAULT_DIMENSION   1.5
#define FRACTAL_MAX_CHILDREN        1024
#define FRACTAL_DEFAULT_SCALE_DECAY 0.5
#define FRACTAL_TV_INHERITANCE      0.9
#define FRACTAL_MIN_SCALE           0.001
```

## Dependencies

- `atomspace.h` - Base AtomSpace API
- `cogutil.h` - Common utilities and logging
- Standard C libraries: `<stdlib.h>`, `<string.h>`, `<math.h>`

## Testing Requirements

- [ ] Unit test: Create and destroy empty fractal atomspace
- [ ] Unit test: Create fractal node with default properties
- [ ] Unit test: Create fractal link with outgoing set
- [ ] Unit test: Memory allocation growth under load
- [ ] Unit test: Type validation functions

## Acceptance Criteria

1. Fractal atomspace can be created with various configurations
2. Fractal nodes and links can be created successfully
3. Memory management handles growth and cleanup correctly
4. All fractal types are properly defined and validatable
5. No memory leaks in lifecycle operations

## Estimated Effort

- **Complexity:** Medium
- **Time Estimate:** 3-5 days
- **Risk Level:** Low

## Notes

This phase establishes the foundation for all subsequent phases. Quality and correctness here are critical for the stability of the entire fractal subsystem.
