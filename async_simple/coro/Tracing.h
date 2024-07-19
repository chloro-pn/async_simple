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

#ifndef ASYNC_SIMPLE_TRACING_H
#define ASYNC_SIMPLE_TRACING_H

#include <any>
#include <concepts>
#include <source_location>
#include <type_traits>

#include "async_simple/Executor.h"
#include "async_simple/coro/Lazy.h"
#include "async_simple/experimental/coroutine.h"

namespace async_simple {
namespace coro {

enum class TracingEvent : uint8_t {
    awaiter_ready_true,
    awaiter_ready_false,
    awaiter_suspend_false,
    awaiter_suspend_start,
    awaiter_suspend_resume,
};

template <typename T>
concept Subscriber = requires(T& subscribers, const T::AwaiterIdType& id,
                              TracingEvent te, Executor* ex,
                              const std::source_location& sl,
                              const std::any& user_data) {
    typename T::AwaiterIdType;
    std::default_initializable<typename T::AwaiterIdType>;
    {
        subscribers.generate_awaiter_id()
        } -> std::same_as<typename T::AwaiterIdType>;
    { subscribers.accept(id, te, ex, sl, user_data) } -> std::same_as<void>;
};

namespace detail {

template <typename RetT, typename T, typename... Args>
constexpr bool is_noexcept_member_function(RetT (T::*)(Args...) noexcept) {
    return true;
}

template <typename RetT, typename T, typename... Args>
constexpr bool is_noexcept_member_function(RetT (T::*)(Args...)) {
    return false;
}

template <typename Awaiter, Subscriber SubscriberType>
class TracingAwaiter {
private:
    using AwaiterIdType = SubscriberType::AwaiterIdType;

    constexpr static bool await_suspend_return_bool =
        requires(Awaiter & awaiter) {
        { awaiter.await_suspend() } -> std::same_as<bool>;
    };

public:
    TracingAwaiter(SubscriberType& subscriber, Awaiter&& awaiter, Executor* ex,
                   std::source_location location, std::any&& user_data)
        : _subscriber(subscriber),
          _awaiter(std::move(awaiter)),
          _awaiter_id{},
          _ex(ex),
          _location(std::move(location)),
          _user_data(std::move(user_data)) {
        _awaiter_id = _subscriber.generate_awaiter_id();
    }

    bool await_ready() noexcept(
        is_noexcept_member_function(&Awaiter::await_ready) &&
        is_noexcept_member_function(&SubscriberType::accept)) {
        bool ready = _awaiter.await_ready();
        if (ready == true) {
            _subscriber.accpet(_awaiter_id, TracingEvent::awaiter_ready_true,
                               _ex, _location, _user_data);
        } else {
            _subscriber.accpet(_awaiter_id, TracingEvent::awaiter_ready_false,
                               _ex, _location, _user_data);
        }
        return ready;
    }

    template <typename PromiseType>
    auto await_suspend(CoroHandle<PromiseType> h) noexcept(
        is_noexcept_member_function(&Awaiter::await_suspend) &&
        is_noexcept_member_function(&SubscriberType::accept)) {
        static_assert(std::is_base_of<LazyPromiseBase, PromiseType>::value,
                      "TracingAwaiter is only allowed to be called by Lazy");
        if constexpr (await_suspend_return_bool) {
            bool suspend = _awaiter.await_suspend(h);
            if (suspend == false) {
                _subscriber.accpet(_awaiter_id,
                                   TracingEvent::awaiter_suspend_false, _ex,
                                   _location, _user_data);
            } else {
                _subscriber.accpet(_awaiter_id,
                                   TracingEvent::awaiter_suspend_start, _ex,
                                   _location, _user_data);
            }
            return suspend;
        }
        _subscriber.accpet(_awaiter_id, TracingEvent::awaiter_suspend_start,
                           _ex, _location, _user_data);
        return _awaiter.await_suspend(h);
    }

    auto await_resume() noexcept(
        is_noexcept_member_function(&Awaiter::await_resume) &&
        is_noexcept_member_function(&SubscriberType::accept)) {
        _subscriber.accpet(_awaiter_id, TracingEvent::awaiter_suspend_resume,
                           _ex, _location, _user_data);
        return _awaiter.await_resume();
    }

private:
    SubscriberType& _subscriber;
    Awaiter _awaiter;
    AwaiterIdType _awaiter_id;
    Executor* _ex;
    std::source_location _location;
    std::any _user_data;
};

template <typename Awaitable, Subscriber SubscriberType>
class TracingAwaitable {
public:
    TracingAwaitable(SubscriberType& subscriber, Awaitable&& awaitable,
                     std::source_location location, std::any&& user_data)
        : _subscriber(subscriber),
          _awaitable(std::forward(awaitable)),
          _location(std::move(location)),
          _user_data(std::move(user_data)) {}

    auto coAwait(Executor* ex) && {
        auto awaiter = detail::coAwait(ex, std::move(_awaitable));
        return TracingAwaiter(_subscriber, std::move(awaiter), ex,
                              std::move(_location), std::move(_user_data));
    }

private:
    SubscriberType& _subscriber;
    Awaitable _awaitable;
    std::source_location _location;
    std::any _user_data;
};

}  // namespace detail

template <typename Awaitable, Subscriber SubscriberType>
detail::TracingAwaitable<Awaitable, SubscriberType> Tracing(
    Awaitable&& awaitable, SubscriberType& subscriber, std::any user_data = {},
    const std::source_location location = std::source_location::current()) {
    return detail::TracingAwaitable(subscriber,
                                    std::forward<Awaitable>(awaitable),
                                    location, std::move(user_data));
}
}  // namespace coro
}  // namespace async_simple
#endif
