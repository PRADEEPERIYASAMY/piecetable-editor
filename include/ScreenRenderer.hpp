#pragma once
#include <string>

class TextEditor;

// A simple growable output buffer so the whole frame is written to the
// terminal in one syscall instead of many small ones (avoids visible
// flicker/tearing during redraw).
struct FrameBuffer {
    std::string data;
    void append(const char* s, size_t len) { data.append(s, len); }
    void appendStr(const std::string& s) { data.append(s); }
};

// ============================================================================
// ScreenRenderer
//
// Knows how to turn TextEditor's state into ANSI escape sequences. Contains
// zero editing logic — it only reads from TextEditor's public accessors.
// This separation means the rendering strategy (e.g. swapping ANSI output
// for a curses backend) can change without touching editing logic at all.
// ============================================================================
namespace ScreenRenderer {

void render(const TextEditor& editor);

} // namespace ScreenRenderer
