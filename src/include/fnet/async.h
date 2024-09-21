/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "dll/dllh.h"
#include <cstdint>
#include <utility>
#include "error.h"
#include "address.h"
#include <optional>

namespace futils::fnet {

    template <class View>
    struct SockMsg {
        View data;
        View control;
    };

    enum class NotifyState {
        wait,  // waiting notification, callback will be called
        done,  // operation done, callback will not be called
    };

    struct fnet_class_export Canceler {
       private:
        std::uint64_t cancel_code = 0;
        bool w = false;
        void* b = nullptr;
        friend Canceler make_canceler(std::uint64_t code, bool w, void* b);

       public:
        constexpr Canceler() = default;

        constexpr Canceler(Canceler&& c)
            : cancel_code(c.cancel_code), w(c.w), b(std::exchange(c.b, nullptr)) {}

        constexpr Canceler& operator=(Canceler&& c) {
            if (this == &c) {
                return *this;
            }
            this->~Canceler();
            cancel_code = c.cancel_code;
            w = c.w;
            b = std::exchange(c.b, nullptr);
            return *this;
        }

        expected<void> cancel();

        ~Canceler();
    };

    struct AsyncResult {
        Canceler cancel;
        NotifyState state;
        size_t processed_bytes;
    };

    template <typename T>
    struct BufferManager {
        T buffer;
        NetAddrPort address;

        BufferManager() = default;

        explicit BufferManager(T&& buf)
            : buffer(std::move(buf)) {}

        BufferManager(T&& buf, NetAddrPort&& addr)
            : buffer(std::move(buf)), address(std::move(addr)) {}

        T& get_buffer() {
            return buffer;
        }

        NetAddrPort& get_address() {
            return address;
        }
    };

    struct DeferredCallback {
       private:
        friend struct Socket;

        friend constexpr DeferredCallback make_deferred_callback(void* task_ptr, void (*call)(void*, bool delete_only));
        void* task_ptr = nullptr;
        void (*call)(void*, bool) = nullptr;

        constexpr DeferredCallback(void* p, void (*c)(void*, bool delete_only))
            : task_ptr(p), call(c) {}

       public:
        constexpr DeferredCallback() = default;
        constexpr DeferredCallback(DeferredCallback&& i)
            : task_ptr(std::exchange(i.task_ptr, nullptr)), call(std::exchange(i.call, nullptr)) {}

        constexpr DeferredCallback& operator=(DeferredCallback&& i) {
            if (this == &i) {
                return *this;
            }
            task_ptr = std::exchange(i.task_ptr, nullptr);
            call = std::exchange(i.call, nullptr);
            return *this;
        }

        constexpr explicit operator bool() const {
            return call != nullptr;
        }

        constexpr void invoke() {
            if (call) {
                const auto d = helper::defer([&] {
                    call = nullptr;
                    task_ptr = nullptr;
                });
                call(task_ptr, false);
            }
        }

        constexpr void cancel() {
            if (call) {
                const auto d = helper::defer([&] {
                    call = nullptr;
                    task_ptr = nullptr;
                });
                call(task_ptr, true);
            }
        }

        constexpr ~DeferredCallback() {
            cancel();
        }
    };

    constexpr DeferredCallback make_deferred_callback(void* task_ptr, void (*call)(void*, bool delete_only)) {
        if (!call) {
            return DeferredCallback{};
        }
        return DeferredCallback(task_ptr, call);
    }

    template <typename T>
    concept AsyncBufferType = requires(T t) {
        { t.get_buffer() } -> std::convertible_to<view::wvec>;
        { t.get_address() } -> std::convertible_to<NetAddrPort&>;
    };

    using NotifyResult_v = expected<std::optional<size_t>>;

    struct NotifyResult {
       private:
        NotifyResult_v result;

       public:
        constexpr NotifyResult(auto&& v)
            : result(std::move(v)) {}
        constexpr NotifyResult() = default;

        NotifyResult_v& value() {
            return result;
        }

        expected<view::wvec> read_unwrap(view::wvec buf, auto&& read_op) {
            if (!result) {
                return result.transform([&](auto) { return buf; });
            }
            if (*result) {
                return buf.substr(0, **result);
            }
            else {
                auto res = read_op(buf);
                if (!res) {
                    return res.transform([&](auto) { return buf; });
                }
                return *res;
            }
        }

        expected<std::pair<view::wvec, NetAddrPort>> readfrom_unwrap(view::wvec buf, NetAddrPort& addr, auto&& read_op) {
            if (!result) {
                return result.transform([&](auto) { return std::make_pair(buf, addr); });
            }
            if (*result) {
                return std::make_pair(buf.substr(0, **result), std::move(addr));
            }
            else {
                auto res = read_op(buf);
                if (!res) {
                    return res.transform([&](auto) { return std::make_pair(buf, addr); });
                }
                return *res;
            }
        }

        expected<view::rvec> write_unwrap(view::rvec buf, auto&& write_op) {
            if (!result) {
                return result.transform([&](auto) { return buf; });
            }
            if (*result) {
                return buf.substr(**result);
            }
            else {
                auto res = write_op(buf);
                if (!res) {
                    return res.transform([&](auto) { return buf; });
                }
                return *res;
            }
        }
    };

    // AsyncContData is used for async operation
    // means async continuation data
    // this is used to notify async operation result
    // pooling this object or of it storage is recommended if you use async operation frequently
    template <class Fn, class Buf, class Del>
    struct AsyncContData : helper::omit_empty<Del> {
        Fn fn;
        Buf buffer_mgr;

        constexpr AsyncContData(auto&& f, auto&& buf, auto&& del)
            : helper::omit_empty<Del>(std::forward<decltype(del)>(del)), fn(std::forward<decltype(f)>(f)), buffer_mgr(std::forward<decltype(buf)>(buf)) {}

        void del(auto p) {
            auto a = this->om_value();
            a(p);
        }
    };

    // AsyncContDataWithAddress is used for async operation when result has address
    // this is used to notify async operation result
    // pooling this object or of it storage is recommended if you use async operation frequently
    template <class Base>
    struct AsyncContDataWithAddress : Base {
        NetAddrPort address;

        constexpr AsyncContDataWithAddress(auto&& fn, auto&& buf, auto&& del)
            : Base(std::forward<decltype(fn)>(fn), std::forward<decltype(buf)>(buf), std::forward<decltype(del)>(del)) {}

        constexpr AsyncContDataWithAddress(auto&& fn, auto&& buf, auto&& del, auto&& notify)
            : Base(std::forward<decltype(fn)>(fn), std::forward<decltype(buf)>(buf), std::forward<decltype(del)>(del), std::forward<decltype(notify)>(notify)) {}
    };

    struct Socket;

    template <class Notify, class Base, class SockTy = Socket>
    struct AsyncContDataWithNotify : Base {
        Notify notify;
        SockTy socket;
        NotifyResult result;

        constexpr AsyncContDataWithNotify(auto&& fn, auto&& buf, auto&& del, auto&& notify)
            : Base(std::forward<decltype(fn)>(fn), std::forward<decltype(buf)>(buf), std::forward<decltype(del)>(del)), notify(std::forward<decltype(notify)>(notify)) {}
    };

    // async_then creates AsyncContData object
    // this is wrapper for AsyncContData creator
    template <class Fn, class BufMgr, class Base = AsyncContData<std::decay_t<Fn>, std::decay_t<BufMgr>, internal::Delete>>
    auto async_then(BufMgr&& buffer_mgr, Fn&& fn) {
        using T = Base;
        auto ptr = alloc_normal(sizeof(T), alignof(T), DNET_DEBUG_MEMORY_LOCINFO(true, sizeof(T), alignof(T)));
        if (!ptr) {
            return (T*)nullptr;
        }
        auto safe = helper::defer([&] { free_normal(ptr, DNET_DEBUG_MEMORY_LOCINFO(true, sizeof(T), alignof(T))); });
        auto p = new (ptr) T{std::forward<decltype(fn)>(fn), std::forward<decltype(buffer_mgr)>(buffer_mgr), internal::Delete{}};
        safe.cancel();
        return p;
    }

    template <class Fn, class BufMgr>
    auto async_addr_then(BufMgr&& buffer_mgr, Fn&& fn) {
        using WithAddr = AsyncContDataWithAddress<AsyncContData<std::decay_t<decltype(fn)>, std::decay_t<decltype(buffer_mgr)>, internal::Delete>>;
        return async_then<decltype(fn), decltype(buffer_mgr), WithAddr>(std::forward<decltype(buffer_mgr)>(buffer_mgr), std::forward<decltype(fn)>(fn));
    }

    template <class Fn, class Buf, class Base = AsyncContData<std::decay_t<Fn>, std::decay_t<Buf>, internal::Delete>>
    auto async_notify_then(Buf&& buffer_mgr, auto&& notify, Fn&& fn) {
        using T = AsyncContDataWithNotify<std::decay_t<decltype(notify)>, Base>;
        auto ptr = alloc_normal(sizeof(T), alignof(T), DNET_DEBUG_MEMORY_LOCINFO(true, sizeof(T), alignof(T)));
        if (!ptr) {
            return (T*)nullptr;
        }
        auto safe = helper::defer([&] { free_normal(ptr, DNET_DEBUG_MEMORY_LOCINFO(true, sizeof(T), alignof(T))); });
        auto p = new (ptr) T{std::forward<decltype(fn)>(fn), std::forward<decltype(buffer_mgr)>(buffer_mgr), internal::Delete{}, std::forward<decltype(notify)>(notify)};
        safe.cancel();
        return p;
    }

    template <class Fn, class Buf>
    auto async_notify_addr_then(Buf&& buffer_mgr, auto&& notify, Fn&& fn) {
        using WithAddr = AsyncContDataWithAddress<AsyncContData<std::decay_t<Fn>, std::decay_t<Buf>, internal::Delete>>;
        return async_notify_then<Fn, Buf, WithAddr>(std::forward<decltype(buffer_mgr)>(buffer_mgr), std::forward<decltype(notify)>(notify), std::forward<decltype(fn)>(fn));
    }

    using stream_notify_t = void (*)(Socket&&, void*, NotifyResult&& err);
    using recvfrom_notify_t = void (*)(Socket&&, NetAddrPort&&, void*, NotifyResult&& err);

}  // namespace futils::fnet
