#pragma once
#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <functional>

// ============================================================================
// AutosaveWorker
//
// Runs on its own thread and periodically writes the document to disk so a
// crash or accidental quit doesn't lose work — without ever blocking the
// thread the user is typing on.
//
// Concurrency design:
//   - The main (UI/input) thread owns the document and calls
//     `notifyDirty()` after edits; this only flips an atomic flag, so it's
//     effectively free and never blocks typing.
//   - The worker thread wakes on its own timer (or when notified), and when
//     it decides to save, calls the injected `snapshotProvider` callback
//     *while holding `documentMutex`* just long enough to copy the current
//     text out. The actual disk write happens *after* releasing the lock,
//     so slow I/O never stalls the editor.
//   - `documentMutex` is the single lock protecting any state shared
//     between the UI thread and this worker; callers must take it
//     whenever they touch the buffer from more than one thread's logic.
// ============================================================================
class AutosaveWorker {
public:
    using SnapshotProvider = std::function<std::string()>;
    using DiskWriter = std::function<bool(const std::string& content)>;
    using SaveNotifier = std::function<void(const std::string& savedContent)>;

    AutosaveWorker(std::mutex& documentMutex,
                    SnapshotProvider snapshotProvider,
                    DiskWriter diskWriter,
                    SaveNotifier onSaved,
                    std::chrono::seconds interval = std::chrono::seconds(10));
    ~AutosaveWorker();

    void start();
    void stop();
    void notifyDirty();

private:
    void run();

    std::mutex& documentMutex_;
    SnapshotProvider snapshotProvider_;
    DiskWriter diskWriter_;
    SaveNotifier onSaved_;
    std::chrono::seconds interval_;

    std::thread worker_;
    std::atomic<bool> running_{false};
    std::atomic<bool> dirty_{false};

    std::mutex wakeMutex_;
    std::condition_variable wakeSignal_;
};
