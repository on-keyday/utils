/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// iodef - define io interface
#pragma once

#include <cstddef>

#include "../../helper/sfinae.h"

#include "../../helper/deref.h"

#include <helper/appender.h>

#include "../../helper/view.h"

// #include "../../wrap/light/string.h"

namespace utils {
    namespace net {

        enum class State {
            undefined,
            running,
            complete,
            failed,
            invalid_argument,
        };

        struct ReadInfo {
            char* byte;
            size_t size;
            size_t read;
            int err;
        };

        struct WriteInfo {
            const char* byte;
            size_t size;
            size_t written;
            int err;
        };

        namespace internal {

            SFINAE_BLOCK_T_BEGIN(writeable,
                                 std::declval<T>().write(std::declval<const char*>(), std::declval<size_t>()));
            constexpr static State write(T& t, const char* byte, size_t size) {
                return t.write(byte, size);
            }
            SFINAE_BLOCK_T_ELSE(writeable)

            SFINAE_BLOCK_T_END()

            SFINAE_BLOCK_T_BEGIN(readable,
                                 std::declval<T>().read(std::declval<char*>(), std::declval<size_t>(), std::declval<size_t*>()));
            constexpr static State read(T& t, char* byte, size_t size, size_t* red) {
                return t.read(byte, size, red);
            }
            SFINAE_BLOCK_T_ELSE(readable)
            /*constexpr static State read(T& t, const char* byte, size_t size, size_t* red) {
                return State::undefined;
            }*/
            SFINAE_BLOCK_T_END()
        }  // namespace internal

        template <class T>
        constexpr State write(T& t, const char* byte, size_t size) {
            return t.write(byte, size);
        }

        template <class T>
        constexpr State read(T& t, char* byte, size_t size, size_t* red) {
            return t.read(byte, size, red);
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

    }  // namespace net
}  // namespace utils
