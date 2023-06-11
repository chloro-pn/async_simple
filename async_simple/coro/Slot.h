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

#ifndef ASYNC_SIMPLE_CORO_SLOT_H
#define ASYNC_SIMPLE_CORO_SLOT_H

#include <concepts>
#include <optional>
#include <stdexcept>
#include <utility>

#include "async_simple/Common.h"
#include "async_simple/Executor.h"
#include "async_simple/coro/Dispatch.h"

namespace async_simple {
namespace coro {

namespace detail {

class SlotNotifierBase {
protected:
    SlotNotifierBase(std::coroutine_handle<> h, Executor* e,
                     Executor::Context ctx)
        : _handle(h), _ex(e), _ctx(ctx) {}

    // move-only type
    SlotNotifierBase(const SlotNotifierBase&) = delete;
    SlotNotifierBase& operator=(const SlotNotifierBase&) = delete;

    SlotNotifierBase(SlotNotifierBase&&) = default;
    SlotNotifierBase& operator=(SlotNotifierBase&&) = delete;

public:
    void resumeInCoAwaitThread() && {
        logicAssert(_ex != nullptr,
                    "resumeInCoAwaitThread called with empty executor");
        bool succ = _ex->checkin(_handle, _ctx);
        if (succ == false)
            AS_UNLIKELY {
                throw std::runtime_error("schedule to executor failed");
            }
    }

    void resumeInRandomThread() && {
        logicAssert(_ex != nullptr,
                    "resumeInCoAwaitThread called with empty executor");
        bool succ = _ex->schedule(_handle);
        if (succ == false)
            AS_UNLIKELY {
                throw std::runtime_error("schedule to executor failed");
            }
    }

    void resumeInSpecifiedExecutor(Executor* ex) && { dispatch(ex, _handle); }

protected:
    std::coroutine_handle<> _handle;
    Executor* _ex;
    Executor::Context _ctx;
};
}  // namespace detail

template <typename ResumeType>
class SlotNotifier : public detail::SlotNotifierBase {
public:
    SlotNotifier(std::coroutine_handle<> h, Executor* e, Executor::Context ctx,
                 std::optional<ResumeType>& optional_v)
        : SlotNotifierBase(h, e, ctx), _resume_value(optional_v) {}

    template <typename T>
    requires std::is_convertible_v<T, ResumeType>
    void SetValue(T&& v) { _resume_value.emplace(std::forward<T>(v)); }

private:
    std::optional<ResumeType>& _resume_value;
};

template <>
class SlotNotifier<void> : public detail::SlotNotifierBase {
public:
    SlotNotifier(std::coroutine_handle<> h, Executor* e, Executor::Context ctx)
        : SlotNotifierBase(h, e, ctx) {}
};

namespace detail {

template <typename T>
concept IsVoidSlot = requires(T& slot) {
    { slot.await_resume() } -> std::same_as<void>;
};

template <typename SlotType>
requires IsVoidSlot<SlotType>
class SlotVoidAwaiter {
public:
    SlotVoidAwaiter(SlotType&& slot, Executor* ex)
        : _slot(std::move(slot)), _executor(ex) {}

    bool await_ready() { return _slot.await_ready(); }

    bool await_suspend(std::coroutine_handle<> h) {
        SlotNotifier<void> sn(h, _executor, _executor->checkout());
        return _slot.await_suspend(std::move(sn));
    }

    auto await_resume() { return _slot.await_resume(); }

private:
    SlotType _slot;
    Executor* _executor;
};

template <typename SlotType>
class SlotAwaiter {
public:
    using resume_type = decltype(std::declval<SlotType&>().await_resume());
    static_assert(!std::is_same<resume_type, void>::value);

    explicit SlotAwaiter(SlotType&& slot, Executor* ex)
        : _slot(std::move(slot)), _executor(ex) {}

    bool await_ready() { return _slot.await_ready(); }

    bool await_suspend(std::coroutine_handle<> h) {
        SlotNotifier<resume_type> sn(h, _executor, _executor->checkout(),
                                     _resume_value);
        return _slot.await_suspend(std::move(sn));
    }

    auto await_resume() {
        _slot.setValue(std::move(_resume_value).value());
        return _slot.await_resume();
    }

private:
    SlotType _slot;
    Executor* _executor;
    std::optional<resume_type> _resume_value;
};

template <typename SlotType>
class SlotAwaitable {
public:
    template <typename... Args>
    explicit SlotAwaitable(Args&&... args)
        : _slot(std::forward<Args>(args)...) {}

    auto coAwait(Executor* _ex) {
        if constexpr (IsVoidSlot<SlotType>) {
            return SlotVoidAwaiter(std::move(_slot), _ex);
        } else {
            return SlotAwaiter(std::move(_slot), _ex);
        }
    }

private:
    SlotType _slot;
};

}  // namespace detail

template <typename SlotType, typename... Args>
detail::SlotAwaitable<SlotType> Slot(Args&&... args) {
    return detail::SlotAwaitable<SlotType>(std::forward<Args>(args)...);
}

}  // namespace coro
}  // namespace async_simple
#endif