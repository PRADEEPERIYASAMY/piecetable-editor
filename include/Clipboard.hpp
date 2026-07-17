#pragma once
#include <string>

// ============================================================================
// Clipboard
//
// Note: This is an in-process, single-slot clipboard. It does not integrate
// with the OS clipboard (X11/Wayland selection buffers, Windows clipboard).
// OS integration requires platform-specific code and dependencies, so we
// keep it strictly in-memory for now. If OS support is needed later,
// this class is the natural integration point.
// ============================================================================
class Clipboard {
public:
    void set(const std::string& text) { contents_ = text; }
    const std::string& contents() const { return contents_; }
    bool empty() const { return contents_.empty(); }

private:
    std::string contents_;
};
