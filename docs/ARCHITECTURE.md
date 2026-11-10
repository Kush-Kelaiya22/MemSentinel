# MemCheck Architecture & Design Specification

MemCheck is architected as a two-tier verification system designed to eradicate memory unsafety in C and C++ without restricting idiomatic raw pointer usage, pointer arithmetic, or `malloc`/`free` and `new`/`delete`.

```
                        C / C++ Source File
                                │
                                ▼
         ┌──────────────────────────────────────────────┐
         │          Compile-Time Static Tier            │
         │  • GIMPLE SSA / Control Flow Graph Pass      │
         │  • Intra-procedural Typestate Analysis       │
         │  • Pointer Arithmetic Shift Validation       │
         │  • Fatal Compilation Error Emission          │
         └──────────────────────┬───────────────────────┘
                                │
                  Passes Static Verification?
                     /                      \
                   NO                        YES
                   │                          │
                   ▼                          ▼
          Compilation Halted!         Compilation Succeeds
          (error_at with origin       (Optional: Runtime Guardrails)
           and violation details)             │
                                              ▼
                             ┌─────────────────────────────────┐
                             │    Runtime Guardrail Tier       │
                             │  • libmemcheck_rt.so / .dll     │
                             │  • Safe Allocation Ledger       │
                             │  • Exact Base Offset Checking   │
                             │  • Quarantine & UAF Trapping    │
                             │  • memcheck_runtime.log Writer  │
                             └─────────────────────────────────┘
```

---

## 1. The Typestate State Machine

Each tracked pointer is represented in a lattice of typestates:

```
  [UNALLOCATED] ──(malloc/new)──> [ALLOCATED] ──(free/delete)──> [FREED]
        │                              │                            │
        │                              │ (offset != 0)              │ (dereference / free)
        │                              ▼                            ▼
        │                      [INTERIOR_OFFSET]            FATAL COMPILER ERROR
        │                              │                    (Use-After-Free /
        │                              │ (free called)       Double-Free)
        ▼                              ▼
  FATAL ERROR                 FATAL COMPILER ERROR
  (Wild / Non-Heap Free)      (Attempt to free altered base)
```

### Invariant Rules:
1. **Double Free:** A deallocation function invocation (`free`, `delete`, `delete[]`, `realloc`) on an address whose current typestate is `FREED` triggers a fatal compilation error.
2. **Use After Free (UAF):** Any read, write, member dereference, array subscript, or argument transmission of a pointer whose state is `FREED` triggers a fatal compilation error.
3. **Interior Offset Free:** Deallocating a pointer that has undergone pointer arithmetic ($P_{derived} = P_{base} + \delta$, where $\delta \neq 0$) without restoring its base address triggers a fatal compilation error.
4. **Memory Leak / Unfreed:** If a dynamically allocated pointer's last SSA alias is overwritten or exits function scope without being freed and without escaping to caller/global return, a compilation error is issued.

---

## 2. Memory-Safe Self-Implementation

The MemCheck analyzer and runtime library are implemented exclusively using **safe C++20**:
- **Zero Raw Owning Pointers:** All allocations are managed through `std::unique_ptr` and `std::shared_ptr`.
- **Bounds Checking:** Views are wrapped in `std::span` with debug assertions enabled.
- **Resource Cleanup:** Guaranteed through RAII wrappers on all OS resources, files, and AST nodes.
- **Thread Safety:** Runtime ledger utilizes read-write locks (`std::shared_mutex`) to guarantee thread safety during multi-threaded target execution.

---

## 3. GCC Plugin Interface & Compiler Baseline

- GCC introduced plugin capabilities in **GCC 4.5.0**.
- MemCheck targets all GCC versions from 4.5.0 up through GCC 15+.
- Plugin compatibility is ensured by using stable core GIMPLE traversal APIs and conditional preprocessor macros for compiler API evolution across major GCC versions.
