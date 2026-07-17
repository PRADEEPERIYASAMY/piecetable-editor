#include "AutosaveWorker.hpp"

AutosaveWorker::AutosaveWorker(std::mutex& documentMutex,
                                 SnapshotProvider snapshotProvider,
                                 DiskWriter diskWriter,
                                 SaveNotifier onSaved,
                                 std::chrono::seconds interval)
    : documentMutex_(documentMutex),
      snapshotProvider_(std::move(snapshotProvider)),
      diskWriter_(std::move(diskWriter)),
      onSaved_(std::move(onSaved)),
      interval_(interval) {}

AutosaveWorker::~AutosaveWorker() {
    stop();
}

void AutosaveWorker::start() {
    if (running_.exchange(true)) return; // already running
    worker_ = std::thread(&AutosaveWorker::run, this);
}

void AutosaveWorker::stop() {
    if (!running_.exchange(false)) return; // wasn't running
    wakeSignal_.notify_all();
    if (worker_.joinable()) worker_.join();
}

void AutosaveWorker::notifyDirty() {
    dirty_.store(true);
}

void AutosaveWorker::run() {
    while (running_.load()) {
        {
            std::unique_lock<std::mutex> wakeLock(wakeMutex_);
            wakeSignal_.wait_for(wakeLock, interval_, [this] { return !running_.load(); });
        }
        if (!running_.load()) break;
        if (!dirty_.exchange(false)) continue; // nothing changed since last save

        // Hold the document lock only long enough to copy the text out —
        // the disk write below happens unlocked so it never blocks typing.
        std::string snapshot;
        {
            std::lock_guard<std::mutex> lock(documentMutex_);
            snapshot = snapshotProvider_();
        }

        if (diskWriter_(snapshot) && onSaved_) {
            onSaved_(snapshot);
        }
    }
}
