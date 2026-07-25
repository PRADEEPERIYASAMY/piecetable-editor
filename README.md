# PieceTable Editor

> A high-performance, feature-complete terminal text editor built from scratch in modern C++17 — no external dependencies, no runtime libraries, no shortcuts.

<div align="center">

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![C++ Standard](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![Build](https://img.shields.io/badge/build-make-brightgreen.svg)](Makefile)
[![Tests](https://img.shields.io/badge/tests-50%20%2F%2050%20passing-brightgreen.svg)](tests/tests.cpp)
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20WSL2-lightgrey.svg)](#build--run)
[![Compiler](https://img.shields.io/badge/compiler-g%2B%2B%20%E2%89%A5%2010-orange.svg)](#build--run)
[![Sanitizers](https://img.shields.io/badge/sanitizers-ASan%20%2B%20UBSan-red.svg)](#memory-safety)
[![No Dependencies](https://img.shields.io/badge/dependencies-none-brightgreen.svg)](#build--run)
[![Threads](https://img.shields.io/badge/threads-pthreads-blueviolet.svg)](#6-background-autosave--concurrency-design)
[![PRs Welcome](https://img.shields.io/badge/PRs-welcome-brightgreen.svg)](CONTRIBUTING.md)
[![Changelog](https://img.shields.io/badge/changelog-Keep%20a%20Changelog-orange.svg)](CHANGELOG.md)
[![Cite](https://img.shields.io/badge/cite-CITATION.cff-blue.svg)](CITATION.cff)

</div>

---

Every component was engineered deliberately: algorithmic trade-offs are documented, benchmarked, and justified; every class has a single clearly-defined responsibility enforced by the type system; and the concurrency model is explicit about what is shared and what is not.

---

## Table of Contents

1. [Features](#features)
2. [Project Structure](#project-structure)
3. [Architecture](#architecture)
4. [Component Reference](#component-reference)
5. [Engineering Design Notes](#engineering-design-notes)
6. [Tests & Performance](#tests--performance)
7. [Keybindings](#keybindings)
8. [Build & Run](#build--run)
9. [Memory Safety](#memory-safety)
10. [Community & Contributing](#community--contributing)
11. [Changelog](#changelog)
12. [Citation](#citation)
13. [License](#license)

---

## Features

| Category | Capability |
|---|---|
| **File I/O** | Open, edit, and save any plain-text or source-code file; `Ctrl+A` prompts for a new filename |
| **Undo / Redo** | Per-keystroke undo/redo via the Command pattern — O(edit) memory, not O(document) |
| **Incremental search** | Live match highlighting across every visible line as you type (`Ctrl+F`) |
| **Regex search** | Toggle with `Ctrl+R`; Thompson NFA — O(|pattern|×|text|) worst-case, no catastrophic backtracking |
| **Syntax highlighting** | C, C++, and Python: keywords, strings, line/block comments, preprocessor directives, numbers |
| **Selection & clipboard** | Mark a range with `Ctrl+Space`; copy/cut/paste across line boundaries via offset-based range API |
| **Version history** | Every save snapshotted with a `std::time_t` timestamp; `Ctrl+D` shows LCS diff since the last save |
| **Background autosave** | Dedicated `std::thread`; disk write happens **after** mutex release — zero typing latency impact |
| **Dirty tracking** | FNV-based content hash — the editor knows precisely whether anything changed since the last save |
| **Viewport scrolling** | Soft-wrapped viewport follows the cursor; terminal resize handled via `SIGWINCH` |
| **Flicker-free rendering** | Entire frame accumulated in a `FrameBuffer` and flushed in a single `write()` syscall |
| **No external deps** | Zero dependencies beyond the C++17 standard library and POSIX |

**Project overview — all subsystems at a glance:**

```mermaid
flowchart LR
    subgraph storage["Text Storage"]
        PT["PieceTable\noriginalText + appendedText\nPiece list + coalesce"]
        GB["GapBuffer\nContiguous array\nMovable gap · 4 KB default"]
    end
    subgraph document["Document Layer"]
        TD["TextDocument\nlineStarts_ vector\nIncremental index\nOffset-based range API"]
    end
    subgraph editing["Editing"]
        EC["EditCommand\nInsert · Delete · Merge · Newline"]
        UR["UndoRedoStack\nundo stack · redo stack"]
    end
    subgraph search["Search"]
        LS["Literal incremental search"]
        RE["RegexEngine\nThompson NFA\nEpsilon-closure simulation"]
    end
    subgraph highlight["Syntax Highlighting"]
        SH["SyntaxHighlighter\nC/C++ · Python\nStateless per-line API"]
    end
    subgraph rendering["Rendering"]
        SR["ScreenRenderer\nFrameBuffer\nSingle write syscall"]
        TN["Terminal\nPOSIX raw mode · SIGWINCH"]
    end
    subgraph selection["Selection and Clipboard"]
        SEL["Selection\nanchor · active · normalized"]
        CB["Clipboard\nIn-process single slot"]
    end
    subgraph history["Version History"]
        VH["VersionHistory\nFull-text snapshots · savedAt"]
        DE["DiffEngine\nLCS dynamic program"]
    end
    subgraph concurrency["Concurrency"]
        AW["AutosaveWorker\nstd::thread\ncondition_variable\natomic dirty flag"]
    end
    subgraph testing["Testing"]
        TS["50 unit tests\n4-scenario benchmark\nASan + UBSan"]
    end
```

---

## Project Structure

```
piecetable-editor/
│
├── include/                      # One header per class / module
│   ├── AutosaveWorker.hpp        #   Background autosave thread (condition variable wake)
│   ├── Clipboard.hpp             #   In-process single-slot string store
│   ├── DiffEngine.hpp            #   O(n·m) LCS line diff; DiffOp / DiffLine types
│   ├── EditCommand.hpp           #   CursorOwner interface + abstract EditCommand + 4 concrete subclasses
│   ├── GapBuffer.hpp             #   Contiguous-array editor backend (benchmark comparator)
│   ├── InputHandler.hpp          #   Raw-byte → TextEditor call mapper (namespace, no state)
│   ├── PieceTable.hpp            #   Two-buffer piece-list text store; Piece / SourceBuffer types
│   ├── RegexEngine.hpp           #   Thompson NFA regex engine; Match type
│   ├── ScreenRenderer.hpp        #   ANSI escape emitter (namespace); FrameBuffer type
│   ├── Selection.hpp             #   Value-type selection (anchor + active + normalized())
│   ├── SyntaxHighlighter.hpp     #   Stateless per-line tokenizer; TokenType / SyntaxToken / Language
│   ├── Terminal.hpp              #   POSIX raw mode + SIGWINCH (namespace, no state)
│   ├── TextDocument.hpp          #   Row/col abstraction + incremental lineStarts_ vector
│   ├── TextEditor.hpp            #   Orchestrator; CursorOwner impl; all editor state
│   ├── UndoRedoStack.hpp         #   Two std::vector<unique_ptr<EditCommand>> stacks
│   └── VersionHistory.hpp        #   Full-snapshot history; Snapshot struct with savedAt timestamp
│
├── src/                          # Implementations (one .cpp per .hpp)
│   ├── AutosaveWorker.cpp
│   ├── DiffEngine.cpp
│   ├── EditCommand.cpp
│   ├── GapBuffer.cpp
│   ├── InputHandler.cpp
│   ├── PieceTable.cpp
│   ├── RegexEngine.cpp           # ~400 lines: NFA parser + simulator
│   ├── ScreenRenderer.cpp        # ~330 lines: syntax-aware ANSI frame builder
│   ├── SyntaxHighlighter.cpp     # ~280 lines: C/C++/Python tokenizer
│   ├── Terminal.cpp
│   ├── TextDocument.cpp
│   ├── TextEditor.cpp            # ~560 lines: all editor operations
│   ├── UndoRedoStack.cpp
│   ├── VersionHistory.cpp
│   └── main.cpp                  # 18 lines: raw mode, SIGWINCH, openFile, run()
│
├── tests/
│   └── tests.cpp                 # 660 lines: 50 unit tests + benchmark harness
│
├── Makefile
├── CHANGELOG.md
├── CITATION.cff
├── CODE_OF_CONDUCT.md
├── CONTRIBUTING.md
├── LICENSE
├── SECURITY.md
└── SUPPORT.md
```

**Source-file sizes at a glance:**

| File | Size |
|---|---|
| `src/RegexEngine.cpp` | ~12.6 KB |
| `src/TextEditor.cpp` | ~17.8 KB |
| `src/ScreenRenderer.cpp` | ~10.5 KB |
| `src/SyntaxHighlighter.cpp` | ~8.8 KB |
| `src/InputHandler.cpp` | ~4.8 KB |
| `src/PieceTable.cpp` | ~6.9 KB |
| `src/GapBuffer.cpp` | ~3.7 KB |
| `tests/tests.cpp` | ~24.2 KB |

---

## Architecture

The three top-level boundaries are enforced by the **type system**, not convention:

- `ScreenRenderer::render` takes `const TextEditor&` — the compiler rejects any write.
- `InputHandler` has no `FrameBuffer` in scope — it structurally cannot emit output.

```mermaid
flowchart TB
    subgraph entry["main.cpp — Entry Point"]
        M1["Terminal::enableRawMode()"]
        M2["signal(SIGWINCH, handler)"]
        M3["editor.openFile(argv[1])"]
        M4["editor.run()"]
    end

    subgraph editor["TextEditor — Orchestrator"]
        direction TB
        subgraph state["Owned State"]
            TD["TextDocument\nPieceTable + lineStarts_"]
            UR["UndoRedoStack\nundo / redo vectors"]
            SEL["Selection\nanchor + active"]
            CB["Clipboard\nsingle-slot string"]
            VH["VersionHistory\nfull-text snapshots"]
        end
        subgraph concurrency["Concurrency"]
            AW["AutosaveWorker\nstd::thread\ncondition_variable\natomic dirty flag"]
            MX["documentMutex_\nstd::mutex"]
        end
    end

    subgraph rendering["Read-only — const TextEditor&"]
        SR["ScreenRenderer\nnamespace"]
        FB["FrameBuffer\nstd::string accumulator"]
        SH["SyntaxHighlighter\nstateless tokenizer"]
    end

    subgraph inputpath["Write-path — TextEditor&"]
        IH["InputHandler\nnamespace"]
    end

    subgraph posix["OS / POSIX"]
        T["Terminal namespace\nraw mode · ioctl · SIGWINCH"]
        DISK["Filesystem\n.bak autosave"]
    end

    M1 --> M2 --> M3 --> M4
    M4 --> editor
    editor -->|"const TextEditor&"| SR
    SR --> SH
    SR --> FB
    FB -->|"single write() syscall"| T
    IH -->|"TextEditor& method calls"| editor
    T -->|"raw bytes"| IH
    AW -->|"lock · snapshot · unlock"| MX
    AW -->|"write after unlock"| DISK
    MX --- TD
```

**Full class hierarchy and relationships:**

```mermaid
classDiagram
    direction TB

    class CursorOwner {
        <<interface>>
        +setCursor(col, row)
    }

    class PieceTable {
        -string originalText_
        -string appendedText_
        -Piece[] pieces_
        -size_t documentLength_
        +loadFromFile(path)
        +insert(position, text)
        +erase(position, length)
        +text() string
        +size() size_t
        +charAt(index) char
        +substring(start, len) string
        -appendAndTrackPiece(text) Piece
        -coalesceAdjacentPieces()
    }

    class Piece {
        +SourceBuffer source
        +size_t offset
        +size_t length
    }

    class GapBuffer {
        -char[] buf_
        -size_t gapStart_
        -size_t gapEnd_
        +insert(position, text)
        +erase(position, length)
        +text() string
        +size() size_t
        +charAt(index) char
        -moveGapTo(pos)
        -ensureGap(needed)
    }

    class TextDocument {
        -PieceTable storage_
        -size_t[] lineStarts_
        +lineCount() int
        +lineText(row) string
        +lineLength(row) int
        +fullText() string
        +insertChar(row, col, c)
        +deleteChar(row, col)
        +insertNewline(row, splitCol)
        +mergeWithNextLine(row)
        +toOffset(row, col) size_t
        +textInRange(start, end) string
        +eraseRange(start, end)
        +insertAt(offset, text)
        +replaceAllText(text)
    }

    class EditCommand {
        <<abstract>>
        +undo()
        +redo()
    }

    class InsertCharCommand {
        -int row_, col_
        -char character_
        -CursorPos before_, after_
        +undo()
        +redo()
    }

    class DeleteCharCommand {
        -int row_, col_
        -char deletedCharacter_
        -CursorPos before_, after_
        +undo()
        +redo()
    }

    class MergeLinesCommand {
        -int row_, previousLineLength_
        -CursorPos before_, after_
        +undo()
        +redo()
    }

    class InsertNewlineCommand {
        -int row_, splitColumn_
        -CursorPos before_, after_
        +undo()
        +redo()
    }

    class UndoRedoStack {
        -EditCommand[] undoStack_
        -EditCommand[] redoStack_
        +push(command)
        +undo()
        +redo()
        +canUndo() bool
        +canRedo() bool
    }

    class Selection {
        +bool active
        +int anchorRow
        +int anchorCol
        +int activeRow
        +int activeCol
        +clear()
        +begin(row, col)
        +moveTo(row, col)
        +isEmpty() bool
        +normalized(sR, sC, eR, eC)
    }

    class Clipboard {
        -string contents_
        +set(text)
        +contents() string
        +empty() bool
    }

    class DiffEngine {
        <<static>>
        +diffLines(old, new) DiffLine[]
        +splitIntoLines(text) string[]
    }

    class VersionHistory {
        -Snapshot[] snapshots_
        +recordSnapshot(content)
        +snapshotCount() size_t
        +snapshotAt(index) Snapshot
        +hasHistory() bool
        +diffAgainstLatest(content) DiffLine[]
        +diffBetween(older, newer) DiffLine[]
    }

    class Snapshot {
        +time_t savedAt
        +string content
    }

    class SyntaxHighlighter {
        <<static>>
        +fromFilename(name) Language
        +tokenize(line, lang, inBlock) SyntaxToken[]
        +ansiColor(type) char*
    }

    class RegexEngine {
        <<static>>
        +findAll(pattern, text) Match[]
        +isValid(pattern) bool
    }

    class AutosaveWorker {
        -thread worker_
        -bool running_
        -bool dirty_
        -condition_variable wakeSignal_
        +start()
        +stop()
        +notifyDirty()
    }

    class Terminal {
        <<namespace>>
        +enableRawMode()
        +disableRawMode()
        +windowSize(rows, cols) int
        +clearScreen()
        +moveCursorTo(row, col)
    }

    class FrameBuffer {
        +string data
        +append(s, len)
        +appendStr(s)
    }

    class ScreenRenderer {
        <<namespace>>
        +render(editor)
    }

    class InputHandler {
        <<namespace>>
        +processNextKeypress(editor)
    }

    class TextEditor {
        -TextDocument document_
        -UndoRedoStack undoRedo_
        -Selection selection_
        -Clipboard clipboard_
        -VersionHistory versionHistory_
        -mutex documentMutex_
        -string filename_
        -bool dirty_
        -uint64_t savedContentHash_
        +run()
        +insertCharacter(c)
        +undo()
        +redo()
        +save()
        +saveAs()
        +openFile(path)
        +beginSearch()
        +toggleRegexSearch()
        +setCursor(col, row)
    }

    CursorOwner <|.. TextEditor : implements
    EditCommand <|-- InsertCharCommand
    EditCommand <|-- DeleteCharCommand
    EditCommand <|-- MergeLinesCommand
    EditCommand <|-- InsertNewlineCommand

    TextDocument *-- PieceTable : storage_
    PieceTable *-- Piece : pieces_

    TextEditor *-- TextDocument
    TextEditor *-- UndoRedoStack
    TextEditor *-- Selection
    TextEditor *-- Clipboard
    TextEditor *-- VersionHistory
    TextEditor *-- AutosaveWorker

    UndoRedoStack o-- EditCommand : stacks

    InsertCharCommand --> TextDocument
    InsertCharCommand --> CursorOwner
    DeleteCharCommand --> TextDocument
    DeleteCharCommand --> CursorOwner
    MergeLinesCommand --> TextDocument
    MergeLinesCommand --> CursorOwner
    InsertNewlineCommand --> TextDocument
    InsertNewlineCommand --> CursorOwner

    VersionHistory *-- Snapshot
    VersionHistory ..> DiffEngine : uses

    ScreenRenderer ..> TextEditor : reads const ref
    ScreenRenderer ..> FrameBuffer : builds
    ScreenRenderer ..> SyntaxHighlighter : tokenizes
    InputHandler ..> TextEditor : drives
    RegexEngine ..> TextEditor : used by search
```

**Data model — entities and relationships:**

```mermaid
erDiagram
    TextEditor {
        int cursorCol
        int cursorRow
        int screenRows
        int screenCols
        int rowOffset
        int colOffset
        string filename
        bool dirty
        uint64 savedContentHash
        string searchQuery
        bool searchActive
        bool regexSearch
    }
    TextDocument {
        vector lineStarts
    }
    PieceTable {
        string originalText
        string appendedText
        size_t documentLength
    }
    Piece {
        SourceBuffer source
        size_t offset
        size_t length
    }
    UndoRedoStack {
        vector undoStack
        vector redoStack
    }
    EditCommand {
        int row
        int col
        CursorPos before
        CursorPos after
    }
    Selection {
        bool active
        int anchorRow
        int anchorCol
        int activeRow
        int activeCol
    }
    Clipboard {
        string contents
    }
    VersionHistory {
        vector snapshots
    }
    Snapshot {
        time_t savedAt
        string content
    }
    AutosaveWorker {
        chrono_seconds interval
        atomic_bool running
        atomic_bool dirty
    }

    TextEditor ||--|| TextDocument : "owns"
    TextEditor ||--|| UndoRedoStack : "owns"
    TextEditor ||--|| Selection : "owns"
    TextEditor ||--|| Clipboard : "owns"
    TextEditor ||--|| VersionHistory : "owns"
    TextEditor ||--|| AutosaveWorker : "owns"
    TextDocument ||--|| PieceTable : "storage_"
    PieceTable ||--o{ Piece : "pieces_"
    UndoRedoStack ||--o{ EditCommand : "undo and redo stacks"
    VersionHistory ||--o{ Snapshot : "snapshots_"
```

**Editor lifecycle — from launch to quit:**

| Phase | Event | Details |
|---|---|---|
| **Startup** | Terminal raw mode enabled | `enableRawMode()` saves `termios`; `SIGWINCH` handler registered |
| | File opened | `PieceTable` loads `originalText_`; `lineStarts_` built by full scan (once); `savedContentHash_` computed |
| | AutosaveWorker started | `std::thread` launched; `condition_variable` initialized; 30-second timer begins |
| **Interactive Editing** | First frame rendered | `ScreenRenderer` builds `FrameBuffer`; single `write()` syscall flushes frame |
| | Character typed | `InputHandler` reads raw byte; `InsertCharCommand` pushed to undo stack; `lineStarts_` shifted O(lines after cursor); `notifyDirty()` flips atomic flag |
| | `Ctrl+S` Save | File written to disk; `Snapshot{savedAt, content}` recorded; `savedContentHash_` updated |
| | Autosave fires | Worker wakes after 30 s; mutex locked → text copied → mutex released; `.bak` written **after** mutex release |
| | `Ctrl+Z` Undo | `EditCommand::undo()` reverses edit; cursor restored to `CursorPos before_` |
| | `Ctrl+F` Search | `refreshMatches()` called per keystroke; `RegexEngine::findAll()` if regex mode |
| | `Ctrl+D` Diff | `DiffEngine::diffLines()` over saved snapshot; LCS DP O(n×m) over line vectors |
| **Shutdown** | `Ctrl+Q` pressed | `QuitPrompt` shown if `dirty_ == true` |
| | User confirms | `AutosaveWorker::stop()` joins thread; `disableRawMode()` restores `termios`; process exits |

**Editor mode state machine:**

```mermaid
stateDiagram-v2
    [*] --> Normal : editor.run() starts

    Normal --> Search : Ctrl+F beginSearch()
    Normal --> SelectionActive : Ctrl+Space beginSelection()
    Normal --> Dirty : any insert or delete or paste
    Normal --> [*] : Ctrl+Q and not dirty

    Dirty --> Dirty : more edits
    Dirty --> Saved : Ctrl+S save()
    Dirty --> QuitPrompt : Ctrl+Q

    Saved --> Normal : continue editing
    Saved --> [*] : Ctrl+Q

    QuitPrompt --> Dirty : user declines
    QuitPrompt --> [*] : user confirms quit

    Search --> RegexSearch : Ctrl+R toggleRegexSearch()
    Search --> Normal : Escape or Enter
    Search --> Search : character typed refreshMatches()

    RegexSearch --> Search : Ctrl+R again
    RegexSearch --> Normal : Escape or Enter

    SelectionActive --> Normal : Ctrl+Space again or Escape
    SelectionActive --> ClipboardOp : Ctrl+C or Ctrl+X
    ClipboardOp --> Normal : clipboard updated selection cleared

    Normal --> Paste : Ctrl+V
    Paste --> Dirty : text inserted at cursor

    Normal --> UndoRedo : Ctrl+Z or Ctrl+Y
    UndoRedo --> Normal : command applied

    Normal --> DiffView : Ctrl+D
    DiffView --> Normal : any key
```

---

## Component Reference

### PieceTable

`include/PieceTable.hpp` · `src/PieceTable.cpp`

The same technique used by VS Code and Atom. Two immutable-ish backing buffers plus an ordered list of `Piece` values; editing means editing the list, not copying characters.

**Types:**
```cpp
enum class SourceBuffer { Original, Appended };

struct Piece {
    SourceBuffer source;
    size_t offset;   // start offset within the source buffer
    size_t length;   // number of characters this piece contributes
};
```

**Public API:**
```cpp
class PieceTable {
public:
    void        loadFromFile(const std::string& filePath);
    void        insert(size_t position, const std::string& text);
    void        erase(size_t position, size_t length);
    std::string text()                              const;
    size_t      size()                              const;
    char        charAt(size_t index)                const;
    std::string substring(size_t start, size_t len) const;
};
```

**Insert algorithm:**

```mermaid
flowchart TD
    A(["insert(position, text)"]) --> B{"position == size?"}

    B -->|"Yes — append path"| C["appendedText_ += text\nextend last piece length\n1 integer update"]
    C --> Z(["Done"])

    B -->|"No — mid-document"| D["Walk pieces_ list\naccumulate offset until\nrunning total >= position"]
    D --> E{"position falls at\na piece boundary?"}

    E -->|"Yes"| F["Insert new Piece into appendedText_\nNo split needed"]
    E -->|"No — split required"| G["Split target piece P into\nP_left: offset to split\nP_right: split to end"]
    G --> H["Insert new Piece between\nP_left and P_right"]

    F --> I["appendedText_ += text"]
    H --> I
    I --> J["documentLength_ += text.size()"]
    J --> Z
```

**Erase algorithm:**

```mermaid
flowchart TD
    A(["erase(position, length)"]) --> B["Walk pieces_ to find\nfirst affected piece"]
    B --> C{"Erase range within\none piece?"}

    C -->|"Yes"| D{"Split the piece?"}
    D -->|"Erase at start"| E["Adjust piece offset and length"]
    D -->|"Erase at end"| F["Reduce piece length"]
    D -->|"Erase in middle"| G["Split piece into two\nwith gap between"]

    C -->|"No — spans multiple"| H["Trim first affected piece"]
    H --> I["Remove fully-covered middle pieces"]
    I --> J["Trim last affected piece"]

    E --> K["coalesceAdjacentPieces()"]
    F --> K
    G --> K
    J --> K

    K --> L["Merge adjacent pieces in same buffer"]
    L --> M["documentLength_ -= length"]
    M --> Z(["Done"])
```

---

### GapBuffer

`include/GapBuffer.hpp` · `src/GapBuffer.cpp`

Not used at runtime. Implemented as a benchmark comparator. `kDefaultGap = 1 << 12` (4 KB).

**Physical layout:**
```
[ pre-gap text (gapStart_ bytes) ] [ gap — 4KB unused capacity ] [ post-gap text ]
```

**Complexity:**

| Operation | Cost |
|---|---|
| Insert at gap | O(k) amortized — no memory movement |
| Insert elsewhere | O(dist_to_gap) reposition + O(k) copy |
| Erase at gap | O(dist_to_gap) reposition, then O(1) |
| `charAt` / `text()` | O(1) / O(n) |

**Public API** (mirrors `PieceTable` for transparent swap):
```cpp
class GapBuffer {
public:
    void        insert(size_t position, const std::string& text);
    void        erase(size_t position, size_t length);
    std::string text()                              const;
    size_t      size()                              const;
    char        charAt(size_t index)                const;
    std::string substring(size_t start, size_t len) const;
};
```

---

### TextDocument

`include/TextDocument.hpp` · `src/TextDocument.cpp`

Row/column abstraction over `PieceTable`. The key contribution: **incremental `lineStarts_` maintenance** — no full rescan per keystroke.

**Incremental update cost:**

| Edit | Cost |
|---|---|
| Insert non-`\n` character | O(lines after cursor) |
| Insert `\n` | O(lines after cursor) |
| Backspace at column 0 (merge) | O(lines after cursor) |
| Multi-line paste / cut | O(total lines) — full rescan (rare) |

**Line index update logic:**

```mermaid
flowchart TD
    A(["Edit operation"]) --> B{"Kind of edit?"}

    B -->|"insertChar non-newline"| C["PT.insert(toOffset(row,col), c)"]
    C --> D["shiftLineStartsAfter(row, +1)\nO(lines after cursor)"]
    D --> Z(["Done"])

    B -->|"insertNewline"| E["PT.insert(toOffset(row,splitCol), newline)"]
    E --> F["lineStarts_.insert(row+1 entry)"]
    F --> G["shiftLineStartsAfter(row+1, +1)\nO(lines after cursor)"]
    G --> Z

    B -->|"mergeWithNextLine\nbackspace at col 0"| H["delta = -(lineLength(row+1)+1)"]
    H --> I["PT.erase at row+1 start - 1"]
    I --> J["lineStarts_.erase(row+1 entry)"]
    J --> K["shiftLineStartsAfter(row, delta)\nO(lines after cursor)"]
    K --> Z

    B -->|"Multi-line paste or cut"| L["PT.eraseRange or insertAt"]
    L --> M["Full lineStarts_ rescan\nO(total lines) — rare path"]
    M --> Z
```

**Public API:**
```cpp
class TextDocument {
public:
    int         lineCount()                                        const;
    std::string lineText(int row)                                  const;
    int         lineLength(int row)                                const;
    std::string fullText()                                         const;
    void        appendLine(const std::string& text);
    void        insertChar(int row, int col, char c);
    void        deleteChar(int row, int col);
    void        insertNewline(int row, int splitCol);
    void        mergeWithNextLine(int row);
    size_t      toOffset(int row, int col)                         const;
    std::string textInRange(size_t startOffset, size_t endOffset)  const;
    void        eraseRange(size_t startOffset, size_t endOffset);
    void        insertAt(size_t offset, const std::string& text);
    void        replaceAllText(const std::string& newText);
};
```

---

### EditCommand Hierarchy

`include/EditCommand.hpp` · `src/EditCommand.cpp`

> Command subclasses depend only on `CursorOwner` and `TextDocument&`. They cannot reach into search state, clipboard, autosave, or any other editor concern. Adding a new command type requires **zero changes to `TextEditor`**.

**Interfaces:**
```cpp
class CursorOwner {
public:
    virtual void setCursor(int col, int row) = 0;
};

struct CursorPos { int col, row; };

class EditCommand {
public:
    virtual void undo() = 0;
    virtual void redo() = 0;
};
```

**Concrete subclasses:**

| Class | Records | Undo action |
|---|---|---|
| `InsertCharCommand` | `(row, col, char, before, after)` | Delete `char`; restore `before` cursor |
| `DeleteCharCommand` | `(row, col, deletedChar, before, after)` | Re-insert `deletedChar`; restore `before` cursor |
| `MergeLinesCommand` | `(row, prevLineLength, before, after)` | Re-split the merged line; restore `before` cursor |
| `InsertNewlineCommand` | `(row, splitColumn, before, after)` | Merge the two lines back; restore `before` cursor |

**Undo / Redo sequence:**

```mermaid
sequenceDiagram
    participant User
    participant TE as TextEditor
    participant UR as UndoRedoStack
    participant CMD as EditCommand subclass
    participant TD as TextDocument
    participant CO as CursorOwner

    Note over User,CO: Normal edit — push command
    User ->> TE: insertCharacter('A')
    TE ->> TE: create InsertCharCommand(doc, this, row, col, 'A', before, after)
    TE ->> UR: push(command)
    UR ->> CMD: redo() — execute the edit
    CMD ->> TD: insertChar(row, col, 'A')
    UR ->> UR: undoStack_.push_back(cmd) redoStack_.clear()

    Note over User,CO: Ctrl+Z Undo
    User ->> TE: undo()
    TE ->> UR: undo()
    UR ->> CMD: undo()
    CMD ->> TD: deleteChar(row, col)
    CMD ->> CO: setCursor(before.col, before.row)
    UR ->> UR: redoStack_.push_back(cmd)

    Note over User,CO: Ctrl+Y Redo
    User ->> TE: redo()
    TE ->> UR: redo()
    UR ->> CMD: redo()
    CMD ->> TD: insertChar(row, col, 'A')
    CMD ->> CO: setCursor(after.col, after.row)
    UR ->> UR: undoStack_.push_back(cmd)
```

---

### UndoRedoStack

`include/UndoRedoStack.hpp` · `src/UndoRedoStack.cpp`

Uses `std::vector<std::unique_ptr<EditCommand>>` (not `std::stack`) for both stacks, giving `reserve` semantics. Pushing a new command clears the redo stack.

```cpp
class UndoRedoStack {
public:
    void push(std::unique_ptr<EditCommand> command);
    void undo();
    void redo();
    bool canUndo() const;
    bool canRedo() const;
};
```

---

### Selection

`include/Selection.hpp`

A pure value type — no virtual methods, no editor logic. `normalized()` handles backward selections.

```cpp
struct Selection {
    bool active = false;
    int anchorRow = 0, anchorCol = 0;
    int activeRow = 0, activeCol = 0;

    void clear();
    void begin(int row, int col);
    void moveTo(int row, int col);
    bool isEmpty() const;
    void normalized(int& startRow, int& startCol,
                    int& endRow,   int& endCol) const;
};
```

**Selection copy / cut / paste flow:**

```mermaid
sequenceDiagram
    participant User
    participant IH as InputHandler
    participant TE as TextEditor
    participant SEL as Selection
    participant TD as TextDocument
    participant CB as Clipboard

    Note over User,CB: Begin selection
    User ->> IH: Ctrl+Space
    IH ->> TE: beginSelection()
    TE ->> SEL: begin(cursorRow_, cursorCol_)

    Note over User,CB: Extend with arrow keys
    User ->> IH: Arrow keys
    IH ->> TE: moveCursorRight() etc.
    TE ->> SEL: moveTo(cursorRow_, cursorCol_)

    Note over User,CB: Copy
    User ->> IH: Ctrl+C
    IH ->> TE: copySelectionToClipboard()
    TE ->> SEL: normalized(startRow, startCol, endRow, endCol)
    TE ->> TD: toOffset(startRow, startCol)
    TE ->> TD: toOffset(endRow, endCol)
    TE ->> TD: textInRange(startOff, endOff)
    TE ->> CB: set(text)
    TE ->> SEL: clear()

    Note over User,CB: Paste
    User ->> IH: Ctrl+V
    IH ->> TE: pasteFromClipboard()
    TE ->> CB: contents()
    TE ->> TD: insertAt(toOffset(cursorRow_, cursorCol_), text)
```

---

### Clipboard

`include/Clipboard.hpp`

In-process single-slot store. OS clipboard integration (`xclip`/`pbcopy`) would be added here.

```cpp
class Clipboard {
public:
    void              set(const std::string& text);
    const std::string& contents() const;
    bool              empty()     const;
};
```

---

### DiffEngine

`include/DiffEngine.hpp` · `src/DiffEngine.cpp`

O(n·m) LCS dynamic program over `std::vector<std::string>` of lines.

```cpp
enum class DiffOp { Unchanged, Added, Removed };
struct DiffLine { DiffOp op; std::string text; };

class DiffEngine {
public:
    static std::vector<DiffLine> diffLines(
        const std::vector<std::string>& oldLines,
        const std::vector<std::string>& newLines);
    static std::vector<std::string> splitIntoLines(const std::string& text);
};
```

---

### VersionHistory

`include/VersionHistory.hpp` · `src/VersionHistory.cpp`

Full-text snapshots per save. `diffBetween` enables arbitrary historical comparisons.

```cpp
struct Snapshot {
    std::time_t savedAt;
    std::string content;
};

class VersionHistory {
public:
    void                   recordSnapshot(const std::string& content);
    size_t                 snapshotCount()              const;
    const Snapshot&        snapshotAt(size_t index)     const;
    bool                   hasHistory()                 const;
    std::vector<DiffLine>  diffAgainstLatest(const std::string& current) const;
    std::vector<DiffLine>  diffBetween(size_t older, size_t newer)       const;
};
```

**Save and diff flow:**

```mermaid
flowchart TD
    A(["Ctrl+S"]) --> B["TextEditor::save()"]
    B --> C["writeCurrentTextToDisk()"]
    C --> D["VersionHistory::recordSnapshot(content)"]
    D --> E["snapshots_.push_back(Snapshot{now, content})"]
    E --> F["savedContentHash_ = hash(content)"]
    F --> G["dirty_ = false"]

    H(["Ctrl+D"]) --> I["TextEditor::showVersionHistory()"]
    I --> J{"hasHistory()?"}
    J -->|"No"| K["setStatusMessage: no saved version"]
    J -->|"Yes"| L["currentText = document_.fullText()"]
    L --> M["diffAgainstLatest(currentText)"]
    M --> N["DiffEngine::splitIntoLines(oldText)"]
    N --> O["DiffEngine::splitIntoLines(newText)"]
    O --> P["Build LCS table dp[n+1][m+1]"]
    P --> Q["Backtrack: classify each line\nUnchanged / Added / Removed"]
    Q --> R["Return vector of DiffLine"]
    R --> S["Display in status area"]
    K --> S
    S --> T(["Rendered on next frame"])
```

---

### SyntaxHighlighter

`include/SyntaxHighlighter.hpp` · `src/SyntaxHighlighter.cpp`

Stateless per-line tokenizer. The caller (`ScreenRenderer`) threads `inBlockComment` state across rows.

```cpp
enum class TokenType { Normal, Keyword, Preprocessor, String, Number, Comment };

struct SyntaxToken {
    int start;    // 0-indexed column
    int length;
    TokenType type;
};

class SyntaxHighlighter {
public:
    enum class Language { None, Cpp, Python };
    static Language         fromFilename(const std::string& filename);
    static std::vector<SyntaxToken> tokenize(const std::string& line,
                                             Language lang,
                                             bool& inBlockComment);
    static const char*      ansiColor(TokenType type);
};
```

| Language | Extensions |
|---|---|
| `Cpp` | `.cpp` `.cc` `.cxx` `.hpp` `.hxx` `.h` `.c` |
| `Python` | `.py` |
| `None` | anything else |

**Per-line scan state machine:**

```mermaid
flowchart TD
    A(["tokenize(line, lang, inBlockComment)"]) --> B{"lang == None?"}

    B -->|"Yes"| C["Return single Normal token\ncovering entire line"]
    B -->|"No"| D{"inBlockComment == true?"}

    D -->|"Yes — inside block comment"| E{"Line contains close?"}
    E -->|"No"| F["Return single Comment token\ninBlockComment stays true"]
    E -->|"Yes"| G["Emit Comment tokens up to close\nend block comment\ncontinue scan after close"]

    D -->|"No"| H{"First char == hash?"}
    H -->|"Yes — C/C++ only"| I["Entire line is Preprocessor token"]
    H -->|"No"| J["Begin left-to-right scan\nO(line length) each char once"]

    J --> K{"Current char"}
    K -->|"slash"| L{"Next char == star?"}
    L -->|"Yes"| M["Start Comment token\nset inBlockComment = true\nscan until close or EOL"]
    L -->|"No"| N{"Next char == slash?"}
    N -->|"Yes"| O["Rest of line is Comment token"]
    N -->|"No"| P["Emit Normal token slash"]

    K -->|"quote char"| Q["Scan string literal\nstop at matching close\nEmit String token"]
    K -->|"digit 0-9"| R["Scan numeric literal\nEmit Number token"]
    K -->|"letter or underscore"| S["Scan identifier\nlookup in unordered_set O(1)\nEmit Keyword or Normal token"]
    K -->|"other"| T["Emit single-char Normal token"]

    M --> U{"End of line?"}
    O --> U
    P --> U
    Q --> U
    R --> U
    S --> U
    T --> U

    G --> U
    U -->|"No"| K
    U -->|"Yes"| V(["Return tokens covering every character"])
```

---

### RegexEngine

`include/RegexEngine.hpp` · `src/RegexEngine.cpp`

Thompson NFA construction + simulation. Guarantees O(|pattern| × |text|) worst-case.

```cpp
struct Match { int col; int length; };

class RegexEngine {
public:
    static std::vector<Match> findAll(const std::string& pattern,
                                      const std::string& text);
    static bool isValid(const std::string& pattern);
};
```

| Syntax | Meaning |
|---|---|
| `.` | Any character except newline |
| `*` `+` `?` | Greedy quantifiers |
| `\|` | Alternation |
| `(...)` | Grouping |
| `[abc]` `[a-z]` `[^abc]` | Character classes |
| `^` `$` | Start/end anchors |
| `\d` `\w` `\s` | Shorthands; `\D \W \S` negated |

**Thompson NFA compile and simulate:**

```mermaid
flowchart TD
    subgraph compile["Step 1 — Compile Pattern to NFA"]
        A(["pattern string"]) --> B["Recursive-descent parser"]
        B --> C{"Next token"}

        C -->|"literal char"| D["cat fragment\nstate match char state"]
        C -->|"dot"| E["cat fragment\nmatch any non-newline"]
        C -->|"pipe alternation"| F["alt fragment\ntwo epsilon branches"]
        C -->|"star"| G["star fragment\nloop or skip via epsilon"]
        C -->|"plus"| H["plus fragment\none mandatory then loop"]
        C -->|"question"| I["quest fragment\nmatch or skip via epsilon"]
        C -->|"open paren"| J["Recurse into sub-expression\nthen apply outer quantifier"]
        C -->|"bracket class"| K["Build char-set matcher\nfor ranges and negations"]
        C -->|"backslash escape"| L["Map to char-set\nd=digit w=word s=space"]
        C -->|"caret anchor"| M["anchor_start flag"]
        C -->|"dollar anchor"| N["anchor_end flag"]

        D --> O["Assembled NFA via epsilon transitions"]
        E --> O
        F --> O
        G --> O
        H --> O
        I --> O
        J --> O
        K --> O
        L --> O
        M --> O
        N --> O
    end

    subgraph simulate["Step 2 — Simulate NFA"]
        O --> P(["text string"])
        P --> Q["Compute initial epsilon-closure\nof NFA start state"]
        Q --> R["current_states = closure(start)"]

        R --> S{"For each char in text"}
        S --> T["For each state in current_states\nthat matches char: add successor"]
        T --> U["epsilon-closure of next_states"]
        U --> V{"Accept state in current_states?"}
        V -->|"Yes"| W["Record match col and length"]
        V -->|"No"| X["Continue"]
        W --> X
        X --> S
        S -->|"End of text"| Y(["Return vector of Match"])
    end
```

> Reference: Russ Cox — *"Regular Expression Matching Can Be Simple And Fast"* — <https://swtch.com/~rsc/regexp/regexp1.html>

---

### AutosaveWorker

`include/AutosaveWorker.hpp` · `src/AutosaveWorker.cpp`

**Concurrency primitives:**

| Primitive | Purpose |
|---|---|
| `documentMutex_` (ref) | Guards document during snapshot copy |
| `running_` (`atomic<bool>`) | Stop signal from `stop()` |
| `dirty_` (`atomic<bool>`) | Flip from `notifyDirty()` — cost ≈ one atomic store |
| `wakeSignal_` (`condition_variable`) | Wakes worker early when dirty |

**Injected callbacks:**
```cpp
using SnapshotProvider = std::function<std::string()>;    // called under lock
using DiskWriter       = std::function<bool(const std::string&)>; // called after unlock
using SaveNotifier     = std::function<void(const std::string&)>; // on success
```

**Constructor:**
```cpp
AutosaveWorker(std::mutex& documentMutex,
               SnapshotProvider snapshotProvider,
               DiskWriter diskWriter,
               SaveNotifier onSaved,
               std::chrono::seconds interval = std::chrono::seconds(10));
```

**Thread lifecycle:**

```mermaid
stateDiagram-v2
    [*] --> Constructed : AutosaveWorker constructor

    Constructed --> Running : start()
    Running --> Sleeping : wait_for(wakeSignal_, interval)

    Sleeping --> WokenTimer : 30s timeout elapsed
    Sleeping --> WokenNotify : notifyDirty() called from main thread

    WokenTimer --> CheckDirty
    WokenNotify --> CheckDirty

    CheckDirty --> Sleeping : dirty_.exchange(false) == false
    CheckDirty --> LockAndSnapshot : dirty_.exchange(false) == true

    LockAndSnapshot --> CopyText : lock_guard acquire documentMutex_
    CopyText --> UnlockAndWrite : snapshotProvider() called text copied lock released

    UnlockAndWrite --> WriteSuccess : diskWriter returns true
    UnlockAndWrite --> WriteFailed : diskWriter returns false

    WriteSuccess --> Notify : onSaved_ callback
    WriteFailed --> Sleeping : retry next wake

    Notify --> Sleeping : callback complete

    Running --> Stopped : stop() running_.store(false) thread.join()
    Stopped --> [*]
```

**Autosave concurrency sequence:**

```mermaid
sequenceDiagram
    participant MT as "Main Thread (typing)"
    participant MX as documentMutex_
    participant AW as "AutosaveWorker thread"
    participant FS as Filesystem

    Note over MT,AW: Two threads — one shared mutex

    MT ->> MT: insertCharacter(c) — no lock needed
    MT ->> AW: notifyDirty() — atomic store; free

    loop worker wake cycle
        AW ->> AW: wait_for(wakeSignal_, 30s)
        AW ->> AW: dirty_.exchange(false) returns true
        AW ->> MX: lock_guard acquire
        AW ->> MT: snapshotProvider() — fast memory copy
        MT -->> AW: string snapshot
        AW ->> MX: lock_guard release
        Note over MX: Lock released BEFORE slow I/O
        AW ->> FS: diskWriter(snapshot) writes .bak
        FS -->> AW: success
        AW ->> AW: onSaved_ callback
    end

    Note over MT: Main thread NEVER blocked by filesystem latency
```

---

### Terminal

`include/Terminal.hpp` · `src/Terminal.cpp`

Free functions with no state beyond the saved `termios` struct.

```cpp
namespace Terminal {
    void enableRawMode();
    void disableRawMode();
    int  windowSize(int& rows, int& cols);
    void clearScreen();
    void moveCursorTo(int row, int col);
    int  cursorPosition(int& row, int& col);
}
```

---

### FrameBuffer & ScreenRenderer

`include/ScreenRenderer.hpp` · `src/ScreenRenderer.cpp`

```cpp
struct FrameBuffer {
    std::string data;
    void append(const char* s, size_t len);
    void appendStr(const std::string& s);
};

namespace ScreenRenderer {
    void render(const TextEditor& editor); // const — cannot mutate editor
}
```

**Frame rendering pipeline:**

```mermaid
flowchart TD
    A(["render(const TextEditor&)"]) --> B["Create empty FrameBuffer"]
    B --> C["Hide cursor ESC 25l"]
    C --> D["Move to home ESC H"]
    D --> E["Pre-scan rows ABOVE viewport\ncompute inBlockComment\nfor first visible row"]

    E --> F{"For each visible row"}
    F --> G["lineText = editor.lineText(rowOffset + row)"]
    G --> H["tokens = SyntaxHighlighter::tokenize(line, lang, inBlockComment)"]
    H --> I{"For each SyntaxToken"}
    I --> J["Append ANSI color for token.type"]
    J --> K["Append token text substring"]
    K --> L["Append ANSI reset"]
    L --> I
    I -->|"All tokens done"| M["Append CRLF clear to EOL"]
    M --> F

    F -->|"All rows done"| N["Render status bar\nfilename  dirty+  row:col  mode"]
    N --> O{"isSearchActive?"}
    O -->|"Yes"| P["Render search bar with query"]
    O -->|"No"| Q["Skip"]
    P --> Q

    Q --> R["Show cursor ESC 25h"]
    R --> S["Position cursor ESC row;col H"]
    S --> T["write(STDOUT_FILENO, framebuffer.data)\nSingle syscall — no flicker"]
    T --> Z(["Done"])
```

---

### InputHandler

`include/InputHandler.hpp` · `src/InputHandler.cpp`

No state. Key bindings can change without touching any editing logic.

```cpp
namespace InputHandler {
    void processNextKeypress(TextEditor& editor);
}
```

---

### TextEditor

`include/TextEditor.hpp` · `src/TextEditor.cpp`

The orchestrator. Implements `CursorOwner`. Does not know how to draw or decode a keypress.

**Keystroke processing loop:**

```mermaid
sequenceDiagram
    participant OS as "OS / stdin"
    participant IH as InputHandler
    participant TE as TextEditor
    participant UR as UndoRedoStack
    participant TD as TextDocument
    participant PT as PieceTable
    participant SR as ScreenRenderer
    participant FB as FrameBuffer
    participant AW as AutosaveWorker

    loop editor.run() — one iteration per keypress
        TE ->> SR: render(editor)
        SR ->> FB: accumulate ANSI frame
        FB -->> OS: write() single syscall

        OS -->> IH: processNextKeypress(editor)
        IH ->> IH: read raw byte from stdin
        IH ->> TE: insertCharacter(c)

        TE ->> UR: push(InsertCharCommand)
        UR ->> TD: insertChar(row, col, c)
        TD ->> PT: insert(offset, c)
        PT ->> PT: appendAndTrackPiece(c)
        PT ->> PT: extend last piece length
        TD ->> TD: shiftLineStartsAfter(row, +1)

        TE ->> AW: notifyDirty()
        Note over AW: atomic dirty_.store(true) wakeSignal_.notify_one()
    end
```

**File open / save / save-as sequence:**

```mermaid
sequenceDiagram
    participant User
    participant IH as InputHandler
    participant TE as TextEditor
    participant TD as TextDocument
    participant PT as PieceTable
    participant VH as VersionHistory
    participant AW as AutosaveWorker
    participant FS as Filesystem

    Note over User,FS: Opening a file
    TE ->> TD: loadFromFile(path)
    TD ->> PT: loadFromFile(path)
    PT ->> FS: read file into originalText_
    FS -->> PT: file content
    PT ->> PT: reset to single spanning Piece
    TD ->> TD: rebuild lineStarts_ full scan once
    TE ->> TE: savedContentHash_ = hash(content)
    TE ->> AW: start()

    Note over User,FS: Ctrl+S Save
    User ->> IH: Ctrl+S
    IH ->> TE: save()
    TE ->> FS: writeCurrentTextToDisk()
    FS -->> TE: success
    TE ->> VH: recordSnapshot(content)
    VH ->> VH: snapshots_.push_back(Snapshot)
    TE ->> TE: savedContentHash_ = hash(content)
    TE ->> TE: dirty_ = false

    Note over User,FS: Ctrl+A Save-As
    User ->> IH: Ctrl+A
    IH ->> TE: saveAs()
    TE ->> TE: promptUser("Save as:")
    TE -->> User: inline terminal prompt
    User -->> TE: new filename
    TE ->> TE: filename_ = new filename
    TE ->> TE: save()
```

**Search flow (literal and regex):**

```mermaid
sequenceDiagram
    participant User
    participant IH as InputHandler
    participant TE as TextEditor
    participant RE as RegexEngine
    participant TD as TextDocument

    User ->> IH: Ctrl+F
    IH ->> TE: beginSearch()
    TE ->> TE: searchActive_ = true

    loop each character typed in search bar
        User ->> IH: char c
        IH ->> TE: searchQuery_ += c
        TE ->> TE: refreshMatches()

        alt regexSearch_ == false
            TE ->> TD: lineText(row) for each row
            TD -->> TE: line string
            TE ->> TE: string::find(query) push MatchPosition
        else regexSearch_ == true
            TE ->> TD: lineText(row) for each row
            TD -->> TE: line string
            TE ->> RE: findAll(searchQuery_, line)
            RE ->> RE: compile pattern to NFA
            RE ->> RE: simulate epsilon-closure sets
            RE -->> TE: vector of Match col and length
            TE ->> TE: push MatchPosition row col len
        end
    end

    User ->> IH: Ctrl+N
    IH ->> TE: findNext()
    TE ->> TE: currentMatchIndex_++
    TE ->> TE: scrollViewportToCursor()

    User ->> IH: Ctrl+R
    IH ->> TE: toggleRegexSearch()
    TE ->> TE: regexSearch_ = not regexSearch_
    TE ->> TE: refreshMatches()
```

**Public API summary:**
```cpp
// Editing
void insertCharacter(char c);
void insertNewline();
void deleteCharacterBeforeCursor();
void undo();
void redo();

// Cursor
void moveCursorLeft(); void moveCursorRight();
void moveCursorUp();   void moveCursorDown();

// Selection / Clipboard
void beginSelection();
void extendSelectionTo(int row, int col);
void clearSelection();
void copySelectionToClipboard();
void cutSelectionToClipboard();
void pasteFromClipboard();

// File I/O
void save();   void saveAs();   void openFile(const std::string& path);

// Search
void beginSearch();   void findNext();   void findPrevious();
void toggleRegexSearch();

// Version history
void showVersionHistory();

// CursorOwner
void setCursor(int col, int row) override;
```

---

## Engineering Design Notes

### 1. Text Storage — Piece Table

**Data structures considered:**

| Structure | Insert | Memory | Notes |
|---|---|---|---|
| `std::string` / `std::vector<char>` | O(n) | Contiguous | Simple; slow for large files |
| Linked list of chars | O(1) insert | Fragmented | Cache-hostile; massive pointer overhead |
| **Piece Table** ✅ | O(1) amortized append; O(k) mid-doc | Low | Two buffers + piece list |
| Gap Buffer | O(k) at gap; O(n) to reposition | Contiguous | Fast sequential; slow random |
| Rope | O(log n) all ops | Tree nodes | Complex; justified at gigabyte scale only |

**Trade-off positioning:**

```mermaid
quadrantChart
    title Text Storage Structure Trade-offs
    x-axis Sequential edit speed low to high
    y-axis Random position edit speed low to high
    quadrant-1 Fast both ways
    quadrant-2 Fast random slow sequential
    quadrant-3 Slow both ways
    quadrant-4 Fast sequential slow random
    std string: [0.10, 0.10]
    Linked list: [0.15, 0.80]
    Piece Table chosen: [0.90, 0.35]
    Gap Buffer comparator: [0.85, 0.20]
    Rope: [0.60, 0.90]
```

For sequential typing (dominant workload) Piece Table wins. No text is ever copied or moved on append — one integer update. `coalesceAdjacentPieces()` after each erase prevents unbounded piece-list growth.

---

### 2. Gap Buffer — Benchmark Comparator

The Gap Buffer stores text contiguously with a movable gap at the most recent edit point. Insert-at-gap is O(k) with no memory movement. Reposition-gap is O(distance) — exactly where Piece Table wins on sequential workloads.

---

### 3. TextDocument — Incremental Line Index

`lineStarts_` is updated incrementally — O(lines after cursor) per single-character edit, not O(file size). Only multi-line range operations fall back to a full rescan; they are rare enough that the added complexity of an incremental path is not worthwhile.

---

### 4. Undo / Redo — Command Pattern

Each `EditCommand` stores `CursorPos before_` and `CursorPos after_` so both `undo()` and `redo()` restore the cursor precisely without any additional editor state access. Command subclasses depend on **only** `TextDocument&` and `CursorOwner` — they cannot reach into search state, clipboard, or autosave.

---

### 5. Rendering, Input, Editing — Separated by Type

| Class / namespace | Type constraint | Cannot |
|---|---|---|
| `TextEditor` | Owns all mutable state | Emit ANSI codes; decode raw bytes |
| `ScreenRenderer` | `const TextEditor&` | Mutate the document — compiler rejects it |
| `InputHandler` | `TextEditor&` (write path only) | Access `FrameBuffer`; render anything |

`ScreenRenderer` accumulates ANSI sequences in a `FrameBuffer` and flushes in a **single `write()` syscall** — partial frames never appear on screen.

---

### 6. Background Autosave — Concurrency Design

The critical invariant: `documentMutex_` is held **only** long enough to call `snapshotProvider()` (a fast in-memory copy). The actual `diskWriter()` call happens after the mutex is released. The main thread is **never** blocked by filesystem latency.

---

### 7. Selection and Clipboard

`Selection::normalized()` handles backward selections so callers never need to think about direction. Copy/cut/paste use `TextDocument`'s offset-based range API — multi-line selections work without row-by-row special-casing. `Clipboard` is a single-slot in-process store; OS clipboard integration is a localized change.

---

### 8. Version History and Diff Engine

`DiffEngine` uses the O(n·m) LCS DP rather than Myers' O(ND) — simpler and perfectly adequate at editor-buffer scale. `VersionHistory` stores full-text `Snapshot` objects with `savedAt` timestamps. `diffBetween(older, newer)` enables arbitrary historical comparisons.

---

### 9. Syntax Highlighting

**Stateless per-line API** — `ScreenRenderer` holds `bool inBlockComment` and passes it by reference. Trivially thread-safe. Python triple-quoted strings are deliberately not tracked (cross-line state without proportional benefit). Token coverage is guaranteed: no gaps, no overlaps — verified by `test_syntax_tokens_cover_entire_line`.

---

### 10. Regex Engine — Thompson NFA

`std::regex` uses recursive backtracking — certain patterns cause O(2^n) behaviour, unacceptable for a search-as-you-type UI. The Thompson NFA maintains the **epsilon-closure set** of active states; advancing the entire set per input character guarantees O(|pattern| × |text|) with no catastrophic backtracking.

---

## Tests & Performance

### Unit Tests — 50 / 50 Passing

**Test distribution by subsystem:**

```mermaid
pie title "50 Unit Tests by Subsystem"
    "RegexEngine Thompson NFA" : 15
    "SyntaxHighlighter" : 9
    "GapBuffer" : 10
    "TextDocument" : 5
    "DiffEngine and VersionHistory" : 4
    "TextEditor undo redo search" : 3
    "PieceTable" : 3
    "Selection and Clipboard" : 1
```

#### PieceTable — 3 tests

| Test | Verifies |
|---|---|
| `test_piece_table_insert` | Sequential insert at two offsets builds `"Hello World"` |
| `test_piece_table_erase` | `erase(5,1)` on `"Hello World"` yields `"HelloWorld"` |
| `test_piece_table_midstream_insert_and_erase` | Mid-document split + erase: `ACE` → `ABCDE` → `ABE` |

#### GapBuffer — 10 tests

| Test | Verifies |
|---|---|
| `test_gapbuffer_append_sequential` | Character-by-character sequential append |
| `test_gapbuffer_insert_at_beginning` | Front-of-buffer insert |
| `test_gapbuffer_insert_in_middle` | Mid-buffer insert (`ACE` → `ABCDE`) |
| `test_gapbuffer_erase_front` | `erase(0,6)` on `"Hello World"` yields `"World"` |
| `test_gapbuffer_erase_middle` | `erase(5,1)` removes the space |
| `test_gapbuffer_erase_then_insert` | `"aXb"` → erase `X` → insert `c` → `"acb"` |
| `test_gapbuffer_charAt` | `charAt(0)=='H'`, `charAt(4)=='o'` |
| `test_gapbuffer_size` | Size tracks insert and erase correctly |
| `test_gapbuffer_empty_string_insert` | `insert(0,"")` must not crash or change state |
| `test_gapbuffer_insert_past_end` | Index 9999 clamps to end: `"abc"` → `"abcX"` |

#### TextDocument — 5 tests

| Test | Verifies |
|---|---|
| `test_document_insert_char` | `insertChar(0,5,'!')` appends `!` to line 0 |
| `test_document_newline_split` | `insertNewline(0,5)` on `"HelloWorld"` → `"Hello"` / `"World"` |
| `test_document_merge_lines` | `mergeWithNextLine(0)` on two lines → one merged line |
| `test_document_line_starts_stay_correct_after_many_edits` | `lineStarts_` invariant after mixed inserts and merges |
| `test_row_col_to_offset` | `toOffset(1,2)` returns `6` for `"abc\ndef"` |

#### TextEditor — 3 tests

| Test | Verifies |
|---|---|
| `test_undo_insert` | Two inserts then two undos restores empty buffer |
| `test_undo_then_redo` | Undo then redo restores both characters |
| `test_search_finds_all_matches` | Literal search finds both occurrences of `"world"` across two lines |

#### DiffEngine / VersionHistory — 4 tests

| Test | Verifies |
|---|---|
| `test_diff_detects_pure_addition` | `Added` count == 1 when one line appended |
| `test_diff_detects_pure_removal` | `Removed` count == 1 when one line deleted |
| `test_diff_identical_inputs_are_all_unchanged` | All 3 entries `Unchanged` on identical inputs |
| `test_version_history_diff_against_latest` | Returns 1 `Added` for an appended third line |

#### Selection / Clipboard — 1 test

| Test | Verifies |
|---|---|
| `test_copy_paste_round_trip` | Copy first 5 chars, paste; first 5 chars of result match `"hello"` |

#### SyntaxHighlighter — 9 tests

| Test | Verifies |
|---|---|
| `test_syntax_cpp_keyword` | `int` in `"int main() {"` → `Keyword` |
| `test_syntax_cpp_line_comment` | `//` comment → `Comment` token |
| `test_syntax_cpp_block_comment_span` | `inBlockComment` true after unclosed `/*`; next line all `Comment` |
| `test_syntax_cpp_string_literal` | `"hello"` → `String` token |
| `test_syntax_cpp_preprocessor` | `#include <vector>` → single `Preprocessor` token |
| `test_syntax_python_keyword` | `def` in Python mode → `Keyword` |
| `test_syntax_none_language_passthrough` | `Language::None` covers entire line as `Normal` |
| `test_syntax_tokens_cover_entire_line` | Token lengths sum exactly to `line.size()` |
| `test_syntax_language_detection` | `.cpp`→Cpp, `.hpp`→Cpp, `.py`→Python, `.md`→None |

#### RegexEngine — 15 tests

| Test | Pattern | Input | Expected |
|---|---|---|---|
| `test_regex_literal_match` | `hello` | `"say hello world"` | col=4, len=5 |
| `test_regex_dot_star` | `a.*b` | `"xafoob"` | col=1, len=5 |
| `test_regex_plus_quantifier` | `a+` | `"baaac"` | col=1, len=3 |
| `test_regex_question_quantifier` | `colou?r` | `"The colour and the color"` | 2 matches |
| `test_regex_alternation` | `cat\|dog` | `"I have a cat and a dog"` | 2 matches |
| `test_regex_character_class` | `[0-9]+` | `"abc 42 and 007"` | 2 matches: len 2, len 3 |
| `test_regex_negated_class` | `[^aeiou ]+` | `"cat"` | non-empty match set |
| `test_regex_digit_shorthand` | `\d+` | `"score: 1234 and 56"` | 2 matches: len 4, len 2 |
| `test_regex_word_shorthand` | `\w+` | `"hello world"` | 2 matches |
| `test_regex_no_match` | `xyz` | `"hello world"` | empty vector |
| `test_regex_anchored_start` | `^hello` | `"hello world"` | col=0; `^world` → empty |
| `test_regex_anchored_end` | `world$` | `"hello world"` | 1 match; `hello$` → empty |
| `test_regex_grouping` | `(ab)+` | `"ababab"` | col=0, len=6 |
| `test_regex_validity_check` | `a+b`, `[a-z]+`, `""` | — | all `isValid() == true` |
| `test_regex_string_empty` | `abc` | `""` | empty vector |

---

### Performance Benchmarks

**Setup:** N=50,000 operations each · WSL2 · `g++ -O2` · `std::chrono::high_resolution_clock`. Winner declared when ≥10% faster.

```mermaid
xychart-beta
    title "PieceTable vs GapBuffer Benchmark (ms) — N=50,000 ops"
    x-axis ["A: Sequential append", "B: Random mid insert", "C: Sequential erase", "D: Ping-pong"]
    y-axis "Time (ms)" 0 --> 3000
    bar [2.38, 2933, 2.56, 856]
    line [2.78, 2.75, 0.33, 13.84]
```

> **Bar = PieceTable · Line = GapBuffer** · Lower is faster. Scenario B: PieceTable 2,933 ms vs GapBuffer 2.75 ms (×1,000 slower) — the Y-axis compresses this visually.

| Scenario | PieceTable | GapBuffer | Winner |
|---|---:|---:|---|
| A: Sequential append (cursor always at end) | 2.38 ms | 2.78 ms | **PieceTable** |
| B: Random mid-document insert | 2,933 ms | 2.75 ms | **GapBuffer ×1,000×** |
| C: Sequential erase from front | 2.56 ms | 0.33 ms | **GapBuffer ×8×** |
| D: Alternating front/back insert (ping-pong) | 856 ms | 13.84 ms | **GapBuffer ×62×** |

**Conclusion:** sequential typing (Scenario A) is the dominant workload. Piece Table is the correct choice — no text is ever copied or moved on append.

---

## Keybindings

| Key | Action |
|---|---|
| Arrow keys | Move cursor (bounds-checked) |
| `Enter` | Insert newline |
| `Backspace` | Delete before cursor; merges lines at column 0 |
| `Ctrl+Z` | Undo |
| `Ctrl+Y` | Redo |
| `Ctrl+F` | Open incremental search |
| `Ctrl+R` | Toggle regex search mode (`[regex]` in status bar) |
| `Ctrl+N` | Next search match |
| `Ctrl+B` | Previous search match |
| `Ctrl+Space` | Begin / anchor selection |
| `Ctrl+C` | Copy selection |
| `Ctrl+X` | Cut selection |
| `Ctrl+V` | Paste from clipboard |
| `Ctrl+D` | Show LCS diff vs. last save |
| `Ctrl+S` | Save |
| `Ctrl+A` | Save as |
| `Ctrl+Q` | Quit (prompts if unsaved) |

---

## Build & Run

**Requirements:** Linux or WSL2 · `g++ ≥ 10` · `make`

```bash
git clone https://github.com/PRADEEPERIYASAMY/piecetable-editor.git
cd piecetable-editor

make               # build editor binary
./editor file.cpp  # open a file
./editor           # open with no file — name later with Ctrl+A
make test          # run all 50 tests + benchmark
make debug         # build with ASan + UBSan
make clean
```

**Makefile targets:**

```mermaid
flowchart TD
    A(["make all"]) --> B["Compile 14 src/*.cpp + main.cpp\nusing -Wall -Wextra -std=c++17 -Iinclude -pthread"]
    B --> C["Link OBJ + MAIN_OBJ -o editor"]
    C --> D(["editor binary"])

    F(["make debug"]) --> G["CXXFLAGS += -g -O0\n-fsanitize=address,undefined\n-fno-omit-frame-pointer"]
    G --> H["make clean"]
    H --> A

    I(["make test"]) --> J["Compile tests/tests.cpp + OBJ\n-o tests/run_tests"]
    J --> K["./tests/run_tests"]
    K --> L["50 unit tests across all subsystems"]
    L --> M["4-scenario benchmark\nPieceTable vs GapBuffer\nN=50,000 ops each"]
    M --> N(["All 50/50 tests passed!"])

    O(["make clean"]) --> P["rm -f src/*.o editor tests/run_tests"]
```

---

## Memory Safety

```bash
make debug
./editor yourfile.cpp   # interactive under sanitizers
./tests/run_tests       # full suite under sanitizers
```

| Sanitizer | Catches |
|---|---|
| ASan | Use-after-free, heap/stack buffer overflows, uninitialized memory |
| UBSan | Signed integer overflow, invalid shifts, null dereference |

Both are especially valuable for `PieceTable` manual offset arithmetic, `RegexEngine` NFA state-set bookkeeping, and `AutosaveWorker` shared-state access. The full test suite passes cleanly with zero sanitizer errors.

---

## Community & Contributing

| Document | Purpose |
|---|---|
| [CONTRIBUTING.md](CONTRIBUTING.md) | Dev setup, coding standards, Conventional Commits, PR process |
| [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md) | Contributor Covenant v2.1 and 4-tier enforcement |
| [SECURITY.md](SECURITY.md) | Private vulnerability reporting via GitHub Security Advisories |
| [SUPPORT.md](SUPPORT.md) | Where to get help, build failure checklist, out-of-scope items |
| [CHANGELOG.md](CHANGELOG.md) | Version history following [Keep a Changelog](https://keepachangelog.com) |
| [CITATION.cff](CITATION.cff) | CFF 1.2.0 citation metadata |

GitHub issue templates: [Bug Report](.github/ISSUE_TEMPLATE/bug_report.md) · [Feature Request](.github/ISSUE_TEMPLATE/feature_request.md) · [Pull Request Template](.github/PULL_REQUEST_TEMPLATE.md)

**Branching strategy:**

```mermaid
gitGraph
    commit id: "v1.0.0 initial release" tag: "v1.0.0"

    branch feat/os-clipboard
    checkout feat/os-clipboard
    commit id: "Add Clipboard setFromOS xclip"
    commit id: "Add Clipboard getFromOS pbcopy"
    checkout main
    merge feat/os-clipboard id: "feat: OS clipboard integration"

    branch fix/piece-table-coalesce
    checkout fix/piece-table-coalesce
    commit id: "Fix coalesce skipping first piece"
    checkout main
    merge fix/piece-table-coalesce id: "fix: coalesce boundary off-by-one"

    branch feat/mouse-support
    checkout feat/mouse-support
    commit id: "Terminal enable mouse reporting"
    commit id: "InputHandler parse mouse event bytes"
    commit id: "TextEditor moveCursorToMouse"
    checkout main
    merge feat/mouse-support id: "feat: click-to-move cursor"

    branch perf/frame-diff
    checkout perf/frame-diff
    commit id: "FrameBuffer store prev frame"
    commit id: "ScreenRenderer emit only changed cells"
    checkout main
    merge perf/frame-diff id: "perf: differential frame rendering"

    commit id: "chore: update CHANGELOG for v1.1.0" tag: "v1.1.0"
```

---

## Changelog

See [CHANGELOG.md](CHANGELOG.md) for a full history of changes.

---

## Citation

```
Eriyasamy, P. (2025). PieceTable Editor — A High-Performance C++ Terminal Editor.
https://github.com/PRADEEPERIYASAMY/piecetable-editor
```

See [CITATION.cff](CITATION.cff) for full CFF 1.2.0 metadata.

---

## License

MIT — see [LICENSE](LICENSE) for the full text.
