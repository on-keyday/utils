/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// Code generated by ifacegen (https://github.com/on-keyday/utils)

#pragma once
#include "../../helper/deref.h"
#include <platform/detect.h>

#ifndef NOVTABLE__
#ifdef UTILS_PLATFORM_WINDOWS
#define NOVTABLE__ __declspec(novtable)
#else
#define NOVTABLE__
#endif
#endif

namespace utils {
    namespace cmdline {
        namespace subcmd {
            template <typename Ctx>
            struct CommandRunner {
               private:
                struct NOVTABLE__ interface__ {
                    virtual int operator()(Ctx& ctx) = 0;

                    virtual ~interface__() = default;
                };

                template <class T__>
                struct implements__ : interface__ {
                    T__ t_holder_;

                    template <class V__>
                    implements__(V__&& args)
                        : t_holder_(std::forward<V__>(args)) {}

                    int operator()(Ctx& ctx) override {
                        auto t_ptr_ = utils::helper::deref(this->t_holder_);
                        if (!t_ptr_) {
                            return int{};
                        }
                        return (*t_ptr_)(ctx);
                    }
                };

                interface__* iface = nullptr;

               public:
                constexpr CommandRunner() {}

                constexpr CommandRunner(std::nullptr_t) {}

                template <class T__>
                CommandRunner(T__&& t) {
                    static_assert(!std::is_same<std::decay_t<T__>, CommandRunner>::value, "can't accept same type");
                    if (!utils::helper::deref(t)) {
                        return;
                    }
                    iface = new implements__<std::decay_t<T__>>(std::forward<T__>(t));
                }

                constexpr CommandRunner(CommandRunner&& in) noexcept {
                    iface = in.iface;
                    in.iface = nullptr;
                }

                CommandRunner& operator=(CommandRunner&& in) noexcept {
                    if (this == std::addressof(in)) return *this;
                    delete iface;
                    iface = in.iface;
                    in.iface = nullptr;
                    return *this;
                }

                explicit operator bool() const noexcept {
                    return iface != nullptr;
                }

                bool operator==(std::nullptr_t) const noexcept {
                    return iface == nullptr;
                }

                ~CommandRunner() {
                    delete iface;
                }

                int operator()(Ctx& ctx) {
                    return iface ? iface->operator()(ctx) : int{};
                }

                CommandRunner(const CommandRunner&) = delete;

                CommandRunner& operator=(const CommandRunner&) = delete;

                CommandRunner(CommandRunner&) = delete;

                CommandRunner& operator=(CommandRunner&) = delete;
            };

        }  // namespace subcmd
    }      // namespace cmdline
}  // namespace utils
