#include "UndoRedoStack.hpp"

void UndoRedoStack::push(std::unique_ptr<EditCommand> command) {
    undoStack_.push_back(std::move(command));
    redoStack_.clear();
}

void UndoRedoStack::undo() {
    if (undoStack_.empty()) return;
    auto command = std::move(undoStack_.back());
    undoStack_.pop_back();
    command->undo();
    redoStack_.push_back(std::move(command));
}

void UndoRedoStack::redo() {
    if (redoStack_.empty()) return;
    auto command = std::move(redoStack_.back());
    redoStack_.pop_back();
    command->redo();
    undoStack_.push_back(std::move(command));
}
