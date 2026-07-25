# Contributing to PieceTable Editor

Thank you for your interest in contributing! This document explains how to get involved effectively.

---

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [How to Report a Bug](#how-to-report-a-bug)
- [How to Request a Feature](#how-to-request-a-feature)
- [Development Setup](#development-setup)
- [Making Changes](#making-changes)
- [Coding Standards](#coding-standards)
- [Commit Message Guidelines](#commit-message-guidelines)
- [Running Tests](#running-tests)
- [Pull Request Process](#pull-request-process)

---

## Code of Conduct

This project follows our [Code of Conduct](CODE_OF_CONDUCT.md). By participating you agree to uphold it. Please report unacceptable behaviour to the project maintainer.

---

## How to Report a Bug

1. **Search existing issues** first — the bug may already be tracked.
2. Open a new issue using the **Bug Report** template.
3. Include:
   - OS and compiler version (`g++ --version`, `uname -r`)
   - Minimal reproduction steps
   - Expected vs. actual behaviour
   - Relevant terminal output or a stack trace if you have one

---

## How to Request a Feature

1. Open an issue using the **Feature Request** template.
2. Describe the use case and why it fits the project's scope (a production-grade C++ terminal editor).
3. If you are willing to implement it, say so in the issue — maintainers can confirm design direction before you write code.

---

## Development Setup

**Requirements:** Linux or WSL2 · `g++ ≥ 10` · `make`

```bash
# Clone
git clone https://github.com/<your-username>/piecetable-editor.git
cd piecetable-editor

# Build
make

# Build with sanitizers (recommended while developing)
make debug

# Run all 50 unit tests + benchmark
make test
```

---

## Making Changes

1. **Fork** the repository and create a branch from `main`:
   ```bash
   git checkout -b feat/my-feature   # new feature
   git checkout -b fix/issue-42      # bug fix
   ```
2. Make your changes. Keep each commit focused on a single logical change.
3. Add or update tests for any new or changed behaviour.
4. Ensure `make test` passes with zero failures before opening a PR.
5. If you change the Piece Table, GapBuffer, or any core algorithm, run `make debug` (AddressSanitizer + UBSan) and confirm no sanitizer errors.

---

## Coding Standards

- **Standard:** C++17 (`-std=c++17`)
- **Flags:** code must compile cleanly under `-Wall -Wextra` with no new warnings
- **Style:**
  - 4-space indentation, no tabs
  - `camelCase` for local variables and parameters; `PascalCase` for types; `camelCase_` suffix for private member variables
  - `#include` order: corresponding header → other project headers → standard library headers
- **RAII:** manage resources with RAII wrappers; no raw `new`/`delete` unless absolutely necessary
- **No external dependencies** beyond the C++17 standard library and POSIX

---

## Commit Message Guidelines

Follow the [Conventional Commits](https://www.conventionalcommits.org/) format:

```
<type>(<scope>): <short summary>

[optional body]

[optional footer: e.g., Closes #42]
```

**Types:** `feat`, `fix`, `perf`, `refactor`, `test`, `docs`, `chore`

**Examples:**
```
feat(regex): add possessive quantifier support
fix(piece-table): prevent piece-list growth after sequential erases
perf(screen-renderer): reduce ANSI writes by diffing frame buffers
docs(readme): update benchmark results for g++ 13
```

---

## Running Tests

```bash
# Full suite (50 unit tests + benchmark)
make test

# Memory-safe build — run this before submitting a PR
make debug
./test_runner
```

All 50 tests must pass. Do not submit a PR that introduces failures.

---

## Pull Request Process

1. Ensure your branch is up to date with `main`.
2. Fill in the PR template completely.
3. Link the relevant issue(s) with `Closes #<number>` in the PR description.
4. A maintainer will review your PR. Expect feedback within a few days.
5. Address review comments in new commits (do not force-push during review).
6. Once approved, the maintainer will squash-merge your branch.

---

Thank you for helping make PieceTable Editor better!
