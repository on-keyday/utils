/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
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

        template <class Arg = std::shared_ptr<void>>
        struct FragmentSaver {
           private:
            Arg arg;
            SaveFragmentCallback<Arg> callback = nullptr;

           public:
            constexpr FragmentSaver() = default;
            FragmentSaver(Arg&& a, SaveFragmentCallback<Arg> cb) {
                if (cb) {
                    arg = std::move(a);
                    callback = cb;
                }
                else {
                    arg = Arg{};
                    callback = nullptr;
                }
            }

            FragmentSaver(const Arg& a, SaveFragmentCallback<Arg> cb) {
                if (cb) {
                    arg = a;
                    callback = cb;
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

            void recv_callback(auto d) {}
            void send_callback(auto d) {}
        };
    }  // namespace fnet::quic::stream::impl
}  // namespace utils
