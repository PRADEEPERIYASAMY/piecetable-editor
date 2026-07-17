#pragma once
#include <string>
#include <vector>
#include <cstddef>

// ============================================================================
// GapBuffer
//
// An alternative text-storage backend to PieceTable. The document is stored
// as a single contiguous array with a movable "gap" — a reserved block of
// free capacity positioned at the most recent edit point.
//
// Complexity (n = document size, k = text being inserted/erased):
//   Insert at gap position  : O(k) amortized (shrink the gap in place)
//   Insert elsewhere        : O(dist_to_gap) to reposition + O(k) copy
//   Erase at gap position   : O(dist_to_gap) to reposition, then O(1)
//   charAt / text()         : O(1) / O(n)
//
// This makes sequential typing (the dominant editing pattern) extremely fast
// because the gap is always left at the cursor position. But for alternating
// or random-position edits the gap must be moved — O(n) per repositioning —
// which is exactly where a piece table wins. The benchmark in tests.cpp
// quantifies this tradeoff across four representative scenarios.
//
// Public interface mirrors PieceTable so both backends can be swapped
// transparently by the caller without changing any editor logic.
// ============================================================================

class GapBuffer {
public:
    GapBuffer();

    void loadFromFile(const std::string& filePath);

    // Inserts `text` at logical document position `position`
    // (clamped to [0, size()]).
    void insert(size_t position, const std::string& text);

    // Removes `length` characters starting at logical `position`.
    void erase(size_t position, size_t length);

    std::string text() const;
    size_t size() const { return buf_.size() - gapSize(); }
    char charAt(size_t index) const;
    std::string substring(size_t start, size_t length) const;

private:
    static constexpr size_t kDefaultGap = 1 << 12; // 4 KB starting gap

    // Physical layout:  [pre-gap text (gapStart_ bytes)]
    //                   [gap         (gapEnd_-gapStart_ bytes — unused capacity)]
    //                   [post-gap text (buf_.size()-gapEnd_ bytes)]
    std::vector<char> buf_;
    size_t gapStart_ = 0;  // first index of the gap
    size_t gapEnd_   = 0;  // one-past the last index of the gap

    size_t gapSize() const { return gapEnd_ - gapStart_; }

    // Repositions the gap so gapStart_ == pos (logical coordinate).
    // Copies at most |pos - gapStart_| characters — O(distance).
    void moveGapTo(size_t pos);

    // Grows the gap to at least `needed` bytes, reallocating if necessary.
    void ensureGap(size_t needed);
};
