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

#include "../../wrap/lite/string.h"

namespace utils {
    namespace net {

        enum class State {
            undefined,
            running,
            complete,
            failed,
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
            constexpr static State read(T& t, const char* byte, size_t size, size_t* red) {
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
        constexpr State read(T& t, const char* byte, size_t size, size_t* red) {
            return internal::readable<T>::read(t, byte, size, red);
        }

    }  // namespace net
}  // namespace utils
