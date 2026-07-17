#pragma once

// ============================================================================
// Selection
//
// A simple range between an anchor point (where the user started selecting)
// and an active point (where the cursor currently is). Deliberately a plain
// value type with no behavior beyond normalization — keeping it dumb makes
// it trivial to reason about and test independently of TextEditor.
// ============================================================================
struct Selection {
    bool active = false;
    int anchorRow = 0, anchorCol = 0;
    int activeRow = 0, activeCol = 0;

    void clear() { active = false; }

    void begin(int row, int col) {
        active = true;
        anchorRow = activeRow = row;
        anchorCol = activeCol = col;
    }

    void moveTo(int row, int col) {
        activeRow = row;
        activeCol = col;
    }

    // Returns true if the selection spans at least one character.
    bool isEmpty() const {
        return anchorRow == activeRow && anchorCol == activeCol;
    }

    // Normalizes so `first` is always the earlier point in document order —
    // callers shouldn't have to care whether the user selected forward or
    // backward.
    void normalized(int& startRow, int& startCol, int& endRow, int& endCol) const {
        if (anchorRow < activeRow || (anchorRow == activeRow && anchorCol <= activeCol)) {
            startRow = anchorRow; startCol = anchorCol;
            endRow = activeRow;   endCol = activeCol;
        } else {
            startRow = activeRow; startCol = activeCol;
            endRow = anchorRow;   endCol = anchorCol;
        }
    }
};
