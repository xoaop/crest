#pragma once

#include <vector>
#include <thread>
#include <queue>
#include <future>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <type_traits>
#include <utility>
#include <stdexcept>

struct ThreadPool {
    explicit ThreadPool(size_t count_threads);

    void shutdown_gracefully();
    void shutdown_immediately();

    ~ThreadPool();

    template <class F, class... Args>
    auto submit(F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>>
    {
        using R = std::invoke_result_t<F, Args...>;

        auto task_ptr = std::make_shared<std::packaged_task<R()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        std::future<R> fut = task_ptr->get_future();
        enqueue_task([task_ptr] {
            (*task_ptr)();
        });
        return fut;
    }

private:
    void worker_loop();
    void enqueue_task(std::function<void()> job);
    void request_stop(bool clear_pending_tasks);
    void join_all();

    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;

    std::mutex mtx;
    std::condition_variable cv;
    bool stopping;
};
