#include "VersionHistory.hpp"

void VersionHistory::recordSnapshot(const std::string& content) {
    snapshots_.push_back({std::time(nullptr), content});
}

std::vector<DiffLine> VersionHistory::diffAgainstLatest(const std::string& currentContent) const {
    if (snapshots_.empty()) {
        // No history yet — everything currently present is "new".
        std::vector<DiffLine> result;
        for (auto& line : DiffEngine::splitIntoLines(currentContent))
            result.push_back({DiffOp::Added, line});
        return result;
    }
    auto oldLines = DiffEngine::splitIntoLines(snapshots_.back().content);
    auto newLines = DiffEngine::splitIntoLines(currentContent);
    return DiffEngine::diffLines(oldLines, newLines);
}

std::vector<DiffLine> VersionHistory::diffBetween(size_t olderIndex, size_t newerIndex) const {
    auto oldLines = DiffEngine::splitIntoLines(snapshots_.at(olderIndex).content);
    auto newLines = DiffEngine::splitIntoLines(snapshots_.at(newerIndex).content);
    return DiffEngine::diffLines(oldLines, newLines);
}
