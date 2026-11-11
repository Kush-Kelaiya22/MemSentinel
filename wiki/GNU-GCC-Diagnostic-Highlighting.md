# GNU/GCC Diagnostic Highlighting

## Overview

Safe-C formats diagnostics to strictly mirror GNU GCC and Clang compiler output:

```
tests/compile_fail/double_free.c:6:5: error: [Safe-C] Double free detected on pointer 'ptr'.
    6 |     free(ptr); // ERROR: Double free must abort compilation
      |     ^~~~
tests/compile_fail/double_free.c:5:5: note: Previous deallocation occurred here:
    5 |     free(ptr);
      |     ^~~~
compilation terminated due to memory safety violations.
```

## Elements

1. **Location Header:** `filename:line:col:` rendered in bold (`\033[01m`).
2. **Diagnostic Severity:**
   - `error:` rendered in bold red (`\033[01;31m`).
   - `note:` rendered in bold cyan (`\033[01;36m`).
   - `warning:` rendered in bold magenta (`\033[01;35m`).
3. **Quoted Identifiers:** Symbols enclosed in single quotes like `'ptr'` are highlighted in bold white.
4. **Source Gutter:** Line numbers and vertical separator (`   6 | `) rendered in bold blue (`\033[01;34m`).
5. **Token Caret & Underline:** Caret `^` aligned with the token's column and underlined with tildes `~` matching the token length.
6. **Color Autodetection:**
   - Automatically enabled for TTY terminals.
   - Respects `NO_COLOR` and empty `GCC_COLORS`.
   - On Windows consoles, enables `ENABLE_VIRTUAL_TERMINAL_PROCESSING`.
   - Can be overridden using `SAFE_C_COLOR=always|never|auto`.
