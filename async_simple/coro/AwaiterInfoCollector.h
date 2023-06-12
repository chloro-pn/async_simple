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

#ifndef ASYNC_SIMPLE_CORO_AWAITERINFOCOLLECTOR_H
#define ASYNC_SIMPLE_CORO_AWAITERINFOCOLLECTOR_H

#include <async_simple/Executor.h>
#include <chrono>
#include <ctime>
#include <iostream>
#include <source_location>
#include <string>
#include <vector>

namespace async_simple {
namespace coro {

enum class AwaiterInfoType {
    AWAITER_READY_TRUE,
    AWAITER_SUSPEND_FALSE,
    AWAITER_SUSPEND_START,
    AWAITER_SUSPEND_RESUME,
};

class AwaiterInfoCollector {
public:
    AwaiterInfoCollector() : awaiter_id(0) {}

    uint64_t generateAwaiterId() noexcept { return awaiter_id.fetch_add(1); }

    virtual void collect(AwaiterInfoType info_type, Executor* ex, uint64_t id,
                         const std::string& desc,
                         const std::source_location& location) = 0;

    virtual ~AwaiterInfoCollector() {}

private:
    std::atomic<uint64_t> awaiter_id;
};

}  // namespace coro
}  // namespace async_simple

#endif
