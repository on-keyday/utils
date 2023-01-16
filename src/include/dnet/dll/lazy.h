/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// lazy_func - lazy dll load and function load
#pragma once
#include <atomic>
#include "../../helper/defer.h"

extern void abort();

namespace utils {
    namespace dnet::lazy {
#ifdef _WIN32
        using dll_path = const wchar_t*;
#elif
        using dll_path = const char*;
#endif

        using func_t = void (*)();

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

        struct DLL {
           private:
            friend struct DLLInit;
            dll_path path;
            void* target = nullptr;
            std::atomic_bool loaded;
            AtomicLock lock;
            bool sys = false;

            bool (*init_hook)(DLLInit dll, bool on_load);
            std::pair<func_t, bool> (*lookup_hook)(DLLInit dll, const char* name);

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
                if (loaded) {
                    return true;
                }
                lock.lock();
                const auto r = helper::defer([&] {
                    lock.unlock();
                });
                if (loaded) {
                    return true;
                }
                if (!load_platform()) {
                    return false;
                }
                if (init_hook) {
                    if (!init_hook(DLLInit{this}, true)) {
                        return false;
                    }
                }
                loaded.store(true);
                return true;
            }

            func_t lookup(const char* name) {
                if (!load()) {
                    return false;
                }
                return lookup_unsafe(name);
            }

            DLL(dll_path path, bool is_sys, bool (*init)(DLLInit, bool) = nullptr, std::pair<func_t, bool> (*find)(DLLInit, const char*) = nullptr)
                : path(path), sys(is_sys), init_hook(init), lookup_hook(find) {}

            void replace_path(dll_path new_path, bool is_sys, bool (*init)(DLLInit, bool) = nullptr, std::pair<func_t, bool> (*find)(DLLInit, const char*) = nullptr) {
                lock.lock();
                const auto r = helper::defer([&] { lock.unlock(); });
                if (loaded) {
                    return;
                }
                this->path = new_path;
                this->sys = is_sys;
                this->init_hook = init;
                this->lookup_hook = find;
            }

            ~DLL() {
                if (loaded) {
                    if (init_hook) {
                        init_hook(DLLInit{this}, false);
                    }
                    unload_platform();
                }
            }
        };

        template <class Derived, class T>
        struct Call;

        template <class Derived, class Ret, class... Arg>
        struct Call<Derived, Ret(Arg...)> {
            Ret operator()(Arg... arg) {
                auto p = static_cast<Derived*>(this);
                if (!p->find()) {
                    abort();
                }
                return p->fn(arg...);
            }

            constexpr Ret unsafe_call(Arg... arg) {
                return static_cast<Derived*>(this)->fn(arg...);
            }
        };

        template <class T>
        struct Func : Call<Func<T>, T> {
            friend struct Call<Func<T>, T>;

           private:
            DLL& dll;
            std::atomic<T*> fn = nullptr;
            const char* name;
            AtomicLock lock;

           public:
            Func(DLL& d, const char* name)
                : dll(d), name(name) {}

            bool find() {
                if (fn) {
                    return true;
                }
                lock.lock();
                const auto r = helper::defer([&] { lock.unlock(); });
                if (fn) {
                    return true;
                }
                fn = reinterpret_cast<T*>(dll.lookup(name));
                return fn != nullptr;
            }
        };

        namespace test {
            inline void a(int, int*, bool){};

            inline void check_instance() {
#ifdef _WIN32
                DLL dll(L"path", false);
#else
                DLL dll("path", false);
#endif

                Func<decltype(a)> a_(dll, "a");
            }
        }  // namespace test

    }  // namespace dnet::lazy
}  // namespace utils
