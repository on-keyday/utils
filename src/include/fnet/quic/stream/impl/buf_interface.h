/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../../../error.h"
#include "../stream_id.h"
#include "../../types.h"
#include "../fragment.h"
#include "../core/write_state.h"

namespace futils {
    namespace fnet::quic::stream::impl {

        template <class T>
        struct SendBufInterface {
            T impl;

            constexpr void append(core::StreamWriteBufferState& state, auto&&... a) {
                impl.append(state, std::forward<decltype(a)>(a)...);
            }

            constexpr auto data() {
                return impl.data();
            }

            constexpr size_t size() const {
                return impl.size();
            }

            constexpr auto shift_front(size_t n) {
                return impl.shift_front(n);
            }

            constexpr void shrink_to_fit() {
                impl.shrink_to_fit();
            }

            // this is called with unlocked this_
            constexpr void send_callback(auto this_) {
                impl.send_callback(this_);
            }

            // this is called with unlocked this_
            constexpr void recv_callback(auto this_) {
                impl.send_callback(this_);
            }

            constexpr std::uint64_t fairness_limit() {
                return impl.fairness_limit();
            }

            constexpr auto get_specific() {
                return impl.get_specific();
            }

            constexpr void on_data_added(std::shared_ptr<void>&& conn_ctx, StreamID id) {
                impl.on_data_added(std::move(conn_ctx), id);
            }
        };

        template <class T>
        struct RecvBufInterface {
            T impl;

            std::pair<bool, error::Error> save(StreamID id, FrameType type, Fragment frag, std::uint64_t recv_bytes, std::uint64_t err_code) {
                return impl.save(id, type, frag, recv_bytes, err_code);
            }

            constexpr void send_callback(auto this_) {
                impl.send_callback(this_);
            }

            constexpr void recv_callback(auto this_) {
                impl.send_callback(this_);
            }

            constexpr auto get_specific() {
                return impl.get_specific();
            }
        };
    }  // namespace fnet::quic::stream::impl
}  // namespace futils
