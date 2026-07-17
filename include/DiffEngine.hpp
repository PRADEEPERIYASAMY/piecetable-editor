#pragma once
#include <string>
#include <vector>

// ============================================================================
// DiffEngine
//
// Line-based diff using the classic Longest Common Subsequence dynamic
// program (O(n*m) time and space over line counts).
// We use the textbook LCS formulation rather than the more complex Myers
// O(ND) algorithm. For a text editor's save-history feature, files rarely
// exceed a few thousand lines in a single diff. The simpler LCS DP performs
// perfectly well at this scale, making the added complexity of Myers
// unnecessary for our use case.
// ============================================================================

enum class DiffOp { Unchanged, Added, Removed };

struct DiffLine {
    DiffOp op;
    std::string text;
};

class DiffEngine {
public:
    // Computes a line-by-line diff transforming `oldLines` into `newLines`.
    static std::vector<DiffLine> diffLines(const std::vector<std::string>& oldLines,
                                            const std::vector<std::string>& newLines);

    static std::vector<std::string> splitIntoLines(const std::string& text);
};
