# Phase 4: Pattern Matching & Self-Similarity

## Overview

Implement scale-invariant pattern matching and self-similarity detection for fractal atoms. This enables finding recurring patterns across different levels of the hierarchy and measuring the fractal dimension of knowledge structures.

## Objectives

- Implement multi-scale pattern matching
- Create self-similarity detection algorithms
- Implement fractal dimension calculation
- Enable structure comparison across scales

## Prerequisites

- Phase 1: Core Fractal Infrastructure (complete)
- Phase 2: Hierarchy Management (complete)
- Phase 3: Truth Value & Attention Propagation (complete)

## Actionable Steps

### Step 4.1: Implement All-Scales Pattern Matching

**Task:** Match a pattern at all scales in the fractal hierarchy

**Functions to Implement:**
```c
COGUTIL_API cog_result_t fractal_pattern_match_all_scales(
    fractal_atomspace_t fas,
    atom_handle_t pattern,
    atom_handle_t** matches,
    size_t* count
);
```

**Actions:**
1. Extract pattern structure (nodes, links, connections)
2. Traverse entire fractal hierarchy
3. At each level, compare structure with pattern
4. Apply scale-invariant comparison (ignore absolute scale)
5. Collect all matching substructures
6. Return matches with their scale/depth context

**Pattern Comparison Algorithm:**
```c
// Structural equivalence check
bool structures_match(fractal_atom_t* a, fractal_atom_t* b) {
    // 1. Same type
    if (get_type(a) != get_type(b)) return false;
    
    // 2. Same arity (for links)
    if (a->child_count != b->child_count) return false;
    
    // 3. Recursive match on children
    for (size_t i = 0; i < a->child_count; i++) {
        if (!structures_match(a->children[i], b->children[i]))
            return false;
    }
    return true;
}
```

**Deliverables:**
- [ ] `fractal_pattern_match_all_scales()` implementation
- [ ] Scale-invariant comparison logic
- [ ] Match result data structure

---

### Step 4.2: Implement Depth-Specific Pattern Matching

**Task:** Match patterns only at a specific depth level

**Functions to Implement:**
```c
COGUTIL_API cog_result_t fractal_pattern_match_at_depth(
    fractal_atomspace_t fas,
    atom_handle_t pattern,
    uint32_t depth,
    atom_handle_t** matches,
    size_t* count
);
```

**Actions:**
1. Filter to atoms at specified depth
2. Apply pattern matching only to those atoms
3. Return matches at that depth

**Deliverables:**
- [ ] `fractal_pattern_match_at_depth()` implementation
- [ ] Efficient depth-filtered search

---

### Step 4.3: Implement Self-Similarity Detection

**Task:** Find substructures that are similar to ancestors

**Functions to Implement:**
```c
COGUTIL_API cog_result_t fractal_find_self_similar(
    fractal_atomspace_t fas,
    atom_handle_t root,
    float min_similarity,
    atom_handle_t** matches,
    float** similarities,
    size_t* count
);
```

**Actions:**
1. For each descendant of root
2. Compare structure to root (ignoring scale)
3. Calculate similarity score
4. If similarity >= min_similarity, add to results
5. Return matches with their similarity scores

**Similarity Calculation:**
```c
float calculate_similarity(fractal_atom_t* a, fractal_atom_t* b) {
    // Jaccard similarity of structure
    size_t intersection = count_matching_substructures(a, b);
    size_t union_size = count_total_substructures(a) + 
                        count_total_substructures(b) - intersection;
    return (float)intersection / (float)union_size;
}
```

**Deliverables:**
- [ ] `fractal_find_self_similar()` implementation
- [ ] Similarity score calculation
- [ ] Min similarity threshold filtering

---

### Step 4.4: Implement Self-Similarity Check

**Task:** Check if two specific atoms are self-similar

**Functions to Implement:**
```c
COGUTIL_API cog_result_t fractal_check_self_similarity(
    fractal_atomspace_t fas,
    atom_handle_t atom1,
    atom_handle_t atom2,
    float* similarity_score
);
```

**Actions:**
1. Compare structural topology
2. Compare type distributions
3. Compare branching patterns
4. Calculate overall similarity score

**Deliverables:**
- [ ] `fractal_check_self_similarity()` pairwise comparison
- [ ] Detailed similarity metric

---

### Step 4.5: Implement Fractal Dimension Calculation

**Task:** Calculate the fractal dimension of a structure

**Functions to Implement:**
```c
COGUTIL_API cog_result_t fractal_calculate_dimension(
    fractal_atomspace_t fas,
    atom_handle_t root,
    fractal_dimension_method_t method,
    float* dimension
);
```

**Dimension Calculation Methods:**

```c
typedef enum {
    FRACTAL_DIM_HAUSDORFF,      // Hausdorff dimension
    FRACTAL_DIM_BOX_COUNTING,   // Box-counting dimension
    FRACTAL_DIM_CORRELATION,    // Correlation dimension
    FRACTAL_DIM_INFORMATION,    // Information dimension
    FRACTAL_DIM_ESTIMATED       // Estimated from structure
} fractal_dimension_method_t;
```

**Box-Counting Algorithm:**
```c
float box_counting_dimension(fractal_atom_t* root) {
    // Count atoms at each scale level
    size_t counts[FRACTAL_MAX_DEPTH];
    for (uint32_t d = 0; d < max_depth; d++) {
        counts[d] = count_atoms_at_depth(root, d);
    }
    
    // Fit log-log linear regression
    // D = -slope of log(N(ε)) vs log(ε)
    return linear_regression_slope(log_scales, log_counts);
}
```

**Estimated Dimension (from branching factor):**
```c
float estimate_dimension(fractal_atom_t* root) {
    float avg_branching = calculate_avg_branching(root);
    float scale_decay = root->properties.scale_factor;
    
    // D = log(N) / log(1/r) where N = branching, r = scale
    return log(avg_branching) / log(1.0f / scale_decay);
}
```

**Deliverables:**
- [ ] `fractal_calculate_dimension()` with method selection
- [ ] Box-counting implementation
- [ ] Estimated dimension calculation
- [ ] Log-log regression utility

---

### Step 4.6: Implement Fractal Properties Access

**Task:** Get and set fractal properties

**Functions to Implement:**
```c
COGUTIL_API cog_result_t fractal_set_properties(
    fractal_atomspace_t fas,
    atom_handle_t handle,
    const fractal_properties_t* properties
);

COGUTIL_API cog_result_t fractal_get_properties(
    fractal_atomspace_t fas,
    atom_handle_t handle,
    fractal_properties_t* properties
);
```

**Actions:**
1. Validate handle
2. Copy properties to/from atom
3. Validate property ranges (dimension [1,3], scale (0,1])

**Deliverables:**
- [ ] `fractal_set_properties()` with validation
- [ ] `fractal_get_properties()` accessor

---

### Step 4.7: Implement Recursive/Self-Referential Support

**Task:** Handle atoms that reference themselves

**Functions to Implement:**
```c
COGUTIL_API cog_result_t fractal_create_recursive(
    fractal_atomspace_t fas,
    fractal_type_t type,
    const char* name,
    fractal_atom_t** atom
);

// Internal helpers
static bool is_self_referential(fractal_atom_t* atom);
static atom_handle_t resolve_self_reference(fractal_atom_t* atom);
```

**Actions:**
1. Create atom with self-reference placeholder
2. Set self-reference handle to own handle
3. Mark as self-referential in properties
4. Handle in pattern matching (prevent infinite loops)

**Deliverables:**
- [ ] Self-referential atom creation
- [ ] Loop detection in pattern matching
- [ ] Self-reference resolution

---

## Matching Result Structure

```c
typedef struct fractal_match_result {
    atom_handle_t* matches;        // Array of matching atoms
    float* similarity_scores;      // Similarity score for each match
    uint32_t* depths;              // Depth of each match
    float* scales;                 // Scale of each match
    size_t count;                  // Number of matches
} fractal_match_result_t;

COGUTIL_API void fractal_match_result_free(fractal_match_result_t* result);
```

## Testing Requirements

- [ ] Unit test: Pattern match at single scale
- [ ] Unit test: Pattern match across all scales
- [ ] Unit test: Find self-similar structures
- [ ] Unit test: Calculate box-counting dimension
- [ ] Unit test: Calculate estimated dimension
- [ ] Unit test: Handle self-referential atoms
- [ ] Unit test: Pattern match with min similarity threshold
- [ ] Unit test: Empty/single-node structures

## Acceptance Criteria

1. Pattern matching finds all structural matches
2. Self-similarity detection correctly identifies recurring patterns
3. Fractal dimension calculation produces valid results (1.0-3.0 range)
4. Self-referential atoms are handled without infinite loops
5. Performance is acceptable for large hierarchies

## Estimated Effort

- **Complexity:** High
- **Time Estimate:** 6-8 days
- **Risk Level:** Medium-High (algorithm complexity, infinite loop risk)

## Mathematical Background

### Fractal Dimension

The **fractal dimension** D measures self-similarity:
- D = 1 for a line
- D = 2 for a plane
- D ∈ (1, 2) for fractal curves like Koch snowflake (D ≈ 1.26)
- D ∈ (2, 3) for fractal surfaces

For discrete hierarchies:
```
D = log(N) / log(1/r)
```
where N = number of self-similar pieces, r = scaling factor.

### Self-Similarity Metrics

**Structural Similarity:**
- Compare topology (node/link types)
- Compare arity (branching pattern)
- Ignore absolute scale

**Statistical Similarity:**
- Compare branching factor distribution
- Compare depth distribution
- Compare type frequency distribution

### Pattern Matching Complexity

- **Naive:** O(n² * m) where n = atoms, m = pattern size
- **Optimized (hash-based):** O(n * m) with structural hashing
- **Scale-filtered:** O(n/k * m) where k = number of scales
