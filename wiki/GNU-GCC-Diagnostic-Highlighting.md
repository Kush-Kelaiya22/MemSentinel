# GNU/GCC Diagnostic Highlighting & Syntax Highlighting

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

---

## Compiler Flags Supported

Safe-C natively parses and honors standard GCC diagnostic formatting flags:

| Flag | Purpose | Default |
| :--- | :--- | :--- |
| `-fdiagnostics-color=always` / `-fdiagnostics-color` | Force ANSI color escape sequences on diagnostics | Active for TTY |
| `-fdiagnostics-color=never` / `-fno-diagnostics-color` | Disable ANSI color escape sequences (plain text output) | - |
| `-fdiagnostics-color=auto` | Automatically detect terminal and virtual terminal capability | Yes |
| `-fdiagnostics-show-highlight-colors` | Enable lexical C/C++ syntax highlighting on source code lines | Enabled |
| `-fno-diagnostics-show-highlight-colors` | Disable lexical syntax highlighting on source lines | - |

---

## Syntax Highlighting Elements

When color is active, Safe-C applies lexical syntax highlighting to source lines:

1. **Preprocessor Directives:** `#include`, `#define`, `#ifdef`, etc. highlighted in bold yellow (`\033[01;33m`).
2. **C/C++ Types:** `void`, `int`, `char`, `size_t`, `int64_t`, `auto`, etc. in bold green/cyan (`\033[01;32m`).
3. **C/C++ Keywords & Memory Primitives:** `free`, `malloc`, `new`, `delete`, `return`, `if`, `struct`, etc. in bold cyan (`\033[01;36m`).
4. **Target Violation Token:** The token causing the violation (`col` to `col + len`) is highlighted in bold red (`\033[01;31m`) for errors or bold cyan (`\033[01;36m`) for notes.
5. **String and Character Literals:** `"..."` and `'...'` in green (`\033[32m`).
6. **Numeric Literals:** Hex (`0x...`), decimal, floating-point in magenta (`\033[35m`).
7. **Comments:** Single-line (`// ...`) and block (`/* ... */`) in muted grey (`\033[90m`).

---

## Palette Customization via `GCC_COLORS`

Safe-C parses the standard `GCC_COLORS` environment variable to allow custom palette configurations:

```bash
export GCC_COLORS='error=01;31:warning=01;35:note=01;36:caret=01;32:locus=01:quote=01'
```

If `GCC_COLORS=""` is defined empty, colored output is suppressed. Safe-C also honors the `NO_COLOR` standard.
