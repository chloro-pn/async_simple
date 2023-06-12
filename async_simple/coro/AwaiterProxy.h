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

#ifndef ASYNC_SIMPLE_CORO_AWAITERPROXY_H
#define ASYNC_SIMPLE_CORO_AWAITERPROXY_H

#include "async_simple/Executor.h"
#include "async_simple/coro/AwaiterInfoCollector.h"
#include "async_simple/coro/Lazy.h"
#include "async_simple/coro/ViaCoroutine.h"

#include <concepts>
#include <source_location>

namespace async_simple {
namespace coro {

namespace detail {

template <typename Awaiter>
class AwaiterWrapper {
private:
    constexpr static bool await_suspend_return_bool =
        requires(Awaiter & awaiter) {
        { awaiter.await_suspend() } -> std::same_as<bool>;
    };

public:
    explicit AwaiterWrapper(Awaiter&& awaiter, AwaiterInfoCollector& ac,
                            std::string&& desc, Executor* ex,
                            const std::source_location& location)
        : awaiter_(std::move(awaiter)),
          ac_(ac),
          id_(ac_.generateAwaiterId()),
          desc_(std::move(desc)),
          ex_(ex),
          location_(location) {}

    bool await_ready() {
        bool ready = awaiter_.await_ready();
        if (ready == true) {
            ac_.collect(AwaiterInfoType::AWAITER_READY_TRUE, ex_, id_, desc_,
                        location_);
        }
        return ready;
    }

    template <typename PromiseType>
    auto await_suspend(std::coroutine_handle<PromiseType> h) {
        static_assert(std::is_base_of<LazyPromiseBase, PromiseType>::value,
                      "AwaiterProxy is only allowed to be called by Lazy");
        if constexpr (await_suspend_return_bool) {
            bool suspend = awaiter_.await_suspend(h);
            if (suspend == false) {
                ac_.collect(AwaiterInfoType::AWAITER_SUSPEND_FALSE, ex_, id_,
                            desc_, location_);
            } else {
                ac_.collect(AwaiterInfoType::AWAITER_SUSPEND_START, ex_, id_,
                            desc_, location_);
            }
            return suspend;
        }
        ac_.collect(AwaiterInfoType::AWAITER_SUSPEND_START, ex_, id_, desc_,
                    location_);
        return awaiter_.await_suspend(h);
    }

    auto await_resume() {
        ac_.collect(AwaiterInfoType::AWAITER_SUSPEND_RESUME, ex_, id_, desc_,
                    location_);
        return awaiter_.await_resume();
    }

private:
    Awaiter awaiter_;
    AwaiterInfoCollector& ac_;
    uint64_t id_;
    std::string desc_;
    Executor* ex_;
    std::source_location location_;
};

template <typename Awaitable>
class AwaiterProxyAwaitable {
public:
    explicit AwaiterProxyAwaitable(Awaitable&& awaitable,
                                   AwaiterInfoCollector& ac,
                                   const std::string& desc,
                                   const std::source_location& location)
        : awaitable_(std::move(awaitable)),
          ac_(ac),
          desc_(desc),
          location_(location) {}

    auto coAwait(Executor* ex) && {
        auto awaiter = detail::coAwait(ex, std::move(awaitable_));
        return AwaiterWrapper(std::move(awaiter), ac_, std::move(desc_), ex,
                              location_);
    }

private:
    Awaitable awaitable_;
    AwaiterInfoCollector& ac_;
    std::string desc_;
    std::source_location location_;
};

}  // namespace detail

template <typename Awaitable>
detail::AwaiterProxyAwaitable<Awaitable> AwaiterProxy(
    Awaitable&& awaitable, AwaiterInfoCollector& ac,
    const std::string& desc = "",
    const std::source_location location = std::source_location::current()) {
    return detail::AwaiterProxyAwaitable(std::forward(awaitable), ac, desc,
                                         location);
}

}  // namespace coro
}  // namespace async_simple

#endif
