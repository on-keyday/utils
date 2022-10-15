/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// raii - resource allocation is initialization
#pragma once
#include <utility>
#include <memory>

namespace utils {
    namespace quic {
        namespace mem {

            template <class T, class Fn>
            struct RAII {
                T t;
                Fn fn;
                constexpr RAII(T resource, Fn dest)
                    : t(std::move(resource)), fn(std::move(dest)) {}
                ~RAII() {
                    fn(t);
                }

                operator T&() {
                    return t;
                }

                T& operator()() {
                    return t;
                }

                auto operator->() {
                    if constexpr (std::is_pointer_v<T>) {
                        return t;
                    }
                    else {
                        return std::addressof(t);
                    }
                }
            };
        }  // namespace mem
    }      // namespace quic
}  // namespace utils
