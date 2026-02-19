/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
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

        constexpr operator bool() const noexcept {
            return b != nullptr;
        }

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

    template <class Soc>
    struct AcceptAsyncResult {
        Canceler cancel;
        NotifyState state;
        Soc socket;
    };

    template <typename T>
    struct BufferManager {
        T buffer;
        // NetAddrPort address;

        BufferManager() = default;
        BufferManager(const BufferManager&) = default;
        BufferManager(BufferManager&& i) = default;

        template <typename V, helper_disable_self(BufferManager, V)>
        explicit BufferManager(V&& buf)
            : buffer(std::forward<V>(buf)) {}

        T& get_buffer() {
            return buffer;
        }

        /*
        NetAddrPort& get_address() {
            return address;
        }
        */
    };

    struct DeferredCallback {
       private:
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

        constexpr void operator()() {
            invoke();
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
        //{ t.get_address() } -> std::convertible_to<NetAddrPort&>;
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
    template <class Fn, class Del>
    struct AsyncContData : helper::omit_empty<Del> {
        Fn fn;

        constexpr AsyncContData(auto&& f, auto&& del)
            : helper::omit_empty<Del>(std::forward<decltype(del)>(del)), fn(std::forward<decltype(f)>(f)) {}

        void del(auto p) {
            this->om_value()(p);
        }
    };

    template <class Buf, class Base>
    struct AsyncContDataWithBuffer : Base {
        Buf buffer_mgr;

        constexpr AsyncContDataWithBuffer(auto&& fn, auto&& buf, auto&& del)
            : buffer_mgr(std::forward<decltype(buf)>(buf)), Base(std::forward<decltype(fn)>(fn), std::forward<decltype(del)>(del)) {}
    };

    // AsyncContDataWithAddress is used for async operation when result has address
    // this is used to notify async operation result
    // pooling this object or of it storage is recommended if you use async operation frequently
    template <class Base>
    struct AsyncContDataWithAddress : Base {
        NetAddrPort address;

        constexpr AsyncContDataWithAddress(auto&&... args)
            : Base(std::forward<decltype(args)>(args)...) {}
    };

    struct Socket;

    template <class Notify, class Base, class SockTy = Socket>
    struct AsyncContDataWithNotify : Base {
        Notify notify;
        SockTy socket;
        NotifyResult result;

        constexpr AsyncContDataWithNotify(auto&& notify, auto&&... args)
            : Base(std::forward<decltype(args)>(args)...), notify(std::forward<decltype(notify)>(notify)) {}
    };

    template <class Notify, class Base, class SockTy = Socket>
    struct AsyncContDataWithNotifyAccept : Base {
        Notify notify;
        SockTy socket;
        SockTy accepted;
        NotifyResult result;

        constexpr AsyncContDataWithNotifyAccept(auto&& notify, auto&&... args)
            : Base(std::forward<decltype(args)>(args)...), notify(std::forward<decltype(notify)>(notify)) {}
    };

    template <class Fn, class Buf, class Del>
    using AsyncContBufData = AsyncContDataWithBuffer<Buf, AsyncContData<Fn, Del>>;

    template <class Fn, class Buf, class Del, class Notify>
    using AsyncContNotifyBufData = AsyncContDataWithNotify<Notify, AsyncContBufData<Fn, Buf, Del>>;

    template <class Fn, class Buf, class Del>
    using AsyncContBufAddrData = AsyncContDataWithAddress<AsyncContBufData<Fn, Buf, Del>>;

    template <class Fn, class Buf, class Del, class Notify>
    using AsyncContNotifyBufAddrData = AsyncContDataWithNotify<Notify, AsyncContBufAddrData<Fn, Buf, Del>>;

    template <class Fn, class Del>
    using AsyncContAddrData = AsyncContDataWithAddress<AsyncContData<Fn, Del>>;

    template <class Fn, class Del, class Notify>
    using AsyncContNotifyAddrData = AsyncContDataWithNotify<Notify, AsyncContAddrData<Fn, Del>>;

    template <class Fn, class Del, class Notify>
    using AsyncContNotifyAddrDataAccept = AsyncContDataWithNotifyAccept<Notify, AsyncContAddrData<Fn, Del>>;

    namespace internal {
        template <class T>
        T* alloc_and_construct(auto&&... args) {
            auto ptr = alloc_normal(sizeof(T), alignof(T), DNET_DEBUG_MEMORY_LOCINFO(true, sizeof(T), alignof(T)));
            if (!ptr) {
                return (T*)nullptr;
            }
            auto safe = helper::defer([&] { free_normal(ptr, DNET_DEBUG_MEMORY_LOCINFO(true, sizeof(T), alignof(T))); });
            auto p = new (ptr) T{std::forward<decltype(args)>(args)...};
            safe.cancel();
            return p;
        }
    }  // namespace internal

    DeferredCallback alloc_deferred_callback(auto&& lambda) {
        using Lambda = std::decay_t<decltype(lambda)>;
        return make_deferred_callback(
            internal::alloc_and_construct<Lambda>(std::forward<decltype(lambda)>(lambda)),
            +[](void* ptr, bool del_only) {
                auto p = static_cast<Lambda*>(ptr);
                auto d = helper::defer([=] {
                    internal::Delete{}(p);
                });

                if (!del_only) {
                    (*p)();
                }
            });
    }

    // async_then creates AsyncContData object
    // this is wrapper for AsyncContData creator
    template <class Fn, class BufMgr, class Base = AsyncContBufData<std::decay_t<Fn>, std::decay_t<BufMgr>, internal::Delete>>
    Base* async_then(BufMgr&& buffer_mgr, Fn&& fn) {
        return internal::alloc_and_construct<Base>(std::forward<decltype(fn)>(fn), std::forward<decltype(buffer_mgr)>(buffer_mgr), internal::Delete{});
    }

    template <class Fn, class BufMgr>
    AsyncContBufAddrData<std::decay_t<Fn>, std::decay_t<BufMgr>, internal::Delete>* async_addr_then(BufMgr&& buffer_mgr, Fn&& fn, const NetAddrPort& addr = {}) {
        using WithAddr = AsyncContBufAddrData<std::decay_t<Fn>, std::decay_t<BufMgr>, internal::Delete>;
        auto p = async_then<Fn, BufMgr, WithAddr>(std::forward<BufMgr>(buffer_mgr), std::forward<Fn>(fn));
        if (p) {
            p->address = addr;
        }
        return p;
    }

    template <class Fn, class Buf, class Notify, class Base = AsyncContNotifyBufData<std::decay_t<Fn>, std::decay_t<Buf>, internal::Delete, Notify>>
    Base* async_notify_then(Buf&& buffer_mgr, Notify&& notify, Fn&& fn) {
        return internal::alloc_and_construct<Base>(std::forward<decltype(notify)>(notify), std::forward<decltype(fn)>(fn), std::forward<decltype(buffer_mgr)>(buffer_mgr), internal::Delete{});
    }

    template <class Fn, class Buf, class Notify>
    AsyncContNotifyBufAddrData<std::decay_t<Fn>, std::decay_t<Buf>, internal::Delete, std::decay_t<Notify>>* async_notify_addr_then(Buf&& buffer_mgr, Notify&& notify, Fn&& fn, const NetAddrPort& addr = {}) {
        using WithAddr = AsyncContNotifyBufAddrData<std::decay_t<Fn>, std::decay_t<Buf>, internal::Delete, std::decay_t<Notify>>;
        auto p = async_notify_then<Fn, Buf, Notify, WithAddr>(std::forward<Buf>(buffer_mgr), std::forward<Notify>(notify), std::forward<Fn>(fn));
        if (p) {
            p->address = addr;
        }
        return p;
    }

    template <class Fn, class Base = AsyncContAddrData<std::decay_t<Fn>, internal::Delete>>
    Base* async_accept_then(Fn&& fn, const NetAddrPort& addr = {}) {
        auto p = internal::alloc_and_construct<Base>(std::forward<decltype(fn)>(fn), internal::Delete{});
        if (p) {
            p->address = addr;
        }
        return p;
    }

    template <class Fn, class Notify, class Base = AsyncContNotifyAddrDataAccept<std::decay_t<Fn>, internal::Delete, std::decay_t<Notify>>>
    Base* async_accept_notify_then(Notify&& notify, Fn&& fn, const NetAddrPort& addr = {}) {
        auto p = internal::alloc_and_construct<Base>(std::forward<decltype(notify)>(notify), std::forward<decltype(fn)>(fn), internal::Delete{});
        if (p) {
            p->address = addr;
        }
        return p;
    }

    template <class Fn>
    AsyncContAddrData<std::decay_t<Fn>, internal::Delete>* async_connect_then(const NetAddrPort& addr, Fn&& fn) {
        return async_accept_then(std::forward<decltype(fn)>(fn), addr);
    }

    template <class Fn, class Notify>
    AsyncContNotifyAddrData<std::decay_t<Fn>, internal::Delete, std::decay_t<Notify>>* async_connect_notify_then(const NetAddrPort& addr, Notify&& notify, Fn&& fn) {
        using Connect = AsyncContNotifyAddrData<std::decay_t<Fn>, internal::Delete, std::decay_t<Notify>>;
        return async_accept_notify_then<Fn, Notify, Connect>(std::forward<decltype(notify)>(notify), std::forward<decltype(fn)>(fn), addr);
    }

    using stream_notify_t = void (*)(Socket&&, void*, NotifyResult&& err);
    using recvfrom_notify_t = void (*)(Socket&&, NetAddrPort&&, void*, NotifyResult&& err);
    using accept_notify_t = void (*)(Socket&& listener, Socket&& accepted, NetAddrPort&&, void*, NotifyResult&& err);

}  // namespace futils::fnet
