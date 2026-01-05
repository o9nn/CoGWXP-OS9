# CoGWXP-OS9 Formal Specifications and Architecture Documentation

This directory contains comprehensive technical architecture documentation and Z++ formal specifications for the CoGWXP-OS9 cognitive operating system.

## Contents

### Architecture Documentation

- **[architecture_overview.md](architecture_overview.md)** - Comprehensive technical architecture documentation with Mermaid diagrams covering:
  - High-level system architecture
  - Component architecture (OpenCog, Plan 9, Inferno)
  - b9/p9/j9 integration model
  - Data flow architecture
  - Integration boundaries
  - Technology stack summary
  - Performance characteristics
  - Security model

### Formal Specifications (Z++)

Located in `formal_specs/`:

1. **[data_model.zpp](formal_specs/data_model.zpp)** - Formalizes the data layer
   - Atom types (nodes and links)
   - Truth values and attention values
   - AtomSpace hypergraph structure
   - Attention bank
   - Data integrity invariants

2. **[system_state.zpp](formal_specs/system_state.zpp)** - Formalizes system state
   - System state enumeration
   - Agent and task state
   - 9P connection state
   - Dis VM state
   - Niche construction engine state
   - Beast mode reactor state
   - PLN engine state
   - CogServer state
   - b9/p9/j9 integration state
   - Top-level CoGWXPSystem state
   - Global system invariants

3. **[operations.zpp](formal_specs/operations.zpp)** - Formalizes core operations
   - AtomSpace operations (add node, add link, query, stimulate)
   - PLN operations (forward chain, backward chain, apply rule)
   - CogServer operations (spawn agent, submit task, assign goal)
   - Niche construction operations (opponent cycle)
   - Beast mode operations (cognitive fusion)
   - System lifecycle operations (initialize, start, stop)

4. **[integrations.zpp](formal_specs/integrations.zpp)** - Formalizes integration contracts
   - 9P protocol operations (attach, walk, read, write, clunk)
   - Dis VM operations (load module, execute bytecode, channel communication)
   - Distributed communication (connect peer, distributed inference, sync)
   - Integration invariants

## Architecture Overview

CoGWXP-OS9 integrates four major computing paradigms:

1. **Windows XP NT Kernel** - Microkernel architecture
2. **OpenCog Framework** - Hypergraph-based AGI
3. **Plan 9** - Distributed computing with 9P protocol
4. **Inferno-OS** - Dis VM and Limbo language

### Key Components

- **AtomSpace**: Hypergraph knowledge base with truth values and attention allocation
- **PLN Engine**: Probabilistic Logic Networks for uncertain reasoning
- **CogServer**: Agent orchestration and task scheduling
- **Niche Construction**: Adaptive learning via opponent processing
- **Beast Mode**: High-intensity cognitive fusion
- **9P Protocol**: Network-transparent resource access
- **Dis VM**: Portable bytecode execution
- **b9/p9/j9**: Three-layer integration (rooted trees, nested scopes, gradients)

## Formal Specification Methodology

The Z++ formal specifications follow a structured approach:

### Step 0: Repository Analysis
- Analyzed actual C header files from cogwxp/
- Identified core data structures and operations
- Mapped technology stack and architectural patterns

### Step 1: Data Layer Formalization
- Defined base types and atom types
- Formalized truth values and attention values
- Specified AtomSpace hypergraph structure
- Defined data integrity invariants

### Step 2: System State Formalization
- Defined state schemas for all major components
- Specified relationships between components
- Defined global system invariants

### Step 3: Operations Formalization
- Specified preconditions and postconditions for each operation
- Defined frame conditions (what remains unchanged)
- Captured state transitions formally

### Step 4: Integration Contracts Formalization
- Specified 9P protocol operations
- Formalized Dis VM execution model
- Defined distributed communication protocols
- Specified integration invariants

## Key Invariants

### Data Integrity
- `NO_DANGLING_REFS`: All link outgoing sets reference valid atoms
- `INCOMING_OUTGOING_CONSISTENT`: Incoming sets match outgoing sets
- `UNIQUE_NODE_IDENTIFICATION`: Nodes with same type and name are identical
- `UNIQUE_LINK_IDENTIFICATION`: Links with same type and outgoing set are identical

### System Consistency
- `AGENT_CONSISTENCY`: Agent atomspaces are children of main atomspace
- `TASK_ASSIGNMENT_CONSISTENCY`: Assigned agents exist in registry
- `ATTENTION_BANK_CONSISTENCY`: Attention bank tracks atomspace
- `MEMORY_BOUNDS`: Memory usage within heap size limits
- `RUNNING_TASKS_HAVE_AGENTS`: Running tasks have assigned active agents

### Integration Integrity
- `P9_FID_CONSISTENCY`: All FIDs map to valid paths
- `DIS_THREAD_MODULE_LOADED`: Threads reference loaded modules
- `DISTRIBUTED_PEER_CONNECTION_VALID`: Peer connections exist
- `ATOMSPACE_9P_IDENTITY_PRESERVATION`: 9P paths bijectively map to atoms

## Usage

### Reading the Specifications

1. Start with `architecture_overview.md` to understand the system architecture
2. Read `data_model.zpp` to understand the core data structures
3. Read `system_state.zpp` to see how components fit together
4. Read `operations.zpp` to understand system behavior
5. Read `integrations.zpp` to understand external interfaces

### Z++ Notation Guide

- `schema Name` - Defines a data structure with fields and constraints
- `where` - Constraints that must hold
- `Δ Schema` - Operation that changes Schema (before/after states)
- `Ξ Schema` - Operation that reads Schema (no changes)
- `?` suffix - Input parameter
- `!` suffix - Output parameter
- `'` suffix - After-state variable
- `∀` - Universal quantifier (for all)
- `∃` - Existential quantifier (there exists)
- `⇒` - Implication
- `⇔` - If and only if
- `∧` - Logical AND
- `∨` - Logical OR
- `¬` - Logical NOT
- `∈` - Element of
- `⊆` - Subset of
- `⇀` - Partial function
- `↦` - Maps to
- `ℙ` - Power set
- `ℕ` - Natural numbers
- `ℤ` - Integers
- `ℝ` - Real numbers
- `𝔹` - Booleans
- `seq` - Sequence
- `⟨⟩` - Empty sequence
- `⁀` - Sequence concatenation

## Grounding in Implementation

All specifications are grounded in the actual CoGWXP-OS9 codebase:

- `data_model.zpp` ← `cogwxp/opencog/atomspace/atomspace.h`
- `system_state.zpp` ← `cogwxp/integration/cogwxp_os.h`, `cogwxp/integration/niche_construction.h`, `cogwxp/integration/beast_mode.h`
- `operations.zpp` ← `cogwxp/opencog/atomspace/atomspace.h`, `cogwxp/opencog/pln/pln.h`
- `integrations.zpp` ← `cogwxp/plan9/9p/9p.h`, `cogwxp/inferno/dis/`, `cogwxp/inferno/styx/`

## Future Extensions

Potential additions to the formal specifications:

1. **Temporal Logic**: Specify liveness and safety properties
2. **Concurrency Model**: Formalize thread scheduling and synchronization
3. **Security Properties**: Specify access control and authentication
4. **Persistence Model**: Formalize AtomSpace save/load operations
5. **Hyperon Integration**: Extend with MeTTa language semantics
6. **GPU Operations**: Specify parallel inference on GPU
7. **Quantum Operations**: Formalize quantum inference integration

## References

### OpenCog
- [OpenCog Framework](https://opencog.org/)
- [AtomSpace Documentation](https://wiki.opencog.org/w/AtomSpace)
- [PLN Book](https://opencog.org/papers/pln_book/)

### Plan 9 / Inferno
- [Plan 9 from Bell Labs](http://9p.io/plan9/)
- [9P Protocol Specification](http://9p.io/sys/man/5/INDEX.html)
- [Inferno OS](http://www.vitanuova.com/inferno/)
- [Limbo Language](http://www.vitanuova.com/inferno/papers/limbo.html)

### Z Notation
- [Z Notation Reference](http://www.usingz.com/)
- [The Z Notation: A Reference Manual](https://www.amazon.com/Notation-Reference-International-Computer-Science/dp/0139785299)

### Formal Methods
- [Software Specification Methods](https://link.springer.com/book/10.1007/978-1-4471-3127-2)
- [Formal Methods in Practice](https://ieeexplore.ieee.org/document/268951)

## License

This documentation and specifications are part of the CoGWXP-OS9 project.
Components are under various licenses:
- Windows XP SP1 source: Research/Educational use
- OpenCog: AGPL-3.0
- Plan 9: MIT/Lucent Public License
- Inferno-OS: MIT/GPL

## Contributing

Contributions to the formal specifications are welcome. Please ensure:

1. Specifications remain grounded in actual code
2. Z++ notation is used consistently
3. Invariants are verifiable
4. Operations specify pre/post conditions and frame conditions
5. Documentation explains the specification's purpose and grounding

## Contact

For questions about the formal specifications or architecture documentation, please open an issue in the repository.
