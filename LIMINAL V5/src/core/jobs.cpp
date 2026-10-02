#include "core/jobs.hpp"

#include <algorithm>
#include <chrono>

#include "core/log.hpp"

namespace lim {

JobSystem::JobSystem(unsigned workers) {
    if (workers == 0) {
        unsigned hw = std::max(2u, std::thread::hardware_concurrency());
        // Hauptthread und Audiothread behalten je einen Kern.
        workers = std::clamp(hw - 2, 1u, 12u);
    }
    for (unsigned i = 0; i < workers; ++i) threads_.emplace_back([this] { workerLoop(); });
    log::info("JobSystem: {} Arbeitsthreads", workers);
}

JobSystem::~JobSystem() {
    {
        std::lock_guard lock(mutex_);
        stop_ = true;
    }
    cv_.notify_all();
    for (auto& t : threads_) t.join();
}

void JobSystem::submit(std::function<void()> job, bool highPriority) {
    pending_.fetch_add(1);
    {
        std::lock_guard lock(mutex_);
        if (highPriority) queue_.push_front(std::move(job));
        else queue_.push_back(std::move(job));
    }
    cv_.notify_one();
}

void JobSystem::completeOnMain(std::function<void()> fn) {
    std::lock_guard lock(doneMutex_);
    done_.push_back(std::move(fn));
}

int JobSystem::drainCompletions(double maxMs) {
    auto t0 = std::chrono::steady_clock::now();
    int n = 0;
    while (true) {
        std::function<void()> fn;
        {
            std::lock_guard lock(doneMutex_);
            if (done_.empty()) break;
            fn = std::move(done_.front());
            done_.pop_front();
        }
        fn();
        ++n;
        if (maxMs > 0.0) {
            double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
            if (ms > maxMs) break;
        }
    }
    return n;
}

bool JobSystem::runOne() {
    std::function<void()> job;
    {
        std::lock_guard lock(mutex_);
        if (queue_.empty()) return false;
        job = std::move(queue_.front());
        queue_.pop_front();
    }
    job();
    if (pending_.fetch_sub(1) == 1) {
        std::lock_guard lock(mutex_);
        idleCv_.notify_all();
    }
    return true;
}

void JobSystem::workerLoop() {
    while (true) {
        std::function<void()> job;
        {
            std::unique_lock lock(mutex_);
            cv_.wait(lock, [this] { return stop_ || !queue_.empty(); });
            if (stop_ && queue_.empty()) return;
            job = std::move(queue_.front());
            queue_.pop_front();
        }
        job();
        if (pending_.fetch_sub(1) == 1) {
            std::lock_guard lock(mutex_);
            idleCv_.notify_all();
        }
    }
}

void JobSystem::waitIdle() {
    while (runOne()) {
    }
    std::unique_lock lock(mutex_);
    idleCv_.wait(lock, [this] { return pending_.load() == 0; });
}

void JobSystem::parallelFor(int n, const std::function<void(int)>& fn) {
    if (n <= 0) return;
    // Der Zustand liegt im Heap: Hilfsjobs koennen noch starten, nachdem alle
    // Indizes erledigt sind und diese Funktion bereits zurueckgekehrt ist. fn wird
    // nur fuer i < n aufgerufen - dann wartet der Aufrufer garantiert noch.
    struct State {
        std::atomic<int> next{0};
        std::atomic<int> remaining{0};
        std::mutex m;
        std::condition_variable done;
    };
    auto st = std::make_shared<State>();
    st->remaining = n;
    const auto* f = &fn;
    auto body = [st, f, n] {
        int i;
        while ((i = st->next.fetch_add(1)) < n) {
            (*f)(i);
            if (st->remaining.fetch_sub(1) == 1) {
                std::lock_guard lock(st->m);
                st->done.notify_all();
            }
        }
    };
    int helpers = std::min<int>((int)threads_.size(), n - 1);
    for (int k = 0; k < helpers; ++k) submit(body, true);
    body();
    std::unique_lock lock(st->m);
    st->done.wait(lock, [&] { return st->remaining.load() == 0; });
}

}  // namespace lim
