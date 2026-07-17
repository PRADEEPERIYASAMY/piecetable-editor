#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <memory>
#include <cstdint>
#include <ctime>

#include "TextDocument.hpp"
#include "UndoRedoStack.hpp"
#include "EditCommand.hpp"
#include "Selection.hpp"
#include "Clipboard.hpp"
#include "VersionHistory.hpp"
#include "AutosaveWorker.hpp"

// ============================================================================
// TextEditor
//
// Owns editing state and exposes the operations InputHandler drives and the
// read-only view ScreenRenderer needs. It does NOT know how to draw itself
// to a terminal or how to decode a keypress — those are separate classes,
// which keeps this class focused on "what editing operations exist and how
// do they affect document + cursor state" (Single Responsibility Principle).
// ============================================================================
class TextEditor : public CursorOwner {
public:
    TextEditor();
    ~TextEditor();

    void run();
    void handleTerminalResize();

    // ----- Editing -----------------------------------------------------
    void insertCharacter(char c);
    void insertNewline();
    void deleteCharacterBeforeCursor();
    void undo() { undoRedo_.undo(); recomputeDirtyFlag(); }
    void redo() { undoRedo_.redo(); recomputeDirtyFlag(); }

    // ----- Cursor movement (bounds-checked against the document) --------
    void moveCursorLeft();
    void moveCursorRight();
    void moveCursorUp();
    void moveCursorDown();
    void clampCursorToLine();

    // ----- Selection / Clipboard ----------------------------------------
    void beginSelection();
    void extendSelectionTo(int row, int col);
    void clearSelection();
    void copySelectionToClipboard();
    void cutSelectionToClipboard();
    void pasteFromClipboard();
    bool hasActiveSelection() const { return selection_.active && !selection_.isEmpty(); }
    const Selection& selection() const { return selection_; }

    // ----- File I/O -------------------------------------------------------
    std::string promptUser(const std::string& message);
    void save();
    void saveAs();
    void openFile(const std::string& path);

    // ----- Search -----------------------------------------------------
    void beginSearch();
    void findNext();
    void findPrevious();
    void toggleRegexSearch();

    // ----- Version history (diff-based save history) --------------------
    void showVersionHistory();

    // Lets InputHandler surface transient prompts (e.g. the quit
    // confirmation) through the same status-message channel used
    // elsewhere.
    void notify(const std::string& message) { setStatusMessage(message); }

    // ----- CursorOwner (used by EditCommand undo/redo) -------------------
    void setCursor(int col, int row) override { cursorCol_ = col; cursorRow_ = row; }

    // ----- Read-only accessors for rendering -----------------------------
    const std::string lineText(int row) const { return document_.lineText(row); }
    int lineCount() const { return document_.lineCount(); }
    int cursorCol() const { return cursorCol_; }
    int cursorRow() const { return cursorRow_; }
    int screenRows() const { return screenRows_; }
    int screenCols() const { return screenCols_; }
    int rowOffset() const { return rowOffset_; }
    int colOffset() const { return colOffset_; }
    const std::string& filename() const { return filename_; }
    bool isDirty() const { return dirty_; }
    const std::string& statusMessage() const { return statusMessage_; }
    std::time_t statusMessageTime() const { return statusMessageTime_; }
    bool isSearchActive() const { return searchActive_; }
    const std::string& searchQuery() const { return searchQuery_; }
    bool regexSearch() const { return regexSearch_; }
    int currentMatchRow() const { return currentMatchRow_; }
    int currentMatchCol() const { return currentMatchCol_; }
    int currentMatchLen() const { return currentMatchLen_; }

    struct MatchPosition { int row, col, len; };
    const std::vector<MatchPosition>& searchMatches() const { return searchMatches_; }
    bool running() const { return running_; }
    void quitImmediately(); // clears the screen and stops the run loop

    // Exposed so InputHandler can react to Ctrl+Q's save-before-quit flow.
    std::mutex& documentMutex() { return documentMutex_; }

    // ----- Test-only helpers ---------------------------------------------
    // Grouped and named distinctly (testXxx) so it's obvious at every call
    // site that these exist purely to make unit tests possible without a
    // real terminal, and aren't part of the editor's runtime behavior.
    void testAppendLine(const std::string& s) { document_.appendLine(s); }
    void testSetSearchQuery(const std::string& q) { searchQuery_ = q; refreshMatches(); }
    int testMatchCount() const { return static_cast<int>(searchMatches_.size()); }

private:
    // ----- Cursor / viewport state ---------------------------------------
    int cursorCol_ = 1, cursorRow_ = 1;
    int screenRows_ = 0, screenCols_ = 0;
    int rowOffset_ = 0, colOffset_ = 0;

    // ----- Core data ------------------------------------------------------
    TextDocument document_;
    UndoRedoStack undoRedo_;
    Selection selection_;
    Clipboard clipboard_;
    VersionHistory versionHistory_;

    // ----- File state -----------------------------------------------------
    std::string filename_;
    bool dirty_ = false;
    uint64_t savedContentHash_ = 0;
    bool running_ = true;

    // ----- Status/message bar ----------------------------------------------
    std::string statusMessage_;
    std::time_t statusMessageTime_ = 0;

    // ----- Search state -----------------------------------------------------
    std::string searchQuery_;
    bool searchActive_   = false;
    bool regexSearch_    = false; // toggled by Ctrl+R; uses RegexEngine when true
    std::vector<MatchPosition> searchMatches_;
    int currentMatchIndex_ = -1;
    int currentMatchRow_   = -1;
    int currentMatchCol_   = -1;
    int currentMatchLen_   = 0;

    // ----- Concurrency: background autosave --------------------------------
    std::mutex documentMutex_;
    std::unique_ptr<AutosaveWorker> autosaveWorker_;

    void refreshMatches();
    void scrollViewportToCursor();
    void setStatusMessage(const std::string& message);
    void recomputeDirtyFlag();
    uint64_t hashOf(const std::string& text) const;
    int writeCurrentTextToDisk();
};

extern TextEditor* g_activeEditor;
void onTerminalResizeSignal(int);
