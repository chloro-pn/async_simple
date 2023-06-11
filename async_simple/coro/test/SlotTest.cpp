/*
 * Copyright (c) 2022, Alibaba Group Holding Limited;
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "async_simple/coro/Lazy.h"
#include "async_simple/coro/Slot.h"
#include "async_simple/coro/SyncAwait.h"
#include "async_simple/executors/SimpleExecutor.h"
#include "async_simple/test/unittest.h"

#include <thread>

namespace async_simple::coro {

struct SlotTest {
    bool await_ready() { return false; }

    bool await_suspend(SlotNotifier<int>&& notifier) {
        std::thread([notifier = std::move(notifier)]() mutable -> void {
            notifier.SetValue(10);
            std::move(notifier).resumeInCoAwaitThread();
        }).detach();
        return true;
    }

    int await_resume() { return v; }

    void setValue(int i) { v = i; }

    int v;
};

struct SlotTestVoid {
    bool await_ready() { return false; }

    bool await_suspend(SlotNotifier<void>&& notifier) {
        std::thread([notifier = std::move(notifier)]() mutable -> void {
            std::move(notifier).resumeInRandomThread();
        }).detach();
        return true;
    }

    void await_resume() {}
};

struct SlotWithExecutor {
    SlotWithExecutor(Executor* ex, bool dispatch)
        : ex_(ex), dispatch_(dispatch) {}

    bool await_ready() { return false; }

    bool await_suspend(SlotNotifier<void>&& notifier) {
        std::thread([notifier = std::move(notifier), this]() mutable -> void {
            if (dispatch_ == true) {
                std::move(notifier).resumeInSpecifiedExecutor(ex_);
            } else {
                std::move(notifier).resumeInRandomThread();
            }
        }).detach();
        return true;
    }

    void await_resume() {}

    Executor* ex_;
    bool dispatch_;
};

TEST(SlotTest, testSlot) {
    async_simple::executors::SimpleExecutor ex(2);
    async_simple::executors::SimpleExecutor ex2(2);
    auto task = [&]() -> Lazy<> {
        int v = co_await Slot<SlotTest>();
        EXPECT_EQ(v, 10);
        co_await Slot<SlotTestVoid>();
        co_await Slot<SlotWithExecutor>(&ex2, true);
        Executor* ex = co_await CurrentExecutor{};
        EXPECT_EQ(ex, &ex2);
    };
    syncAwait(task().via(&ex));
}

}  // namespace async_simple::coro
