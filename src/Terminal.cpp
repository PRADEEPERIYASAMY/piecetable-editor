#include "Terminal.hpp"
#include <termios.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/ioctl.h>

namespace {
    termios g_originalTermios;
}

namespace Terminal {

void disableRawMode() {
    write(STDOUT_FILENO, "\x1b[?25h", 6);
    write(STDOUT_FILENO, "\x1b[?1049l", 8);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_originalTermios);
}

void enableRawMode() {
    tcgetattr(STDIN_FILENO, &g_originalTermios);
    atexit([] { disableRawMode(); });

    termios raw = g_originalTermios;
    write(STDOUT_FILENO, "\x1b[?1049h", 8);
    write(STDOUT_FILENO, "\x1b[?25l", 6);
    raw.c_lflag &= ~(ECHO | ICANON | ISIG);
    raw.c_iflag &= ~(IXON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int cursorPosition(int& row, int& col) {
    char buf[32];
    unsigned int i = 0;

    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

    while (i < sizeof(buf) - 1) {
        if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
        if (buf[i] == 'R') break;
        i++;
    }
    buf[i] = '\0';

    if (buf[0] != '\x1b' || buf[1] != '[') return -1;
    if (sscanf(&buf[2], "%d;%d", &row, &col) != 2) return -1;
    return 0;
}

int windowSize(int& rows, int& cols) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != -1 && ws.ws_col != 0) {
        cols = ws.ws_col;
        rows = ws.ws_row;
        return 0;
    }

    // The cursor-position fallback below writes a query escape sequence
    // and then blocks on read() waiting for the terminal to answer. That's
    // only safe when stdin is an actual interactive terminal — running
    // under a test harness, a CI runner, or with input piped/redirected,
    // nothing will ever answer and the read() would hang forever. Detect
    // that case up front and just use a sane default instead.
    if (!isatty(STDIN_FILENO)) {
        rows = 24;
        cols = 80;
        return -1;
    }

    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
    if (cursorPosition(rows, cols) == -1) {
        rows = 24;
        cols = 80;
        return -1;
    }
    return 0;
}

void clearScreen() {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
}

void moveCursorTo(int row, int col) {
    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", row, col);
    write(STDOUT_FILENO, buf, strlen(buf));
}

} // namespace Terminal
