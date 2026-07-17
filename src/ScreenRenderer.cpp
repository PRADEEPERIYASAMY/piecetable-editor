#include "ScreenRenderer.hpp"
#include "TextEditor.hpp"
#include "SyntaxHighlighter.hpp"
#include "RegexEngine.hpp"
#include <unistd.h>
#include <cstring>
#include <algorithm>

namespace {

constexpr const char* kSearchMatchStyle  = "\x1b[30;43m"; // black on yellow
constexpr const char* kCurrentMatchStyle = "\x1b[30;46m"; // black on cyan
constexpr const char* kSelectionStyle    = "\x1b[30;47m"; // black on white
constexpr const char* kResetStyle        = "\x1b[0m";

// Returns true and writes [start,end) column range if `row` intersects the
// current selection. Only reads from Selection's public API, touching nothing
// else inside TextEditor.
bool selectionRangeOnRow(const TextEditor& editor, int row, int& start, int& end) {
    if (!editor.hasActiveSelection()) return false;
    int startRow, startCol, endRow, endCol;
    editor.selection().normalized(startRow, startCol, endRow, endCol);
    if (row < startRow || row > endRow) return false;
    start = (row == startRow) ? startCol : 0;
    end   = (row == endRow)   ? endCol   : static_cast<int>(editor.lineText(row).size());
    return true;
}

// ── Render the text rows ──────────────────────────────────────────────────────

void drawRows(const TextEditor& editor, FrameBuffer& fb) {
    // Detect language once per frame from the filename.
    SyntaxHighlighter::Language lang = SyntaxHighlighter::fromFilename(editor.filename());

    // Pre-scan all rows above the viewport so the block-comment state is
    // correct when we start rendering the first visible row. This is an
    // O(lines-above-viewport × avg-line-length) scan per frame — negligible
    // for typical files and vastly simpler than caching state per line.
    bool inBlockComment = false;
    for (int r = 0; r < editor.rowOffset() && r < editor.lineCount(); ++r)
        SyntaxHighlighter::tokenize(editor.lineText(r), lang, inBlockComment);

    for (int i = 0; i < editor.screenRows(); i++) {
        fb.append("\x1b[K", 3);
        int fileRow = i + editor.rowOffset();

        // ── Welcome splash ────────────────────────────────────────────────
        if (editor.lineCount() == 0 && i == editor.screenRows() / 3) {
            const char* msg = "PieceTable Editor -- version 1.0";
            int len     = std::min((int)strlen(msg), editor.screenCols());
            int padding = (editor.screenCols() - len) / 2;
            if (padding) { fb.append("~", 1); padding--; }
            while (padding--) fb.append(" ", 1);
            fb.append(msg, len);

        // ── File content ──────────────────────────────────────────────────
        } else if (fileRow < editor.lineCount()) {
            std::string line   = editor.lineText(fileRow);
            int colOff         = std::clamp(editor.colOffset(), 0, (int)line.size());
            int visibleLen     = std::min(editor.screenCols(), (int)line.size() - colOff);
            std::string visible = visibleLen > 0 ? line.substr(colOff, visibleLen) : "";

            // Always tokenize to keep inBlockComment accurate for subsequent rows.
            // The tokens vector is only *used* in the normal (no-search, no-selection)
            // rendering branch below.
            auto tokens = SyntaxHighlighter::tokenize(line, lang, inBlockComment);

            int  selStart = 0, selEnd = 0;
            bool hasSelection = selectionRangeOnRow(editor, fileRow, selStart, selEnd);

            // ── Branch 1: search active — highlight matches ───────────────
            if (editor.isSearchActive() && !editor.searchQuery().empty()) {
                if (!editor.regexSearch()) {
                    // Literal search: re-scan the visible portion for matches.
                    size_t pos = 0, last = 0;
                    const std::string& q = editor.searchQuery();
                    while ((pos = visible.find(q, last)) != std::string::npos) {
                        bool isCurrent = (fileRow == editor.currentMatchRow() &&
                                           (int)(pos + colOff) == editor.currentMatchCol());
                        fb.appendStr(visible.substr(last, pos - last));
                        fb.appendStr(isCurrent ? kCurrentMatchStyle : kSearchMatchStyle);
                        fb.appendStr(visible.substr(pos, q.size()));
                        fb.appendStr(kResetStyle);
                        last = pos + q.size();
                    }
                    fb.appendStr(visible.substr(last));
                } else {
                    // Regex search: use RegexEngine to find matches in the visible window.
                    auto matches = RegexEngine::findAll(editor.searchQuery(), visible);
                    int last = 0;
                    for (auto& m : matches) {
                        if (m.length <= 0) continue;
                        bool isCurrent = (fileRow == editor.currentMatchRow() &&
                                           (m.col + colOff) == editor.currentMatchCol());
                        fb.appendStr(visible.substr(last, m.col - last));
                        fb.appendStr(isCurrent ? kCurrentMatchStyle : kSearchMatchStyle);
                        fb.appendStr(visible.substr(m.col, m.length));
                        fb.appendStr(kResetStyle);
                        last = m.col + m.length;
                    }
                    fb.appendStr(visible.substr(last));
                }

            // ── Branch 2: selection active — highlight selection ──────────
            } else if (hasSelection) {
                int localStart = std::clamp(selStart - colOff, 0, (int)visible.size());
                int localEnd   = std::clamp(selEnd   - colOff, 0, (int)visible.size());
                fb.appendStr(visible.substr(0, localStart));
                fb.appendStr(kSelectionStyle);
                fb.appendStr(visible.substr(localStart, localEnd - localStart));
                fb.appendStr(kResetStyle);
                fb.appendStr(visible.substr(localEnd));

            // ── Branch 3: normal — syntax-highlight using token spans ─────
            } else {
                // Map tokens from full-line coordinates into the viewport window
                // [colOff, colOff+screenCols). Tokens that fall entirely outside
                // the viewport are skipped; tokens that straddle the edges are
                // clipped so we never emit characters outside the visible area.
                int viewEnd = colOff + editor.screenCols();
                TokenType currentType = TokenType::Normal;
                fb.appendStr(SyntaxHighlighter::ansiColor(currentType));

                for (auto& tok : tokens) {
                    int tokEnd = tok.start + tok.length;
                    if (tokEnd <= colOff) continue;   // entirely left of viewport
                    if (tok.start >= viewEnd) break;   // entirely right of viewport

                    int clipStart = std::max(tok.start, colOff);
                    int clipEnd   = std::min(tokEnd, viewEnd);
                    if (clipEnd <= clipStart) continue;

                    if (tok.type != currentType) {
                        fb.appendStr(SyntaxHighlighter::ansiColor(tok.type));
                        currentType = tok.type;
                    }
                    fb.appendStr(line.substr(clipStart, clipEnd - clipStart));
                }
                fb.appendStr(kResetStyle);
            }

        // ── Tilde for lines past end-of-file ─────────────────────────────
        } else {
            fb.append("~", 1);
        }

        fb.append("\r\n", 2);
    }
}

void drawStatusBar(const TextEditor& editor, FrameBuffer& fb) {
    fb.append("\x1b[7m", 4);
    constexpr int kRightMargin = 2;

    std::string left = editor.filename().empty() ? "[No Name]" : editor.filename();
    if (editor.isDirty()) left += " *";

    // Show regex mode indicator in the status bar so the user always knows
    // whether Ctrl+F will perform a literal or regex search.
    std::string right = std::string(editor.regexSearch() ? "[regex] " : "") +
                        "Ln "  + std::to_string(editor.cursorRow()) +
                        ", Col " + std::to_string(editor.cursorCol());

    int usableWidth = editor.screenCols() - kRightMargin;
    if ((int)right.size() > usableWidth) right.resize(usableWidth);
    int leftWidth = std::max(0, usableWidth - (int)right.size());
    if ((int)left.size() > leftWidth) left.resize(leftWidth);

    std::string status = left;
    status.append(leftWidth - status.size(), ' ');
    status += right;

    fb.appendStr(status);
    fb.append("  ", kRightMargin);
    fb.append("\x1b[m", 3);
    fb.append("\r\n", 2);
}

void drawMessageBar(const TextEditor& editor, FrameBuffer& fb) {
    fb.append("\x1b[K", 3);
    if (!editor.statusMessage().empty() &&
        (std::time(nullptr) - editor.statusMessageTime()) < 5) {
        int len = std::min((int)editor.statusMessage().size(), editor.screenCols());
        fb.append(editor.statusMessage().c_str(), len);
    }
}

} // namespace

namespace ScreenRenderer {

void render(const TextEditor& editor) {
    FrameBuffer fb;
    fb.append("\x1b[?25l", 6); // hide cursor during redraw to prevent flicker
    fb.append("\x1b[H",   3);  // move to top-left

    drawRows(editor, fb);
    drawStatusBar(editor, fb);
    drawMessageBar(editor, fb);

    // Reposition the hardware cursor.
    int screenY = std::max(1, editor.cursorRow() - editor.rowOffset());
    int screenX = std::max(1, editor.cursorCol() - editor.colOffset());
    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", screenY, screenX);
    fb.append(buf, strlen(buf));

    fb.append("\x1b[?25h", 6); // show cursor again
    write(STDOUT_FILENO, fb.data.c_str(), fb.data.size());
}

} // namespace ScreenRenderer
