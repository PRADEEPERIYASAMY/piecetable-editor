#pragma once

class TextEditor;

// ============================================================================
// InputHandler
//
// Translates raw bytes read from stdin into calls on TextEditor's public
// API. Keeping this separate from TextEditor means the key bindings can be
// changed (or a config-driven keymap added later) without touching any
// editing logic, and editing logic can be unit-tested without a terminal
// or keyboard at all.
//
// Keybindings:
//   Arrows          move cursor (extends selection if one is active)
//   Enter           newline
//   Backspace       delete before cursor
//   Ctrl+S          save
//   Ctrl+A          save as
//   Ctrl+Q          quit (prompts to save if dirty)
//   Ctrl+Z / Ctrl+Y undo / redo
//   Ctrl+F          search        Ctrl+N / Ctrl+B  next / previous match
//   Ctrl+Space      start/stop marking a selection
//   Ctrl+C / Ctrl+X copy / cut selection      Ctrl+V  paste
//   Ctrl+D          show diff summary vs. last saved version
// ============================================================================
namespace InputHandler {

void processNextKeypress(TextEditor& editor);

} // namespace InputHandler
