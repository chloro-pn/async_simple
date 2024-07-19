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

#include <cstdint>
#include <source_location>
#include <type_traits>
#include "async_simple/Executor.h"
#include "async_simple/coro/Dispatch.h"
#include "async_simple/coro/Tracing.h"
#include "async_simple/executors/SimpleExecutor.h"
#include "async_simple/test/unittest.h"

namespace async_simple::coro {

template <Subscriber T>
bool test_func(T v) {
    return true;
}

template <typename T>
requires(!Subscriber<T>) bool test_func(T v) { return false; }

template <typename IdType>
struct TestSubscriber1 {
    using AwaiterIdType = IdType;
    IdType generate_awaiter_id() { return 0; }
    void accept(IdType awaiter_id, TracingEvent te, Executor* ex,
                std::source_location sl, const std::any& user_data) {}
};

TEST(TracingTest, test_Subscriber) {
    EXPECT_TRUE(test_func(TestSubscriber1<uint64_t>{}));

    struct invalidIdType {
        int i;
        invalidIdType(int x) : i(x) {}
    };
    EXPECT_FALSE(test_func(TestSubscriber1<invalidIdType>{}));
}

}  // namespace async_simple::coro