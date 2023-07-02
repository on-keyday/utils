/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../../../storage.h"

namespace utils {
    namespace fnet::quic::stream::impl {

        namespace internal {
            template <class T>
            concept has_shift_front = requires(T t) {
                { t.shift_front(size_t()) };
            };
        }

        template <class T = flex_storage>
        struct SendBuffer {
            T src;

            auto data() {
                return src.data();
            }

            size_t size() const {
                return src.size();
            }

            void append(auto&& input) {
                src.append(input);
            }

            void shift_front(size_t n) {
                if constexpr (internal::has_shift_front<decltype(src)>) {
                    src.shift_front(n);
                }
                else {
                    src.erase(0, n);
                }
            }

            void shrink_to_fit() {
                src.shrink_to_fit();
            }

            void get_specific() {}
            void send_callback(auto d) {}
            void recv_callback(auto d) {}
            std::uint64_t fairness_limit() const {
                return ~0;  // No Limit!
            }
        };
    }  // namespace fnet::quic::stream::impl
}  // namespace utils
