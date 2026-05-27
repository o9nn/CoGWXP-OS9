# Fractality Implementation Roadmap

## Overview

This document provides a comprehensive roadmap for implementing the **Fractal AtomSpace** extension for CoGWXP-OS9. The fractality subsystem extends the base AtomSpace with self-similar recursive hypergraph structures, enabling hierarchical knowledge representation where patterns repeat at different scales.

## Vision

The Fractal AtomSpace enables:
- **Self-similar knowledge structures** - Patterns that repeat at multiple scales
- **Scale-invariant reasoning** - Truth values and attention propagate across hierarchy levels
- **Lazy generation** - Infinite or large-scale fractal patterns without pre-allocation
- **Fractal pattern matching** - Finding recurring patterns across scales
- **Integration with AGI systems** - Seamless connection with PLN, Niche Construction, and Beast Mode

## Phase Summary

| Phase | Name | Description | Estimated Time | Dependencies |
|-------|------|-------------|----------------|--------------|
| 1 | Core Infrastructure | Data structures, types, lifecycle | 3-5 days | None |
| 2 | Hierarchy Management | Parent-child relations, ancestry | 4-6 days | Phase 1 |
| 3 | TV/AV Propagation | Truth and attention value flow | 5-7 days | Phase 1, 2 |
| 4 | Pattern Matching | Self-similarity detection | 6-8 days | Phase 1, 2, 3 |
| 5 | Generation & Expansion | Lazy generation, cloning | 5-7 days | Phase 1, 2, 3 |
| 6 | Persistence & Statistics | Save/load, metrics | 4-5 days | Phase 1, 2 |
| 7 | Traversal & Integration | Algorithms, system integration | 7-10 days | Phase 1-6 |

**Total Estimated Time: 34-48 days**

## Phase Details

### Phase 1: Core Fractal Infrastructure
**[PHASE_1_CORE_INFRASTRUCTURE.md](PHASE_1_CORE_INFRASTRUCTURE.md)**

Establishes the foundational data structures and lifecycle management:
- `fractal_atomspace_t` - Wrapper around base AtomSpace
- `fractal_atom_t` - Extended atom with hierarchy info
- `fractal_properties_t` - Scale, dimension, propagation settings
- Fractal type system (nodes, links, meta-types)
- Creation and destruction functions

**Key Functions:**
- `fractal_atomspace_create()`
- `fractal_atomspace_destroy()`
- `fractal_create_node()`
- `fractal_create_link()`

---

### Phase 2: Hierarchy Management
**[PHASE_2_HIERARCHY_MANAGEMENT.md](PHASE_2_HIERARCHY_MANAGEMENT.md)**

Implements the complete hierarchy system:
- Parent-child relationships
- Ancestry and descendant queries
- Reparenting operations
- Depth-level queries
- Sibling connections

**Key Functions:**
- `fractal_add_child()`
- `fractal_remove_child()`
- `fractal_reparent()`
- `fractal_get_ancestors()`
- `fractal_get_descendants()`

---

### Phase 3: Truth Value & Attention Propagation
**[PHASE_3_TRUTH_ATTENTION_PROPAGATION.md](PHASE_3_TRUTH_ATTENTION_PROPAGATION.md)**

Implements bidirectional value propagation:
- Downward TV inheritance (parent → children)
- Upward TV aggregation (children → parent)
- Attention distribution and aggregation
- Scale-aware decay formulas
- Subtree focus operations

**Key Functions:**
- `fractal_propagate_tv_down()`
- `fractal_propagate_tv_up()`
- `fractal_propagate_attention_down()`
- `fractal_propagate_attention_up()`
- `fractal_focus_subtree()`

---

### Phase 4: Pattern Matching & Self-Similarity
**[PHASE_4_PATTERN_MATCHING_SELF_SIMILARITY.md](PHASE_4_PATTERN_MATCHING_SELF_SIMILARITY.md)**

Implements scale-invariant pattern recognition:
- Multi-scale pattern matching
- Self-similarity detection
- Fractal dimension calculation
- Structure comparison algorithms

**Key Functions:**
- `fractal_pattern_match_all_scales()`
- `fractal_find_self_similar()`
- `fractal_calculate_dimension()`
- `fractal_check_self_similarity()`

---

### Phase 5: Generation & Expansion
**[PHASE_5_GENERATION_EXPANSION.md](PHASE_5_GENERATION_EXPANSION.md)**

Implements lazy generation and dynamic structures:
- Generator atom creation
- On-demand child generation
- Depth-targeted expansion
- Subtree collapse for memory management
- Subtree cloning
- Built-in fractal generators

**Key Functions:**
- `fractal_create_generator()`
- `fractal_expand_generator()`
- `fractal_collapse_subtree()`
- `fractal_clone_subtree()`

---

### Phase 6: Persistence & Statistics
**[PHASE_6_PERSISTENCE_STATISTICS.md](PHASE_6_PERSISTENCE_STATISTICS.md)**

Implements storage and monitoring:
- Binary save/load format
- Scheme expression export
- Comprehensive statistics collection
- Memory usage tracking
- Pruning and compaction

**Key Functions:**
- `fractal_save()`
- `fractal_load()`
- `fractal_export_scheme()`
- `fractal_get_stats()`
- `fractal_prune_by_scale()`
- `fractal_compact()`

---

### Phase 7: Traversal & Integration
**[PHASE_7_TRAVERSAL_INTEGRATION.md](PHASE_7_TRAVERSAL_INTEGRATION.md)**

Implements algorithms and system integration:
- Depth-first and breadth-first traversal
- Scale-based traversal
- 9P filesystem integration (/fractal/)
- CogServer command handlers
- PLN fractal reasoning rules
- Niche Construction integration
- Beast Mode integration

**Key Functions:**
- `fractal_traverse_depth_first()`
- `fractal_traverse_breadth_first()`
- `fractal_traverse_by_scale()`
- CogServer/PLN/Niche/Beast integrations

---

## Architecture Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                    Fractal AtomSpace                         │
│                                                               │
│  ┌─────────────────────────────────────────────────────────┐ │
│  │ Phase 7: Integration                                     │ │
│  │  - 9P Filesystem (/fractal/)                            │ │
│  │  - CogServer Commands                                   │ │
│  │  - PLN Rules                                            │ │
│  │  - Niche/Beast Integration                              │ │
│  └─────────────────────────────────────────────────────────┘ │
│                                                               │
│  ┌─────────────────────────────────────────────────────────┐ │
│  │ Phase 4-6: Advanced Features                             │ │
│  │  ┌─────────────┐ ┌─────────────┐ ┌─────────────────────┐ │ │
│  │  │ Pattern     │ │ Generation  │ │ Persistence         │ │ │
│  │  │ Matching    │ │ Expansion   │ │ Statistics          │ │ │
│  │  └─────────────┘ └─────────────┘ └─────────────────────┘ │ │
│  └─────────────────────────────────────────────────────────┘ │
│                                                               │
│  ┌─────────────────────────────────────────────────────────┐ │
│  │ Phase 2-3: Core Operations                               │ │
│  │  ┌─────────────────────┐ ┌─────────────────────────────┐ │ │
│  │  │ Hierarchy           │ │ TV/AV Propagation           │ │ │
│  │  │ Management          │ │                             │ │ │
│  │  └─────────────────────┘ └─────────────────────────────┘ │ │
│  └─────────────────────────────────────────────────────────┘ │
│                                                               │
│  ┌─────────────────────────────────────────────────────────┐ │
│  │ Phase 1: Core Infrastructure                             │ │
│  │  - Data Structures                                       │ │
│  │  - Type System                                           │ │
│  │  - Lifecycle Management                                  │ │
│  │  - Memory Management                                     │ │
│  └─────────────────────────────────────────────────────────┘ │
│                                                               │
│  ┌─────────────────────────────────────────────────────────┐ │
│  │ Base AtomSpace                                           │ │
│  └─────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
```

## API Overview

### Types

```c
// Opaque handle
typedef struct fractal_atomspace* fractal_atomspace_t;

// Atom structure
typedef struct fractal_atom {
    atom_handle_t handle;
    atom_handle_t parent;
    atom_handle_t* children;
    size_t child_count;
    fractal_properties_t properties;
    // ...
} fractal_atom_t;

// Properties
typedef struct fractal_properties {
    uint32_t depth;
    float fractal_dimension;
    float scale_factor;
    fractal_scale_type_t scale_type;
    fractal_propagation_t propagation;
    // ...
} fractal_properties_t;

// Configuration
typedef struct fractal_config {
    atomspace_t base_atomspace;
    uint32_t max_depth;
    float default_scale_decay;
    bool enable_lazy_generation;
    // ...
} fractal_config_t;
```

### Key Functions

```c
// Lifecycle
fractal_atomspace_create(config, &fas);
fractal_atomspace_destroy(fas, preserve_base);

// Atom Creation
fractal_create_node(fas, type, name, parent, props, &atom);
fractal_create_link(fas, type, outgoing, count, parent, props, &atom);
fractal_create_generator(fas, type, name, generator_fn, context, &atom);

// Hierarchy
fractal_add_child(fas, parent, child);
fractal_get_parent(fas, handle);
fractal_get_children(fas, handle, &children, &count);
fractal_get_descendants(fas, handle, max_depth, &descendants, &count);

// Propagation
fractal_propagate_tv_down(fas, root, max_depth);
fractal_propagate_tv_up(fas, leaf);
fractal_focus_subtree(fas, root, sti_boost);

// Pattern Matching
fractal_pattern_match_all_scales(fas, pattern, &matches, &count);
fractal_find_self_similar(fas, root, min_sim, &matches, &sims, &count);
fractal_calculate_dimension(fas, root, method, &dimension);

// Generation
fractal_expand_generator(fas, generator, target_depth);
fractal_collapse_subtree(fas, root, keep_depth);
fractal_clone_subtree(fas, source, dest_parent, max_depth, &new_root);

// Traversal
fractal_traverse_depth_first(fas, root, max_depth, visitor, user_data);
fractal_traverse_breadth_first(fas, root, max_depth, visitor, user_data);

// Persistence
fractal_save(fas, path);
fractal_load(fas, path);
fractal_get_stats(fas, &stats);
```

## Success Criteria

1. **Correctness** - All operations produce mathematically correct results
2. **Performance** - Handles millions of fractal atoms efficiently
3. **Memory Safety** - No leaks, proper cleanup, handles out-of-memory
4. **Integration** - Seamlessly works with existing CoGWXP-OS9 systems
5. **Stability** - Handles edge cases, cycles, and self-references safely

## Testing Strategy

- **Unit Tests** - Per-function testing for all phases
- **Integration Tests** - Cross-phase functionality
- **Performance Tests** - Large-scale fractal structures
- **Stress Tests** - Memory limits, deep hierarchies, many children
- **Fuzz Tests** - Invalid inputs, random operations

## Documentation Deliverables

- [ ] API reference documentation
- [ ] Usage examples and tutorials
- [ ] Integration guides
- [ ] Performance tuning guide
- [ ] Troubleshooting guide

## Risk Mitigation

| Risk | Mitigation |
|------|------------|
| Infinite loops (self-reference) | Cycle detection in all traversals |
| Memory exhaustion | Generation limits, pruning, compaction |
| Performance degradation | Caching, indexing, lazy evaluation |
| Integration complexity | Clear interfaces, comprehensive testing |

## Contact

For questions or issues, please file a GitHub issue with the `fractality` label.

---

*This roadmap is a living document and will be updated as implementation progresses.*
