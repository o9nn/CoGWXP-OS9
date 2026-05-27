# Phase 5: Generation & Expansion

## Overview

Implement lazy generation and dynamic expansion of fractal structures. Generator atoms can create children on-demand, enabling infinite or large-scale fractal patterns without pre-allocating all levels.

## Objectives

- Implement generator atom creation
- Enable lazy (on-demand) child generation
- Implement expansion to target depth
- Enable subtree collapse for memory management
- Implement subtree cloning

## Prerequisites

- Phase 1: Core Fractal Infrastructure (complete)
- Phase 2: Hierarchy Management (complete)
- Phase 3: Truth Value & Attention Propagation (complete)

## Actionable Steps

### Step 5.1: Implement Generator Atom Creation

**Task:** Create atoms that can generate children dynamically

**Functions to Implement:**
```c
COGUTIL_API cog_result_t fractal_create_generator(
    fractal_atomspace_t fas,
    fractal_type_t type,
    const char* name,
    fractal_generator_fn generator,
    void* context,
    fractal_atom_t** atom
);
```

**Generator Callback Signature:**
```c
typedef cog_result_t (*fractal_generator_fn)(
    fractal_atom_t* parent,
    uint32_t child_index,
    fractal_atom_t** child,
    void* context
);
```

**Actions:**
1. Create fractal atom with FRACTAL_TYPE_GENERATOR_NODE
2. Store generator function pointer
3. Store generator context (user data)
4. Mark as generator in properties
5. Do NOT generate children immediately

**Deliverables:**
- [ ] `fractal_create_generator()` implementation
- [ ] Generator callback type definition
- [ ] Context storage and lifecycle

---

### Step 5.2: Implement Lazy Generation

**Task:** Generate children only when accessed

**Functions to Implement:**
```c
// Internal helper
static cog_result_t generate_child(
    fractal_atomspace_t* fas,
    fractal_atom_t* parent,
    uint32_t index
);

// Modified accessor
COGUTIL_API cog_result_t fractal_get_children(
    fractal_atomspace_t fas,
    atom_handle_t handle,
    atom_handle_t** children,
    size_t* count
);
```

**Actions:**
1. Check if atom is a generator
2. If generator and enable_lazy_generation:
   - Generate children on first access
   - Cache generated children
3. Return children (generated or pre-existing)

**Configuration:**
```c
typedef struct fractal_config {
    bool enable_lazy_generation;   // Generate children on access
    uint32_t lazy_gen_batch_size;  // Children to generate per access
    size_t max_generated_children; // Limit total generated
} fractal_config_t;
```

**Deliverables:**
- [ ] Lazy generation in `fractal_get_children()`
- [ ] Configuration options for lazy generation
- [ ] Generated child caching

---

### Step 5.3: Implement Depth Expansion

**Task:** Expand generator to specified depth

**Functions to Implement:**
```c
COGUTIL_API cog_result_t fractal_expand_generator(
    fractal_atomspace_t fas,
    atom_handle_t generator,
    uint32_t target_depth
);
```

**Actions:**
1. Validate atom is a generator
2. Calculate current expanded depth
3. For each level from current to target:
   - Generate children for all atoms at that level
   - Propagate fractal properties
4. Update statistics
5. Respect max_depth config limit

**Expansion Algorithm:**
```c
cog_result_t expand_to_depth(fas, generator, target_depth) {
    queue = [generator];
    while (!empty(queue)) {
        atom = dequeue(queue);
        if (atom->properties.depth >= target_depth) continue;
        
        for (i = 0; i < desired_children; i++) {
            generate_child(fas, atom, i);
            enqueue(queue, atom->children[i]);
        }
    }
    return COG_SUCCESS;
}
```

**Deliverables:**
- [ ] `fractal_expand_generator()` implementation
- [ ] Depth-limited expansion
- [ ] Statistics updates

---

### Step 5.4: Implement Subtree Collapse

**Task:** Remove generated children to save memory

**Functions to Implement:**
```c
COGUTIL_API cog_result_t fractal_collapse_subtree(
    fractal_atomspace_t fas,
    atom_handle_t root,
    uint32_t keep_depth
);
```

**Actions:**
1. Traverse subtree depth-first
2. For atoms deeper than keep_depth:
   - Remove from parent's child list
   - Free fractal atom structure
   - Optionally remove from base atomspace
3. Reset generator to unexpanded state
4. Update statistics

**Collapse Algorithm:**
```c
cog_result_t collapse(fas, root, keep_depth) {
    // Post-order traversal (children first)
    for each child of root {
        collapse(fas, child, keep_depth);
    }
    
    if (root->properties.depth > keep_depth) {
        remove_from_parent(root);
        free_fractal_atom(root);
    }
    return COG_SUCCESS;
}
```

**Deliverables:**
- [ ] `fractal_collapse_subtree()` implementation
- [ ] Proper cleanup of collapsed atoms
- [ ] Generator state reset

---

### Step 5.5: Implement Subtree Cloning

**Task:** Clone a subtree to a new location

**Functions to Implement:**
```c
COGUTIL_API cog_result_t fractal_clone_subtree(
    fractal_atomspace_t fas,
    atom_handle_t source_root,
    atom_handle_t dest_parent,
    uint32_t max_depth,
    atom_handle_t* new_root
);
```

**Actions:**
1. Create copy of source_root under dest_parent
2. Recursively clone children up to max_depth
3. Recalculate properties (depth, scale) for cloned subtree
4. Do NOT clone generator functions (mark as non-generator)
5. Preserve truth values and names

**Clone Algorithm:**
```c
cog_result_t clone_subtree(fas, source, dest_parent, max_depth, new_root) {
    // Create clone of source
    clone = create_node_like(fas, source, dest_parent);
    
    // Clone children if within depth
    if (source->properties.depth < max_depth) {
        for each child of source {
            atom_handle_t cloned_child;
            clone_subtree(fas, child, clone->handle, max_depth, &cloned_child);
        }
    }
    
    *new_root = clone->handle;
    return COG_SUCCESS;
}
```

**Deliverables:**
- [ ] `fractal_clone_subtree()` implementation
- [ ] Depth-limited cloning
- [ ] Property recalculation for clones

---

### Step 5.6: Implement Built-in Generators

**Task:** Create common fractal pattern generators

**Built-in Generator Functions:**
```c
// Binary tree generator (D ≈ 2)
cog_result_t fractal_gen_binary_tree(
    fractal_atom_t* parent,
    uint32_t child_index,
    fractal_atom_t** child,
    void* context
);

// Sierpinski triangle generator (D ≈ 1.585)
cog_result_t fractal_gen_sierpinski(
    fractal_atom_t* parent,
    uint32_t child_index,
    fractal_atom_t** child,
    void* context
);

// Cantor set generator (D ≈ 0.631)
cog_result_t fractal_gen_cantor(
    fractal_atom_t* parent,
    uint32_t child_index,
    fractal_atom_t** child,
    void* context
);

// Balanced n-ary tree generator
cog_result_t fractal_gen_nary_tree(
    fractal_atom_t* parent,
    uint32_t child_index,
    fractal_atom_t** child,
    void* context  // n stored in context
);
```

**Deliverables:**
- [ ] Binary tree generator
- [ ] Sierpinski generator
- [ ] Cantor set generator
- [ ] N-ary tree generator
- [ ] Generator documentation

---

### Step 5.7: Implement Mirror Atoms

**Task:** Create atoms that mirror parent structure

**Functions to Implement:**
```c
COGUTIL_API cog_result_t fractal_create_mirror(
    fractal_atomspace_t fas,
    atom_handle_t source,
    atom_handle_t parent,
    fractal_atom_t** mirror
);
```

**Actions:**
1. Create FRACTAL_TYPE_MIRROR_NODE
2. Store reference to source atom
3. When accessed, reflect source's current structure
4. Scale properties appropriately

**Deliverables:**
- [ ] `fractal_create_mirror()` implementation
- [ ] Mirror update mechanism

---

## Generator Context Management

```c
typedef struct fractal_gen_context {
    uint32_t max_children;         // Max children per node
    float child_scale;             // Scale factor for children
    void* user_data;               // User-provided data
    void (*cleanup)(void*);        // Cleanup function for user_data
} fractal_gen_context_t;

COGUTIL_API fractal_gen_context_t* fractal_gen_context_create(
    uint32_t max_children,
    float child_scale,
    void* user_data,
    void (*cleanup)(void*)
);

COGUTIL_API void fractal_gen_context_destroy(fractal_gen_context_t* ctx);
```

## Testing Requirements

- [ ] Unit test: Create generator atom
- [ ] Unit test: Lazy generation on access
- [ ] Unit test: Expand to target depth
- [ ] Unit test: Collapse subtree
- [ ] Unit test: Clone subtree
- [ ] Unit test: Binary tree generator
- [ ] Unit test: N-ary tree generator
- [ ] Unit test: Mirror atom creation
- [ ] Unit test: Memory usage under expansion/collapse cycles

## Acceptance Criteria

1. Generator atoms create children on demand
2. Expansion respects depth limits
3. Collapse properly frees memory
4. Cloning preserves structure correctly
5. Built-in generators produce expected patterns
6. No memory leaks in expand/collapse cycles

## Estimated Effort

- **Complexity:** High
- **Time Estimate:** 5-7 days
- **Risk Level:** Medium (memory management, infinite generation)

## Performance Considerations

### Memory Management

Generators enable very deep structures. Implement safeguards:
- `max_total_atoms` config limit
- `max_depth` config limit
- Automatic collapse of unused subtrees (LRU)

### Generation Speed

For high-performance generation:
- Batch generate children
- Cache generator function results
- Pool allocate fractal atoms

### Common Fractal Dimensions

| Pattern | Children | Scale | Dimension |
|---------|----------|-------|-----------|
| Binary Tree | 2 | 0.5 | 2.0 |
| Ternary Tree | 3 | 0.33 | 2.0 |
| Sierpinski | 3 | 0.5 | 1.585 |
| Cantor Set | 2 | 0.33 | 0.631 |
| Koch Curve | 4 | 0.33 | 1.262 |
