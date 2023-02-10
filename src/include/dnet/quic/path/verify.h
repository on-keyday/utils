/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../frame/path.h"
#include "../frame/writer.h"
#include "../ioresult.h"
#include "../ack/ack_lost_record.h"
#include "../transport_error.h"

namespace utils {
    namespace dnet::quic::mtu {
        template <template <class...> class Que>
        struct PathVerifier {
           private:
            std::uint64_t send_data = 0;
            bool verify_required = false;
            std::shared_ptr<ack::ACKLostRecord> wait;
            Que<std::uint64_t> recv_data;

           public:
            constexpr void recv_path_challange(const frame::PathChallengeFrame& resp) noexcept {
                recv_data.push_back(resp.data);
            }

            constexpr IOResult send_path_response(frame::fwriter& w) {
                if (recv_data.size() == 0) {
                    return IOResult::no_data;
                }
                if (w.remain().size() < 9) {
                    return IOResult::no_capacity;
                }
                frame::PathResponseFrame path;
                path.data = recv_data[0];
                recv_data.pop_front();
                return w.write(path) ? IOResult::ok : IOResult::fatal;
            }

            constexpr IOResult send_path_challange(frame::fwriter& w, auto&& observer_vec) {
                if (!verify_required) {
                    return IOResult::no_data;
                }
                if (wait) {
                    if (wait->is_ack() || !wait->is_lost()) {
                        return IOResult::ok;
                    }
                }
                if (w.remain().size() < 9) {
                    return IOResult::no_capacity;
                }
                frame::PathChallengeFrame ch;
                ch.data = send_data;
                if (!w.write(ch)) {
                    return IOResult::fatal;
                }
                if (wait) {
                    wait->wait();
                }
                else {
                    wait = ack::make_ack_wait();
                }
                observer_vec.push_back(wait);
                return IOResult::ok;
            }

            constexpr error::Error recv_path_response(const frame::PathResponseFrame& resp) {
                if (!verify_required) {
                    return QUICError{
                        .msg = "no PathChallange sent before but PathResponse recieved",
                        .transport_error = TransportError::PROTOCOL_VIOLATION,
                        .frame_type = FrameType::PATH_RESPONSE,
                    };
                }
                if (send_data != resp.data) {
                    return QUICError{
                        .msg = "PathResponse data is not matched to this sent",
                        .transport_error = TransportError::PROTOCOL_VIOLATION,
                        .frame_type = FrameType::PATH_RESPONSE,
                    };
                }
                wait = nullptr;
                verify_required = false;
                return error::none;
            }

            constexpr bool request_path_verification(std::uint64_t data) {
                if (verify_required) {
                    return false;
                }
                send_data = data;
                verify_required = true;
                return true;
            }
        };
    }  // namespace dnet::quic::mtu
}  // namespace utils
