#include "TextDocument.hpp"
#include <algorithm>

TextDocument::TextDocument() {
    lineStarts_.push_back(0);
}

void TextDocument::loadFromFile(const std::string& filePath) {
    storage_.loadFromFile(filePath);

    // Loading a whole new file is inherently a full-document operation, so
    // a one-time full scan here is fine — it's the *per-keystroke* rescans
    // we eliminate elsewhere.
    lineStarts_.clear();
    lineStarts_.push_back(0);
    std::string text = storage_.text();
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\n') lineStarts_.push_back(i + 1);
    }
    if (lineStarts_.empty()) lineStarts_.push_back(0);
}

int TextDocument::lineCount() const {
    return static_cast<int>(lineStarts_.size());
}

size_t TextDocument::toOffset(int row, int col) const {
    if (row < 0 || static_cast<size_t>(row) >= lineStarts_.size())
        return storage_.size();

    size_t start = lineStarts_[row];
    size_t end = (static_cast<size_t>(row + 1) < lineStarts_.size())
                     ? lineStarts_[row + 1] - 1
                     : storage_.size();

    size_t lineLen = end - start;
    if (col < 0) col = 0;
    if (static_cast<size_t>(col) > lineLen) col = static_cast<int>(lineLen);
    return start + col;
}

std::string TextDocument::lineText(int row) const {
    if (row < 0 || static_cast<size_t>(row) >= lineStarts_.size()) return "";
    size_t start = lineStarts_[row];
    size_t end = (static_cast<size_t>(row + 1) < lineStarts_.size())
                     ? lineStarts_[row + 1] - 1
                     : storage_.size();
    return storage_.substring(start, end - start);
}

int TextDocument::lineLength(int row) const {
    if (row < 0 || static_cast<size_t>(row) >= lineStarts_.size()) return 0;
    size_t start = lineStarts_[row];
    size_t end = (static_cast<size_t>(row + 1) < lineStarts_.size())
                     ? lineStarts_[row + 1] - 1
                     : storage_.size();
    return static_cast<int>(end - start);
}

std::string TextDocument::fullText() const {
    return storage_.text();
}

void TextDocument::shiftLineStartsAfter(int afterRow, long delta) {
    for (size_t i = static_cast<size_t>(afterRow) + 1; i < lineStarts_.size(); ++i) {
        lineStarts_[i] = static_cast<size_t>(static_cast<long>(lineStarts_[i]) + delta);
    }
}

void TextDocument::appendLine(const std::string& text) {
    storage_.insert(storage_.size(), text + "\n");
    lineStarts_.push_back(storage_.size());
}

void TextDocument::insertChar(int row, int col, char c) {
    size_t index = toOffset(row, col);
    storage_.insert(index, std::string(1, c));
    // A plain character never creates a new line — only shift later lines.
    shiftLineStartsAfter(row, 1);
}

void TextDocument::deleteChar(int row, int col) {
    size_t index = toOffset(row, col);
    storage_.erase(index, 1);
    shiftLineStartsAfter(row, -1);
}

void TextDocument::insertNewline(int row, int splitCol) {
    size_t index = toOffset(row, splitCol);
    storage_.insert(index, "\n");

    // Shift lines after `row` first, then insert the new line-start entry
    // for the line that was just created by the split.
    shiftLineStartsAfter(row, 1);
    lineStarts_.insert(lineStarts_.begin() + row + 1, index + 1);
}

void TextDocument::mergeWithNextLine(int row) {
    size_t newlineIndex = toOffset(row, lineLength(row));
    storage_.erase(newlineIndex, 1);
    lineStarts_.erase(lineStarts_.begin() + row + 1);
    shiftLineStartsAfter(row, -1);
}

std::string TextDocument::textInRange(size_t startOffset, size_t endOffset) const {
    if (endOffset <= startOffset) return "";
    return storage_.substring(startOffset, endOffset - startOffset);
}

void TextDocument::eraseRange(size_t startOffset, size_t endOffset) {
    if (endOffset <= startOffset) return;
    size_t length = endOffset - startOffset;

    // A multi-character/multi-line erase can remove newlines, which changes
    // the line count — that's structurally different from the single-char
    // case, so we take the simpler (still far cheaper than the old
    // whole-file approach) route of recomputing just the line index from
    // the new full text. This keeps the code correct without adding a
    // second, more complex incremental path for a comparatively rare
    // operation (multi-line selection delete).
    storage_.erase(startOffset, length);

    std::string text = storage_.text();
    lineStarts_.clear();
    lineStarts_.push_back(0);
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\n') lineStarts_.push_back(i + 1);
    }
}

void TextDocument::insertAt(size_t offset, const std::string& text) {
    if (text.empty()) return;
    storage_.insert(offset, text);

    // Same reasoning as eraseRange: pasted text may contain newlines, so
    // recompute the line index rather than special-case every combination.
    std::string full = storage_.text();
    lineStarts_.clear();
    lineStarts_.push_back(0);
    for (size_t i = 0; i < full.size(); ++i) {
        if (full[i] == '\n') lineStarts_.push_back(i + 1);
    }
}

void TextDocument::replaceAllText(const std::string& newText) {
    storage_.erase(0, storage_.size());
    storage_.insert(0, newText);
    lineStarts_.clear();
    lineStarts_.push_back(0);
    for (size_t i = 0; i < newText.size(); ++i) {
        if (newText[i] == '\n') lineStarts_.push_back(i + 1);
    }
}
