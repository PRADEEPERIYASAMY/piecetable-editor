#pragma once
#include <string>
#include <vector>
#include <cstddef>

// ============================================================================
// PieceTable
//
// A classic piece-table text buffer (same technique used by VS Code and
// Atom's early editors). Instead of mutating one big string on every edit,
// we keep two immutable-ish backing buffers:
//
//   originalText - the file's contents as loaded from disk (never mutated)
//   appendedText - a single growing buffer that all *new* text is appended to
//
// The document itself is represented as an ordered list of `Piece` values,
// each describing a contiguous slice of one of the two backing buffers.
// Editing the document means editing this list of pieces, not copying
// characters around — that's what makes inserts/deletes cheap even on large
// files.
// ============================================================================

enum class SourceBuffer { Original, Appended };

struct Piece {
    SourceBuffer source;
    size_t offset;   // start offset within the source buffer
    size_t length;   // number of characters this piece contributes
};

class PieceTable {
public:
    PieceTable();

    // Loads `originalText` from disk and resets the table to a single
    // piece spanning the whole file. Any unsaved edits are discarded.
    void loadFromFile(const std::string& filePath);

    // Inserts `text` at document offset `position` (clamped to [0, size()]).
    void insert(size_t position, const std::string& text);

    // Removes `length` characters starting at document offset `position`.
    void erase(size_t position, size_t length);

    std::string text() const;
    size_t size() const { return documentLength_; }
    char charAt(size_t index) const;
    std::string substring(size_t start, size_t length) const;

private:
    std::string originalText_;
    std::string appendedText_;
    std::vector<Piece> pieces_;
    size_t documentLength_ = 0;

    // Appends `text` to the shared append buffer and returns a Piece
    // describing it. If the new text is directly contiguous with the
    // buffer's current last piece (the common case: someone typing left
    // to right), the existing piece is extended in place instead of a new
    // one being allocated — this keeps piece count roughly proportional to
    // the number of *edit locations*, not the number of keystrokes.
    Piece appendAndTrackPiece(const std::string& text);

    // After a structural edit, merges any now-adjacent pieces that
    // reference contiguous ranges of the same backing buffer. Bounds the
    // growth of `pieces_` so long editing sessions don't degrade
    // insert/erase into an O(n) scan over thousands of tiny fragments.
    void coalesceAdjacentPieces();
};
