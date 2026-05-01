#include "thread_pool.hpp"

namespace {
    
void clear_task_queue(std::queue<std::function<void()>>& tasks) {
    std::queue<std::function<void()>> empty;
    std::swap(tasks, empty);
}

}

ThreadPool::ThreadPool(size_t count_threads) : stopping(false) {
    workers.reserve(count_threads);
    for(size_t i = 0; i < count_threads; ++i) {
        workers.emplace_back([this] {
            worker_loop();
        });
    }
}

void ThreadPool::shutdown_gracefully() {
    request_stop(false);
    join_all();
}

void ThreadPool::shutdown_immediately() {
    request_stop(true);
    join_all();
}

ThreadPool::~ThreadPool() {
    request_stop(false);
    join_all();
}

void ThreadPool::worker_loop() {
    for(;;) {
        std::function<void()> job;

        {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [this] {
                return stopping || !tasks.empty();
            });

            if(stopping && tasks.empty()) {
                return;
            }

            job = std::move(tasks.front());
            tasks.pop();
        }

        job();
    }
}

void ThreadPool::enqueue_task(std::function<void()> job) {
    {
        std::lock_guard<std::mutex> lock(mtx);
        if(stopping) {
            throw std::runtime_error("submit on stopped ThreadPool");
        }
        tasks.emplace(std::move(job));
    }

    cv.notify_one();
}

void ThreadPool::request_stop(bool clear_pending_tasks) {
    {
        std::lock_guard<std::mutex> lock(mtx);
        if(clear_pending_tasks) {
            clear_task_queue(tasks);
        }

        if(stopping) {
            return;
        }

        stopping = true;
    }

    cv.notify_all();
}

void ThreadPool::join_all() {
    for(auto& t : workers) {
        if(t.joinable()) {
            t.join();
        }
    }
}
