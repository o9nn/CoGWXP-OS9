# Phase 2: Hierarchy Management

## Overview

Implement the complete hierarchy management system for fractal atoms, enabling parent-child relationships, ancestry tracking, and sibling connections within the self-similar recursive structure.

## Objectives

- Implement parent-child relationship management
- Create hierarchy traversal utilities
- Enable reparenting and structural modifications
- Build ancestry and descendant queries

## Prerequisites

- Phase 1: Core Fractal Infrastructure (complete)

## Actionable Steps

### Step 2.1: Implement Child Management

**Task:** Add and remove children from fractal atoms

**Functions to Implement:**
```c
COGUTIL_API cog_result_t fractal_add_child(
    fractal_atomspace_t fas,
    atom_handle_t parent,
    atom_handle_t child
);

COGUTIL_API cog_result_t fractal_remove_child(
    fractal_atomspace_t fas,
    atom_handle_t parent,
    atom_handle_t child
);
```

**Actions:**
1. Validate parent and child handles
2. Check for existing relationship (prevent duplicates)
3. Grow child array if needed
4. Set child's parent pointer
5. Update depth and scale properties
6. Update statistics

**Deliverables:**
- [ ] `fractal_add_child()` with duplicate prevention
- [ ] `fractal_remove_child()` with proper cleanup
- [ ] Child array dynamic growth

---

### Step 2.2: Implement Reparenting

**Task:** Move atoms to new parents in the hierarchy

**Functions to Implement:**
```c
COGUTIL_API cog_result_t fractal_reparent(
    fractal_atomspace_t fas,
    atom_handle_t atom,
    atom_handle_t new_parent
);
```

**Actions:**
1. Remove from current parent's child list
2. Add to new parent's child list
3. Recalculate depth for moved subtree
4. Recalculate scale factors
5. Update inherited TV/AV values

**Deliverables:**
- [ ] `fractal_reparent()` with subtree depth recalculation
- [ ] Cascading property updates for moved subtree

---

### Step 2.3: Implement Atom Lookup

**Task:** Find fractal atoms efficiently

**Functions to Implement:**
```c
COGUTIL_API cog_result_t fractal_get_atom(
    fractal_atomspace_t fas,
    atom_handle_t handle,
    fractal_atom_t** atom
);

static fractal_atom_t* find_atom(
    fractal_atomspace_t* fas,
    atom_handle_t handle
);
```

**Actions:**
1. Implement linear search (initial version)
2. Consider hash table for O(1) lookup (optimization)
3. Handle invalid handles gracefully

**Deliverables:**
- [ ] `fractal_get_atom()` public lookup function
- [ ] `find_atom()` internal helper
- [ ] Handle validation

---

### Step 2.4: Implement Parent and Children Accessors

**Task:** Query immediate hierarchy relationships

**Functions to Implement:**
```c
COGUTIL_API atom_handle_t fractal_get_parent(
    fractal_atomspace_t fas,
    atom_handle_t handle
);

COGUTIL_API cog_result_t fractal_get_children(
    fractal_atomspace_t fas,
    atom_handle_t handle,
    atom_handle_t** children,
    size_t* count
);
```

**Actions:**
1. Get parent handle from fractal atom
2. Copy children array to output (caller owns memory)
3. Return count of children

**Deliverables:**
- [ ] `fractal_get_parent()` parent accessor
- [ ] `fractal_get_children()` with allocated output array

---

### Step 2.5: Implement Ancestor Query

**Task:** Get path from atom to root

**Functions to Implement:**
```c
COGUTIL_API cog_result_t fractal_get_ancestors(
    fractal_atomspace_t fas,
    atom_handle_t handle,
    atom_handle_t** ancestors,
    size_t* count
);
```

**Actions:**
1. Follow parent pointers to root
2. Collect handles in array (leaf to root order)
3. Handle root atoms (no ancestors)
4. Detect and prevent infinite loops (self-reference safety)

**Deliverables:**
- [ ] `fractal_get_ancestors()` with full path
- [ ] Cycle detection for self-referential structures

---

### Step 2.6: Implement Descendant Query

**Task:** Get all descendants recursively

**Functions to Implement:**
```c
COGUTIL_API cog_result_t fractal_get_descendants(
    fractal_atomspace_t fas,
    atom_handle_t handle,
    uint32_t max_depth,
    atom_handle_t** descendants,
    size_t* count
);
```

**Actions:**
1. Recursively collect all children
2. Respect max_depth limit
3. Handle generator atoms (trigger expansion if needed)
4. Return flat array of all descendants

**Deliverables:**
- [ ] `fractal_get_descendants()` with depth limiting
- [ ] Generator atom handling

---

### Step 2.7: Implement Sibling Query

**Task:** Get atoms with same parent

**Functions to Implement:**
```c
COGUTIL_API cog_result_t fractal_get_siblings(
    fractal_atomspace_t fas,
    atom_handle_t handle,
    atom_handle_t** siblings,
    size_t* count
);
```

**Actions:**
1. Get parent atom
2. Get parent's children
3. Filter out the queried atom
4. Return remaining siblings

**Deliverables:**
- [ ] `fractal_get_siblings()` excluding self

---

### Step 2.8: Implement Depth Query

**Task:** Query atoms by depth level

**Functions to Implement:**
```c
COGUTIL_API uint32_t fractal_get_depth(
    fractal_atomspace_t fas,
    atom_handle_t handle
);

COGUTIL_API cog_result_t fractal_get_at_depth(
    fractal_atomspace_t fas,
    uint32_t depth,
    atom_handle_t** atoms,
    size_t* count
);
```

**Actions:**
1. Return stored depth from fractal properties
2. Collect all atoms at specified depth level
3. Use stats.atoms_by_depth for optimization

**Deliverables:**
- [ ] `fractal_get_depth()` depth accessor
- [ ] `fractal_get_at_depth()` depth level query

---

## Internal Helpers to Implement

```c
static void recalculate_depth(fractal_atom_t* atom, uint32_t new_depth);
static void recalculate_scale(fractal_atom_t* atom, float parent_scale);
static bool detect_cycle(fractal_atomspace_t* fas, atom_handle_t start, atom_handle_t target);
```

## Testing Requirements

- [ ] Unit test: Add child to parent
- [ ] Unit test: Remove child from parent
- [ ] Unit test: Reparent atom
- [ ] Unit test: Get ancestors for deep hierarchy
- [ ] Unit test: Get descendants with depth limit
- [ ] Unit test: Get siblings
- [ ] Unit test: Query atoms at specific depth
- [ ] Unit test: Cycle detection

## Acceptance Criteria

1. Parent-child relationships are correctly maintained
2. Reparenting updates all relevant properties
3. Ancestor/descendant queries handle deep hierarchies
4. Cycle detection prevents infinite loops
5. Memory is properly allocated and freed for query results

## Estimated Effort

- **Complexity:** Medium-High
- **Time Estimate:** 4-6 days
- **Risk Level:** Medium (recursive operations, memory management)

## Notes

Hierarchy management is central to the fractal concept. Correctness in depth calculation and cycle detection is critical for system stability.
