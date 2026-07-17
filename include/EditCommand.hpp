#pragma once
#include <string>

class TextDocument;

// ============================================================================
// EditCommand (Command pattern)
//
// SOLID note: the original project had undo/redo operations hold a raw
// `Editor*` and reach into its private members (via friend declarations) to
// move the cursor and touch the buffer. That couples undo/redo to the
// concrete Editor class and violates the Dependency Inversion Principle —
// every new Editor field risks having to update unrelated command classes.
//
// Here, commands depend only on this tiny `CursorOwner` interface and a
// `TextDocument`, so they can't reach into anything else. TextEditor
// implements CursorOwner; that's the only coupling point.
// ============================================================================
class CursorOwner {
public:
    virtual ~CursorOwner() = default;
    virtual void setCursor(int col, int row) = 0;
};

class EditCommand {
public:
    virtual ~EditCommand() = default;
    virtual void undo() = 0;
    virtual void redo() = 0;
};

struct CursorPos { int col, row; };

class InsertCharCommand : public EditCommand {
public:
    InsertCharCommand(TextDocument& doc, CursorOwner& cursor,
                       int row, int col, char character,
                       CursorPos before, CursorPos after);
    void undo() override;
    void redo() override;

private:
    TextDocument& doc_;
    CursorOwner& cursor_;
    int row_, col_;
    char character_;
    CursorPos before_, after_;
};

class DeleteCharCommand : public EditCommand {
public:
    DeleteCharCommand(TextDocument& doc, CursorOwner& cursor,
                       int row, int col, char deletedCharacter,
                       CursorPos before, CursorPos after);
    void undo() override;
    void redo() override;

private:
    TextDocument& doc_;
    CursorOwner& cursor_;
    int row_, col_;
    char deletedCharacter_;
    CursorPos before_, after_;
};

class MergeLinesCommand : public EditCommand {
public:
    MergeLinesCommand(TextDocument& doc, CursorOwner& cursor,
                       int row, int previousLineLength,
                       CursorPos before, CursorPos after);
    void undo() override;
    void redo() override;

private:
    TextDocument& doc_;
    CursorOwner& cursor_;
    int row_, previousLineLength_;
    CursorPos before_, after_;
};

class InsertNewlineCommand : public EditCommand {
public:
    InsertNewlineCommand(TextDocument& doc, CursorOwner& cursor,
                          int row, int splitColumn,
                          CursorPos before, CursorPos after);
    void undo() override;
    void redo() override;

private:
    TextDocument& doc_;
    CursorOwner& cursor_;
    int row_, splitColumn_;
    CursorPos before_, after_;
};
