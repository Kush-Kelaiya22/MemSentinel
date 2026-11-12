# Memory Safety Invariants (v1.2.0)

Safe-C enforces compile-time memory safety across both C and C++ while eliminating false positives on valid, idiomatic pointer patterns. Checks already built into stock GCC/G++ (`-Wreturn-local-addr`, `-Wfree-nonheap-object`, `-Wuninitialized`, `-Wmismatched-new-delete`) are left to GCC directly.

## Violations Caught at Compile Time

| Violation | Description | Diagnostic |
|---|---|---|
| **Memory Leak** | Heap allocation (`malloc`, `calloc`, `aligned_alloc`, `realloc`, `new`, `new[]`) that does not escape and is not freed or is overwritten before free/exit. | `leak of 'p' ... [-Wsafec-leak]` |
| **Double Free** | Calling `free`, `delete`, `delete[]`, or `realloc` on an already freed pointer. | `double free of 'p' [-Wsafec-double-free]` |
| **Use-After-Free** | Dereferencing (`*p`, `p->f`, `p[i]`, `memcpy`/`memset`) a freed pointer. | `use-after-free on pointer 'p' [-Wsafec-use-after-free]` |
| **Modified Base Free** | Freeing an interior/shifted pointer whose base address was lost. | `attempt to free interior pointer 'p' ... [-Wsafec-modified-base]` |
| **Out-of-Bounds Access** | Static spatial overflow or negative indexing underflow (`p[-1]`, `p[idx]`, `memcpy`, `memset`). | `out-of-bounds memory access on 'p' ... [-Wsafec-bounds]` |
| **Null Pointer Dereference** | Dereferencing a pointer known to be `NULL` / `nullptr`. | `null pointer dereference of 'p' [-Wsafec-null-deref]` |
| **Iterator & Data Handle Invalidation** | Mutating an STL container (`push_back`, `insert`, `erase`, `clear`, `resize`, etc.) while iterating (`for (x : v)`, active `begin()`/`find()` iterator) or before reusing `.data()` / `.c_str()`. | `container 'v' mutated while iterating with invalidated iterator [-Wsafec-iterator-invalidation]` |

## Valid Patterns Supported (Zero False Positives)

- **Returning Allocated Pointers:** Allocating inside a helper function and returning the pointer transfers ownership to the caller.
- **Scope-Changing Escapes:** Assigning an inner-block allocation to an outer-scope variable (`outer = inner;`) preserves the outer variable's lifetime.
- **Pointer Arithmetic with Base Retention:** Modifying a cursor (`b += 5;`) is valid as long as another variable (`a = b;`) retains the base address.
- **Null-Safe Idioms:** `free(NULL)`, `delete nullptr`, and `if (p != NULL) { ... free(p); }` are recognized as safe.
- **Reassignment & `realloc`:** Reassigning a freed pointer to a new allocation and `realloc` null-failure fallback (`tmp = realloc(p, n); if (tmp) p = tmp; free(p);`) are handled accurately via CFG + `GIMPLE_PHI` analysis.
- **C++ RAII, STL & Placement `new`:** `std::vector` resizing/mutation, `std::unique_ptr`, and STL placement `new` (`::new(loc) T(...)`) compile cleanly without false positives.
