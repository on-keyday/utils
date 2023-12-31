/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../../../error.h"
#include "../stream_id.h"
#include "../fragment.h"
#include <memory>

namespace utils {
    namespace fnet::quic::stream::impl {
        // returns (all_recved,err)
        template <class Arg>
        using SaveFragmentCallback = std::pair<bool, error::Error> (*)(Arg& arg, StreamID id, FrameType type, Fragment frag, std::uint64_t total_recv_bytes, std::uint64_t err_code);

        // this has two mode:
        // query_update = true: if this returns non-zero, then call update
        // query_update = false: if this returns larger limit than lim.current_limit() then update limit
        template <class Arg>
        using AutoUpdateCallback = std::uint64_t (*)(Arg&, core::Limiter lim, std::uint64_t initial_limit, bool query_update);

        template <class Arg = std::shared_ptr<void>>
        struct FragmentSaver {
           private:
            Arg arg;
            SaveFragmentCallback<Arg> callback = nullptr;
            AutoUpdateCallback<Arg> auto_update = nullptr;

            void do_auto_update(auto t) {
                if (!auto_update) {
                    return;
                }
                if (!auto_update(arg, {}, 0, true)) {
                    return;
                }
                t->update_recv_limit([&](core::Limiter lim, std::uint64_t ini_limit) {
                    return auto_update(arg, lim, ini_limit, false);
                });
            }

           public:
            constexpr FragmentSaver() = default;
            FragmentSaver(const Arg& a, SaveFragmentCallback<Arg> cb, AutoUpdateCallback<Arg> aup = nullptr) {
                if (cb) {
                    arg = a;
                    callback = cb;
                    auto_update = aup;
                }
                else {
                    arg = Arg{};
                    callback = nullptr;
                }
            }

            std::pair<bool, error::Error> save(StreamID id, FrameType type, Fragment frag, std::uint64_t total_recv, std::uint64_t error_code) {
                if (callback) {
                    return callback(arg, id, type, frag, total_recv, error_code);
                }
                return {false, error::none};
            }

            Arg get_specific() {
                return arg;
            }

            void recv_callback(auto d) {
                do_auto_update(d);
            }
            void send_callback(auto d) {
                do_auto_update(d);
            }
        };
    }  // namespace fnet::quic::stream::impl
}  // namespace utils
