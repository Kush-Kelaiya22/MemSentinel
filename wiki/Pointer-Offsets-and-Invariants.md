# Pointer Offsets and Base Invariants

## Invariant Specification

Safe-C permits unrestricted pointer arithmetic (`++`, `--`, `+=`, `-=`, pointer addition, and subscripting) under one strict safety contract:

> **The offset of a pointer may be modified if and only if there exists an accessible variable in scope that retains the original allocation base address (`offset == 0`).**

## Permitted Patterns

### Pattern A: Cursor Pointer
A cursor or temporary pointer is created from the base address and incremented through the buffer. The base pointer remains unchanged:

```c
void process_buffer(void) {
    char* base = (char*)malloc(64);
    char* cursor = base;
    cursor += 8;
    cursor++;
    free(base); // Valid: base holds offset 0
}
```

### Pattern B: Base Shift with Backup Variable
The original pointer variable is shifted, but a backup pointer retains the original base address:

```c
void shift_with_backup(void) {
    char* base = (char*)malloc(64);
    char* orig = base;
    base += 16;
    free(orig); // Valid: orig holds offset 0
}
```

## Prohibited Patterns

### Pattern C: Shift Without Retained Base
The pointer is shifted when no other variable in scope retains the base address:

```c
void lost_base(void) {
    char* p = (char*)malloc(32);
    p += 4; // FATAL ERROR: No variable retains base address
    free(p);
}
```

### Pattern D: Interior Deallocation
Passing a pointer with non-zero offset to `free()` or `delete`:

```c
void interior_free(void) {
    char* base = (char*)malloc(32);
    char* interior = base + 4;
    free(interior); // FATAL ERROR: Attempt to free interior pointer shifted by 4 bytes
}
```
