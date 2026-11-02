#include "coroutine.hpp"

#include <print>
#include "common.hpp"

CoroutineManager::~CoroutineManager() {
    assert(queue.empty());
    assert(live_frame_count == 0);
}

void* CoroutineManager::allocate_frame(std::size_t size) {
    auto* header = static_cast<FrameHeader*>(
        ::operator new(sizeof(FrameHeader) + size)
    );
    ::new (header) FrameHeader{this, size};
    ++live_frame_count;
    ++allocation_count;
    return header + 1;
}

void CoroutineManager::deallocate_frame(void* frame) noexcept {
    if (frame == nullptr) {
        return;
    }

    auto* header = static_cast<FrameHeader*>(frame) - 1;
    assert(header->manager->live_frame_count > 0);
    --header->manager->live_frame_count;
    header->~FrameHeader();
    ::operator delete(header);
}

bool CoroutineManager::schedule(CoroutineTaskControl& control) {
    if (control.manager != this) {
        return false;
    }
    if (control.state != CoroutineState::Created
        && control.state != CoroutineState::Suspended) {
        return false;
    }

    control.state = CoroutineState::Queued;
    queue.push_back(&control);
    return true;
}

bool CoroutineManager::wait_for(
    CoroutineTaskControl& dependent,
    CoroutineTaskControl& dependency
) {
    assert(dependent.manager == this);
    assert(dependent.state == CoroutineState::Running);
    assert(dependent.waiting_for == nullptr);

    if (dependency.manager != this) {
        fail(dependent, CoroutineError::ManagerMismatch);
        return true;
    }
    if (&dependent == &dependency) {
        fail(dependent, CoroutineError::SelfDependency);
        return true;
    }
    if (dependency.state == CoroutineState::Empty) {
        fail(dependent, CoroutineError::InvalidDependency);
        return true;
    }
    if (dependency.state == CoroutineState::Failed) {
        fail(dependent, CoroutineError::AwaitedTaskFailed);
        return true;
    }
    if (dependency.state == CoroutineState::Completed) {
        return false;
    }

    for (CoroutineTaskControl* current = &dependency;
         current != nullptr;
         current = current->waiting_for) {
        if (current == &dependent) {
            fail(dependent, CoroutineError::DependencyCycle);
            return true;
        }
    }

    dependent.waiting_for = &dependency;
    dependency.dependents.push_back(&dependent);
    dependent.state = CoroutineState::Waiting;

    if (dependency.state == CoroutineState::Created
        || dependency.state == CoroutineState::Suspended) {
        bool scheduled = schedule(dependency);
        assert(scheduled);
    }
    return true;
}

void CoroutineManager::complete(CoroutineTaskControl& control) noexcept {
    assert(control.handle.done());
    assert(control.waiting_for == nullptr);

    control.state = CoroutineState::Completed;
    control.error = CoroutineError::None;

    for (CoroutineTaskControl* dependent : control.dependents) {
        assert(dependent->waiting_for == &control);
        assert(dependent->state == CoroutineState::Waiting);
        dependent->state = CoroutineState::Queued;
        queue.push_back(dependent);
    }
}

void CoroutineManager::fail(
    CoroutineTaskControl& control,
    CoroutineError error
) noexcept {
    if (control.state == CoroutineState::Empty
        || control.state == CoroutineState::Completed
        || control.state == CoroutineState::Failed) {
        return;
    }

    remove_from_queue(control);
    detach_dependency(control);
    control.state = CoroutineState::Failed;
    control.error = error;

    while (!control.dependents.empty()) {
        CoroutineTaskControl* dependent = control.dependents.back();
        control.dependents.pop_back();
        assert(dependent->waiting_for == &control);
        dependent->waiting_for = nullptr;
        fail(*dependent, CoroutineError::AwaitedTaskFailed);
    }
}

void CoroutineManager::cancel(CoroutineTaskControl& control) noexcept {
    assert(control.state != CoroutineState::Running);

    remove_from_queue(control);
    detach_dependency(control);

    while (!control.dependents.empty()) {
        CoroutineTaskControl* dependent = control.dependents.back();
        control.dependents.pop_back();
        assert(dependent->waiting_for == &control);
        dependent->waiting_for = nullptr;
        fail(*dependent, CoroutineError::DependencyDestroyed);
    }

    control.state = CoroutineState::Empty;
}

void CoroutineManager::detach_dependency(
    CoroutineTaskControl& dependent
) noexcept {
    CoroutineTaskControl* dependency = dependent.waiting_for;
    if (dependency == nullptr) {
        return;
    }

    for (auto it = dependency->dependents.begin();
         it != dependency->dependents.end();
         ++it) {
        if (*it == &dependent) {
            dependency->dependents.erase(it);
            dependent.waiting_for = nullptr;
            return;
        }
    }
    assert(false);
}

void CoroutineManager::remove_from_queue(
    CoroutineTaskControl& control
) noexcept {
    if (control.state != CoroutineState::Queued) {
        return;
    }

    for (auto it = queue.begin(); it != queue.end(); ++it) {
        if (*it == &control) {
            queue.erase(it);
            return;
        }
    }
    assert(false);
}

bool CoroutineManager::run_one() {
    if (queue.empty()) {
        return false;
    }

    CoroutineTaskControl* control = queue.front();
    queue.pop_front();

    assert(control->state == CoroutineState::Queued);
    assert(!control->handle.done());

    control->state = CoroutineState::Running;
    control->handle.resume();

    if (control->handle.done()) {
        complete(*control);
    } else if (control->state == CoroutineState::Running) {
        control->state = CoroutineState::Suspended;
    } else {
        assert(control->state == CoroutineState::Waiting
            || control->state == CoroutineState::Failed);
    }
    return true;
}

void CoroutineManager::run() {
    while (run_one()) {}
}


static CoroutineTask<int> value_task(CoroutineManager& manager, int value) {
    co_return value;
}

static CoroutineTask<int> add_temporary(CoroutineManager& manager) {
    int value = co_await value_task(manager, 41);
    co_return value + 1;
}

static CoroutineTask<int> add_one(
    CoroutineManager& manager,
    CoroutineTask<int>& dependency
) {
    int value = co_await dependency;
    co_return value + 1;
}

static CoroutineTask<int> indirect_dependency(
    CoroutineManager& manager,
    CoroutineTask<int>** dependency
) {
    int value = co_await **dependency;
    co_return value;
}

static CoroutineTask<void> consume_value(
    CoroutineManager& manager,
    CoroutineTask<int>& dependency,
    int& output
) {
    output = co_await dependency;
    co_return;
}

void test_co() {
    CoroutineManager manager;

    {
        auto task = add_temporary(manager);
        assert(manager.schedule(task));
        manager.run();

        assert(task.done());
        assert(task.result() == 42);
        assert(manager.get_live_frame_count() == 1);
        std::println("temporary: result={}, live_frames={}",
            task.result(),
            manager.get_live_frame_count()
        );
    }
    assert(manager.get_live_frame_count() == 0);

    {
        auto leaf = value_task(manager, 40);
        auto middle = add_one(manager, leaf);
        auto root = add_one(manager, middle);

        assert(manager.schedule(root));
        manager.run();

        assert(leaf.result() == 40);
        assert(middle.result() == 41);
        assert(root.result() == 42);
        std::println("chain: root={}, state={}",
            root.result(),
            to_string(root.state())
        );
    }

    {
        auto child = value_task(manager, 7);
        int first = 0;
        int second = 0;
        auto first_parent = consume_value(manager, child, first);
        auto second_parent = consume_value(manager, child, second);

        assert(manager.schedule(first_parent));
        assert(manager.schedule(second_parent));
        manager.run();

        assert(first_parent.done());
        assert(second_parent.done());
        assert(first == 7);
        assert(second == 7);
        std::println("shared: first={}, second={}", first, second);
    }

    {
        auto child = value_task(manager, 1);
        auto parent = add_one(manager, child);

        assert(manager.schedule(parent));
        assert(manager.run_one());
        assert(parent.state() == CoroutineState::Waiting);
        child = {};

        assert(parent.failed());
        assert(parent.error() == CoroutineError::DependencyDestroyed);
        std::println("destroyed dependency: error={}",
            to_string(parent.error())
        );
    }

    {
        CoroutineTask<int>* second_pointer = nullptr;
        auto first = indirect_dependency(manager, &second_pointer);
        CoroutineTask<int>* first_pointer = &first;
        auto second = indirect_dependency(manager, &first_pointer);
        second_pointer = &second;

        assert(manager.schedule(first));
        manager.run();

        assert(first.failed());
        assert(second.failed());
        assert(first.error() == CoroutineError::AwaitedTaskFailed);
        assert(second.error() == CoroutineError::DependencyCycle);
        std::println("cycle: first={}, second={}",
            to_string(first.error()),
            to_string(second.error())
        );
    }

    {
        CoroutineManager other_manager;
        auto child = value_task(other_manager, 1);
        auto parent = add_one(manager, child);

        assert(manager.schedule(parent));
        manager.run();

        assert(parent.failed());
        assert(parent.error() == CoroutineError::ManagerMismatch);
        assert(child.state() == CoroutineState::Created);
        assert(other_manager.get_queued_task_count() == 0);
        std::println("manager mismatch: error={}",
            to_string(parent.error())
        );
    }

    assert(manager.get_queued_task_count() == 0);
    assert(manager.get_live_frame_count() == 0);
    std::println("destroyed: live_frames={}, allocations={}",
        manager.get_live_frame_count(),
        manager.get_allocation_count()
    );
}
