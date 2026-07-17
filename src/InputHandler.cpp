#include "InputHandler.hpp"
#include "TextEditor.hpp"
#include "Terminal.hpp"
#include "ScreenRenderer.hpp"
#include <unistd.h>

namespace {

// ASCII control-code constants, named instead of magic numbers so the
// mapping between "Ctrl+X" and its byte value is obvious at the call site.
constexpr char kCtrlSpace = 0;
constexpr char kCtrlA     = 1;
constexpr char kCtrlB     = 2;
constexpr char kCtrlC     = 3;
constexpr char kCtrlD     = 4;
constexpr char kCtrlF     = 6;
constexpr char kCtrlN     = 14;
constexpr char kCtrlQ     = 17;
constexpr char kCtrlR     = 18; // toggle regex search
constexpr char kCtrlS     = 19;
constexpr char kCtrlV     = 22;
constexpr char kCtrlX     = 24;
constexpr char kCtrlY     = 25;
constexpr char kCtrlZ     = 26;
constexpr char kEscape    = 27;
constexpr char kBackspace = 127;

// Ctrl+Q: quit, prompting to save first if there are unsaved changes.
void handleQuit(TextEditor& editor) {
    if (!editor.isDirty()) { editor.quitImmediately(); return; }

    editor.notify("Unsaved changes! Save before quitting? (y = save, n = discard, c = cancel)");
    ScreenRenderer::render(editor);

    char choice;
    if (read(STDIN_FILENO, &choice, 1) != 1) return;

    if (choice == 'y' || choice == 'Y') {
        if (editor.filename().empty()) editor.saveAs(); else editor.save();
        if (!editor.isDirty()) editor.quitImmediately();
    } else if (choice == 'n' || choice == 'N') {
        editor.quitImmediately();
    } else {
        editor.notify("Quit cancelled.");
    }
}

// Arrow keys arrive as a 3-byte escape sequence: ESC, '[', then a letter
// identifying the direction. This function is only called after the
// leading ESC byte has already been consumed by the caller, and it always
// consumes exactly the two follow-up bytes, matching how every VT100-style
// terminal emits these regardless of which arrow was pressed.
//
// Note: this project can't distinguish plain arrows from Shift+Arrow —
// that needs parsing extended CSI sequences (e.g. "\x1b[1;2C") that most
// terminals only emit when a modifier key is held, which is out of scope
// here. Instead, Ctrl+Space toggles "selection mode": while it's on, any
// arrow-key movement extends the current selection.
void handleArrowKeySequence(TextEditor& editor) {
    char seq[2];
    if (read(STDIN_FILENO, &seq[0], 1) != 1) return;
    if (read(STDIN_FILENO, &seq[1], 1) != 1) return;
    if (seq[0] != '[') return;

    switch (seq[1]) {
        case 'A': editor.moveCursorUp();    break;
        case 'B': editor.moveCursorDown();  break;
        case 'C': editor.moveCursorRight(); break;
        case 'D': editor.moveCursorLeft();  break;
        default: return;
    }

    editor.clampCursorToLine();
    if (editor.selection().active) {
        editor.extendSelectionTo(editor.cursorRow() - 1, editor.cursorCol() - 1);
    }
}

// Ctrl+Space is a toggle: press once to drop an anchor and start marking a
// selection, press again to release it. This keeps the raw-terminal input
// model simple (no modifier-aware arrow parsing needed) while still giving
// a full select -> copy/cut -> paste workflow.
void toggleSelectionMode(TextEditor& editor) {
    if (editor.selection().active) {
        editor.clearSelection();
    } else {
        editor.beginSelection();
    }
}

} // namespace

namespace InputHandler {

void processNextKeypress(TextEditor& editor) {
    char c;
    if (read(STDIN_FILENO, &c, 1) != 1) return;

    if (c == kCtrlQ) { handleQuit(editor); return; }

    if (c == kEscape) {
        if (editor.isSearchActive()) return; // beginSearch()'s own loop owns ESC while active
        handleArrowKeySequence(editor);
        return;
    }

    if (c >= 32 && c <= 126) { editor.insertCharacter(c); return; }
    if (c == '\r' || c == '\n') { editor.insertNewline(); return; }
    if (c == kBackspace) { editor.deleteCharacterBeforeCursor(); return; }

    if (c == kCtrlS) { editor.save(); return; }
    if (c == kCtrlA) { editor.saveAs(); return; }
    if (c == kCtrlZ) { editor.undo(); return; }
    if (c == kCtrlY) { editor.redo(); return; }
    if (c == kCtrlF) { editor.beginSearch(); return; }
    if (c == kCtrlN) { editor.findNext(); return; }
    if (c == kCtrlB) { editor.findPrevious(); return; }
    if (c == kCtrlR) { editor.toggleRegexSearch(); return; }
    if (c == kCtrlSpace) { toggleSelectionMode(editor); return; }
    if (c == kCtrlC) { editor.copySelectionToClipboard(); return; }
    if (c == kCtrlX) { editor.cutSelectionToClipboard(); return; }
    if (c == kCtrlV) { editor.pasteFromClipboard(); return; }
    if (c == kCtrlD) { editor.showVersionHistory(); return; }
}

} // namespace InputHandler
