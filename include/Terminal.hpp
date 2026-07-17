#pragma once

// Low-level terminal control: raw mode, window size, cursor positioning.
// Deliberately free functions with no state of their own beyond the saved
// termios struct — there is exactly one terminal per process, so a class
// with a singleton instance would add ceremony without adding value.
namespace Terminal {

void enableRawMode();
void disableRawMode();
int  windowSize(int& rows, int& cols);
void clearScreen();
void moveCursorTo(int row, int col);
int  cursorPosition(int& row, int& col);

} // namespace Terminal
