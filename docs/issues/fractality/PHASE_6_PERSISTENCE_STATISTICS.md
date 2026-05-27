# Phase 6: Persistence & Statistics

## Overview

Implement persistence mechanisms for fractal atomspaces and comprehensive statistics collection. Enable saving/loading fractal structures and monitoring performance/memory metrics.

## Objectives

- Implement binary persistence format
- Enable Scheme expression export
- Create comprehensive statistics tracking
- Implement memory management utilities
- Enable fractal structure analysis

## Prerequisites

- Phase 1: Core Fractal Infrastructure (complete)
- Phase 2: Hierarchy Management (complete)

## Actionable Steps

### Step 6.1: Implement Binary Save

**Task:** Save fractal atomspace to binary file

**Functions to Implement:**
```c
COGUTIL_API cog_result_t fractal_save(
    fractal_atomspace_t fas,
    const char* path
);
```

**Binary Format:**
```
Header (32 bytes):
  - Magic number: "FRAC" (4 bytes)
  - Version: uint32_t (4 bytes)
  - Atom count: size_t (8 bytes)
  - Config hash: uint64_t (8 bytes)
  - Reserved: (8 bytes)

Config section:
  - Full fractal_config_t serialized

Atom section (per atom):
  - Handle: atom_handle_t (8 bytes)
  - Type: uint32_t (4 bytes)
  - Parent: atom_handle_t (8 bytes)
  - Child count: size_t (8 bytes)
  - Children: atom_handle_t[] (child_count * 8 bytes)
  - Properties: fractal_properties_t (serialized)
  - Name length: uint32_t (4 bytes)
  - Name: char[] (name_length bytes)
  - Generator flag: bool (1 byte)
```

**Actions:**
1. Open file for binary write
2. Write header with metadata
3. Serialize configuration
4. For each fractal atom:
   - Serialize handle, type, hierarchy
   - Serialize properties
   - Serialize name if node
   - Mark generators (cannot serialize functions)
5. Close file

**Deliverables:**
- [ ] `fractal_save()` implementation
- [ ] Binary format specification
- [ ] Error handling for I/O

---

### Step 6.2: Implement Binary Load

**Task:** Load fractal atomspace from binary file

**Functions to Implement:**
```c
COGUTIL_API cog_result_t fractal_load(
    fractal_atomspace_t fas,
    const char* path
);
```

**Actions:**
1. Open file for binary read
2. Read and validate header
3. Check version compatibility
4. Load configuration
5. For each serialized atom:
   - Create fractal atom
   - Restore hierarchy relationships
   - Restore properties
   - Restore name
   - Mark former generators (no function available)
6. Rebuild indices and statistics
7. Close file

**Loading Considerations:**
- Handle handle remapping if base atomspace has conflicts
- Support incremental loading into existing atomspace
- Validate structural integrity

**Deliverables:**
- [ ] `fractal_load()` implementation
- [ ] Handle remapping support
- [ ] Version compatibility checks

---

### Step 6.3: Implement Scheme Export

**Task:** Export subtree as Scheme s-expression

**Functions to Implement:**
```c
COGUTIL_API cog_result_t fractal_export_scheme(
    fractal_atomspace_t fas,
    atom_handle_t root,
    uint32_t max_depth,
    char** scheme_expr
);
```

**Scheme Format:**
```scheme
(FractalNode "root" 
  (fractal-props 
    (depth 0) 
    (dimension 1.5) 
    (scale 1.0))
  (children
    (FractalNode "child1" 
      (fractal-props (depth 1) (dimension 1.5) (scale 0.5))
      (children ...))
    (FractalNode "child2" ...)))
```

**Actions:**
1. Traverse subtree depth-first
2. Generate Scheme expression for each atom
3. Include fractal properties
4. Recurse into children up to max_depth
5. Return allocated string (caller frees)

**Deliverables:**
- [ ] `fractal_export_scheme()` implementation
- [ ] Proper string escaping
- [ ] Depth limiting

---

### Step 6.4: Implement Statistics Collection

**Task:** Track comprehensive statistics

**Statistics Structure:**
```c
typedef struct fractal_stats {
    /* Atom counts */
    size_t total_fractal_atoms;
    size_t atoms_by_depth[FRACTAL_MAX_DEPTH];
    size_t generator_atoms;
    size_t self_referential_atoms;
    
    /* Hierarchy metrics */
    uint32_t max_observed_depth;
    float avg_depth;
    float avg_branching_factor;
    float avg_fractal_dimension;
    
    /* Scale metrics */
    float min_scale_factor;
    float max_scale_factor;
    float avg_scale_factor;
    
    /* Performance */
    uint64_t traversals;
    uint64_t generations;
    uint64_t cache_hits;
    uint64_t cache_misses;
    
    /* Memory */
    size_t memory_used_bytes;
    size_t peak_memory_bytes;
    
    /* Truth value stats */
    float avg_tv_strength;
    float avg_tv_confidence;
    
    /* Attention stats */
    int64_t total_sti;
    int64_t total_lti;
} fractal_stats_t;
```

**Functions to Implement:**
```c
COGUTIL_API cog_result_t fractal_get_stats(
    fractal_atomspace_t fas,
    fractal_stats_t* stats
);

COGUTIL_API void fractal_reset_stats(fractal_atomspace_t fas);
```

**Actions:**
1. Calculate aggregate statistics
2. Track performance counters
3. Calculate averages
4. Track memory usage

**Deliverables:**
- [ ] `fractal_get_stats()` implementation
- [ ] `fractal_reset_stats()` counter reset
- [ ] Real-time statistics updates

---

### Step 6.5: Implement Memory Analysis

**Task:** Analyze memory usage patterns

**Functions to Implement:**
```c
COGUTIL_API size_t fractal_get_memory_usage(
    fractal_atomspace_t fas
);

COGUTIL_API cog_result_t fractal_get_memory_breakdown(
    fractal_atomspace_t fas,
    size_t* atoms_bytes,
    size_t* children_bytes,
    size_t* properties_bytes,
    size_t* names_bytes
);
```

**Actions:**
1. Calculate memory for atom structures
2. Calculate memory for children arrays
3. Calculate memory for names
4. Calculate overhead

**Deliverables:**
- [ ] `fractal_get_memory_usage()` total usage
- [ ] `fractal_get_memory_breakdown()` detailed breakdown

---

### Step 6.6: Implement Pruning

**Task:** Remove atoms below minimum scale

**Functions to Implement:**
```c
COGUTIL_API cog_result_t fractal_prune_by_scale(
    fractal_atomspace_t fas,
    float min_scale
);
```

**Actions:**
1. Find atoms with scale < min_scale
2. Remove those atoms and their descendants
3. Update parent references
4. Update statistics

**Deliverables:**
- [ ] `fractal_prune_by_scale()` implementation
- [ ] Cascading removal of descendants

---

### Step 6.7: Implement Compaction

**Task:** Compact atomspace, removing unused space

**Functions to Implement:**
```c
COGUTIL_API cog_result_t fractal_compact(fractal_atomspace_t fas);
```

**Actions:**
1. Identify unused slots in atom array
2. Compact array by moving atoms
3. Update handles (if needed)
4. Shrink array allocation if possible
5. Compact child arrays similarly

**Deliverables:**
- [ ] `fractal_compact()` implementation
- [ ] Efficient compaction algorithm

---

### Step 6.8: Implement Memory Freeing Utilities

**Task:** Provide memory cleanup functions

**Functions to Implement:**
```c
COGUTIL_API void fractal_atom_free(fractal_atom_t* atom);
COGUTIL_API void fractal_handles_free(atom_handle_t* handles);
```

**Actions:**
1. Free fractal atom structure
2. Free children array
3. Free siblings array
4. Free name if owned

**Deliverables:**
- [ ] `fractal_atom_free()` complete cleanup
- [ ] `fractal_handles_free()` array cleanup

---

## Persistence Configuration

```c
typedef struct fractal_config {
    /* Persistence */
    bool enable_persistence;
    const char* persistence_path;
    
    /* Auto-save */
    bool enable_autosave;
    uint32_t autosave_interval_ms;
    
    /* Checkpointing */
    bool enable_checkpoints;
    size_t checkpoint_frequency;  // Save every N modifications
} fractal_config_t;
```

## Testing Requirements

- [ ] Unit test: Save empty atomspace
- [ ] Unit test: Save atomspace with atoms
- [ ] Unit test: Load saved atomspace
- [ ] Unit test: Save/load round-trip integrity
- [ ] Unit test: Scheme export
- [ ] Unit test: Statistics collection
- [ ] Unit test: Memory usage tracking
- [ ] Unit test: Pruning by scale
- [ ] Unit test: Compaction

## Acceptance Criteria

1. Saved atomspaces can be fully restored
2. Scheme export produces valid Scheme code
3. Statistics accurately reflect atomspace state
4. Memory tracking matches actual usage
5. Pruning removes correct atoms
6. Compaction reclaims memory

## Estimated Effort

- **Complexity:** Medium
- **Time Estimate:** 4-5 days
- **Risk Level:** Low-Medium (file I/O, memory management)

## File Format Versioning

### Version History

| Version | Features |
|---------|----------|
| 1 | Initial format with basic atoms |
| 2 | Added generator flags |
| 3 | Added sibling arrays |

### Backward Compatibility

- Always support loading older versions
- Provide upgrade path for loaded data
- Document version differences

## Performance Targets

| Operation | Target |
|-----------|--------|
| Save 1M atoms | < 5 seconds |
| Load 1M atoms | < 5 seconds |
| Get stats | < 10 ms |
| Memory query | < 1 ms |
| Compact | < 1 second |
