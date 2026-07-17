#include "TextEditor.hpp"
#include "Terminal.hpp"
#include "ScreenRenderer.hpp"
#include "InputHandler.hpp"
#include "RegexEngine.hpp"
#include <unistd.h>
#include <fcntl.h>
#include <algorithm>
#include <sstream>

TextEditor* g_activeEditor = nullptr;

void onTerminalResizeSignal(int) {
    if (g_activeEditor) g_activeEditor->handleTerminalResize();
}

// FNV-1a — cheap, well-distributed, good enough for "did the content change
// since last save" dirty-tracking (not used for anything security-sensitive).
static uint64_t fnv1a(const std::string& s) {
    uint64_t hash = 1469598103934665603ull;
    for (unsigned char c : s) {
        hash ^= c;
        hash *= 1099511628211ull;
    }
    return hash;
}

TextEditor::TextEditor() {
    Terminal::windowSize(screenRows_, screenCols_);
    screenRows_ -= 2; // reserve status bar + message bar

    // The autosave worker only ever touches the document through these two
    // callbacks, both of which take documentMutex_ appropriately — see
    // AutosaveWorker's class comment for the concurrency contract.
    autosaveWorker_ = std::make_unique<AutosaveWorker>(
        documentMutex_,
        [this] { return document_.fullText(); },
        [this](const std::string& content) -> bool {
            if (filename_.empty()) return false; // nothing to autosave to yet
            int fd = ::open(filename_.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd == -1) return false;
            bool ok = write(fd, content.c_str(), content.size()) != -1;
            close(fd);
            return ok;
        },
        [this](const std::string& content) {
            savedContentHash_ = hashOf(content);
            dirty_ = false;
            setStatusMessage("Autosaved");
        });
    autosaveWorker_->start();
}

TextEditor::~TextEditor() {
    autosaveWorker_->stop();
}

uint64_t TextEditor::hashOf(const std::string& text) const { return fnv1a(text); }

void TextEditor::recomputeDirtyFlag() {
    dirty_ = (hashOf(document_.fullText()) != savedContentHash_);
}

void TextEditor::setStatusMessage(const std::string& message) {
    statusMessage_ = message;
    statusMessageTime_ = std::time(nullptr);
}

void TextEditor::handleTerminalResize() {
    Terminal::windowSize(screenRows_, screenCols_);
    screenRows_ -= 2;
    if (rowOffset_ < 0) rowOffset_ = 0;
    if (colOffset_ < 0) colOffset_ = 0;
}

void TextEditor::scrollViewportToCursor() {
    int fileRow = cursorRow_ - 1;
    if (fileRow < rowOffset_) rowOffset_ = fileRow;
    if (fileRow >= rowOffset_ + screenRows_) rowOffset_ = fileRow - screenRows_ + 1;

    int fileCol = cursorCol_ - 1;
    if (fileCol < colOffset_) colOffset_ = fileCol;
    if (fileCol >= colOffset_ + screenCols_) colOffset_ = fileCol - screenCols_ + 1;
}

// ============================================================================
// Cursor movement
// ============================================================================

void TextEditor::clampCursorToLine() {
    if (cursorCol_ < 1) cursorCol_ = 1;
    if (cursorRow_ < 1) cursorRow_ = 1;
    int lineLen = (cursorRow_ - 1 < document_.lineCount()) ? document_.lineLength(cursorRow_ - 1) : 0;
    if (cursorCol_ > lineLen + 1) cursorCol_ = lineLen + 1;
}

void TextEditor::moveCursorLeft() {
    if (cursorCol_ > 1) {
        cursorCol_--;
    } else if (cursorRow_ > 1) {
        cursorRow_--;
        cursorCol_ = document_.lineLength(cursorRow_ - 1) + 1;
    }
}

void TextEditor::moveCursorRight() {
    if (cursorRow_ > document_.lineCount()) return;
    int lineLen = document_.lineLength(cursorRow_ - 1);
    if (cursorCol_ <= lineLen) {
        cursorCol_++;
    } else if (cursorRow_ < document_.lineCount()) {
        cursorRow_++;
        cursorCol_ = 1;
    }
}

void TextEditor::moveCursorUp() {
    if (cursorRow_ > 1) cursorRow_--;
    clampCursorToLine();
}

void TextEditor::moveCursorDown() {
    if (cursorRow_ < document_.lineCount()) cursorRow_++;
    clampCursorToLine();
}

// ============================================================================
// Editing
// ============================================================================

void TextEditor::insertCharacter(char c) {
    std::lock_guard<std::mutex> lock(documentMutex_); // shared with autosave worker

    if (cursorRow_ > document_.lineCount()) document_.appendLine("");

    int row = cursorRow_ - 1;
    int col = std::min(cursorCol_ - 1, document_.lineLength(row));

    CursorPos before{cursorCol_, cursorRow_};
    document_.insertChar(row, col, c);
    cursorCol_++;
    CursorPos after{cursorCol_, cursorRow_};

    undoRedo_.push(std::make_unique<InsertCharCommand>(document_, *this, row, col, c, before, after));

    recomputeDirtyFlag();
    autosaveWorker_->notifyDirty();
    if (searchActive_ && !searchQuery_.empty()) refreshMatches();
}

void TextEditor::insertNewline() {
    std::lock_guard<std::mutex> lock(documentMutex_);

    if (cursorRow_ > document_.lineCount()) document_.appendLine("");

    CursorPos before{cursorCol_, cursorRow_};
    int row = cursorRow_ - 1;
    int splitCol = cursorCol_ - 1;

    document_.insertNewline(row, splitCol);
    cursorRow_++;
    cursorCol_ = 1;
    CursorPos after{cursorCol_, cursorRow_};

    undoRedo_.push(std::make_unique<InsertNewlineCommand>(document_, *this, row, splitCol, before, after));

    recomputeDirtyFlag();
    autosaveWorker_->notifyDirty();
    if (searchActive_ && !searchQuery_.empty()) refreshMatches();
}

void TextEditor::deleteCharacterBeforeCursor() {
    if (cursorRow_ == 1 && cursorCol_ == 1) return;

    std::lock_guard<std::mutex> lock(documentMutex_);

    if (cursorCol_ > 1) {
        CursorPos before{cursorCol_, cursorRow_};
        int row = cursorRow_ - 1;
        int col = cursorCol_ - 2;
        char deleted = document_.lineText(row)[col];

        document_.deleteChar(row, col);
        cursorCol_--;
        CursorPos after{cursorCol_, cursorRow_};

        undoRedo_.push(std::make_unique<DeleteCharCommand>(document_, *this, row, col, deleted, before, after));
        if (searchActive_ && !searchQuery_.empty()) refreshMatches();
    } else {
        CursorPos before{cursorCol_, cursorRow_};
        int row = cursorRow_ - 1;
        int prevRow = row - 1;
        int prevLen = document_.lineLength(prevRow);

        document_.mergeWithNextLine(prevRow);
        cursorRow_--;
        cursorCol_ = prevLen + 1;
        CursorPos after{cursorCol_, cursorRow_};

        undoRedo_.push(std::make_unique<MergeLinesCommand>(document_, *this, prevRow, prevLen, before, after));
    }

    recomputeDirtyFlag();
    autosaveWorker_->notifyDirty();
}

// ============================================================================
// Selection / Clipboard
// ============================================================================

void TextEditor::beginSelection() {
    selection_.begin(cursorRow_ - 1, cursorCol_ - 1);
}

void TextEditor::extendSelectionTo(int row, int col) {
    if (selection_.active) selection_.moveTo(row, col);
}

void TextEditor::clearSelection() {
    selection_.clear();
}

void TextEditor::copySelectionToClipboard() {
    if (!hasActiveSelection()) return;
    int startRow, startCol, endRow, endCol;
    selection_.normalized(startRow, startCol, endRow, endCol);

    size_t startOffset = document_.toOffset(startRow, startCol);
    size_t endOffset = document_.toOffset(endRow, endCol);
    clipboard_.set(document_.textInRange(startOffset, endOffset));
    setStatusMessage("Copied " + std::to_string(clipboard_.contents().size()) + " characters");
}

void TextEditor::cutSelectionToClipboard() {
    if (!hasActiveSelection()) return;
    std::lock_guard<std::mutex> lock(documentMutex_);

    int startRow, startCol, endRow, endCol;
    selection_.normalized(startRow, startCol, endRow, endCol);

    size_t startOffset = document_.toOffset(startRow, startCol);
    size_t endOffset = document_.toOffset(endRow, endCol);
    clipboard_.set(document_.textInRange(startOffset, endOffset));
    document_.eraseRange(startOffset, endOffset);

    cursorRow_ = startRow + 1;
    cursorCol_ = startCol + 1;
    selection_.clear();

    // Note: cut/paste are not currently pushed onto the undo stack as a
    // single command. Undoing a multi-line cut would require an
    // EditCommand that stores the removed range's full text. This is a
    // known limitation that would require a new Command type to support fully.
    recomputeDirtyFlag();
    autosaveWorker_->notifyDirty();
    setStatusMessage("Cut " + std::to_string(clipboard_.contents().size()) + " characters");
}

void TextEditor::pasteFromClipboard() {
    if (clipboard_.empty()) return;
    std::lock_guard<std::mutex> lock(documentMutex_);

    size_t offset = document_.toOffset(cursorRow_ - 1, cursorCol_ - 1);
    document_.insertAt(offset, clipboard_.contents());

    // Recompute cursor position by walking to the end of the pasted text.
    auto [newRow, newCol] = [&]() -> std::pair<int, int> {
        int row = cursorRow_ - 1;
        int col = cursorCol_ - 1;
        for (char c : clipboard_.contents()) {
            if (c == '\n') { row++; col = 0; } else { col++; }
        }
        return {row, col};
    }();
    cursorRow_ = newRow + 1;
    cursorCol_ = newCol + 1;

    recomputeDirtyFlag();
    autosaveWorker_->notifyDirty();
    setStatusMessage("Pasted " + std::to_string(clipboard_.contents().size()) + " characters");
}

// ============================================================================
// File I/O
// ============================================================================

std::string TextEditor::promptUser(const std::string& message) {
    std::string input;
    while (true) {
        setStatusMessage(message + input);
        ScreenRenderer::render(*this);

        char c;
        if (read(STDIN_FILENO, &c, 1) != 1) continue;

        if (c == '\r' || c == '\n') {
            if (!input.empty()) { setStatusMessage(""); return input; }
        } else if (c == 127) {
            if (!input.empty()) input.pop_back();
        } else if (c == 27) {
            setStatusMessage("");
            return "";
        } else if (c >= 32 && c <= 126) {
            input.push_back(c);
        }
    }
}

int TextEditor::writeCurrentTextToDisk() {
    std::string content = document_.fullText();
    int fd = ::open(filename_.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) return -1;
    ssize_t written = write(fd, content.c_str(), content.size());
    close(fd);
    return static_cast<int>(written);
}

void TextEditor::save() {
    if (filename_.empty()) {
        filename_ = promptUser("Save as: ");
        if (filename_.empty()) return;
    }

    ScreenRenderer::render(*this);

    std::string content;
    {
        std::lock_guard<std::mutex> lock(documentMutex_);
        int bytes = writeCurrentTextToDisk();
        if (bytes == -1) return;
        content = document_.fullText();
        savedContentHash_ = hashOf(content);
        dirty_ = false;
    }

    versionHistory_.recordSnapshot(content);
    setStatusMessage("Saved " + std::to_string(content.size()) + " bytes to " + filename_ +
                      " (version " + std::to_string(versionHistory_.snapshotCount()) + ")");
}

void TextEditor::saveAs() {
    std::string newName = promptUser("Save as: ");
    if (newName.empty()) return;
    filename_ = newName;
    save();
}

void TextEditor::openFile(const std::string& path) {
    filename_ = path;
    document_.loadFromFile(path);
    savedContentHash_ = hashOf(document_.fullText());
    dirty_ = false;
    versionHistory_.recordSnapshot(document_.fullText());
}

void TextEditor::quitImmediately() {
    Terminal::clearScreen();
    Terminal::moveCursorTo(1, 1);
    running_ = false;
}

// ============================================================================
// Search
// ============================================================================

void TextEditor::refreshMatches() {
    searchMatches_.clear();
    currentMatchIndex_ = -1;
    currentMatchLen_   = 0;

    if (searchQuery_.empty()) return;

    for (int row = 0; row < document_.lineCount(); row++) {
        std::string line = document_.lineText(row);
        if (regexSearch_) {
            if (!RegexEngine::isValid(searchQuery_)) {
                setStatusMessage("Invalid regex: " + searchQuery_);
                return;
            }
            for (auto& m : RegexEngine::findAll(searchQuery_, line)) {
                if (m.length > 0)
                    searchMatches_.push_back({row, m.col, m.length});
            }
        } else {
            size_t pos = 0;
            while ((pos = line.find(searchQuery_, pos)) != std::string::npos) {
                searchMatches_.push_back({row, static_cast<int>(pos),
                                          static_cast<int>(searchQuery_.size())});
                pos += searchQuery_.size();
            }
        }
    }
}

void TextEditor::beginSearch() {
    std::string query;
    int savedCol = cursorCol_, savedRow = cursorRow_;

    while (true) {
        std::string counter;
        if (!searchMatches_.empty() && currentMatchIndex_ >= 0) {
            counter = " (" + std::to_string(currentMatchIndex_ + 1) + "/" +
                      std::to_string(searchMatches_.size()) + ")";
        }
        std::string mode = regexSearch_ ? "[regex] " : "";
        setStatusMessage("Search " + mode + ": " + query + counter +
                         " (ESC cancel, Ctrl+R toggle regex)");
        ScreenRenderer::render(*this);

        char c;
        if (read(STDIN_FILENO, &c, 1) != 1) continue;

        if (c == '\r' || c == '\n') break;
        if (c == 27) {
            cursorCol_ = savedCol; cursorRow_ = savedRow;
            searchQuery_.clear(); searchMatches_.clear();
            currentMatchIndex_ = -1; searchActive_ = false;
            setStatusMessage("");
            return;
        }
        if (c == 127) { if (!query.empty()) query.pop_back(); }
        else if (c >= 32 && c <= 126) query.push_back(c);

        searchQuery_ = query;
        refreshMatches();
        searchActive_ = true;
        if (!searchMatches_.empty()) findNext();
    }

    searchActive_ = true;
    setStatusMessage(searchMatches_.empty()
                          ? (regexSearch_ ? "No regex matches" : "No matches")
                          : "Match 1/" + std::to_string(searchMatches_.size()) +
                                "  (Ctrl+N next, Ctrl+B prev)");
}

void TextEditor::findNext() {
    if (!searchActive_ || searchMatches_.empty()) return;
    currentMatchIndex_ = (currentMatchIndex_ + 1) % static_cast<int>(searchMatches_.size());
    currentMatchRow_ = searchMatches_[currentMatchIndex_].row;
    currentMatchCol_ = searchMatches_[currentMatchIndex_].col;
    currentMatchLen_ = searchMatches_[currentMatchIndex_].len;
    cursorRow_ = currentMatchRow_ + 1;
    cursorCol_ = currentMatchCol_ + 1;
    setStatusMessage("Match " + std::to_string(currentMatchIndex_ + 1) + "/" +
                      std::to_string(searchMatches_.size()) + "  (Ctrl+N next, Ctrl+B prev)");
}

void TextEditor::findPrevious() {
    if (!searchActive_ || searchMatches_.empty()) return;
    currentMatchIndex_--;
    if (currentMatchIndex_ < 0) currentMatchIndex_ = static_cast<int>(searchMatches_.size()) - 1;
    currentMatchRow_ = searchMatches_[currentMatchIndex_].row;
    currentMatchCol_ = searchMatches_[currentMatchIndex_].col;
    currentMatchLen_ = searchMatches_[currentMatchIndex_].len;
    cursorRow_ = currentMatchRow_ + 1;
    cursorCol_ = currentMatchCol_ + 1;
    setStatusMessage("Match " + std::to_string(currentMatchIndex_ + 1) + "/" +
                      std::to_string(searchMatches_.size()) + "  (Ctrl+N next, Ctrl+B prev)");
}

void TextEditor::toggleRegexSearch() {
    regexSearch_ = !regexSearch_;
    if (searchActive_ && !searchQuery_.empty()) {
        refreshMatches();
        if (!searchMatches_.empty()) findNext();
    }
    setStatusMessage(std::string("Regex search: ") + (regexSearch_ ? "ON" : "OFF") +
                     "  (Ctrl+F to search)");
}

// ============================================================================
// Version history
// ============================================================================

void TextEditor::showVersionHistory() {
    if (!versionHistory_.hasHistory()) {
        setStatusMessage("No saved versions yet");
        return;
    }
    auto diff = versionHistory_.diffAgainstLatest(document_.fullText());
    int added = 0, removed = 0;
    for (auto& line : diff) {
        if (line.op == DiffOp::Added) added++;
        else if (line.op == DiffOp::Removed) removed++;
    }
    setStatusMessage("Since last save: +" + std::to_string(added) +
                      " / -" + std::to_string(removed) + " lines (" +
                      std::to_string(versionHistory_.snapshotCount()) + " versions total)");
}

// ============================================================================
// Run loop
// ============================================================================

void TextEditor::run() {
    while (running_) {
        scrollViewportToCursor();
        ScreenRenderer::render(*this);
        InputHandler::processNextKeypress(*this);
    }
}
