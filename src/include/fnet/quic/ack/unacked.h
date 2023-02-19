/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// unacked - unacked packet numbers
#pragma once
#include "../../std/vector.h"
#include "../packet_number.h"
#include "../frame/ack.h"
#include "../status/pn_space.h"
#include "ack_waiters.h"
#include "../frame/writer.h"
#include <algorithm>

namespace utils {
    namespace fnet::quic::ack {

        struct UnackedPackets {
           private:
            slib::vector<packetnum::Value> unacked_packets[3]{};
            ACKRangeVec buffer;

            constexpr bool generate_ragnes_from_received(status::PacketNumberSpace space) {
                buffer.clear();
                auto& recved = unacked_packets[space_to_index(space)];
                const auto size = recved.size();
                if (size == 0) {
                    return false;
                }
                if (size == 1) {
                    auto res = recved[0].value;
                    buffer.push_back(frame::ACKRange{res, res});
                    return true;
                }
                std::sort(recved.begin(), recved.end(), std::greater<>{});
                frame::ACKRange cur;
                cur.largest = std::uint64_t(recved[0]);
                cur.smallest = cur.largest;
                for (std::uint64_t i = 1; i < size; i++) {
                    if (recved[i - 1] == recved[i] + 1) {
                        cur.smallest = recved[i];
                    }
                    else {
                        buffer.push_back(std::move(cur));
                        cur.largest = recved[i];
                        cur.smallest = cur.largest;
                    }
                }
                buffer.push_back(std::move(cur));
                return true;
            }

           public:
            void on_ack_sent(status::PacketNumberSpace space) {
                unacked_packets[space_to_index(space)].clear();
            }

            void on_packet_processed(status::PacketNumberSpace space, packetnum::Value val, bool is_ack_eliciting) {
                if (is_ack_eliciting) {
                    unacked_packets[space_to_index(space)].push_back(val);
                }
            }

            error::Error send(frame::fwriter& w, status::PacketNumberSpace space) {
                if (!generate_ragnes_from_received(space)) {
                    return error::none;
                }
                auto [ack, ok] = frame::convert_to_ACKFrame<slib::vector>(buffer);
                if (!ok) {
                    return error::Error("generate ack range failed. library bug!!");
                }
                if (w.remain().size() < ack.len()) {
                    return error::none;  // wait next chance
                }
                if (!w.write(ack)) {
                    return error::Error("failed to render ACKFrame");
                }
                on_ack_sent(space);
                return error::none;
            }
        };
    }  // namespace fnet::quic::ack
}  // namespace utils
