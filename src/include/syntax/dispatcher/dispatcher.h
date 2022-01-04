/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// Code generated by ifacegen (https://github.com/on-keyday/utils)

#pragma once
#include "../../helper/deref.h"
#include "../matching/state.h"

#ifndef NOVTABLE__
#ifdef _WIN32
#define NOVTABLE__ __declspec(novtable)
#else
#define NOVTABLE__
#endif
#endif

namespace utils {
    namespace syntax {
        template <typename T>
        struct Filter {
           private:
            struct NOVTABLE__ interface__ {
                virtual bool operator()(T& ctx) = 0;

                virtual ~interface__() {}
            };

            template <class T__>
            struct implements__ : interface__ {
                T__ t_holder_;

                template <class V__>
                implements__(V__&& args)
                    : t_holder_(std::forward<V__>(args)) {}

                bool operator()(T& ctx) override {
                    auto t_ptr_ = utils::helper::deref(this->t_holder_);
                    if (!t_ptr_) {
                        return false;
                    }
                    return (*t_ptr_)(ctx);
                }
            };

            interface__* iface = nullptr;

           public:
            constexpr Filter() {}

            constexpr Filter(std::nullptr_t) {}

            template <class T__>
            Filter(T__&& t) {
                if (!utils::helper::deref(t)) {
                    return;
                }
                iface = new implements__<std::decay_t<T__>>(std::forward<T__>(t));
            }

            Filter(Filter&& in) {
                iface = in.iface;
                in.iface = nullptr;
            }

            Filter& operator=(Filter&& in) {
                delete iface;
                iface = in.iface;
                in.iface = nullptr;
                return *this;
            }

            explicit operator bool() const {
                return iface != nullptr;
            }

            ~Filter() {
                delete iface;
            }

            bool operator()(T& ctx) {
                return iface ? iface->operator()(ctx) : false;
            }
        };

        template <typename T>
        struct Dispatch {
           private:
            struct NOVTABLE__ interface__ {
                virtual MatchState operator()(T& ctx) = 0;

                virtual ~interface__() {}
            };

            template <class T__>
            struct implements__ : interface__ {
                T__ t_holder_;

                template <class V__>
                implements__(V__&& args)
                    : t_holder_(std::forward<V__>(args)) {}

                MatchState operator()(T& ctx) override {
                    auto t_ptr_ = utils::helper::deref(this->t_holder_);
                    if (!t_ptr_) {
                        return MatchState::succeed;
                    }
                    return (*t_ptr_)(ctx);
                }
            };

            interface__* iface = nullptr;

           public:
            constexpr Dispatch() {}

            constexpr Dispatch(std::nullptr_t) {}

            template <class T__>
            Dispatch(T__&& t) {
                if (!utils::helper::deref(t)) {
                    return;
                }
                iface = new implements__<std::decay_t<T__>>(std::forward<T__>(t));
            }

            Dispatch(Dispatch&& in) {
                iface = in.iface;
                in.iface = nullptr;
            }

            Dispatch& operator=(Dispatch&& in) {
                delete iface;
                iface = in.iface;
                in.iface = nullptr;
                return *this;
            }

            explicit operator bool() const {
                return iface != nullptr;
            }

            ~Dispatch() {
                delete iface;
            }

            MatchState operator()(T& ctx) {
                return iface ? iface->operator()(ctx) : MatchState::succeed;
            }
        };

    }  // namespace syntax
}  // namespace utils
