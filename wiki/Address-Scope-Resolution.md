# Address Scope Resolution

## Overview

In C and C++ toolchains, a single source file can be referenced using diverse path representations depending on the build system, working directory, or compiler invocation:

- Relative with current-directory prefix: `.\x.c` (Windows) or `./x.c` (POSIX)
- Plain relative: `x.c`
- Path with parent navigations: `dir/../x.c`
- Fully qualified absolute paths: `K:\mem_check\x.c` or `/home/user/project/x.c`
- Mixed directory separators: `tests/compile_fail\double_free.c`
- Case variations on case-insensitive filesystems: `C:\Project\Main.C` vs `c:\project\main.c`

## Safe-C Resolution Engine

Safe-C implements physical file identity resolution via `safe_c::resolve_canonical_path`:

1. **Filesystem Canonicalization:** Evaluates `std::filesystem::canonical` (or `std::filesystem::weakly_canonical` for in-memory / simulated translation units) to resolve symlinks and normalize dot-dot segments.
2. **Platform Case Folding:** On Windows platforms, paths are case-folded to ensure case-insensitive matching identical to Windows filesystem semantics.
3. **Display Path Preservation:** `SourceLocation` preserves the user-provided `filename` for diagnostic output while binding the canonical path to `canonical_path` for internal scoping.
4. **Diagnostic Deduplication:** If the same violation type occurs multiple times on the same line across identical physical files, Safe-C suppresses duplicate lines. Distinct error types occurring on the same line are preserved and displayed.
