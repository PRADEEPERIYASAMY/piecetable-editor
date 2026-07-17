// tests.cpp — Unit tests + benchmark harness for piecetable-editor
//
// Build:  make test
// Run:    ./bin/test
//
// This file is intentionally structured so each section can be read
// independently by a recruiter or reviewer scanning the code.

#include <cassert>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <string>
#include <sstream>

#include "PieceTable.hpp"
#include "GapBuffer.hpp"
#include "TextDocument.hpp"
#include "TextEditor.hpp"
#include "DiffEngine.hpp"
#include "VersionHistory.hpp"
#include "SyntaxHighlighter.hpp"
#include "RegexEngine.hpp"

// ============================================================================
// Helpers
// ============================================================================

// Returns elapsed time in milliseconds for calling fn() `iters` times.
template <typename F>
double measure_ms(F&& fn, int iters = 1) {
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iters; ++i) fn();
    auto t1 = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::milli>(t1 - t0).count();
}

static int tests_run = 0, tests_passed = 0;

#define RUN_TEST(fn) do { \
    ++tests_run; \
    fn(); \
    ++tests_passed; \
    std::cout << "  [PASS] " #fn "\n"; \
} while (0)

// ============================================================================
// PieceTable
// ============================================================================

void test_piece_table_insert() {
    PieceTable pt;
    pt.insert(0, "Hello");
    pt.insert(5, " World");
    assert(pt.text() == "Hello World");
}

void test_piece_table_erase() {
    PieceTable pt;
    pt.insert(0, "Hello World");
    pt.erase(5, 1);
    assert(pt.text() == "HelloWorld");
}

void test_piece_table_midstream_insert_and_erase() {
    PieceTable pt;
    pt.insert(0, "ACE");
    pt.insert(1, "B");   // -> ABCE
    pt.insert(3, "D");   // -> ABCDE
    assert(pt.text() == "ABCDE");
    pt.erase(2, 2);      // remove "CD" -> ABE
    assert(pt.text() == "ABE");
}

// ============================================================================
// GapBuffer — correctness
// ============================================================================

void test_gapbuffer_append_sequential() {
    GapBuffer gb;
    for (char c : std::string("Hello World"))
        gb.insert(gb.size(), std::string(1, c));
    assert(gb.text() == "Hello World");
}

void test_gapbuffer_insert_at_beginning() {
    GapBuffer gb;
    gb.insert(0, "World");
    gb.insert(0, "Hello ");
    assert(gb.text() == "Hello World");
}

void test_gapbuffer_insert_in_middle() {
    GapBuffer gb;
    gb.insert(0, "ACE");
    gb.insert(1, "B");  // ABCE
    gb.insert(3, "D");  // ABCDE
    assert(gb.text() == "ABCDE");
}

void test_gapbuffer_erase_front() {
    GapBuffer gb;
    gb.insert(0, "Hello World");
    gb.erase(0, 6); // remove "Hello "
    assert(gb.text() == "World");
}

void test_gapbuffer_erase_middle() {
    GapBuffer gb;
    gb.insert(0, "Hello World");
    gb.erase(5, 1); // remove space
    assert(gb.text() == "HelloWorld");
}

void test_gapbuffer_erase_then_insert() {
    GapBuffer gb;
    gb.insert(0, "aXb");
    gb.erase(1, 1);       // "ab"
    gb.insert(1, "c");    // "acb"
    assert(gb.text() == "acb");
}

void test_gapbuffer_charAt() {
    GapBuffer gb;
    gb.insert(0, "Hello");
    assert(gb.charAt(0) == 'H');
    assert(gb.charAt(4) == 'o');
}

void test_gapbuffer_size() {
    GapBuffer gb;
    gb.insert(0, "abc");
    assert(gb.size() == 3);
    gb.erase(1, 1);
    assert(gb.size() == 2);
}

void test_gapbuffer_empty_string_insert() {
    GapBuffer gb;
    gb.insert(0, "");      // must not crash or change state
    assert(gb.size() == 0);
}

void test_gapbuffer_insert_past_end() {
    GapBuffer gb;
    gb.insert(0, "abc");
    gb.insert(9999, "X"); // should clamp to end
    assert(gb.text() == "abcX");
}

// ============================================================================
// TextDocument
// ============================================================================

void test_document_insert_char() {
    TextDocument doc;
    doc.appendLine("Hello");
    doc.insertChar(0, 5, '!');
    assert(doc.lineText(0) == "Hello!");
}

void test_document_newline_split() {
    TextDocument doc;
    doc.appendLine("HelloWorld");
    doc.insertNewline(0, 5);
    assert(doc.lineText(0) == "Hello");
    assert(doc.lineText(1) == "World");
}

void test_document_merge_lines() {
    TextDocument doc;
    doc.appendLine("Hello");
    doc.appendLine("World");
    doc.mergeWithNextLine(0);
    assert(doc.lineText(0) == "HelloWorld");
}

void test_document_line_starts_stay_correct_after_many_edits() {
    TextDocument doc;
    for (char c : std::string("abc")) doc.insertChar(0, doc.lineLength(0), c);
    doc.insertNewline(0, 1); // "a" | "bc"
    for (char c : std::string("XY")) doc.insertChar(1, doc.lineLength(1), c);

    assert(doc.lineCount() == 2);
    assert(doc.lineText(0) == "a");
    assert(doc.lineText(1) == "bcXY");

    doc.mergeWithNextLine(0);
    assert(doc.lineCount() == 1);
    assert(doc.lineText(0) == "abcXY");
}

void test_row_col_to_offset() {
    TextDocument doc;
    doc.appendLine("abc");
    doc.appendLine("def");
    // "abc\ndef" -> offset of 'f' is 6
    assert(doc.toOffset(1, 2) == 6);
}

// ============================================================================
// TextEditor: undo/redo, search
// ============================================================================

void test_undo_insert() {
    TextEditor editor;
    editor.insertCharacter('H');
    editor.insertCharacter('i');
    editor.undo();
    assert(editor.lineText(0) == "H");
    editor.undo();
    assert(editor.lineText(0) == "");
}

void test_undo_then_redo() {
    TextEditor editor;
    editor.insertCharacter('A');
    editor.insertCharacter('B');
    editor.undo();
    editor.redo();
    assert(editor.lineText(0) == "AB");
}

void test_search_finds_all_matches() {
    TextEditor editor;
    editor.testAppendLine("hello world");
    editor.testAppendLine("world hello");
    editor.testSetSearchQuery("world");
    assert(editor.testMatchCount() == 2);
}

// ============================================================================
// DiffEngine (LCS-based line diff)
// ============================================================================

void test_diff_detects_pure_addition() {
    auto diff = DiffEngine::diffLines({"a", "b"}, {"a", "b", "c"});
    int added = 0;
    for (auto& d : diff) if (d.op == DiffOp::Added) added++;
    assert(added == 1);
}

void test_diff_detects_pure_removal() {
    auto diff = DiffEngine::diffLines({"a", "b", "c"}, {"a", "c"});
    int removed = 0;
    for (auto& d : diff) if (d.op == DiffOp::Removed) removed++;
    assert(removed == 1);
}

void test_diff_identical_inputs_are_all_unchanged() {
    auto diff = DiffEngine::diffLines({"x", "y", "z"}, {"x", "y", "z"});
    for (auto& d : diff) assert(d.op == DiffOp::Unchanged);
    assert(diff.size() == 3);
}

void test_version_history_diff_against_latest() {
    VersionHistory history;
    history.recordSnapshot("line1\nline2\n");
    auto diff = history.diffAgainstLatest("line1\nline2\nline3\n");
    int added = 0;
    for (auto& d : diff) if (d.op == DiffOp::Added) added++;
    assert(added == 1);
}

// ============================================================================
// Selection / Clipboard (via TextEditor's public API)
// ============================================================================

void test_copy_paste_round_trip() {
    TextEditor editor;
    editor.testAppendLine("hello world");
    editor.beginSelection();
    editor.extendSelectionTo(0, 5);
    editor.copySelectionToClipboard();
    editor.clearSelection();
    editor.pasteFromClipboard();
    assert(editor.lineText(0).substr(0, 5) == "hello");
}

// ============================================================================
// SyntaxHighlighter
// ============================================================================

void test_syntax_cpp_keyword() {
    bool inBlock = false;
    auto toks = SyntaxHighlighter::tokenize(
        "int main() {", SyntaxHighlighter::Language::Cpp, inBlock);
    bool foundKw = false;
    for (auto& t : toks)
        if (t.type == TokenType::Keyword) { foundKw = true; break; }
    assert(foundKw && "Expected 'int' to be highlighted as keyword");
}

void test_syntax_cpp_line_comment() {
    bool inBlock = false;
    auto toks = SyntaxHighlighter::tokenize(
        "int x; // a comment", SyntaxHighlighter::Language::Cpp, inBlock);
    bool foundComment = false;
    for (auto& t : toks)
        if (t.type == TokenType::Comment) { foundComment = true; break; }
    assert(foundComment);
}

void test_syntax_cpp_block_comment_span() {
    // Line 1 opens a block comment, line 2 should be classified as Comment.
    bool inBlock = false;
    SyntaxHighlighter::tokenize("/* open", SyntaxHighlighter::Language::Cpp, inBlock);
    assert(inBlock && "Block comment should still be open after line 1");
    auto toks2 = SyntaxHighlighter::tokenize(
        "still in comment", SyntaxHighlighter::Language::Cpp, inBlock);
    bool allComment = true;
    for (auto& t : toks2)
        if (t.type != TokenType::Comment) { allComment = false; break; }
    assert(allComment && "All tokens inside block comment must be Comment type");
}

void test_syntax_cpp_string_literal() {
    bool inBlock = false;
    auto toks = SyntaxHighlighter::tokenize(
        "std::string s = \"hello\";", SyntaxHighlighter::Language::Cpp, inBlock);
    bool foundStr = false;
    for (auto& t : toks)
        if (t.type == TokenType::String) { foundStr = true; break; }
    assert(foundStr);
}

void test_syntax_cpp_preprocessor() {
    bool inBlock = false;
    auto toks = SyntaxHighlighter::tokenize(
        "#include <vector>", SyntaxHighlighter::Language::Cpp, inBlock);
    // Preprocessor lines are returned as a single token covering the whole line.
    assert(!toks.empty() && toks[0].type == TokenType::Preprocessor);
}

void test_syntax_python_keyword() {
    bool inBlock = false;
    auto toks = SyntaxHighlighter::tokenize(
        "def foo():", SyntaxHighlighter::Language::Python, inBlock);
    bool found = false;
    for (auto& t : toks)
        if (t.type == TokenType::Keyword) { found = true; break; }
    assert(found);
}

void test_syntax_none_language_passthrough() {
    bool inBlock = false;
    std::string line = "some random text";
    auto toks = SyntaxHighlighter::tokenize(line, SyntaxHighlighter::Language::None, inBlock);
    int total = 0;
    for (auto& t : toks) total += t.length;
    assert(total == (int)line.size());
}

void test_syntax_tokens_cover_entire_line() {
    // Every character in the line must be covered by exactly one token.
    bool inBlock = false;
    std::string line = "int x = 42; /* answer */ // done";
    auto toks = SyntaxHighlighter::tokenize(line, SyntaxHighlighter::Language::Cpp, inBlock);
    int total = 0;
    for (auto& t : toks) total += t.length;
    assert(total == (int)line.size() &&
           "Tokens must cover the entire line without gaps or overlaps");
}

void test_syntax_language_detection() {
    assert(SyntaxHighlighter::fromFilename("editor.cpp") == SyntaxHighlighter::Language::Cpp);
    assert(SyntaxHighlighter::fromFilename("editor.hpp") == SyntaxHighlighter::Language::Cpp);
    assert(SyntaxHighlighter::fromFilename("script.py")  == SyntaxHighlighter::Language::Python);
    assert(SyntaxHighlighter::fromFilename("README.md")  == SyntaxHighlighter::Language::None);
    assert(SyntaxHighlighter::fromFilename("noextension") == SyntaxHighlighter::Language::None);
}

// ============================================================================
// RegexEngine (Thompson NFA)
// ============================================================================

void test_regex_literal_match() {
    auto m = RegexEngine::findAll("hello", "say hello world");
    assert(m.size() == 1 && m[0].col == 4 && m[0].length == 5);
}

void test_regex_dot_star() {
    auto m = RegexEngine::findAll("a.*b", "xafoob");
    assert(!m.empty() && m[0].col == 1 && m[0].length == 5);
}

void test_regex_plus_quantifier() {
    auto m = RegexEngine::findAll("a+", "baaac");
    assert(m.size() == 1 && m[0].col == 1 && m[0].length == 3);
}

void test_regex_question_quantifier() {
    auto m = RegexEngine::findAll("colou?r", "The colour and the color");
    // Should find both "colour" and "color".
    assert(m.size() == 2);
}

void test_regex_alternation() {
    auto m = RegexEngine::findAll("cat|dog", "I have a cat and a dog");
    assert(m.size() == 2);
}

void test_regex_character_class() {
    auto m = RegexEngine::findAll("[0-9]+", "abc 42 and 007");
    assert(m.size() == 2);
    assert(m[0].length == 2); // "42"
    assert(m[1].length == 3); // "007"
}

void test_regex_negated_class() {
    // [^aeiou]+ should match one or more consonant runs.
    auto m = RegexEngine::findAll("[^aeiou ]+", "cat");
    // "c" and "t" are separate consonant spans (no vowels).
    assert(!m.empty());
}

void test_regex_digit_shorthand() {
    auto m = RegexEngine::findAll("\\d+", "score: 1234 and 56");
    assert(m.size() == 2);
    assert(m[0].length == 4 && m[1].length == 2);
}

void test_regex_word_shorthand() {
    auto m = RegexEngine::findAll("\\w+", "hello world");
    assert(m.size() == 2);
}

void test_regex_no_match() {
    auto m = RegexEngine::findAll("xyz", "hello world");
    assert(m.empty());
}

void test_regex_anchored_start() {
    auto m = RegexEngine::findAll("^hello", "hello world");
    assert(m.size() == 1 && m[0].col == 0);
    auto m2 = RegexEngine::findAll("^world", "hello world");
    assert(m2.empty());
}

void test_regex_anchored_end() {
    auto m = RegexEngine::findAll("world$", "hello world");
    assert(m.size() == 1);
    auto m2 = RegexEngine::findAll("hello$", "hello world");
    assert(m2.empty());
}

void test_regex_grouping() {
    // (ab)+ should match one or more repetitions of "ab".
    auto m = RegexEngine::findAll("(ab)+", "ababab");
    assert(!m.empty() && m[0].col == 0 && m[0].length == 6);
}

void test_regex_validity_check() {
    assert(RegexEngine::isValid("a+b"));
    assert(RegexEngine::isValid("[a-z]+"));
    // Empty pattern is trivially valid.
    assert(RegexEngine::isValid(""));
}

void test_regex_string_empty() {
    auto m = RegexEngine::findAll("abc", "");
    assert(m.empty());
}

// ============================================================================
// Benchmark: PieceTable vs GapBuffer across 4 workloads
//
// The four scenarios capture the trade-off between the two structures:
//
//   Scenario A — Sequential append (cursor always at end):
//     GapBuffer wins: O(k) insert with zero memory movement.
//
//   Scenario B — Random-position insert:
//     PieceTable wins: each insert is O(log n) fragment lookup + O(k) copy;
//     GapBuffer must move O(n) chars to reposition the gap.
//
//   Scenario C — Sequential erase:
//     GapBuffer wins: erase-at-gap is O(1).
//
//   Scenario D — Alternating front/back insert ("ping-pong"):
//     PieceTable wins: GapBuffer must traverse the entire document every other
//     insert; PieceTable never moves any text, just adds a piece record.
//
// Results are printed to stdout as a Markdown table.
// ============================================================================

struct BenchResult { std::string scenario; double ptMs; double gbMs; };

std::string winner(double ptMs, double gbMs) {
    if (ptMs < gbMs * 0.9) return "PieceTable";
    if (gbMs < ptMs * 0.9) return "GapBuffer";
    return "~tie~";
}

void run_benchmark(std::vector<BenchResult>& results) {
    constexpr int N = 50'000;

    // ── Scenario A: sequential append ────────────────────────────────────────
    {
        PieceTable pt;
        double ptMs = measure_ms([&]{ pt.insert(pt.size(), "a"); }, N);
        GapBuffer gb;
        double gbMs = measure_ms([&]{ gb.insert(gb.size(), "a"); }, N);
        results.push_back({"A: Sequential append", ptMs, gbMs});
        assert(pt.size() == N);
        assert(gb.size() == N);
    }

    // ── Scenario B: random-position insert ───────────────────────────────────
    {
        PieceTable pt;
        double ptMs = measure_ms([&]{
            size_t pos = (pt.size() == 0) ? 0 : (pt.size() / 2);
            pt.insert(pos, "a");
        }, N);

        GapBuffer gb;
        double gbMs = measure_ms([&]{
            size_t pos = (gb.size() == 0) ? 0 : (gb.size() / 2);
            gb.insert(pos, "a");
        }, N);
        results.push_back({"B: Random-position insert (mid)", ptMs, gbMs});
    }

    // ── Scenario C: sequential erase from front ───────────────────────────────
    {
        PieceTable pt;
        for (int i = 0; i < N; ++i) pt.insert(pt.size(), "a");
        double ptMs = measure_ms([&]{ if (pt.size() > 0) pt.erase(0, 1); }, N);

        GapBuffer gb;
        for (int i = 0; i < N; ++i) gb.insert(gb.size(), "a");
        double gbMs = measure_ms([&]{ if (gb.size() > 0) gb.erase(0, 1); }, N);
        results.push_back({"C: Sequential erase (from front)", ptMs, gbMs});
    }

    // ── Scenario D: alternating front/back insert (ping-pong) ────────────────
    {
        PieceTable pt;
        bool front = true;
        double ptMs = measure_ms([&]{
            pt.insert(front ? 0 : pt.size(), "a");
            front = !front;
        }, N);

        GapBuffer gb;
        front = true;
        double gbMs = measure_ms([&]{
            gb.insert(front ? 0 : gb.size(), "a");
            front = !front;
        }, N);
        results.push_back({"D: Alternating front/back (ping-pong)", ptMs, gbMs});
    }
}

void print_benchmark_table(const std::vector<BenchResult>& results) {
    // Print as a Markdown table.
    std::cout << "\n### PieceTable vs GapBuffer Benchmark ("
              << "N=50,000 operations each)\n\n";
    std::cout << "| Scenario                            |  PieceTable (ms) |  GapBuffer (ms) |  Winner      |\n";
    std::cout << "|-------------------------------------|:----------------:|:---------------:|:------------:|\n";
    for (auto& r : results) {
        std::ostringstream oss;
        oss << "| " << std::left << std::setw(37) << r.scenario
            << "| " << std::right << std::setw(16) << std::fixed << std::setprecision(2) << r.ptMs
            << " | " << std::setw(15) << r.gbMs
            << " | " << std::setw(12) << winner(r.ptMs, r.gbMs) << " |\n";
        std::cout << oss.str();
    }
    std::cout << "\n(Lower is faster. Winner declared when ≥10% faster.)\n\n";
}

// ============================================================================
// main
// ============================================================================

int main() {
    // ── PieceTable ────────────────────────────────────────────────────────────
    std::cout << "[PieceTable]\n";
    RUN_TEST(test_piece_table_insert);
    RUN_TEST(test_piece_table_erase);
    RUN_TEST(test_piece_table_midstream_insert_and_erase);

    // ── GapBuffer ─────────────────────────────────────────────────────────────
    std::cout << "\n[GapBuffer]\n";
    RUN_TEST(test_gapbuffer_append_sequential);
    RUN_TEST(test_gapbuffer_insert_at_beginning);
    RUN_TEST(test_gapbuffer_insert_in_middle);
    RUN_TEST(test_gapbuffer_erase_front);
    RUN_TEST(test_gapbuffer_erase_middle);
    RUN_TEST(test_gapbuffer_erase_then_insert);
    RUN_TEST(test_gapbuffer_charAt);
    RUN_TEST(test_gapbuffer_size);
    RUN_TEST(test_gapbuffer_empty_string_insert);
    RUN_TEST(test_gapbuffer_insert_past_end);

    // ── TextDocument ──────────────────────────────────────────────────────────
    std::cout << "\n[TextDocument]\n";
    RUN_TEST(test_document_insert_char);
    RUN_TEST(test_document_newline_split);
    RUN_TEST(test_document_merge_lines);
    RUN_TEST(test_document_line_starts_stay_correct_after_many_edits);
    RUN_TEST(test_row_col_to_offset);

    // ── TextEditor ────────────────────────────────────────────────────────────
    std::cout << "\n[TextEditor]\n";
    RUN_TEST(test_undo_insert);
    RUN_TEST(test_undo_then_redo);
    RUN_TEST(test_search_finds_all_matches);

    // ── DiffEngine / VersionHistory ───────────────────────────────────────────
    std::cout << "\n[DiffEngine / VersionHistory]\n";
    RUN_TEST(test_diff_detects_pure_addition);
    RUN_TEST(test_diff_detects_pure_removal);
    RUN_TEST(test_diff_identical_inputs_are_all_unchanged);
    RUN_TEST(test_version_history_diff_against_latest);

    // ── Selection / Clipboard ─────────────────────────────────────────────────
    std::cout << "\n[Selection / Clipboard]\n";
    RUN_TEST(test_copy_paste_round_trip);

    // ── SyntaxHighlighter ─────────────────────────────────────────────────────
    std::cout << "\n[SyntaxHighlighter]\n";
    RUN_TEST(test_syntax_cpp_keyword);
    RUN_TEST(test_syntax_cpp_line_comment);
    RUN_TEST(test_syntax_cpp_block_comment_span);
    RUN_TEST(test_syntax_cpp_string_literal);
    RUN_TEST(test_syntax_cpp_preprocessor);
    RUN_TEST(test_syntax_python_keyword);
    RUN_TEST(test_syntax_none_language_passthrough);
    RUN_TEST(test_syntax_tokens_cover_entire_line);
    RUN_TEST(test_syntax_language_detection);

    // ── RegexEngine ───────────────────────────────────────────────────────────
    std::cout << "\n[RegexEngine — Thompson NFA]\n";
    RUN_TEST(test_regex_literal_match);
    RUN_TEST(test_regex_dot_star);
    RUN_TEST(test_regex_plus_quantifier);
    RUN_TEST(test_regex_question_quantifier);
    RUN_TEST(test_regex_alternation);
    RUN_TEST(test_regex_character_class);
    RUN_TEST(test_regex_negated_class);
    RUN_TEST(test_regex_digit_shorthand);
    RUN_TEST(test_regex_word_shorthand);
    RUN_TEST(test_regex_no_match);
    RUN_TEST(test_regex_anchored_start);
    RUN_TEST(test_regex_anchored_end);
    RUN_TEST(test_regex_grouping);
    RUN_TEST(test_regex_validity_check);
    RUN_TEST(test_regex_string_empty);

    std::cout << "\n[Benchmarks]\n";
    std::vector<BenchResult> bench;
    run_benchmark(bench);
    print_benchmark_table(bench);

    std::cout << "All " << tests_passed << "/" << tests_run << " tests passed!\n";
    return 0;
}
