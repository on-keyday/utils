/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// waker - async task execution resource
#pragma once
#include <memory>
#include <atomic>

namespace futils {
    namespace thread {
        enum class WakerState {
            reset,      // default constructed
            sleep,      // waiting for task (returned by make_waker or allocate_waker)
            waking,     // task done alert callback running
            woken,      // task done alert notified
            running,    // running main
            done,       // running done
            canceling,  // canceling operation
            canceled,   // operation canceled
        };

        struct Waker;

        using WakerCallback = int (*)(const std::shared_ptr<Waker>&, std::shared_ptr<void>&, WakerState call_at, void* arg);

        // Waker is event waiting abstraction object
        // WakerCallback is called at WakerState::waking, WakerState::woken, WakerState::canceling, WakerState::canceled or Waker::running
        // this function result is used when call with WakerState::canceling
        // WakerCallback result meaning is:
        // > 0 - continue operation. also call WakerState::canceled callback
        // = 0 - stop operation. operation was succeeded.
        // < 0 - stop operation. operation was failed.
        struct Waker : std::enable_shared_from_this<Waker> {
           private:
            std::shared_ptr<void> data;
            WakerCallback cb = nullptr;
            std::atomic<WakerState> state = WakerState::reset;

            friend std::shared_ptr<Waker> make_waker(std::shared_ptr<void> param, WakerCallback cb);

            template <class Alloc>
            friend std::shared_ptr<Waker> allocate_waker(Alloc&& al, std::shared_ptr<void> param, WakerCallback cb);

            int invoke(const std::shared_ptr<Waker>& pin, WakerState call, void* arg) {
                if (cb) {
                    return cb(pin, data, call, arg);
                }
                return 1;
            }

            bool try_get_turn(WakerState expect, WakerState next) {
                for (;;) {
                    WakerState e = expect;
                    if (!state.compare_exchange_strong(e, next)) {
                        if (e == next) {
                            continue;
                        }
                        return false;
                    }
                    return true;
                }
            }

           public:
            constexpr Waker() = default;
            Waker(Waker&&) = delete;

            // is_waiting reports whether state is WakerState::sleep
            bool is_waiting() const {
                return state == WakerState::sleep;
            }

            // is_ready reports whether state is WakerState::woken
            bool is_ready() const {
                return state == WakerState::woken;
            }

            bool is_complete() const {
                return state == WakerState::done;
            }

           private:
            bool wakeup_impl(void* arg, bool cancel) {
                if (!try_get_turn(WakerState::sleep, WakerState::waking)) {
                    return false;
                }
                auto pin = shared_from_this();
                if (cancel) {
                    auto res = invoke(pin, WakerState::canceling, arg);
                    if (res <= 0) {
                        state.store(WakerState::sleep);
                        return res == 0;
                    }
                }
                else {
                    invoke(pin, WakerState::waking, arg);
                }
                auto d = data;
                auto c = cb;
                state.store(WakerState::woken);
                // now maybe unsafe by race condition
                // usage of this is, for example, enque waker to execution buffer
                c(pin, d, cancel ? WakerState::canceled : WakerState::woken, arg);
                return true;
            }

           public:
            // wakeup change state to WakerState::woken
            // previous state MUST be WakerState::sleep
            // call callback with WakerState::waking and WakerState::woken
            // callback with WakerState::woken may be in race condition
            // arg is implementation depended argument
            // this function is block if other thread call wakeup() or cancel() in same state
            bool wakeup(void* arg = nullptr) {
                return wakeup_impl(arg, false);
            }

            // cancel may change state to WakerState::woken
            // previous state MUST be WakerState::sleep
            // call callback with WakerState::canceling and may call it with WakerState::canceled
            // this cancel mechanism is implementation dependent
            // even if cancel() is called, run() can be called
            // if internal callback returns <= 0 at WakerState::canceling, wakup() and cancel() also can be called again
            // if callback returns <= 0 at WakerState::canceling and implementation wants user call cancel() only once,
            // implementation should mark this Waker as canceled over Waker::data
            bool cancel(void* arg = nullptr) {
                return wakeup_impl(arg, true);
            }

            // run change state to WakerState::done
            // previous state MUST be WakerState::woken
            // call callback with WakerState::running
            // arg is implementation depended argument
            bool run(void* arg = nullptr) {
                WakerState s = WakerState::woken;
                if (!state.compare_exchange_strong(s, WakerState::running)) {
                    return false;
                }
                auto pin = shared_from_this();
                invoke(pin, WakerState::running, arg);
                state.store(WakerState::done);
                return true;
            }

            // reset change state from WakerState::done to WakerState::sleep
            bool reset(WakerCallback cb, std::shared_ptr<void> param) {
                WakerState s = WakerState::done;
                if (!state.compare_exchange_strong(s, WakerState::reset)) {
                    return false;
                }
                this->cb = cb;
                this->data = std::move(param);
                this->state.store(WakerState::sleep);
                return true;
            }

            std::shared_ptr<void> get_data() const {
                return data;
            }
        };

        inline std::shared_ptr<Waker> make_waker(std::shared_ptr<void> param, WakerCallback cb) {
            auto w = std::make_shared<Waker>();
            w->cb = cb;
            w->data = std::move(param);
            w->state.store(WakerState::sleep);
            return w;
        }

        template <class Alloc>
        inline std::shared_ptr<Waker> allocate_waker(Alloc&& al, std::shared_ptr<void> param, WakerCallback cb) {
            auto w = std::allocate_shared<Waker>(std::forward<decltype(al)>(al));
            w->cb = cb;
            w->data = std::move(param);
            w->state.store(WakerState::sleep);
            return w;
        }

    }  // namespace thread
}  // namespace futils
