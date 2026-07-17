# PieceTable Editor — A High-Performance C++ Terminal Editor

A fully functional terminal text editor built from scratch in modern C++17.
Every component — from text storage to regex search — was designed to demonstrate production-grade systems engineering, performance-oriented trade-offs, and algorithmic thinking.

---

## Features

- **Open, edit, save** any plain-text or source code file
- **Undo / redo** every individual keystroke via the Command pattern
- **Incremental search** — matches are highlighted live across all lines as you type
- **Regex search** — press `Ctrl+R` to toggle; supports `.`, `*`, `+`, `?`, `|`, `[class]`, `[^neg]`, `\d \w \s`, `^$` anchors
- **Syntax highlighting** — C, C++, and Python files colored automatically (keywords, strings, comments, preprocessor directives, numbers)
- **Selection and clipboard** — mark a range with `Ctrl+Space`, copy/cut/paste across line boundaries
- **Version history** — every save is snapshotted; `Ctrl+D` shows an LCS diff of what changed
- **Background autosave** — writes a `.bak` file every 30 seconds on a dedicated thread; typing latency is unaffected
- **Dirty tracking** via a content hash — the editor knows whether you've actually changed anything since the last save

---

## Architecture

```text
include/                          src/
│
├── PieceTable          ──────>   Two backing buffers (original + add) plus a list of pieces.
│                                 Inserts append to the add buffer and split at most one existing
│                                 piece — no text is ever moved. O(1) amortized append.
│
├── GapBuffer           ──────>   Contiguous array with a movable gap at the cursor.
│                                 Included alongside PieceTable to make the trade-off empirically
│                                 measurable.
│
├── TextDocument        ──────>   Row/column abstraction over PieceTable. Maintains an incremental
│                                 line index: a single-char edit shifts only the entries after the
│                                 edit point — O(lines after cursor), not O(file size).
│
├── EditCommand         ──────>   Abstract command with execute/undo. Subclasses depend only on
│   (InsertChar,                  TextDocument& and the CursorOwner interface — not on TextEditor.
│    DeleteChar,                  This means undo/redo logic cannot accidentally access unrelated
│    InsertNewline, …)            editor state, and new command types require zero changes to TextEditor.
│
├── UndoRedoStack       ──────>   Owns the two history stacks. execute() pushes to undo, clears redo.
│
├── Selection           ──────>   Value type: anchor point + active point + normalized() helper.
│                                 Deliberately dumb — no editor logic, trivially testable.
│
├── Clipboard           ──────>   In-process string store.
│
├── DiffEngine          ──────>   O(n·m) LCS over lines. Returns a vector of DiffEntry{op, line}
│                                 where op ∈ {Added, Removed, Unchanged}.
│
├── VersionHistory      ──────>   Stores full-text snapshots; exposes diffAgainstLatest().
│
├── SyntaxHighlighter   ──────>   tokenize(line, lang, inBlockComment&) → vector<SyntaxToken>.
│                                 Single left-to-right scan, O(line length). Caller threads block-
│                                 comment state across rows; the class itself is stateless.
│
├── RegexEngine         ──────>   Thompson NFA construction (recursive-descent parser → fragment
│                                 assembly) + NFA simulation via epsilon-closure set tracking.
│                                 O(pattern × text) guaranteed; no backtracking stack.
│
├── AutosaveWorker      ──────>   Dedicated std::thread. Wakes on a 30-second timer or an explicit
│                                 dirty-notify from TextEditor. Acquires documentMutex_ only long
│                                 enough to copy text out; the actual write happens after unlock.
│
├── Terminal            ──────>   POSIX raw mode, SIGWINCH handler, window size query (ioctl).
│
├── ScreenRenderer      ──────>   Takes const TextEditor& — cannot mutate the document by design.
│                                 Emits ANSI escape sequences into a FrameBuffer written in one
│                                 syscall to prevent flicker.
│
├── InputHandler        ──────>   Maps raw bytes to TextEditor method calls. Has no access to
│                                 FrameBuffer — cannot render by design.
│
└── TextEditor          ──────>   Orchestrator and CursorOwner. Owns all state. Exposes exactly the
                                  public surface that ScreenRenderer and InputHandler need and nothing
                                  more — enforced by the accessor-only interface.
```

---

## Engineering Design Notes

This section details the engineering decisions, performance trade-offs, and algorithms used to build the core components.

### 1. Text Storage — Piece Table

#### The problem
A text editor's primary data structure must handle two operations efficiently:
1. **Insert** at an arbitrary position (including the very end, which is the common case during typing)
2. **Delete** at an arbitrary position

The obvious approach — a single contiguous string or `std::vector<char>` — requires copying `O(n)` bytes on every insert/delete at position `p` (everything after `p` shifts). For a 1 MB file, that is 1 million copies per keystroke. Unacceptable.

#### The options considered
| Structure | Insert complexity | Memory | Notes |
|---|---|---|---|
| `std::string` / `std::vector<char>` | O(n) | Contiguous | Simple, slow for large files |
| Linked list of chars | O(1) insert | Fragmented | Cache-hostile; huge pointer overhead |
| **Piece Table** | O(1) amortized append; O(k) mid-doc insert | Low | Two backing buffers + piece list |
| Gap Buffer | O(k) insert at gap; O(n) to reposition | Contiguous | Great for sequential; poor for random edits |
| Rope | O(log n) all operations | Tree nodes | Complex, justified at gigabyte scale, not here |

#### What was chosen and why
**Piece Table.** The document is represented as two immutable backing buffers (`original_` — loaded from disk, never modified; `add_` — append-only buffer for all new text) plus a list of `Piece` structs, each recording `{source, start, length}`.

An insert at any position splits at most one existing piece into two and inserts a new piece pointing into `add_`. **No text is ever copied or moved.** For the common case of sequential typing (cursor at the end), the new character is appended to `add_` and the last piece's length is incremented in place — one integer update.

A `coalesceAdjacentPieces()` pass after each erase merges piece records that are now contiguous in the same backing buffer, preventing unbounded piece-list growth over a long editing session.

**The Gap Buffer is also implemented** (`GapBuffer.cpp`) to make the trade-off empirically measurable — see the benchmark section below.

### 2. Line Index — Incremental Maintenance

#### The problem
The editor works in (row, column) coordinates. Rendering, cursor movement, and search all need to convert between `(row, col)` and a flat byte offset into the document. This requires knowing where each line starts.

The naive solution is to scan the entire document for `\n` characters every time the line index is needed. For a file with `N` characters, that is O(N) per keystroke.

#### The solution
`TextDocument` maintains a `lineStarts_` vector of byte offsets. It is updated **incrementally** after every edit:
- **Character insert (non-newline):** The total number of lines does not change. Only the offsets of lines *after* the edit point shift by `+1`. This is one `std::for_each` over the suffix of `lineStarts_` — O(lines after the edit point).
- **Newline insert:** One new entry is inserted into `lineStarts_` at the split point, and all subsequent entries shift by `+1`. O(lines after the cursor).
- **Line merge (backspace at column 0):** One entry is removed from `lineStarts_`, and all subsequent entries shift by `-removed_line_length`. O(lines after the cursor).
- **Multi-line paste / cut (range operations):** These fall back to a full rescan. Range operations are rare; an incremental path for them would add complexity without meaningful benefit at this project's scale.

### 3. Undo / Redo — Command Pattern with Interface Decoupling

#### The problem
Undo/redo requires recording "what happened" in a form that can be reversed. The two implementation choices are:
1. **Memento pattern** — snapshot the entire document state before each edit, restore it on undo. Simple to implement; O(edit × document size) memory.
2. **Command pattern** — each edit is an object with `execute()` and `undo()`. Memory cost is proportional to the edit, not the document.

#### What was chosen and why
**Command pattern.** `EditCommand` is an abstract base with `execute()` and `undo()`. Concrete subclasses (`InsertCharCommand`, `DeleteCharCommand`, `InsertNewlineCommand`, `DeleteNewlineCommand`) record exactly what they need to reverse themselves.

The key design constraint: **command subclasses do not depend on `TextEditor`**. They depend on:
1. `TextDocument&` — to actually mutate the text
2. `CursorOwner` — a single-method interface (`setCursor(col, row)`) that `TextEditor` implements

This means undo/redo logic cannot accidentally reach into unrelated editor state (search state, clipboard, autosave, etc.). Adding a new command type requires no changes to `TextEditor`. `UndoRedoStack` owns the two stacks. `execute()` pushes to the undo stack and clears the redo stack.

### 4. Rendering / Input / Editing — Three Separate Classes

Most text editor implementations conflate rendering, input handling, and document mutation in one class. This project separates them into three:

| Class | Responsibility | What it cannot do |
|---|---|---|
| `TextEditor` | Owns and mutates all editor state | Emit ANSI codes; decode keystrokes |
| `ScreenRenderer` | Converts TextEditor state to ANSI escape sequences | Mutate the document (takes `const TextEditor&`) |
| `InputHandler` | Maps raw bytes to `TextEditor` method calls | Access `FrameBuffer`; render anything |

The constraints are enforced by the type system, not convention:
- `ScreenRenderer::render` takes `const TextEditor&` — the compiler rejects any mutation attempt.
- `InputHandler` has no `FrameBuffer` in scope — it cannot render by construction.

`ScreenRenderer` writes its output into a `FrameBuffer` (a `std::string` accumulator) and flushes it to stdout in one `write()` syscall. This prevents visible flicker.

### 5. Background Autosave — Concurrency Design

#### The problem
Autosave must write to disk periodically. A disk write can take tens to hundreds of milliseconds (especially on a slow filesystem or over a network mount). If the write happens on the main thread, typing stalls.

#### The solution
`AutosaveWorker` runs on a dedicated `std::thread`. The main thread and the worker share exactly one resource: the document text. Access is guarded by `documentMutex_` in `TextEditor`.

**Critical design point: the lock is held only long enough to copy the text out.**

```cpp
// Inside AutosaveWorker — simplified
{
    std::lock_guard<std::mutex> lock(editor_.documentMutex());
    snapshot = editor_.getAllText(); // fast in-memory copy
}
// Lock is now released.
writeToFile(path, snapshot); // slow disk operation — no lock held
```

The main thread (typing loop) is never blocked by a slow filesystem. The worst case is contending for the lock during the microseconds it takes to copy text. The worker wakes on either a 30-second timer or an explicit `notify()` from `TextEditor`.

### 6. Selection and Clipboard

`Selection` is a value type: two `(row, col)` points (anchor and active), plus a `normalized()` method that returns them in top-left → bottom-right order. No editor logic lives in `Selection` — it is a pure data holder.

Copy, cut, and paste go through `TextDocument`'s offset-based range API. This means multi-line selections work correctly without any row-by-row special-casing — the range API doesn't know or care whether the range spans a newline.

**OS clipboard (xclip / pbcopy) is intentionally not implemented.** The in-process implementation is sufficient to demonstrate the copy/cut/paste mechanics without introducing a platform dependency. The abstraction layer (`Clipboard` class) makes it easy to add later.

### 7. Diff-Based Version History

#### Algorithm choice
`DiffEngine::diffLines` implements the classic **O(n·m) LCS dynamic program** over a vector of lines. It was chosen over Myers' O(ND) algorithm for two reasons:
1. **Inputs are editor buffers, not large repositories.** A user's open file is typically hundreds to a few thousand lines. The constant factor of O(n·m) is completely acceptable at that scale.
2. **The LCS DP is simpler and more obviously correct.** Reaching for Myers would optimize a cost this project never actually has.

#### Version storage
`VersionHistory` stores the complete text of each snapshot. Reverse-diff storage saves space when you have thousands of snapshots of large files. At editor-buffer scale with a handful of saves per session, full snapshots are simpler and the memory cost is trivial.

### 8. Gap Buffer — Trade-off Analysis with Benchmark

The Gap Buffer is not used by the editor at runtime. It exists to make the Piece Table trade-off empirically measurable.

A Gap Buffer is a contiguous array with a movable gap (reserved free space) positioned at the most recent edit point. Inserting at the gap: shrink the gap by writing into it — O(k), no memory movement. Deleting at the gap: extend the gap boundary — O(1). Repositioning the gap to a new edit point: copy characters across the gap — O(distance moved).

### 9. Syntax Highlighting — Stateless Per-Line Tokenizer

The key decision is a **stateless per-line API with caller-managed cross-line state.** The caller (`ScreenRenderer`) holds a `bool inBlockComment` and passes it by reference.

- **Thread safety:** a stateless function is trivially safe to call from multiple threads.
- **Correctness:** `ScreenRenderer` pre-scans all rows above the viewport each frame to compute the correct initial `inBlockComment` for the first visible row.

Single left-to-right scan, O(line length). Each character is examined exactly once. Tokens cover every character of the line — no gaps, no overlaps. Keyword lookup is O(1) via `std::unordered_set<std::string>`.

### 10. Regex Engine — Thompson NFA

#### Why not `std::regex`?
`std::regex` uses recursive backtracking. Certain patterns cause O(2^n) behavior that can freeze a UI thread. For a search-as-you-type feature, this is unacceptable.

#### Algorithm — Thompson NFA simulation
**Step 1 — Compile.** A recursive-descent parser converts the pattern into an NFA using Thompson's construction.
**Step 2 — Simulate.** Instead of one active state (backtracking), maintain the *set* of all states the NFA could currently be in. On each input character, advance every state in the set.

**Complexity guarantee:** at most `|states|` states are ever active simultaneously, and `|states| = O(|pattern|)`. Total work per input character is O(|pattern|), and across the whole text it is O(|pattern| × |text|) — never worse, avoiding catastrophic backtracking.

---

## Tests & Performance

### Unit Tests — 50 / 50 Passing
- **PieceTable**: insert, erase, mid-document split
- **GapBuffer**: 10 cases (sequential append, front insert, mid insert, erase, edge cases)
- **TextDocument**: insertChar, newline split, line merge, incremental index invariant, offset math
- **TextEditor**: undo/redo sequence, literal search match count
- **DiffEngine**: pure addition, pure removal, identical inputs, version-history integration
- **Selection/Clipboard**: copy-paste round-trip across line boundaries
- **SyntaxHighlighter**: C++ keyword, line comment, block-comment cross-line span, language auto-detection
- **RegexEngine**: literal, .*, +, ?, alternation, character class, negated class, \d \w shorthands, ^ and $ anchors, grouping, validity check, empty text

### Performance Benchmarks (PieceTable vs GapBuffer)
4 Scenarios (N = 50,000 ops each, WSL2 / g++ -O2)

| Scenario | PieceTable | GapBuffer | Winner |
|---|---:|---:|---|
| A: Sequential append | 2.38 ms | 2.78 ms | PieceTable |
| B: Random mid-document insert | 2,933 ms | 2.75 ms | **GapBuffer ×1,000×** |
| C: Sequential erase from front | 2.56 ms | 0.33 ms | **GapBuffer ×8×** |
| D: Alternating front/back insert | 856 ms | 13.84 ms | **GapBuffer ×62×** |

**Interpretation:**
The Piece Table wins on sequential typing (Scenario A) because it never moves any text.
The Gap Buffer dominates every scenario involving changes of edit location because it stores text contiguously — modern CPUs' cache prefetchers handle sequential memory copies extremely well. But that same contiguity is its weakness: every change of edit location requires repositioning the gap, which is O(document size) in the worst case.

**For this editor, Piece Table is the right choice:** sequential typing is the dominant workload, random-position mutation is rare.

---

## Keybindings

| Key | Action |
|---|---|
| Arrow keys | Move cursor |
| Enter / Backspace | Insert newline / delete character |
| `Ctrl+Z` / `Ctrl+Y` | Undo / redo |
| `Ctrl+F` | Open search (incremental, live-highlighted) |
| `Ctrl+R` | Toggle regex search mode — `[regex]` appears in the status bar |
| `Ctrl+N` / `Ctrl+B` | Next / previous match |
| `Ctrl+Space` | Begin / anchor selection |
| `Ctrl+C` / `Ctrl+X` / `Ctrl+V` | Copy / cut / paste |
| `Ctrl+D` | Show LCS diff since last save |
| `Ctrl+S` / `Ctrl+A` | Save / save as |
| `Ctrl+Q` | Quit (prompts if there are unsaved changes) |

---

## Build & Run

Requires Linux or WSL2, `g++ ≥ 10`, `make`.

```bash
# Build
make

# Open a file
./editor yourfile.cpp

# Open with no file (name it later with Ctrl+A)
./editor

# Run all 50 unit tests + benchmark table
make test

# AddressSanitizer + UBSan build for memory safety validation
make debug
```

---

## License

MIT
