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

#include "async_simple/Future.h"
#include "async_simple/Promise.h"
#include "async_simple/coro/AwaiterInfoCollector.h"
#include "async_simple/coro/AwaiterProxy.h"
#include "async_simple/coro/Dispatch.h"
#include "async_simple/coro/FutureAwaiter.h"
#include "async_simple/coro/Lazy.h"
#include "async_simple/coro/SyncAwait.h"
#include "async_simple/executors/SimpleExecutor.h"

#include <thread>
#include "async_simple/test/unittest.h"

namespace async_simple::coro {

inline const char* AwaiterInfoTypeToCStr(
    AwaiterInfoType awaiter_info_type) noexcept {
    switch (awaiter_info_type) {
        case AwaiterInfoType::AWAITER_READY_TRUE: {
            return "awaiter_ready_true";
        }
        case AwaiterInfoType::AWAITER_SUSPEND_FALSE: {
            return "awaiter_suspend_false";
        }
        case AwaiterInfoType::AWAITER_SUSPEND_START: {
            return "awaiter_suspend_start";
        }
        case AwaiterInfoType::AWAITER_SUSPEND_RESUME: {
            return "awaiter_suspend_resume";
        }
    }
    return "invalid awaiter info type";
}

class SimpleAwaiterInfoCollector : public AwaiterInfoCollector {
public:
    void collect(AwaiterInfoType info_type, Executor* ex, uint64_t id,
                 const std::string& desc,
                 const std::source_location& location) override {
        Event event;
        event.time_point_ = std::chrono::system_clock::to_time_t(
            std::chrono::system_clock::now());
        event.info_type_ = info_type;
        event.ex_ = ex;
        event.id_ = id;
        event.desc_ = desc;
        event.location_ = location;
        events_.emplace_back(std::move(event));
    }

    void Print() {
        std::cout << " print begin" << std::endl;
        for (auto it = events_.begin(); it != events_.end(); ++it) {
            std::cout << " event : " << it->id_ << " "
                      << AwaiterInfoTypeToCStr(it->info_type_) << " "
                      << it->desc_ << " " << it->ex_ << " " << it->time_point_
                      << " " << it->location_.file_name() << " "
                      << it->location_.line() << "\n";
        }
        std::cout << " print end" << std::endl;
    }

public:
    struct Event {
        time_t time_point_;
        AwaiterInfoType info_type_;
        Executor* ex_;
        uint64_t id_;
        std::string desc_;
        std::source_location location_;
    };

    std::vector<Event> events_;
};

TEST(AwaiterProxyTest, testAwaiterProxy) {
    async_simple::executors::SimpleExecutor ex1(2);
    async_simple::executors::SimpleExecutor ex2(2);
    SimpleAwaiterInfoCollector ac;
    auto subtask = [&]() -> Lazy<int> { co_return 2; };
    auto task = [&]() -> Lazy<> {
        Executor* ex = co_await CurrentExecutor{};
        EXPECT_EQ(ex, &ex1);
        co_await AwaiterProxy(dispatch(&ex2), ac, "dispatch_awaiter");
        ex = co_await CurrentExecutor{};
        EXPECT_EQ(ex, &ex2);
        int tmp = co_await AwaiterProxy(subtask(), ac, "sub_lazy");
        EXPECT_EQ(tmp, 2);
        Promise<int> promise;
        Future<int> f = promise.getFuture();
        std::thread([pr = std::move(promise)]() mutable -> void {
            pr.setValue(2);
        }).detach();
        int v = co_await AwaiterProxy(std::move(f), ac, "future_awaiter");
        EXPECT_EQ(v, 2);
    };
    syncAwait(task().via(&ex1));
    EXPECT_EQ(ac.events_.size(), 6);
    EXPECT_EQ(ac.events_[0].desc_, "dispatch_awaiter");
    EXPECT_EQ(ac.events_[0].info_type_, AwaiterInfoType::AWAITER_SUSPEND_START);
}

}  // namespace async_simple::coro
