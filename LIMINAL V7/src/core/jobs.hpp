// Einfacher Thread-Pool fuer rechenintensive Hintergrundarbeit
// (Weltgenerierung, Chunk-Meshing, Texturerzeugung, Klangsynthese).
//
// Aufgaben laufen auf Arbeitsthreads; Ergebnisse, die den Hauptthread betreffen
// (z. B. GPU-Uploads), werden ueber Completion-Callbacks in eine Warteschlange
// gelegt und dort von drainCompletions() ausgefuehrt.
#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace lim {

class JobSystem {
public:
    explicit JobSystem(unsigned workers = 0);  // 0 = automatisch
    ~JobSystem();
    JobSystem(const JobSystem&) = delete;
    JobSystem& operator=(const JobSystem&) = delete;

    // Hohe Prioritaet kommt an den Anfang der Warteschlange.
    void submit(std::function<void()> job, bool highPriority = false);
    // Auf dem Hauptthread ausfuehren (wird aus Jobs heraus aufgerufen).
    void completeOnMain(std::function<void()> fn);
    // Fuehrt wartende Hauptthread-Aufgaben aus; maxMs begrenzt die Zeit (0 = alle).
    int drainCompletions(double maxMs = 0.0);
    // Alle Jobs abarbeiten lassen (hilft selbst mit).
    void waitIdle();
    // Parallele Schleife ueber [0, n): verteilt Bloecke auf alle Threads und wartet.
    void parallelFor(int n, const std::function<void(int)>& fn);

    unsigned workerCount() const { return (unsigned)threads_.size(); }
    int pending() const { return pending_.load(); }

private:
    void workerLoop();
    bool runOne();

    std::vector<std::thread> threads_;
    std::deque<std::function<void()>> queue_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::condition_variable idleCv_;
    bool stop_ = false;
    std::atomic<int> pending_{0};

    std::mutex doneMutex_;
    std::deque<std::function<void()>> done_;
};

}  // namespace lim
