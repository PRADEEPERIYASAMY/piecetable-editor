# Changelog

All notable changes to **PieceTable Editor** are documented here.

This project adheres to [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [Unreleased]

### Planned
- OS clipboard integration via `xclip` / `pbcopy` (behind `Clipboard` abstraction)
- Mouse support (click-to-move cursor)
- Line-number gutter
- Multiple open buffers / split-pane view

---

## [1.0.0] — 2025

### Added

#### Core Text Engine
- **Piece Table** text storage — two immutable backing buffers (`original_`, `add_`) plus a `std::list<Piece>`; no text ever moved on insert
- `coalesceAdjacentPieces()` pass after erase to prevent unbounded piece-list growth
- **GapBuffer** implementation alongside Piece Table for empirical trade-off comparison

#### Document Layer
- `TextDocument` row/column abstraction with **incremental line index** (`lineStarts_` vector updated in O(lines after cursor), not O(file size))
- Offset-based range API supporting multi-line copy/cut/paste without row-by-row special-casing

#### Undo / Redo
- **Command pattern** (`EditCommand` abstract base + `InsertCharCommand`, `DeleteCharCommand`, `InsertNewlineCommand`, `DeleteNewlineCommand`)
- Command subclasses depend only on `TextDocument&` and `CursorOwner` interface — not on `TextEditor`
- `UndoRedoStack`: `execute()` pushes to undo stack and clears redo stack

#### Search
- **Incremental literal search** with live match highlighting across all lines
- **Regex search** (`Ctrl+R` toggle): Thompson NFA construction + simulation — O(|pattern| × |text|), no backtracking

#### Regex Engine
- Full recursive-descent parser → NFA fragment assembly (Thompson's construction)
- Supported syntax: `.`, `*`, `+`, `?`, `|`, `[class]`, `[^neg]`, `\d \w \s`, `^ $` anchors, grouping

#### Syntax Highlighting
- **Stateless per-line tokenizer** — caller threads `inBlockComment` state across rows
- Languages: C, C++, Python (keywords, strings, comments, preprocessor directives, numbers)
- Single left-to-right O(line length) scan; keyword lookup via `std::unordered_set`

#### Selection and Clipboard
- `Selection` value type: anchor + active point + `normalized()` helper — no editor logic
- In-process `Clipboard`; multi-line copy/cut/paste via offset-based range API

#### Version History and Diff
- `VersionHistory` — stores full-text snapshots per save
- `DiffEngine::diffLines` — O(n·m) LCS dynamic program over line vectors; displays added/removed/unchanged lines on `Ctrl+D`

#### Background Autosave
- Dedicated `std::thread` in `AutosaveWorker`; wakes on 30-second timer or explicit `notify()`
- Mutex held only during in-memory text copy; disk write occurs after unlock — typing latency unaffected

#### Rendering and Input
- `ScreenRenderer` takes `const TextEditor&` — compiler-enforced read-only access
- `FrameBuffer` accumulator flushed in a single `write()` syscall — no visible flicker
- `InputHandler` maps raw bytes to `TextEditor` methods; no access to `FrameBuffer`
- `Terminal` — POSIX raw mode, `SIGWINCH` handler, `ioctl` window-size query

#### Build and Tests
- 50 unit tests covering all major subsystems (all passing)
- Benchmark suite: Piece Table vs. GapBuffer across 4 scenarios, 50 000 ops each
- `make debug` — AddressSanitizer + UndefinedBehaviorSanitizer build
- `make test` — compiles and runs `test_runner`

---

[Unreleased]: https://github.com/<your-username>/piecetable-editor/compare/v1.0.0...HEAD
[1.0.0]: https://github.com/<your-username>/piecetable-editor/releases/tag/v1.0.0
