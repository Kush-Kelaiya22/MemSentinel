---
name: Bug Report
about: Create a report to help us improve MemCheck
title: '[BUG] '
labels: 'bug'
assignees: ''
---

**Describe the Bug**
A clear and concise description of what the bug is.

**Source Code Triggering the Issue**
```c
// Paste minimal reproducing C/C++ snippet here
```

**Expected Behavior**
Whether this should have triggered a compile-time rejection or runtime guardrail, or if it was a false positive.

**Compiler Environment:**
- OS: [e.g. Windows 11 with MinGW-w64, Ubuntu 22.04]
- GCC/G++ Version: [e.g. GCC 13.2.0, GCC 15.2.0]
- Command Line Invocation: [e.g. `gcc -fplugin=memcheck_plugin.dll -c main.c`]

**Log Output (`memcheck_runtime.log` or compiler diagnostic output):**
```
Paste log output here
```
