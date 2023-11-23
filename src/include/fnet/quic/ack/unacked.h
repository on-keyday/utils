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
#include "../time.h"
#include "../status/config.h"
#include "../../error.h"
#include "../ioresult.h"

namespace utils {
    namespace fnet::quic::ack {

        // old implementation of RecvPacketHistory
        // this will not ackowledge non-ackeliciting packet
        struct UnackedPackets {
           private:
            slib::vector<packetnum::Value> unacked_packets[3]{};
            ACKRangeVec buffer;
            time::Time last_recv = time::invalid;
            time::Timer ack_delay;
            std::uint64_t ack_sent_count = 0;

            constexpr bool generate_ragnes_from_received(status::PacketNumberSpace space) {
                buffer.clear();
                auto& recved = unacked_packets[space_to_index(space)];
                const auto size = recved.size();
                if (size == 0) {
                    return false;
                }
                if (size == 1) {
                    auto res = recved[0].as_uint();
                    buffer.push_back(frame::ACKRange{res, res});
                    return true;
                }
                std::sort(recved.begin(), recved.end(), std::greater<>{});
                frame::ACKRange cur;
                cur.largest = std::uint64_t(recved[0]);
                cur.smallest = cur.largest;
                for (std::uint64_t i = 1; i < size; i++) {
                    if (recved[i - 1] == recved[i] + 1) {
                        cur.smallest = recved[i].as_uint();
                    }
                    else {
                        buffer.push_back(std::move(cur));
                        cur.largest = recved[i].as_uint();
                        cur.smallest = cur.largest;
                    }
                }
                buffer.push_back(std::move(cur));
                return true;
            }

           public:
            void reset() {
                for (auto& up : unacked_packets) {
                    up.clear();
                }
                buffer.clear();
                last_recv = time::invalid;
                ack_delay.cancel();
                ack_sent_count = 0;
            }

            constexpr bool is_duplicated(status::PacketNumberSpace, packetnum::Value pn) const {
                return false;
            }

            constexpr void delete_onertt_under(packetnum::Value) {
            }

            constexpr void on_packet_number_space_discarded(status::PacketNumberSpace space) {}

            void on_ack_sent(status::PacketNumberSpace space) {
                unacked_packets[space_to_index(space)].clear();
                ack_delay.cancel();
                last_recv = time::invalid;
                ack_sent_count++;
            }

            size_t ack_eliciting_packet_since_last_ack() const {
                return unacked_packets[0].size() +
                       unacked_packets[1].size() +
                       unacked_packets[2].size();
            }

            void on_packet_processed(status::PacketNumberSpace space, packetnum::Value val, bool is_ack_eliciting,
                                     const status::InternalConfig& config) {
                if (is_ack_eliciting) {
                    unacked_packets[space_to_index(space)].push_back(val);
                    if (space == status::PacketNumberSpace::application &&
                        ack_delay.not_working() && ack_eliciting_packet_since_last_ack() < config.delay_ack_packet_count) {
                        last_recv = config.clock.now();
                        auto delay = last_recv + config.clock.to_clock_granularity(config.local_max_ack_delay);
                        ack_delay.set_deadline(delay);
                    }
                }
            }

            bool should_send_ack(const status::InternalConfig& config) const {
                return !config.use_ack_delay ||
                       (ack_delay.timeout(config.clock) ||
                        ack_eliciting_packet_since_last_ack() >= config.delay_ack_packet_count);
            }

            IOResult send(frame::fwriter& w, status::PacketNumberSpace space, const status::InternalConfig& config, packetnum::Value&) {
                if (!should_send_ack(config)) {
                    return IOResult::no_data;
                }
                if (!generate_ragnes_from_received(space)) {
                    return IOResult::ok;
                }
                auto [ack, ok] = frame::convert_to_ACKFrame<slib::vector>(buffer);
                if (!ok) {
                    return IOResult::fatal;
                }
                if (space == status::PacketNumberSpace::application && !ack_delay.not_working()) {
                    auto delay = config.clock.now() - last_recv;
                    ack.ack_delay = frame::encode_ack_delay(delay, config.local_ack_delay_exponent);
                }
                if (w.remain().size() < ack.len()) {
                    return IOResult::no_capacity;  // wait next chance
                }
                if (!w.write(ack)) {
                    return IOResult::fatal;
                }
                on_ack_sent(space);
                return IOResult::ok;
            }

            time::Time get_deadline() const {
                return ack_delay.get_deadline();
            }
        };

    }  // namespace fnet::quic::ack
}  // namespace utils
