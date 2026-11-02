#pragma once

#include <cassert>
#include <coroutine>
#include <cstddef>
#include <deque>
#include <exception>
#include <new>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>


enum class CoroutineState {
    Empty,
    Created,
    Queued,
    Running,
    Suspended,
    Waiting,
    Completed,
    Failed,
};

enum class CoroutineError {
    None,
    InvalidDependency,
    ManagerMismatch,
    SelfDependency,
    DependencyCycle,
    DependencyDestroyed,
    AwaitedTaskFailed,
};


class CoroutineManager;

template<typename T>
class CoroutineTask;

struct CoroutineTaskControl {
    CoroutineManager* manager;
    std::coroutine_handle<> handle;
    CoroutineState state;
    CoroutineError error;
    CoroutineTaskControl* waiting_for;
    std::vector<CoroutineTaskControl*> dependents;
};

class CoroutineManager {
    struct alignas(std::max_align_t) FrameHeader {
        CoroutineManager* manager;
        std::size_t size;
    };

    std::deque<CoroutineTaskControl*> queue;
    std::size_t live_frame_count = 0;
    std::size_t allocation_count = 0;

    bool schedule(CoroutineTaskControl& control);
    bool wait_for(CoroutineTaskControl& dependent, CoroutineTaskControl& dependency);
    void complete(CoroutineTaskControl& control) noexcept;
    void fail(CoroutineTaskControl& control, CoroutineError error) noexcept;
    void cancel(CoroutineTaskControl& control) noexcept;
    void detach_dependency(CoroutineTaskControl& dependent) noexcept;
    void remove_from_queue(CoroutineTaskControl& control) noexcept;

    template<typename T>
    friend class CoroutineTask;

public:
    CoroutineManager() = default;
    CoroutineManager(const CoroutineManager&) = delete;
    CoroutineManager& operator=(const CoroutineManager&) = delete;
    ~CoroutineManager();

    void* allocate_frame(std::size_t size);
    static void deallocate_frame(void* frame) noexcept;

    template<typename T>
    bool schedule(CoroutineTask<T>& task);

    bool run_one();
    void run();

    std::size_t get_queued_task_count() const noexcept {
        return queue.size();
    }

    std::size_t get_live_frame_count() const noexcept {
        return live_frame_count;
    }

    std::size_t get_allocation_count() const noexcept {
        return allocation_count;
    }
};


template<typename T>
struct CoroutineResultStorage {
    std::optional<T> value;

    template<typename U>
    void return_value(U&& result) noexcept {
        value.emplace(std::forward<U>(result));
    }

    const T& get() const noexcept {
        assert(value.has_value());
        return *value;
    }

    T take() noexcept {
        assert(value.has_value());
        return std::move(*value);
    }
};

template<>
struct CoroutineResultStorage<void> {
    bool returned = false;

    void return_void() noexcept {
        returned = true;
    }

    void get() const noexcept {
        assert(returned);
    }
};


template<typename T>
class CoroutineTask {
public:
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    struct promise_type : CoroutineResultStorage<T> {
        CoroutineTaskControl control;

        template<typename... Args>
        explicit promise_type(CoroutineManager& manager, Args&&...) noexcept
            : control{
                &manager,
                {},
                CoroutineState::Created,
                CoroutineError::None,
                nullptr,
                {}
            } {}

        CoroutineTask get_return_object() noexcept {
            auto handle = handle_type::from_promise(*this);
            control.handle = handle;
            return CoroutineTask{handle};
        }

        std::suspend_always initial_suspend() const noexcept { return {}; }
        std::suspend_always final_suspend() const noexcept { return {}; }
        void unhandled_exception() noexcept { std::terminate(); }

        template<typename... Args>
        static void* operator new(
            std::size_t size,
            CoroutineManager& manager,
            Args&&...
        ) {
            return manager.allocate_frame(size);
        }

        static void operator delete(void* frame, std::size_t) noexcept {
            CoroutineManager::deallocate_frame(frame);
        }

        template<typename... Args>
        static void operator delete(
            void* frame,
            std::size_t,
            CoroutineManager&,
            Args&&...
        ) noexcept {
            CoroutineManager::deallocate_frame(frame);
        }
    };

    template<bool OwnsTask>
    class Awaiter {
        handle_type handle{};
        CoroutineTaskControl* dependent = nullptr;

    public:
        explicit Awaiter(handle_type handle) noexcept : handle(handle) {}
        Awaiter(const Awaiter&) = delete;
        Awaiter& operator=(const Awaiter&) = delete;

        Awaiter(Awaiter&& other) noexcept
            : handle(std::exchange(other.handle, {})),
              dependent(std::exchange(other.dependent, nullptr)) {}

        ~Awaiter() {
            if constexpr (OwnsTask) {
                CoroutineTask::destroy(handle);
            }
        }

        bool await_ready() const noexcept {
            return handle
                && handle.promise().control.state == CoroutineState::Completed;
        }

        template<typename Promise>
        bool await_suspend(std::coroutine_handle<Promise> parent) noexcept {
            dependent = &parent.promise().control;
            if (!handle) {
                dependent->manager->fail(
                    *dependent,
                    CoroutineError::InvalidDependency
                );
                return true;
            }
            return dependent->manager->wait_for(
                *dependent,
                handle.promise().control
            );
        }

        decltype(auto) await_resume() noexcept {
            assert(handle);
            assert(handle.promise().control.state == CoroutineState::Completed);
            if (dependent != nullptr) {
                dependent->manager->detach_dependency(*dependent);
            }

            if constexpr (std::is_void_v<T>) {
                handle.promise().get();
                return;
            } else if constexpr (OwnsTask) {
                return handle.promise().take();
            } else {
                return handle.promise().get();
            }
        }
    };

private:
    handle_type handle{};

    explicit CoroutineTask(handle_type handle) noexcept : handle(handle) {}

    static void destroy(handle_type handle) noexcept {
        if (!handle) {
            return;
        }

        auto& control = handle.promise().control;
        control.manager->cancel(control);
        handle.destroy();
    }

    void reset() noexcept {
        destroy(handle);
        handle = {};
    }

    friend class CoroutineManager;

public:
    CoroutineTask() = default;
    CoroutineTask(const CoroutineTask&) = delete;
    CoroutineTask& operator=(const CoroutineTask&) = delete;

    CoroutineTask(CoroutineTask&& other) noexcept
        : handle(std::exchange(other.handle, {})) {}

    CoroutineTask& operator=(CoroutineTask&& other) noexcept {
        if (this != &other) {
            reset();
            handle = std::exchange(other.handle, {});
        }
        return *this;
    }

    ~CoroutineTask() { reset(); }

    explicit operator bool() const noexcept { return static_cast<bool>(handle); }

    CoroutineState state() const noexcept {
        if (!handle) {
            return CoroutineState::Empty;
        }
        return handle.promise().control.state;
    }

    CoroutineError error() const noexcept {
        if (!handle) {
            return CoroutineError::InvalidDependency;
        }
        return handle.promise().control.error;
    }

    bool done() const noexcept {
        return state() == CoroutineState::Completed;
    }

    bool failed() const noexcept {
        return state() == CoroutineState::Failed;
    }

    decltype(auto) result() const noexcept {
        assert(done());
        return handle.promise().get();
    }

    Awaiter<false> operator co_await() & noexcept {
        return Awaiter<false>{handle};
    }

    Awaiter<true> operator co_await() && noexcept {
        return Awaiter<true>{std::exchange(handle, {})};
    }
};


template<typename T>
bool CoroutineManager::schedule(CoroutineTask<T>& task) {
    if (!task.handle) {
        return false;
    }
    return schedule(task.handle.promise().control);
}


void test_co();
