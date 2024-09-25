/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <memory>
#include "../../frame/writer.h"
#include "../../ack/ack_lost_record.h"

namespace futils {
    namespace fnet::quic::stream::impl {
        template <class Impl>
        struct ConnHandlerInterface {
           private:
            Impl impl;

           public:
            constexpr Impl* impl_ptr() {
                return std::addressof(impl);
            }

            constexpr void uni_open(auto&& s) {
                impl.uni_open(std::forward<decltype(s)>(s));
            }

            constexpr void uni_accept(auto&& s) {
                impl.uni_accept(std::forward<decltype(s)>(s));
            }

            constexpr void bidi_open(auto&& s) {
                impl.bidi_open(std::forward<decltype(s)>(s));
            }

            constexpr void bidi_accept(auto&& s) {
                impl.bidi_accept(std::forward<decltype(s)>(s));
            }

            constexpr IOResult send_schedule(frame::fwriter& fw, ack::ACKRecorder& observer, auto&& do_default) {
                return impl.send_schedule(fw, observer, do_default);
            }

            // this is called with unlocked this_
            constexpr void send_callback(auto this_) {
                impl.send_callback(this_);
            }

            // this is called with unlocked this_
            constexpr void recv_callback(auto this_) {
                impl.recv_callback(this_);
            }

            constexpr void on_remove_stream(auto this_) {
                impl.on_remove_stream(this_);
            }
        };
    }  // namespace fnet::quic::stream::impl
}  // namespace futils
