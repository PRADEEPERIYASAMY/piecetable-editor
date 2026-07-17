#include "DiffEngine.hpp"
#include <vector>

std::vector<std::string> DiffEngine::splitIntoLines(const std::string& text) {
    std::vector<std::string> lines;
    std::string current;
    for (char c : text) {
        if (c == '\n') {
            lines.push_back(current);
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty()) lines.push_back(current);
    return lines;
}

std::vector<DiffLine> DiffEngine::diffLines(const std::vector<std::string>& oldLines,
                                             const std::vector<std::string>& newLines) {
    size_t n = oldLines.size();
    size_t m = newLines.size();

    // lcsLength[i][j] = length of the LCS of oldLines[i:] and newLines[j:]
    std::vector<std::vector<int>> lcsLength(n + 1, std::vector<int>(m + 1, 0));
    for (size_t i = n; i-- > 0;) {
        for (size_t j = m; j-- > 0;) {
            if (oldLines[i] == newLines[j]) {
                lcsLength[i][j] = lcsLength[i + 1][j + 1] + 1;
            } else {
                lcsLength[i][j] = std::max(lcsLength[i + 1][j], lcsLength[i][j + 1]);
            }
        }
    }

    // Backtrace to reconstruct the edit script.
    std::vector<DiffLine> result;
    size_t i = 0, j = 0;
    while (i < n && j < m) {
        if (oldLines[i] == newLines[j]) {
            result.push_back({DiffOp::Unchanged, oldLines[i]});
            ++i; ++j;
        } else if (lcsLength[i + 1][j] >= lcsLength[i][j + 1]) {
            result.push_back({DiffOp::Removed, oldLines[i]});
            ++i;
        } else {
            result.push_back({DiffOp::Added, newLines[j]});
            ++j;
        }
    }
    while (i < n) { result.push_back({DiffOp::Removed, oldLines[i]}); ++i; }
    while (j < m) { result.push_back({DiffOp::Added, newLines[j]}); ++j; }

    return result;
}
