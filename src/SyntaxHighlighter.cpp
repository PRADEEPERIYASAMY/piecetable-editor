#include "SyntaxHighlighter.hpp"
#include <cctype>
#include <unordered_set>

// ── Language detection ────────────────────────────────────────────────────────

SyntaxHighlighter::Language SyntaxHighlighter::fromFilename(const std::string& filename) {
    size_t dot = filename.rfind('.');
    if (dot == std::string::npos) return Language::None;
    std::string ext = filename.substr(dot);
    if (ext == ".cpp" || ext == ".cc" || ext == ".cxx" ||
        ext == ".hpp" || ext == ".h"  || ext == ".c")
        return Language::Cpp;
    if (ext == ".py")
        return Language::Python;
    return Language::None;
}

// ── ANSI color codes ─────────────────────────────────────────────────────────

const char* SyntaxHighlighter::ansiColor(TokenType type) {
    switch (type) {
        case TokenType::Keyword:      return "\x1b[1;34m"; // bold blue
        case TokenType::Preprocessor: return "\x1b[35m";   // magenta
        case TokenType::String:       return "\x1b[32m";   // green
        case TokenType::Number:       return "\x1b[33m";   // yellow
        case TokenType::Comment:      return "\x1b[90m";   // dark gray
        case TokenType::Normal:
        default:                      return "\x1b[0m";    // reset
    }
}

// ── Keyword sets ──────────────────────────────────────────────────────────────

static const std::unordered_set<std::string>& cppKeywords() {
    static const std::unordered_set<std::string> kw = {
        "alignas","alignof","and","and_eq","asm","auto","bitand","bitor",
        "bool","break","case","catch","char","char8_t","char16_t","char32_t",
        "class","compl","concept","const","consteval","constexpr","constinit",
        "const_cast","continue","co_await","co_return","co_yield","decltype",
        "default","delete","do","double","dynamic_cast","else","enum",
        "explicit","export","extern","false","final","float","for","friend",
        "goto","if","inline","int","long","mutable","namespace","new",
        "noexcept","not","not_eq","nullptr","operator","or","or_eq",
        "override","private","protected","public","register","reinterpret_cast",
        "requires","return","short","signed","sizeof","static","static_assert",
        "static_cast","struct","switch","template","this","thread_local",
        "throw","true","try","typedef","typeid","typename","union","unsigned",
        "using","virtual","void","volatile","wchar_t","while","xor","xor_eq"
    };
    return kw;
}

static const std::unordered_set<std::string>& pyKeywords() {
    static const std::unordered_set<std::string> kw = {
        "False","None","True","and","as","assert","async","await",
        "break","class","continue","def","del","elif","else","except",
        "finally","for","from","global","if","import","in","is",
        "lambda","nonlocal","not","or","pass","raise","return",
        "try","while","with","yield"
    };
    return kw;
}

// ── Tokenizer ─────────────────────────────────────────────────────────────────

std::vector<SyntaxToken> SyntaxHighlighter::tokenize(
    const std::string& line, Language lang, bool& inBlockComment)
{
    std::vector<SyntaxToken> tokens;

    // No-op for unsupported languages; Python has no block comments.
    if (lang == Language::None) {
        if (!line.empty()) tokens.push_back({0, (int)line.size(), TokenType::Normal});
        return tokens;
    }
    if (lang == Language::Python) inBlockComment = false;

    auto push = [&](int s, int len, TokenType t) {
        if (len > 0) tokens.push_back({s, len, t});
    };

    int i = 0;
    int n = (int)line.size();

    while (i < n) {

        // ── Block comment continuation (C++ only) ─────────────────────────
        if (inBlockComment) {
            int start = i;
            while (i < n) {
                if (i + 1 < n && line[i] == '*' && line[i + 1] == '/') {
                    i += 2; inBlockComment = false; break;
                }
                ++i;
            }
            push(start, i - start, TokenType::Comment);
            continue;
        }

        // ── C++ preprocessor directive (# only valid at column 0) ─────────
        if (lang == Language::Cpp && i == 0 && line[0] == '#') {
            push(0, n, TokenType::Preprocessor);
            return tokens;
        }

        // ── Single-line comment ───────────────────────────────────────────
        if (lang == Language::Cpp && i + 1 < n &&
            line[i] == '/' && line[i + 1] == '/') {
            push(i, n - i, TokenType::Comment);
            return tokens;
        }
        if (lang == Language::Python && line[i] == '#') {
            push(i, n - i, TokenType::Comment);
            return tokens;
        }

        // ── Block comment open (C++ only) ─────────────────────────────────
        if (lang == Language::Cpp && i + 1 < n &&
            line[i] == '/' && line[i + 1] == '*') {
            int start = i; i += 2; inBlockComment = true;
            while (i < n) {
                if (i + 1 < n && line[i] == '*' && line[i + 1] == '/') {
                    i += 2; inBlockComment = false; break;
                }
                ++i;
            }
            push(start, i - start, TokenType::Comment);
            continue;
        }

        // ── String literal ────────────────────────────────────────────────
        if (line[i] == '"' || line[i] == '\'') {
            char delim = line[i];
            int start = i++;
            while (i < n) {
                if (line[i] == '\\') { i += 2; continue; } // skip escape
                if (line[i] == delim) { ++i; break; }
                ++i;
            }
            push(start, i - start, TokenType::String);
            continue;
        }

        // ── Numeric literal (integer, float, hex, binary) ─────────────────
        if (std::isdigit((unsigned char)line[i]) ||
            (line[i] == '.' && i + 1 < n && std::isdigit((unsigned char)line[i + 1]))) {
            int start = i;
            while (i < n && (std::isalnum((unsigned char)line[i]) ||
                              line[i] == '.' || line[i] == '_'))
                ++i;
            push(start, i - start, TokenType::Number);
            continue;
        }

        // ── Identifier or keyword ─────────────────────────────────────────
        if (std::isalpha((unsigned char)line[i]) || line[i] == '_') {
            int start = i;
            while (i < n && (std::isalnum((unsigned char)line[i]) || line[i] == '_'))
                ++i;
            std::string word = line.substr(start, i - start);
            TokenType type = TokenType::Normal;
            if (lang == Language::Cpp   && cppKeywords().count(word)) type = TokenType::Keyword;
            if (lang == Language::Python && pyKeywords().count(word))  type = TokenType::Keyword;
            push(start, i - start, type);
            continue;
        }

        // ── Normal characters (punctuation, operators, whitespace) ─────────
        // Batch consecutive characters that can't start a special token.
        {
            int start = i++;
            while (i < n) {
                char ch = line[i];
                if (std::isalpha((unsigned char)ch) || ch == '_') break;
                if (std::isdigit((unsigned char)ch)) break;
                if (ch == '"' || ch == '\'') break;
                if (lang == Language::Cpp) {
                    if (ch == '/') break; // might be // or /*
                    if (i == 0 && ch == '#') break;
                }
                if (lang == Language::Python && ch == '#') break;
                ++i;
            }
            push(start, i - start, TokenType::Normal);
        }
    }

    return tokens;
}
