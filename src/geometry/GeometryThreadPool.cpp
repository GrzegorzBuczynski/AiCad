#include "geometry/GeometryThreadPool.hpp"

#include <algorithm>

namespace geometry {

GeometryThreadPool::GeometryThreadPool(uint32_t worker_count) {
    const uint32_t safe_worker_count = std::max(worker_count, 1U);
    workers_.reserve(safe_worker_count);
    for (uint32_t i = 0; i < safe_worker_count; ++i) {
        workers_.emplace_back([this]() { worker_loop(); });
    }
}

GeometryThreadPool::~GeometryThreadPool() {
    stopping_.store(true);
    condition_.notify_all();
    for (std::thread& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

bool GeometryThreadPool::enqueue(std::function<void()> job) {
    if (!job) {
        return false;
    }

    {
        std::scoped_lock lock(mutex_);
        if (stopping_.load()) {
            return false;
        }
        jobs_.push(std::move(job));
    }

    condition_.notify_one();
    return true;
}

void GeometryThreadPool::worker_loop() {
    while (true) {
        std::function<void()> job{};
        {
            std::unique_lock lock(mutex_);
            condition_.wait(lock, [this]() {
                return stopping_.load() || !jobs_.empty();
            });

            if (stopping_.load() && jobs_.empty()) {
                return;
            }

            job = std::move(jobs_.front());
            jobs_.pop();
        }

        if (job) {
            job();
        }
    }
}

}  // namespace geometry
