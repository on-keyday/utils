/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// Code generated by ifacegen (https://github.com/on-keyday/utils)

#pragma once
#include "../helper/deref.h"

#ifndef NOVTABLE__
#ifdef _WIN32
#define NOVTABLE__ __declspec(novtable)
#else
#define NOVTABLE__
#endif
#endif

namespace utils {
    namespace async {
        struct Any {
           private:
            struct NOVTABLE__ interface__ {
                virtual const void* raw__() const noexcept = 0;
                virtual const std::type_info& type__() const noexcept = 0;

                virtual ~interface__() = default;
            };

            template <class T__>
            struct implements__ : interface__ {
                T__ t_holder_;

                template <class V__>
                implements__(V__&& args)
                    : t_holder_(std::forward<V__>(args)) {}

                const void* raw__() const noexcept override {
                    return static_cast<const void*>(std::addressof(t_holder_));
                }

                const std::type_info& type__() const noexcept override {
                    return typeid(T__);
                }
            };

            interface__* iface = nullptr;

           public:
            constexpr Any() {}

            constexpr Any(std::nullptr_t) {}

            template <class T__>
            Any(T__&& t) {
                static_assert(!std::is_same<std::decay_t<T__>, Any>::value, "can't accept same type");
                if (!utils::helper::deref(t)) {
                    return;
                }
                iface = new implements__<std::decay_t<T__>>(std::forward<T__>(t));
            }

            constexpr Any(Any&& in) noexcept {
                iface = in.iface;
                in.iface = nullptr;
            }

            Any& operator=(Any&& in) noexcept {
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

            ~Any() {
                delete iface;
            }

            template <class T__>
            const T__* type_assert() const {
                if (!iface) {
                    return nullptr;
                }
                if (iface->type__() != typeid(T__)) {
                    return nullptr;
                }
                return static_cast<const T__*>(iface->raw__());
            }

            template <class T__>
            T__* type_assert() {
                if (!iface) {
                    return nullptr;
                }
                if (iface->type__() != typeid(T__)) {
                    return nullptr;
                }
                return static_cast<T__*>(const_cast<void*>(iface->raw__()));
            }

            const void* unsafe_cast() const {
                if (!iface) {
                    return nullptr;
                }
                return iface->raw__();
            }

            void* unsafe_cast() {
                if (!iface) {
                    return nullptr;
                }
                return const_cast<void*>(iface->raw__());
            }

            Any(const Any&) = delete;

            Any& operator=(const Any&) = delete;

            Any(Any&) = delete;

            Any& operator=(Any&) = delete;
        };

        template <typename Ctx>
        struct Task {
           private:
            struct NOVTABLE__ interface__ {
                virtual void operator()(Ctx& ctx) const = 0;

                virtual ~interface__() = default;
            };

            template <class T__>
            struct implements__ : interface__ {
                T__ t_holder_;

                template <class V__>
                implements__(V__&& args)
                    : t_holder_(std::forward<V__>(args)) {}

                void operator()(Ctx& ctx) const override {
                    auto t_ptr_ = utils::helper::deref(this->t_holder_);
                    if (!t_ptr_) {
                        return (void)0;
                    }
                    return (*t_ptr_)(ctx);
                }
            };

            interface__* iface = nullptr;

           public:
            constexpr Task() {}

            constexpr Task(std::nullptr_t) {}

            template <class T__>
            Task(T__&& t) {
                static_assert(!std::is_same<std::decay_t<T__>, Task>::value, "can't accept same type");
                if (!utils::helper::deref(t)) {
                    return;
                }
                iface = new implements__<std::decay_t<T__>>(std::forward<T__>(t));
            }

            constexpr Task(Task&& in) noexcept {
                iface = in.iface;
                in.iface = nullptr;
            }

            Task& operator=(Task&& in) noexcept {
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

            ~Task() {
                delete iface;
            }

            void operator()(Ctx& ctx) const {
                return iface ? iface->operator()(ctx) : (void)0;
            }

            Task(const Task&) = delete;

            Task& operator=(const Task&) = delete;

            Task(Task&) = delete;

            Task& operator=(Task&) = delete;
        };

    }  // namespace async
}  // namespace utils
