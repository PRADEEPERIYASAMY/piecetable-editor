#pragma once
#include <string>
#include <vector>
#include <ctime>
#include "DiffEngine.hpp"

// ============================================================================
// VersionHistory
//
// Keeps a snapshot of the document's full text every time it's saved.
// Note: We store full snapshots rather than compressed reverse-diffs.
// Reverse-diff storage saves memory for very large histories (like Git),
// but adds significant complexity for patch application and conflict handling.
// Since an editor buffer typically only sees a few dozen saves per session
// and file sizes are manageable, full snapshots are a much safer and simpler
// tradeoff here.
// ============================================================================

struct Snapshot {
    std::time_t savedAt;
    std::string content;
};

class VersionHistory {
public:
    void recordSnapshot(const std::string& content);

    size_t snapshotCount() const { return snapshots_.size(); }
    const Snapshot& snapshotAt(size_t index) const { return snapshots_.at(index); }

    // Diff between the most recent snapshot and the current live content.
    std::vector<DiffLine> diffAgainstLatest(const std::string& currentContent) const;

    // Diff between two historical snapshots.
    std::vector<DiffLine> diffBetween(size_t olderIndex, size_t newerIndex) const;

    bool hasHistory() const { return !snapshots_.empty(); }

private:
    std::vector<Snapshot> snapshots_;
};
