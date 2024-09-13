/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../../../storage.h"
#include "../core/write_state.h"

namespace futils {
    namespace fnet::quic::stream::impl {

        namespace internal {
            template <class T>
            concept has_shift_front = requires(T t) {
                { t.shift_front(size_t()) };
            };
        }  // namespace internal

        template <class Buffer = flex_storage>
        struct SendBuffer {
            void (*on_data_added_cb)(std::shared_ptr<void>&& conn_ctx, StreamID id) = nullptr;
            Buffer src;

            auto data() {
                return src.data();
            }

            size_t size() const {
                return src.size();
            }

            void append(core::StreamWriteBufferState& s, auto&&... input) {
                auto old = src.size();
                (src.append(std::forward<decltype(input)>(input)), ...);
                s.perfect_written_buffer_count = sizeof...(input);
                s.written_size_of_last_buffer = 0;
                s.result = IOResult::ok;
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

            auto get_specific() {
                return this;
            }
            void send_callback(auto d) {}
            void recv_callback(auto d) {}
            std::uint64_t fairness_limit() const {
                return ~0;  // No Limit!
            }

            void on_data_added(std::shared_ptr<void>&& conn_ctx, StreamID id) {
                if (on_data_added_cb) {
                    on_data_added_cb(std::move(conn_ctx), id);
                }
            }
        };
    }  // namespace fnet::quic::stream::impl
}  // namespace futils
