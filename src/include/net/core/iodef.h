/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// iodef - define io interface
#pragma once

#include <cstddef>

#include "../../helper/sfinae.h"

#include "../../helper/deref.h"

#include "../../helper/appender.h"

#include "../../helper/view.h"

#include "../../wrap/lite/string.h"

namespace utils {
    namespace net {

        enum class State {
            undefined,
            running,
            complete,
            failed,
            invalid_argument,
        };

        struct WriteInfo {
            const char* byte;
            size_t size;

            size_t written;
        };

        namespace internal {

            SFINAE_BLOCK_T_BEGIN(writeable,
                                 std::declval<T>().write(std::declval<const char*>(), std::declval<size_t>()));
            constexpr static State write(T& t, const char* byte, size_t size) {
                return t.write(byte, size);
            }
            SFINAE_BLOCK_T_ELSE(writeable)
            constexpr static State write(T& t, const char* byte, size_t size) {
                return State::undefined;
            }
            SFINAE_BLOCK_T_END()

            SFINAE_BLOCK_T_BEGIN(readable,
                                 std::declval<T>().read(std::declval<char*>(), std::declval<size_t>(), std::declval<size_t*>()));
            constexpr static State read(T& t, char* byte, size_t size, size_t* red) {
                return t.read(byte, size, red);
            }
            SFINAE_BLOCK_T_ELSE(readable)
            constexpr static State read(T& t, const char* byte, size_t size, size_t* red) {
                return State::undefined;
            }
            SFINAE_BLOCK_T_END()
        }  // namespace internal

        template <class T>
        constexpr State write(T& t, const char* byte, size_t size) {
            return internal::writeable<T>::write(t, byte, size);
        }

        template <class T>
        constexpr State read(T& t, char* byte, size_t size, size_t* red) {
            return internal::readable<T>::read(t, byte, size, red);
        }

        template <size_t inbufsize = 1024, class String, class T>
        constexpr State read(String& buf, T& io) {
            while (true) {
                char inbuf[inbufsize];
                size_t red = 0;
                State st = read(io, inbuf, inbufsize, &red);
                if (st == State::complete) {
                    helper::append(buf, helper::SizedView(inbuf, red));
                    if (red >= inbufsize) {
                        continue;
                    }
                }
                return st;
            }
        }

        struct IO {
           private:
            struct interface {
                virtual State write(const char* byte, size_t size) = 0;
                virtual State read(const char* byte, size_t size, size_t* red) = 0;
                virtual bool is_null() const = 0;
                virtual ~interface() {}
            };

            template <class T>
            struct implement : interface {
                T t;
                State write(const char* byte, size_t size) override {
                    auto v = helper::deref(t);
                    if (!v) {
                        return State::undefined;
                    }
                    return v->write(byte, size);
                }
                State read(const char* byte, size_t size, size_t* red) override {
                    auto v = helper::deref(t);
                    if (!v) {
                        return State::undefined;
                    }
                    return v->read(byte, size, red);
                }

                bool is_null() const override {
                    return helper::deref(t) == nullptr;
                }
            };

            interface* iface = nullptr;
            template <class T>
            void make_iface(T&& t) {
                iface = new implement<std::decay_t<T>>(std::forward<T>(t));
            }

           public:
            template <class T>
            IO(T&& t) {
                make_iface(std::forward<T>(t));
            }

            constexpr IO() {}

            constexpr IO(std::nullptr_t) {}

            IO(IO&& io) {
                iface = io.iface;
                io.iface = nullptr;
            }

            IO& operator=(IO&& io) {
                delete iface;
                iface = io.iface;
                io.iface = nullptr;
                return *this;
            }

            State write(const char* byte, size_t size) {
                if (!iface) {
                    return State::undefined;
                }
                return iface->write(byte, size);
            }
            State read(const char* byte, size_t size, size_t* red) {
                if (!iface) {
                    return State::undefined;
                }
                return iface->read(byte, size, red);
            }

            bool is_null() const {
                return !iface || iface->is_null();
            }

            ~IO() {
                delete iface;
            }
        };

    }  // namespace net
}  // namespace utils
