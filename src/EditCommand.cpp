#include "EditCommand.hpp"
#include "TextDocument.hpp"

// ----- InsertCharCommand ----------------------------------------------------

InsertCharCommand::InsertCharCommand(TextDocument& doc, CursorOwner& cursor,
                                      int row, int col, char character,
                                      CursorPos before, CursorPos after)
    : doc_(doc), cursor_(cursor), row_(row), col_(col),
      character_(character), before_(before), after_(after) {}

void InsertCharCommand::undo() {
    doc_.deleteChar(row_, col_);
    cursor_.setCursor(before_.col, before_.row);
}

void InsertCharCommand::redo() {
    doc_.insertChar(row_, col_, character_);
    cursor_.setCursor(after_.col, after_.row);
}

// ----- DeleteCharCommand -----------------------------------------------------

DeleteCharCommand::DeleteCharCommand(TextDocument& doc, CursorOwner& cursor,
                                      int row, int col, char deletedCharacter,
                                      CursorPos before, CursorPos after)
    : doc_(doc), cursor_(cursor), row_(row), col_(col),
      deletedCharacter_(deletedCharacter), before_(before), after_(after) {}

void DeleteCharCommand::undo() {
    doc_.insertChar(row_, col_, deletedCharacter_);
    cursor_.setCursor(before_.col, before_.row);
}

void DeleteCharCommand::redo() {
    doc_.deleteChar(row_, col_);
    cursor_.setCursor(after_.col, after_.row);
}

// ----- MergeLinesCommand -----------------------------------------------------

MergeLinesCommand::MergeLinesCommand(TextDocument& doc, CursorOwner& cursor,
                                      int row, int previousLineLength,
                                      CursorPos before, CursorPos after)
    : doc_(doc), cursor_(cursor), row_(row),
      previousLineLength_(previousLineLength), before_(before), after_(after) {}

void MergeLinesCommand::undo() {
    doc_.insertNewline(row_, previousLineLength_);
    cursor_.setCursor(before_.col, before_.row);
}

void MergeLinesCommand::redo() {
    doc_.mergeWithNextLine(row_);
    cursor_.setCursor(after_.col, after_.row);
}

// ----- InsertNewlineCommand --------------------------------------------------

InsertNewlineCommand::InsertNewlineCommand(TextDocument& doc, CursorOwner& cursor,
                                            int row, int splitColumn,
                                            CursorPos before, CursorPos after)
    : doc_(doc), cursor_(cursor), row_(row),
      splitColumn_(splitColumn), before_(before), after_(after) {}

void InsertNewlineCommand::undo() {
    doc_.mergeWithNextLine(row_);
    cursor_.setCursor(before_.col, before_.row);
}

void InsertNewlineCommand::redo() {
    doc_.insertNewline(row_, splitColumn_);
    cursor_.setCursor(after_.col, after_.row);
}
