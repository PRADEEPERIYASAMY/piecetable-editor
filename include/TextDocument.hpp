#pragma once
#include <string>
#include <vector>
#include "PieceTable.hpp"

// ============================================================================
// TextDocument
//
// Adds row/column awareness on top of the raw PieceTable so the rest of the
// editor never has to think in flat character offsets.
//
// PERFORMANCE NOTE (this is the fix for the original project's biggest bug):
// the previous implementation recomputed `lineStarts_` by calling
// getFullText() and rescanning every character after *every single
// keystroke* — an O(document size) cost per edit, which quietly defeats the
// entire point of using a piece table.
//
// Here, `lineStarts_` is instead updated incrementally:
//   - inserting/deleting a plain character only shifts the absolute offsets
//     of lines *after* the edit point (O(numLines - row), not O(fileSize))
//   - inserting/removing a newline additionally inserts/removes one entry
//     from `lineStarts_`
//
// For a file with many lines but a huge per-line size, this is dramatically
// cheaper than a full rescan on every keystroke.
// ============================================================================

class TextDocument {
public:
    TextDocument();

    void loadFromFile(const std::string& filePath);

    int lineCount() const;
    std::string lineText(int row) const;
    int lineLength(int row) const;
    std::string fullText() const;

    void appendLine(const std::string& text);
    void insertChar(int row, int col, char c);
    void deleteChar(int row, int col);
    void insertNewline(int row, int splitCol);
    void mergeWithNextLine(int row);

    // Range operations (offsets, not row/col) — used by selection/clipboard
    // and by version-history restore.
    size_t toOffset(int row, int col) const;
    std::string textInRange(size_t startOffset, size_t endOffset) const;
    void eraseRange(size_t startOffset, size_t endOffset);
    void insertAt(size_t offset, const std::string& text);
    void replaceAllText(const std::string& newText);

private:
    PieceTable storage_;
    std::vector<size_t> lineStarts_; // absolute offset where each line begins

    // Shifts every recorded line-start after `afterRow` by `delta` — used
    // whenever an edit changes document length but not the line count.
    void shiftLineStartsAfter(int afterRow, long delta);
};
