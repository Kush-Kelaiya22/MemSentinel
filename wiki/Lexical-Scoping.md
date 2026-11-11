# Lexical Block Scope Resolution

## Scoping Model

Safe-C implements lexical compound statement scoping matching GCC and G++ block rules:

1. **Compound Statements (`{ ... }`):** Entering an opening brace `{` increments the active scope depth.
2. **Local Allocations:** Pointer bindings registered within an inner scope are bound to that specific lexical depth.
3. **Escaping to Outer Scopes:** Assigning an inner pointer to a variable declared in an enclosing outer scope transfers cell reachability to the outer scope.
4. **Scope Exit Validation:** Exiting a closing brace `}` pops all local bindings at that depth. Any heap allocation created at that depth that has not escaped and has not been freed is flagged as an immediate fatal scope-exit memory leak.
5. **Translation Unit Isolation:** Each source file (`.c`, `.cpp`) is analyzed in isolation, preventing state pollution across separate compilation units.

### Example: Inner Block Leak
```c
void test_block_leak(void) {
    int x = 10;
    {
        char* inner_ptr = (char*)malloc(32);
        inner_ptr[0] = 'a';
    } // FATAL ERROR: inner_ptr destroyed here without deallocation
}
```

### Example: Escaped to Enclosing Variable
```c
void test_block_escape(void) {
    char* outer_ptr = 0;
    {
        char* inner_ptr = (char*)malloc(32);
        outer_ptr = inner_ptr; // Escaped to scope depth 1
    } // No leak: outer_ptr maintains reachability
    free(outer_ptr); // Clean deallocation
}
```
