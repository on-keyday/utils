/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <cstdint>
#include <cstddef>

namespace utils {
    namespace fnet {
        using func_t = void (*)();
        using loader_t = func_t (*)(void*, const char*);
        using boot_t = bool (*)(void*, void*, loader_t);
        struct Boot {
#ifdef _DEBUG
            const char* funcname;
#endif
            boot_t load;
            bool not_must = false;
            constexpr Boot(const char* n, boot_t l, bool not_must = false)
                : load(l), not_must(not_must) {
#ifdef _DEBUG
                funcname = n;
#endif
            }
            constexpr Boot() = default;
            constexpr Boot& operator=(const Boot&) = default;
        };

        struct do_load {
            constexpr do_load(auto&& fn) {
                fn();
            }
        };

#define LOADER_BASE(func_type, func_name, libp, CLASS, not_must)                 \
   private:                                                                      \
    do_load loader_of_##func_name{                                               \
        [&] {                                                                    \
            auto fn = +[](void* cls, void* p, loader_t load) {                   \
                auto ptr = static_cast<CLASS*>(cls);                             \
                ptr->func_name##_ =                                              \
                    reinterpret_cast<decltype(func_type)*>(load(p, #func_name)); \
                return ptr->func_name##_ != nullptr;                             \
            };                                                                   \
            libp.push_back(Boot{#func_name, fn, not_must});                      \
        },                                                                       \
    };                                                                           \
                                                                                 \
   public:                                                                       \
    decltype(func_type)* func_name##_ = nullptr;

        template <size_t len>
        struct alib_nptr {
            Boot buf[len];
            size_t index = 0;
            constexpr void push_back(Boot b) {
                if (len <= index) {
                    // terminate!
                    auto ptr = (int*)(nullptr);
                    *ptr = 0;
                }
                buf[index] = b;
                index++;
            }
            constexpr bool load_all(void* libp, void* instance, loader_t loader) {
                for (Boot& bo : buf) {
                    if (!bo.load) {
                        // terminate!
                        int* o = nullptr;
                        *o = 0;
                    }
                    if (!bo.load(instance, libp, loader)) {
                        if (!bo.not_must) {
                            return false;
                        }
                    }
                }
                return true;
            }
        };

        template <size_t len>
        struct alib : public alib_nptr<len> {
            void* libp;
            using alib_nptr<len>::load_all;
            constexpr bool load_all(void* instance, loader_t loader) {
                return alib_nptr<len>::load_all(libp, instance, loader);
            }
        };
        using load_all_t = bool (*)(void* this_, void* lh, void* lp, loader_t loader);
        bool load_library_impl(
            void* this_, void* libp,
            load_all_t load_all, void*& libh,
            const void* libname, bool syslib, bool use_exists);
    }  // namespace fnet
}  // namespace utils
