# Contributing to MemCheck

Thank you for your interest in contributing to **MemCheck**! This guide outlines how to contribute to the project, coding standards, branch rules, and pull request workflows.

## Development Workflow

1. **Fork and Branch:**
   - Create a feature branch from `main`:
     ```bash
     git checkout -b feature/your-feature-name
     ```
2. **Coding Standards:**
   - **Language Standard:** Modern C++ (C++17 / C++20).
   - **Safety Invariants:** All checker code and runtime libraries must be strictly memory-safe.
     - Never use unmanaged owning raw pointers.
     - Utilize `std::unique_ptr`, `std::shared_ptr`, and `std::span`.
     - Zero memory leaks, zero dangling references.
   - **GCC Plugin Compatibility:** Compatible across GCC versions >= 4.5.0 (the first version to support plugins).
3. **Running Tests:**
   - Ensure all `compile_fail/` test cases successfully fail at compile-time.
   - Ensure all `compile_pass/` test cases compile cleanly without warnings or errors.
4. **Submitting Pull Requests:**
   - Link any related GitHub issue (e.g. `Resolves #X`).
   - Write clear commit messages summarizing changes.
   - Maintain documentation in `docs/` and `README.md`.
