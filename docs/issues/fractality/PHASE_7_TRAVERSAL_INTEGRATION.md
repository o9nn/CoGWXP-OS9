# Phase 7: Traversal & Integration

## Overview

Implement comprehensive traversal algorithms and integrate the fractal subsystem with the broader CoGWXP-OS9 ecosystem, including the 9P filesystem, cognitive services, and existing AtomSpace.

## Objectives

- Implement depth-first and breadth-first traversal
- Implement scale-based traversal
- Integrate with 9P filesystem for /fractal/ namespace
- Connect with CogServer and agents
- Enable fractal atoms in PLN reasoning

## Prerequisites

- Phase 1-6: All core fractal functionality (complete)

## Actionable Steps

### Step 7.1: Implement Depth-First Traversal

**Task:** Traverse fractal hierarchy depth-first

**Functions to Implement:**
```c
COGUTIL_API cog_result_t fractal_traverse_depth_first(
    fractal_atomspace_t fas,
    atom_handle_t root,
    uint32_t max_depth,
    fractal_visitor_fn visitor,
    void* user_data
);
```

**Visitor Callback:**
```c
typedef bool (*fractal_visitor_fn)(
    fractal_atom_t* atom,
    uint32_t depth,
    void* user_data
);
// Return true to continue, false to stop traversal
```

**Actions:**
1. Start at root atom
2. Call visitor for root
3. Recursively visit children (pre-order)
4. Respect max_depth limit
5. Stop early if visitor returns false
6. Handle self-referential atoms (cycle detection)

**Traversal Algorithm:**
```c
bool traverse_dfs(atom, depth, max_depth, visitor, user_data, visited) {
    if (depth > max_depth) return true;
    if (set_contains(visited, atom->handle)) return true;  // Cycle
    
    set_add(visited, atom->handle);
    
    if (!visitor(atom, depth, user_data)) return false;  // Stop
    
    for (size_t i = 0; i < atom->child_count; i++) {
        fractal_atom_t* child = get_child(atom, i);
        if (!traverse_dfs(child, depth+1, max_depth, visitor, user_data, visited))
            return false;
    }
    return true;
}
```

**Deliverables:**
- [ ] `fractal_traverse_depth_first()` pre-order DFS
- [ ] Cycle detection via visited set
- [ ] Early termination support

---

### Step 7.2: Implement Breadth-First Traversal

**Task:** Traverse fractal hierarchy breadth-first (level by level)

**Functions to Implement:**
```c
COGUTIL_API cog_result_t fractal_traverse_breadth_first(
    fractal_atomspace_t fas,
    atom_handle_t root,
    uint32_t max_depth,
    fractal_visitor_fn visitor,
    void* user_data
);
```

**Actions:**
1. Use queue for BFS
2. Visit atoms level by level
3. Respect max_depth
4. Handle cycles

**Traversal Algorithm:**
```c
cog_result_t traverse_bfs(fas, root, max_depth, visitor, user_data) {
    Queue queue;
    Set visited;
    
    queue_push(queue, (root, 0));
    
    while (!queue_empty(queue)) {
        (atom, depth) = queue_pop(queue);
        
        if (set_contains(visited, atom->handle)) continue;
        set_add(visited, atom->handle);
        
        if (!visitor(atom, depth, user_data)) break;
        
        if (depth < max_depth) {
            for each child of atom {
                queue_push(queue, (child, depth+1));
            }
        }
    }
    return COG_SUCCESS;
}
```

**Deliverables:**
- [ ] `fractal_traverse_breadth_first()` level-order BFS
- [ ] Queue-based implementation

---

### Step 7.3: Implement Scale-Based Traversal

**Task:** Traverse by scale factor (largest to smallest or vice versa)

**Functions to Implement:**
```c
COGUTIL_API cog_result_t fractal_traverse_by_scale(
    fractal_atomspace_t fas,
    atom_handle_t root,
    bool ascending,
    fractal_visitor_fn visitor,
    void* user_data
);
```

**Actions:**
1. Collect all atoms in subtree
2. Sort by absolute scale factor
3. Visit in scale order
4. If ascending: smallest scale first (deepest)
5. If descending: largest scale first (shallowest)

**Deliverables:**
- [ ] `fractal_traverse_by_scale()` scale-ordered traversal
- [ ] Efficient sorting by scale

---

### Step 7.4: Integrate with 9P Filesystem

**Task:** Expose fractal atomspace via 9P

**9P Namespace Layout:**
```
/fractal/
├── atoms/
│   ├── by-handle/
│   │   ├── 0x0001      # Fractal atom files
│   │   └── 0x0002
│   ├── by-depth/
│   │   ├── 0/
│   │   ├── 1/
│   │   └── 2/
│   └── by-scale/
│       ├── 1.0/
│       ├── 0.5/
│       └── 0.25/
├── generators/
│   ├── binary-tree
│   ├── sierpinski
│   └── cantor
├── stats              # Read-only stats file
└── ctl               # Control file
```

**File Operations:**
```c
// Reading /fractal/atoms/by-handle/0x0001
// Returns atom info in text format:
"type: FRACTAL_TYPE_NODE
name: root
depth: 0
scale: 1.0
dimension: 1.5
children: [0x0002, 0x0003, 0x0004]
parent: (none)
tv: (0.8, 0.9)
"

// Writing to /fractal/ctl
// Commands:
"expand 0x0001 5"      // Expand generator to depth 5
"collapse 0x0001 2"    // Collapse to depth 2
"prune 0.01"           // Prune atoms below scale 0.01
"compact"              // Compact atomspace
"save /path/to/file"   // Save to file
"load /path/to/file"   // Load from file
```

**Deliverables:**
- [ ] 9P filesystem module for fractal
- [ ] Read operations for atoms
- [ ] Write operations for control
- [ ] Stats file

---

### Step 7.5: Integrate with CogServer

**Task:** Enable fractal operations from CogServer agents

**CogServer Commands:**
```scheme
; Create fractal atom
(fractal-node 'FractalTypeNode "root" 
              :parent #f
              :dimension 1.5
              :scale 1.0)

; Add child
(fractal-add-child root-handle child-handle)

; Traverse
(fractal-traverse root-handle 
                  #:max-depth 5
                  #:visitor (lambda (atom depth) 
                              (display atom) #t))

; Get statistics
(fractal-get-stats)

; Pattern match
(fractal-pattern-match pattern-atom)
```

**Agent Integration:**
```c
// Register fractal message handlers with CogServer
void fractal_cogserver_init(cogserver_t server) {
    cogserver_register_handler(server, "fractal-node", handle_fractal_node);
    cogserver_register_handler(server, "fractal-add-child", handle_add_child);
    cogserver_register_handler(server, "fractal-traverse", handle_traverse);
    cogserver_register_handler(server, "fractal-stats", handle_stats);
}
```

**Deliverables:**
- [ ] CogServer command handlers
- [ ] Scheme bindings
- [ ] Agent message handling

---

### Step 7.6: Integrate with PLN

**Task:** Enable fractal atoms in probabilistic reasoning

**PLN Rules for Fractal:**
```scheme
; Fractal Inheritance Rule
; If parent has property P, child inherits P with scaled confidence
(define fractal-inheritance-rule
  (BindLink
    (VariableList
      (TypedVariableLink (VariableNode "$P") (TypeNode "FractalTypeNode"))
      (TypedVariableLink (VariableNode "$C") (TypeNode "FractalTypeNode")))
    (AndLink
      (FractalParentLink (VariableNode "$C") (VariableNode "$P"))
      (EvaluationLink (PredicateNode "has-property")
                      (ListLink (VariableNode "$P") (VariableNode "$PROP"))))
    (EvaluationLink (stv 0.9 0.8)  ; Inherited with decay
      (PredicateNode "has-property")
      (ListLink (VariableNode "$C") (VariableNode "$PROP")))))

; Fractal Similarity Rule
; Self-similar structures share properties
(define fractal-similarity-rule
  (BindLink
    (VariableList
      (TypedVariableLink (VariableNode "$A") (TypeNode "FractalTypeNode"))
      (TypedVariableLink (VariableNode "$B") (TypeNode "FractalTypeNode")))
    (AndLink
      (FractalSimilarityLink (VariableNode "$A") (VariableNode "$B"))
      (EvaluationLink (PredicateNode "has-property")
                      (ListLink (VariableNode "$A") (VariableNode "$PROP"))))
    (EvaluationLink (stv 0.7 0.6)
      (PredicateNode "has-property")
      (ListLink (VariableNode "$B") (VariableNode "$PROP")))))
```

**Deliverables:**
- [ ] Fractal PLN rules
- [ ] Scale-aware truth value calculation
- [ ] Fractal similarity in reasoning

---

### Step 7.7: Integrate with Niche Construction

**Task:** Use fractal structures in skill learning

**Integration Points:**
```c
// Store learned skills as fractal atoms
// Skill variations at different scales
cog_result_t niche_store_skill_fractal(
    niche_engine_t engine,
    niche_skill_t* skill,
    fractal_atomspace_t fas,
    atom_handle_t* skill_atom
);

// Search for skills using fractal pattern matching
cog_result_t niche_query_skills_fractal(
    niche_engine_t engine,
    fractal_atomspace_t fas,
    atom_handle_t pattern,
    niche_skill_t** skills,
    size_t* count
);
```

**Deliverables:**
- [ ] Skill storage as fractal atoms
- [ ] Pattern-based skill retrieval

---

### Step 7.8: Integrate with Beast Mode

**Task:** Use fractal structures in cognitive fusion

**Integration Points:**
```c
// Multi-scale reasoning in beast mode
// Run inference at different fractal scales
cog_result_t beast_fuse_fractal(
    beast_reactor_t reactor,
    fractal_atomspace_t fas,
    atom_handle_t goal,
    beast_fusion_result_t** result
);

// Fractal attention amplification
// Boost attention across fractal hierarchy
cog_result_t beast_amplify_fractal(
    beast_reactor_t reactor,
    fractal_atomspace_t fas,
    atom_handle_t root,
    int16_t base_boost
);
```

**Deliverables:**
- [ ] Multi-scale reasoning
- [ ] Fractal attention amplification

---

## Testing Requirements

- [ ] Unit test: Depth-first traversal
- [ ] Unit test: Breadth-first traversal
- [ ] Unit test: Scale-based traversal
- [ ] Unit test: Cycle handling in traversal
- [ ] Integration test: 9P filesystem read/write
- [ ] Integration test: CogServer commands
- [ ] Integration test: PLN fractal rules
- [ ] Integration test: Niche construction integration
- [ ] Integration test: Beast mode integration

## Acceptance Criteria

1. All traversal methods work correctly
2. 9P filesystem provides full access
3. CogServer commands function properly
4. PLN can reason over fractal structures
5. Integration with other subsystems is seamless

## Estimated Effort

- **Complexity:** High
- **Time Estimate:** 7-10 days
- **Risk Level:** Medium (cross-system integration)

## Integration Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                      Applications                                │
│  (Agents, Limbo Apps, 9P Clients)                               │
└───────────────────────────┬─────────────────────────────────────┘
                            │
┌───────────────────────────┼─────────────────────────────────────┐
│                     CogServer                                    │
│  ┌───────────┐  ┌───────────┐  ┌───────────┐                   │
│  │ PLN       │  │ Niche     │  │ Beast     │                   │
│  │ Reasoning │  │ Construct │  │ Mode      │                   │
│  └─────┬─────┘  └─────┬─────┘  └─────┬─────┘                   │
│        │              │              │                           │
│        └──────────────┼──────────────┘                           │
│                       │                                          │
│  ┌────────────────────┴─────────────────────┐                   │
│  │          Fractal AtomSpace               │                   │
│  │  - Hierarchy Management                  │                   │
│  │  - TV/AV Propagation                     │                   │
│  │  - Pattern Matching                      │                   │
│  │  - Generation/Expansion                  │                   │
│  │  - Persistence                           │                   │
│  └────────────────────┬─────────────────────┘                   │
│                       │                                          │
│  ┌────────────────────┴─────────────────────┐                   │
│  │          Base AtomSpace                  │                   │
│  └────────────────────┬─────────────────────┘                   │
└───────────────────────┼─────────────────────────────────────────┘
                        │
┌───────────────────────┼─────────────────────────────────────────┐
│                   9P Filesystem                                  │
│  /atoms/  /agents/  /fractal/  /tasks/                          │
└─────────────────────────────────────────────────────────────────┘
```

## Summary

This phase completes the fractal subsystem by:
1. Enabling efficient traversal algorithms
2. Exposing fractal atoms via 9P
3. Integrating with cognitive services
4. Enabling fractal reasoning in PLN

The result is a fully integrated fractal knowledge representation system that seamlessly works with the broader CoGWXP-OS9 cognitive architecture.
