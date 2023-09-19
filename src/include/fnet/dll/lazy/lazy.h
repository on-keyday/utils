/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// lazy_func - lazy dll load and function load
#pragma once
#include <atomic>
#include "../../../helper/defer.h"
#include "../../../core/byte.h"
#include "../dll_path.h"

namespace utils {
    namespace fnet::lazy {

        using func_t = void (*)();

        enum class LoadState : byte {
            unused,
            loading,
            loaded,
            unavailable,
        };

        struct LoadStatus {
           private:
            std::atomic<LoadState> state = LoadState::unused;

           public:
            bool should_load() {
                LoadState prev = LoadState::unused;
                return state.compare_exchange_strong(prev, LoadState::loading);
            }

            void load_undone() {
                state.store(LoadState::unused);
            }

            void load_done() {
                state.store(LoadState::loaded);
            }

            void load_unavilable() {
                state.store(LoadState::unavailable);
            }

            bool is_loaded() const {
                return state.load() == LoadState::loaded;
            }

            bool is_unavailable() const {
                return state.load() == LoadState::unavailable;
            }

            LoadState wait_for_load() {
                LoadState prev = LoadState::loaded;
                while (!state.compare_exchange_strong(prev, LoadState::loaded)) {
                    if (prev == LoadState::unused || prev == LoadState::unavailable) {
                        return prev;
                    }
                    // now loading
                    prev = LoadState::loaded;  // reset
                }
                return LoadState::loaded;
            }
        };

        struct AtomicLock {
           private:
            std::atomic_bool lock_;

           public:
            void lock() {
                while (lock_.exchange(true)) {
                    // busy loop
                }
            }

            void unlock() {
                lock_.store(false);
            }
        };

        struct DLL;

        // dll initialization time function lookup
        // this bypass hook set to DLL
        struct DLLInit {
           private:
            DLL* dll = nullptr;
            friend struct DLL;

            DLLInit(DLL* d)
                : dll(d) {}

           public:
            constexpr DLLInit() = default;

            // call platform lookup function directly
            func_t lookup(const char* name);

            template <class T>
            T* lookup(const char* name) {
                return reinterpret_cast<T*>(lookup(name));
            }
        };

        enum class InitState {
            load,
            unload,
        };

        using DLLInitHook = bool (*)(DLLInit, InitState);
        using DLLLookupHook = std::pair<func_t, bool> (*)(DLLInit, const char*);

        struct DLL {
           private:
            friend struct DLLInit;
            dll_path path;
            void* target = nullptr;
            DLLInitHook init_hook = nullptr;
            DLLLookupHook lookup_hook = nullptr;
            LoadStatus status;
            bool sys = false;

            bool load_platform();
            void unload_platform();
            func_t lookup_platform(const char*);

            func_t lookup_unsafe(const char* name) {
                if (lookup_hook) {
                    if (auto [fn, stop] = lookup_hook(DLLInit{this}, name); fn) {
                        return fn;
                    }
                    else if (stop) {
                        return nullptr;
                    }
                }
                return lookup_platform(name);
            }

           public:
            bool load() {
                if (status.is_loaded()) {  // fast pass
                    return true;
                }
                while (true) {
                    if (status.should_load()) {
                        auto r = helper::defer([&] {
                            status.load_undone();
                        });
                        if (!load_platform()) {
                            return false;
                        }
                        if (init_hook) {
                            if (!init_hook(DLLInit{this}, InitState::load)) {
                                unload_platform();  // unload target
                                return false;
                            }
                        }
                        r.cancel();
                        status.load_done();
                        return true;
                    }
                    else {
                        switch (status.wait_for_load()) {
                            case LoadState::loaded:
                                goto DONE;
                            case LoadState::unused:
                                break;  // TODO(on-keyday): should loop or exit?
                            default:
                                goto DONE;  // unavailable?
                        }
                    }
                }
            DONE:
                return status.is_loaded();
            }

            // returns (function,temporary)
            std::pair<func_t, bool> lookup(const char* name) {
                if (!load()) {
                    return {nullptr, true};
                }
                return {lookup_unsafe(name), false};
            }

            DLL(dll_path path, bool is_sys, DLLInitHook init = nullptr, DLLLookupHook find = nullptr)
                : path(path), sys(is_sys), init_hook(init), lookup_hook(find) {}

            void replace_path(dll_path new_path, bool is_sys, DLLInitHook init = nullptr, DLLLookupHook find = nullptr) {
                if (!status.should_load()) {
                    return;
                }
                this->path = new_path;
                this->sys = is_sys;
                this->init_hook = init;
                this->lookup_hook = find;
                status.load_undone();
            }

            ~DLL() {
                if (status.is_loaded()) {
                    if (init_hook) {
                        init_hook(DLLInit{this}, InitState::unload);
                    }
                    unload_platform();
                }
            }
        };

        template <class Derived, class T>
        struct Call;
        void set_call_fail_traits(void (*)(const char* fn));
        void call_fail_traits(const char* fn);

        template <class Derived, class Ret, class... Arg>
        struct Call<Derived, Ret(Arg...)> {
            Ret operator()(Arg... arg) {
                auto p = static_cast<Derived*>(this);
                if (!p->find()) {
                    call_fail_traits(p->name);
                }
                return p->fn(arg...);
            }

            constexpr Ret unsafe_call(Arg... arg) const {
                return static_cast<Derived*>(this)->fn(arg...);
            }
        };

        template <class Derived, class Ret, class... Arg>
        struct Call<Derived, Ret(Arg...) noexcept> {
            Ret operator()(Arg... arg) {
                auto p = static_cast<Derived*>(this);
                if (!p->find()) {
                    call_fail_traits(p->name);
                }
                return p->fn(arg...);
            }

            constexpr Ret unsafe_call(Arg... arg) const {
                return static_cast<Derived*>(this)->fn(arg...);
            }
        };

        void log_fn_load(const char* fn, bool ok);

        template <class T>
        struct Func : Call<Func<T>, T> {
            friend struct Call<Func<T>, T>;

           private:
            DLL& dll;
            T* fn = nullptr;
            const char* name;
            LoadStatus status;

           public:
            Func(DLL& d, const char* name)
                : dll(d), name(name) {}

            bool find() {
                if (auto l = status.is_loaded(); l || status.is_unavailable()) {  // fast pass
                    return l;
                }
                while (true) {
                    if (status.should_load()) {
                        auto [lookedup, tmp] = dll.lookup(name);
                        if (!lookedup) {
                            if (tmp) {
                                status.load_undone();
                            }
                            else {
                                log_fn_load(name, false);
                                status.load_unavilable();
                            }
                            return false;
                        }
                        fn = reinterpret_cast<T*>(lookedup);
                        log_fn_load(name, true);
                        status.load_done();
                        break;
                    }
                    else {
                        switch (status.wait_for_load()) {
                            case LoadState::loaded:
                                goto DONE;
                            case LoadState::unavailable:
                                return false;
                            case LoadState::unused:  // TODO(on-keyday): loop or exit?
                                break;
                            default:
                                break;
                        }
                    }
                }
            DONE:
                return true;
            }

            constexpr T* unsafe_fn() const {
                return fn;
            }
        };

        namespace test {
            inline void a(int, int*, bool){};

            inline void check_instance() {
                DLL dll(fnet_lazy_dll_path("path"), false);

                Func<decltype(a)> a_(dll, "a");
            }
        }  // namespace test

        // for ssl reset
        bool libcrypto_init(utils::fnet::lazy::DLLInit l, InitState on_load);

    }  // namespace fnet::lazy
}  // namespace utils
