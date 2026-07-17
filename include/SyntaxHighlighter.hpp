#pragma once
#include <string>
#include <vector>

// ============================================================================
// SyntaxHighlighter
//
// A stateless, per-line tokenizer that classifies each character span in a
// line of text into one of several semantic token types. Supports C/C++ and
// Python; language is detected from the file extension.
//
// Design decisions:
//
//   Stateless per-line interface — the caller (ScreenRenderer) maintains an
//   `inBlockComment` flag across rows so the highlighter itself holds no
//   mutable state. This makes the class trivially thread-safe and easy to
//   test: just call tokenize() with arbitrary inputs.
//
//   Single left-to-right scan — each character is examined exactly once, so
//   tokenizing a line is O(line length). No regex, no backtracking.
//
//   Python triple-quoted strings (""" ... """) are deliberately not tracked:
//   they require cross-line state analogous to C block comments. We skip them
//   to avoid doubling the cross-line bookkeeping complexity for a relatively
//   infrequent token type.
//
//   Tokens cover the entire line — every character in the line belongs to
//   exactly one token. This simplifies the renderer: it can iterate tokens
//   linearly and emit ANSI escape codes without tracking "holes".
// ============================================================================

enum class TokenType {
    Normal,
    Keyword,
    Preprocessor,   // C/C++ #include, #define, etc.
    String,
    Number,
    Comment,
};

struct SyntaxToken {
    int start;    // column offset within the line (0-indexed)
    int length;   // number of characters
    TokenType type;
};

class SyntaxHighlighter {
public:
    enum class Language { None, Cpp, Python };

    // Infers language from the file extension.
    // .cpp .cc .cxx .hpp .h .c → Cpp;  .py → Python;  anything else → None.
    static Language fromFilename(const std::string& filename);

    // Tokenizes `line` for `lang`.
    // `inBlockComment` is read on entry (true when a C /* comment started
    // on an earlier line) and updated on exit. The returned tokens together
    // cover every character of `line` exactly once.
    static std::vector<SyntaxToken> tokenize(const std::string& line,
                                             Language lang,
                                             bool& inBlockComment);

    // Returns the ANSI color escape sequence for a token type.
    static const char* ansiColor(TokenType type);
};
