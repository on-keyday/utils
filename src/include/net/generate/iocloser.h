/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// Code generated by ifacegen (https://github.com/on-keyday/utils)

#pragma once
#include "../../helper/deref.h"
#include "../core/iodef.h"

namespace utils {
    namespace net {
        struct IOClose {
           private:
            struct interface__ {
                virtual State write(const char* ptr, size_t size) = 0;
                virtual State read(char* ptr, size_t size, size_t* red) = 0;
                virtual State close(bool force) = 0;

                virtual ~interface__() {}
            };

            template <class T__>
            struct implements__ : interface__ {
                T__ t_holder_;

                template <class V__>
                implements__(V__&& args)
                    : t_holder_(std::forward<V__>(args)) {}

                State write(const char* ptr, size_t size) override {
                    auto t_ptr_ = utils::helper::deref(this->t_holder_);
                    if (!t_ptr_) {
                        return State::undefined;
                    }
                    return t_ptr_->write(ptr, size);
                }

                State read(char* ptr, size_t size, size_t* red) override {
                    auto t_ptr_ = utils::helper::deref(this->t_holder_);
                    if (!t_ptr_) {
                        return State::undefined;
                    }
                    return t_ptr_->read(ptr, size, red);
                }

                State close(bool force) override {
                    auto t_ptr_ = utils::helper::deref(this->t_holder_);
                    if (!t_ptr_) {
                        return State::undefined;
                    }
                    return t_ptr_->close(force);
                }
            };

            interface__* iface = nullptr;

           public:
            constexpr IOClose() {}

            constexpr IOClose(std::nullptr_t) {}

            template <class T__>
            IOClose(T__&& t) {
                if (!utils::helper::deref(t)) {
                    return;
                }
                iface = new implements__<std::decay_t<T__>>(std::forward<T__>(t));
            }

            IOClose(IOClose&& in) {
                iface = in.iface;
                in.iface = nullptr;
            }

            IOClose& operator=(IOClose&& in) {
                delete iface;
                iface = in.iface;
                in.iface = nullptr;
                return *this;
            }

            explicit operator bool() const {
                return iface != nullptr;
            }

            ~IOClose() {
                delete iface;
            }

            State write(const char* ptr, size_t size) {
                return iface ? iface->write(ptr, size) : State::undefined;
            }

            State read(char* ptr, size_t size, size_t* red) {
                return iface ? iface->read(ptr, size, red) : State::undefined;
            }

            State close(bool force) {
                return iface ? iface->close(force) : State::undefined;
            }

            template <class T__>
            const T__* type_assert() const {
                if (!iface) {
                    return nullptr;
                }
                if (auto ptr = dynamic_cast<implements__<T__>*>(iface)) {
                    return std::addressof(ptr->t_holder_);
                }
                return nullptr;
            }

            template <class T__>
            T__* type_assert() {
                if (!iface) {
                    return nullptr;
                }
                if (auto ptr = dynamic_cast<implements__<T__>*>(iface)) {
                    return std::addressof(ptr->t_holder_);
                }
                return nullptr;
            }
        };

        struct IO {
           private:
            struct interface__ {
                virtual State write(const char* ptr, size_t size) = 0;
                virtual State read(char* ptr, size_t size, size_t* red) = 0;

                virtual ~interface__() {}
            };

            template <class T__>
            struct implements__ : interface__ {
                T__ t_holder_;

                template <class V__>
                implements__(V__&& args)
                    : t_holder_(std::forward<V__>(args)) {}

                State write(const char* ptr, size_t size) override {
                    auto t_ptr_ = utils::helper::deref(this->t_holder_);
                    if (!t_ptr_) {
                        return State::undefined;
                    }
                    return t_ptr_->write(ptr, size);
                }

                State read(char* ptr, size_t size, size_t* red) override {
                    auto t_ptr_ = utils::helper::deref(this->t_holder_);
                    if (!t_ptr_) {
                        return State::undefined;
                    }
                    return t_ptr_->read(ptr, size, red);
                }
            };

            interface__* iface = nullptr;

           public:
            constexpr IO() {}

            constexpr IO(std::nullptr_t) {}

            template <class T__>
            IO(T__&& t) {
                if (!utils::helper::deref(t)) {
                    return;
                }
                iface = new implements__<std::decay_t<T__>>(std::forward<T__>(t));
            }

            IO(IO&& in) {
                iface = in.iface;
                in.iface = nullptr;
            }

            IO& operator=(IO&& in) {
                delete iface;
                iface = in.iface;
                in.iface = nullptr;
                return *this;
            }

            explicit operator bool() const {
                return iface != nullptr;
            }

            ~IO() {
                delete iface;
            }

            State write(const char* ptr, size_t size) {
                return iface ? iface->write(ptr, size) : State::undefined;
            }

            State read(char* ptr, size_t size, size_t* red) {
                return iface ? iface->read(ptr, size, red) : State::undefined;
            }

            template <class T__>
            const T__* type_assert() const {
                if (!iface) {
                    return nullptr;
                }
                if (auto ptr = dynamic_cast<implements__<T__>*>(iface)) {
                    return std::addressof(ptr->t_holder_);
                }
                return nullptr;
            }

            template <class T__>
            T__* type_assert() {
                if (!iface) {
                    return nullptr;
                }
                if (auto ptr = dynamic_cast<implements__<T__>*>(iface)) {
                    return std::addressof(ptr->t_holder_);
                }
                return nullptr;
            }
        };

    }  // namespace net
}  // namespace utils
