#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace geometry {

/**
 * @brief Minimal fixed-size thread pool for geometry background jobs.
 */
class GeometryThreadPool {
public:
    /**
     * @brief Constructs a pool with worker_count workers.
     * @param worker_count Number of workers. Uses 1 when worker_count is 0.
     */
    explicit GeometryThreadPool(uint32_t worker_count);

    GeometryThreadPool(const GeometryThreadPool&) = delete;
    GeometryThreadPool& operator=(const GeometryThreadPool&) = delete;

    /**
     * @brief Joins workers and drains queued jobs.
     */
    ~GeometryThreadPool();

    /**
     * @brief Enqueues a single job for async execution.
     * @param job Function to execute.
     * @return True when queued successfully.
     */
    bool enqueue(std::function<void()> job);

private:
    void worker_loop();

    std::vector<std::thread> workers_{};
    std::queue<std::function<void()>> jobs_{};
    std::mutex mutex_{};
    std::condition_variable condition_{};
    std::atomic<bool> stopping_{false};
};

}  // namespace geometry
