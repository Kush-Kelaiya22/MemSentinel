# MemCheck Formal Specification

## 1. Safety Invariants Enforced

| Violation Identifier | Category | Static Enforce | Runtime Guardrail |
| :--- | :--- | :--- | :--- |
| `DOUBLE_FREE` | Fatal | Yes (Halt compilation) | Yes (Log & abort) |
| `USE_AFTER_FREE` | Fatal | Yes (Halt compilation) | Yes (Log & abort) |
| `MEMORY_LEAK_UNFREED` | Fatal | Yes (Halt compilation) | Yes (Log at exit) |
| `MODIFIED_BASE_FREE` | Fatal | Yes (Halt compilation) | Yes (Log & abort) |
| `UNALLOCATED_FREE` | Fatal | Yes (Halt compilation) | Yes (Log & abort) |

---

## 2. Compile-Time vs Runtime Responsibility

1. **Compile-Time Static Analyzer:**
   - Evaluates intra-procedural CFG and SSA pointer aliases.
   - Halts build immediately if any unsafe condition is proven.
   - Prevents binary creation (`cc1` / `cc1plus` exits with status 1).
2. **Runtime Guardrails:**
   - Active only when dynamic or inter-procedural opaque pointers bypass complete static proof.
   - Prevents memory corruption before an invalid free or access corrupts heap tables.
   - Writes detailed records to `memcheck_runtime.log`.
