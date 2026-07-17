#include <signal.h>
#include "TextEditor.hpp"
#include "Terminal.hpp"

int main(int argc, char* argv[]) {
    Terminal::enableRawMode();

    TextEditor editor;
    g_activeEditor = &editor;
    signal(SIGWINCH, onTerminalResizeSignal);

    if (argc >= 2) {
        editor.openFile(argv[1]);
    }

    editor.run();
    return 0;
}
