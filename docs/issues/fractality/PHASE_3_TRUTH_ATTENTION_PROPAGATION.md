# Phase 3: Truth Value & Attention Propagation

## Overview

Implement bidirectional truth value and attention value propagation across the fractal hierarchy. This enables scale-invariant knowledge representation where beliefs and attention flow between levels.

## Objectives

- Implement downward truth value propagation (parent → children)
- Implement upward truth value aggregation (children → parent)
- Implement attention value propagation in both directions
- Create scale-aware propagation formulas
- Enable inherited value tracking

## Prerequisites

- Phase 1: Core Fractal Infrastructure (complete)
- Phase 2: Hierarchy Management (complete)

## Actionable Steps

### Step 3.1: Implement Downward Truth Value Propagation

**Task:** Propagate truth values from parent to children

**Functions to Implement:**
```c
COGUTIL_API cog_result_t fractal_propagate_tv_down(
    fractal_atomspace_t fas,
    atom_handle_t root,
    uint32_t max_depth
);
```

**Actions:**
1. Get root atom's truth value
2. Traverse children recursively (up to max_depth)
3. Apply inheritance formula: `child_tv = parent_tv * FRACTAL_TV_INHERITANCE * scale_factor`
4. Store inherited TV in fractal_properties
5. Optionally blend with child's own TV

**Formula:**
```
inherited_strength = parent_strength * inheritance_weight * scale_factor
inherited_confidence = parent_confidence * inheritance_weight
blended_strength = α * own_strength + (1-α) * inherited_strength
```

**Deliverables:**
- [ ] `fractal_propagate_tv_down()` with depth limiting
- [ ] Inheritance weight configuration
- [ ] Scale-aware decay

---

### Step 3.2: Implement Upward Truth Value Aggregation

**Task:** Aggregate children's truth values into parent

**Functions to Implement:**
```c
COGUTIL_API cog_result_t fractal_propagate_tv_up(
    fractal_atomspace_t fas,
    atom_handle_t leaf
);

COGUTIL_API cog_result_t fractal_aggregate_tv(
    fractal_atomspace_t fas,
    atom_handle_t parent,
    truth_value_t* aggregated
);
```

**Actions:**
1. Collect all children's truth values
2. Apply aggregation formula (weighted average by scale)
3. Set parent's aggregated TV
4. Continue upward to ancestors if requested

**Aggregation Formulas:**
```c
// Weighted average aggregation
aggregated_strength = Σ(child_strength * child_scale) / Σ(child_scale)
aggregated_confidence = min(child_confidences) * (n_children / max_children)

// Geometric mean (for self-similar structures)
aggregated_strength = (Π child_strength)^(1/n)
```

**Deliverables:**
- [ ] `fractal_propagate_tv_up()` leaf-to-root propagation
- [ ] `fractal_aggregate_tv()` children aggregation
- [ ] Multiple aggregation strategy options

---

### Step 3.3: Implement Inherited TV Management

**Task:** Track and manage inherited truth values

**Functions to Implement:**
```c
COGUTIL_API cog_result_t fractal_set_inherited_tv(
    fractal_atomspace_t fas,
    atom_handle_t handle,
    truth_value_t tv,
    float weight
);
```

**Actions:**
1. Store inherited TV in fractal_properties
2. Store inheritance weight
3. Update blended TV if applicable
4. Mark atom as having inherited values

**Deliverables:**
- [ ] `fractal_set_inherited_tv()` with weight
- [ ] Inherited value storage in properties
- [ ] Blending calculation

---

### Step 3.4: Implement Downward Attention Propagation

**Task:** Propagate attention values from parent to children

**Functions to Implement:**
```c
COGUTIL_API cog_result_t fractal_propagate_attention_down(
    fractal_atomspace_t fas,
    atom_handle_t root,
    uint32_t max_depth
);
```

**Actions:**
1. Get root atom's attention value (STI, LTI, VLTI)
2. Distribute attention to children based on:
   - Equal distribution
   - Scale-weighted distribution
   - Importance-weighted distribution
3. Apply decay per level
4. Update children's attention values

**Attention Distribution Formula:**
```
child_sti = parent_sti * (child_scale / Σ sibling_scales) * decay_factor
child_lti = parent_lti * av_inheritance_weight
```

**Deliverables:**
- [ ] `fractal_propagate_attention_down()` with depth limit
- [ ] Multiple distribution strategies
- [ ] Attention decay configuration

---

### Step 3.5: Implement Upward Attention Propagation

**Task:** Aggregate attention from children to parent

**Functions to Implement:**
```c
COGUTIL_API cog_result_t fractal_propagate_attention_up(
    fractal_atomspace_t fas,
    atom_handle_t leaf
);
```

**Actions:**
1. Collect children's attention values
2. Aggregate STI (sum or max)
3. Aggregate LTI (max)
4. Update parent's attention value
5. Continue to ancestors if needed

**Deliverables:**
- [ ] `fractal_propagate_attention_up()` aggregation
- [ ] STI/LTI aggregation formulas

---

### Step 3.6: Implement Subtree Focus

**Task:** Boost attention for entire subtree

**Functions to Implement:**
```c
COGUTIL_API cog_result_t fractal_focus_subtree(
    fractal_atomspace_t fas,
    atom_handle_t root,
    int16_t sti_boost
);
```

**Actions:**
1. Apply STI boost to root atom
2. Propagate scaled boost to all descendants
3. Use scale factor for attenuation
4. Update attention allocator if integrated

**Deliverables:**
- [ ] `fractal_focus_subtree()` with scale-aware boost
- [ ] Integration with attention allocator

---

### Step 3.7: Implement Scale Operations

**Task:** Query and manipulate scale factors

**Functions to Implement:**
```c
COGUTIL_API float fractal_get_absolute_scale(
    fractal_atomspace_t fas,
    atom_handle_t handle
);

COGUTIL_API cog_result_t fractal_get_in_scale_range(
    fractal_atomspace_t fas,
    float min_scale,
    float max_scale,
    atom_handle_t** atoms,
    size_t* count
);
```

**Actions:**
1. Calculate absolute scale (product of ancestor scales)
2. Filter atoms by scale range
3. Use for multi-scale analysis

**Deliverables:**
- [ ] `fractal_get_absolute_scale()` cumulative scale
- [ ] `fractal_get_in_scale_range()` scale-based filtering

---

## Configuration Options

```c
typedef struct fractal_config {
    // Propagation defaults
    fractal_propagation_t default_propagation;
    float tv_inheritance_weight;   // Default: 0.9
    float av_inheritance_weight;   // Default: 0.7
    
    // Aggregation strategy
    enum {
        AGGREGATE_WEIGHTED_AVG,
        AGGREGATE_GEOMETRIC_MEAN,
        AGGREGATE_MAX,
        AGGREGATE_MIN
    } tv_aggregation_strategy;
} fractal_config_t;
```

## Scale Types to Support

```c
typedef enum {
    FRACTAL_SCALE_LINEAR,      // s' = k * s
    FRACTAL_SCALE_LOGARITHMIC, // s' = log(k * s)
    FRACTAL_SCALE_EXPONENTIAL, // s' = exp(k * s)
    FRACTAL_SCALE_POWER_LAW,   // s' = s^k
    FRACTAL_SCALE_CUSTOM       // Custom function
} fractal_scale_type_t;
```

## Testing Requirements

- [ ] Unit test: Propagate TV down single level
- [ ] Unit test: Propagate TV down multiple levels with decay
- [ ] Unit test: Aggregate TV from children
- [ ] Unit test: Propagate TV up to root
- [ ] Unit test: Propagate attention down with distribution
- [ ] Unit test: Propagate attention up with aggregation
- [ ] Unit test: Focus subtree with scale decay
- [ ] Unit test: Scale calculation for deep hierarchies
- [ ] Unit test: Different aggregation strategies

## Acceptance Criteria

1. Truth values propagate correctly in both directions
2. Attention values distribute and aggregate properly
3. Scale factors correctly attenuate propagated values
4. Inherited values are tracked and can be queried
5. Multiple aggregation strategies work correctly
6. Performance is acceptable for deep hierarchies

## Estimated Effort

- **Complexity:** High
- **Time Estimate:** 5-7 days
- **Risk Level:** Medium (numerical stability, formula correctness)

## Mathematical Notes

### Truth Value Formulas

For **SimpleTV** (strength, confidence):
- **Inheritance:** `child.s = parent.s * w; child.c = parent.c * sqrt(w)`
- **Aggregation (avg):** `parent.s = Σ(child.s * scale) / Σ(scale)`
- **Aggregation (geo):** `parent.s = exp(Σ(ln(child.s) * scale) / Σ(scale))`

### Attention Value Formulas

For **STI/LTI** distribution:
- **Equal:** `child.sti = parent.sti / n_children * decay`
- **Scaled:** `child.sti = parent.sti * (child.scale / Σ(scales)) * decay`

### Scale Decay

For depth `d` and base decay `k`:
- **Linear:** `scale(d) = 1 - k*d`
- **Exponential:** `scale(d) = k^d`
- **Power Law:** `scale(d) = d^(-k)`
