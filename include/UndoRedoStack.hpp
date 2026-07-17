#pragma once
#include <vector>
#include <memory>
#include "EditCommand.hpp"

// Owns the undo/redo history. Pushing a new command always clears the redo
// stack, matching the behavior every editor's users expect: once you make a
// new edit, the "future" you could have redone no longer exists.
class UndoRedoStack {
public:
    void push(std::unique_ptr<EditCommand> command);
    void undo();
    void redo();

    bool canUndo() const { return !undoStack_.empty(); }
    bool canRedo() const { return !redoStack_.empty(); }

private:
    std::vector<std::unique_ptr<EditCommand>> undoStack_;
    std::vector<std::unique_ptr<EditCommand>> redoStack_;
};
