# Support

This document explains how to get help with **PieceTable Editor**.

---

## Before Asking for Help

Please check the following resources first — your question may already be answered:

1. **[README](README.md)** — build instructions, keybindings, architecture overview, and benchmark results.
2. **[Contributing guide](CONTRIBUTING.md)** — development setup and coding standards.
3. **[Existing Issues](../../issues)** — search open and closed issues for similar problems.

---

## Getting Help

### Bug Reports

If you believe you have found a bug, please open a **GitHub Issue** using the Bug Report template. Include:

- OS and compiler version (`uname -r`, `g++ --version`)
- Exact steps to reproduce the problem
- The file you were editing (if relevant and not sensitive)
- Terminal output or a stack trace

### Build Failures

If the project does not compile:

1. Confirm you are on **Linux or WSL2** with `g++ ≥ 10` and `make` installed.
2. Run `make clean && make` to rule out stale object files.
3. Run `make debug` to check for sanitizer errors that may indicate an environment-specific issue.
4. If the problem persists, open an issue with the full compiler error output.

### Feature Requests

Open a **GitHub Issue** using the Feature Request template. Describe the use case and how the feature fits the project scope.

### General Questions

For design or algorithm questions that are not bugs, you are welcome to open a **GitHub Discussion** (if enabled) or an issue labelled `question`.

---

## What is Out of Scope

- Support for Windows (non-WSL) — the editor uses POSIX raw mode and POSIX signals
- Integration with IDEs or GUI frontends
- Support for non-UTF-8 / binary files

---

## Response Times

This is a single-maintainer project maintained in spare time. Expect responses within a few days. Pull requests with fixes are always welcome and will be reviewed promptly.
