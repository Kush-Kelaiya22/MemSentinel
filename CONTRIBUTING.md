# Contributing to Safe-C

Thank you for your interest in contributing to **Safe-C**! This guide outlines how to contribute to the project, coding standards, branch rules, and pull request workflows.

## Development Workflow

1. **Fork and Branch:**
   - Create a feature branch from `main`:
     ```bash
     git checkout -b feature/your-feature-name
     ```
2. **Coding Standards:**
   - **Language Standard:** Strictly C++20 and C11.
   - **Safety Invariants:** Checker and runtime code must be strictly memory-safe without memory leaks or dangling pointers.
   - **GCC Plugin Compatibility:** Compatible across GCC versions >= 4.5.0.
   - **Pointer Offset Model:** Maintain invariant that a pointer's offset can be modified through arithmetic as long as a variable retains the base address.
3. **Running Tests:**
   - Ensure all `compile_fail/` test cases fail at compile-time with exact diagnostics.
   - Ensure all `compile_pass/` test cases compile cleanly without warnings or errors.
   - Run `ctest --test-dir build --output-on-failure`.
4. **Submitting Pull Requests:**
   - Write clear commit messages summarizing changes.
   - Maintain documentation in `docs/index.html` and `README.md`.
