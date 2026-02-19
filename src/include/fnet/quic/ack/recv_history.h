/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../../std/list.h"
#include <wrap/pair_iter.h>
#include "../time.h"
#include "../ioresult.h"
#include "ack_waiters.h"
#include "../status/config.h"
#include "../frame/writer.h"
#include "../status/pn_space.h"

namespace futils {
    namespace fnet::quic::ack {
        struct RecvRange {
            packetnum::Value begin;  // include
            packetnum::Value end;    // include
        };

        struct RecvSpaceHistory {
           private:
            slib::list<RecvRange> ranges;
            packetnum::Value ignore_under = 0;
            packetnum::Value lowest_recved_since_last_ack = packetnum::infinity;
            packetnum::Value largest_recved_since_last_ack = packetnum::infinity;
            std::uint64_t ack_eliciting_packet_since_last_ack = 0;

           public:
            constexpr std::uint64_t ack_eliciting_packets_since_last_ack() const {
                return ack_eliciting_packet_since_last_ack;
            }

            void reset() {
                ranges.clear();
                ignore_under = 0;
                lowest_recved_since_last_ack = packetnum::infinity;
                largest_recved_since_last_ack = packetnum::infinity;
                ack_eliciting_packet_since_last_ack = 0;
            }

            bool is_duplicated(packetnum::Value pn) const {
                if (pn < ignore_under) {
                    return true;
                }
                auto it = ranges.rbegin();
                for (; it != ranges.rend(); it++) {
                    if (it->begin <= pn && pn <= it->end) {
                        return true;
                    }
                    else if (it->end < pn) {
                        return false;
                    }
                }
                return false;
            }

            void on_ack_sent() {
                ack_eliciting_packet_since_last_ack = 0;
                largest_recved_since_last_ack = packetnum::infinity;
                lowest_recved_since_last_ack = packetnum::infinity;
            }
            bool add_to_range(packetnum::Value pn) {
                if (pn < ignore_under) {
                    return false;
                }
                auto it = ranges.rbegin();
                auto prev = ranges.rend();
                for (; it != ranges.rend(); it++) {
                    if (it->end + 1 == pn) {
                        it->end = pn;
                        if (prev != ranges.rend()) {
                            if (prev->begin == it->end) {
                                prev->begin = it->begin;
                                ranges.erase(it.base());
                            }
                        }
                        return true;
                    }
                    if (it->end < pn + 1) {
                        ranges.insert(it.base(), RecvRange{.begin = pn, .end = pn});
                        return true;
                    }
                    if (it->begin <= pn && pn <= it->end) {
                        return false;  // this is not an error but should be refused by is_duplicated function
                    }
                    prev = it;
                }
                if (prev != ranges.rend()) {
                    if (pn + 1 == prev->begin) {
                        prev->begin = pn;
                        return true;
                    }
                }
                ranges.push_front(RecvRange{.begin = pn, .end = pn});
                return true;
            }

            // returns (should_removed_by_is_duplicate_function)
            bool on_packet_processed(packetnum::Value pn, bool ack_eliciting) {
                if (ack_eliciting) {
                    ack_eliciting_packet_since_last_ack++;
                }
                if (lowest_recved_since_last_ack == packetnum::infinity) {
                    lowest_recved_since_last_ack = pn;
                    largest_recved_since_last_ack = pn;
                }
                if (pn < lowest_recved_since_last_ack) {
                    lowest_recved_since_last_ack = pn;
                }
                if (pn > largest_recved_since_last_ack) {
                    largest_recved_since_last_ack = pn;
                }

                return add_to_range(pn);
            }

            bool delete_under(packetnum::Value pn) {
                if (pn < ignore_under) {
                    return false;
                }
                ignore_under = pn;
                for (auto it = ranges.begin(); it != ranges.end();) {
                    if (it->end < pn) {
                        it = ranges.erase(it);
                        continue;
                    }
                    if (it->begin <= pn && pn <= it->end) {
                        it->begin = pn;
                        return true;
                    }
                    it++;
                }
                return true;
            }

            void get_ranges(ack::ACKRangeVec& r) {
                if (largest_recved_since_last_ack == packetnum::infinity ||
                    lowest_recved_since_last_ack == packetnum::infinity) {
                    return;
                }
                for (auto it = ranges.rbegin(); it != ranges.rend(); it++) {
                    if (largest_recved_since_last_ack < it->begin) {
                        continue;
                    }
                    if (it->end < lowest_recved_since_last_ack) {
                        break;
                    }
                    auto e = it->end;
                    auto b = it->begin;
                    if (largest_recved_since_last_ack < e) {
                        e = largest_recved_since_last_ack;
                    }
                    if (b < lowest_recved_since_last_ack) {
                        b = lowest_recved_since_last_ack;
                    }
                    r.push_back(frame::ACKRange{
                        .largest = std::uint64_t(e),
                        .smallest = std::uint64_t(b),
                    });
                }
            }

            IOResult send(frame::fwriter& w, ACKRangeVec& buffer, time::utime_t* delay, const status::InternalConfig& config, packetnum::Value& largest_ack) {
                largest_ack = packetnum::infinity;
                if (ack_eliciting_packet_since_last_ack == 0) {
                    return IOResult::no_data;
                }
                buffer.clear();
                get_ranges(buffer);
                if (buffer.size() == 0) {
                    return IOResult::no_data;
                }
                auto [f, ok] = frame::convert_to_ACKFrame<slib::vector>(buffer);
                if (!ok) {
                    return IOResult::fatal;
                }
                if (delay) {
                    f.ack_delay = frame::encode_ack_delay(*delay, config.local_ack_delay_exponent);
                }
                if (w.remain().size() < f.len()) {
                    return IOResult::no_capacity;
                }
                if (!w.write(f)) {
                    return IOResult::fatal;
                }
                largest_ack = f.largest_ack.value;
                on_ack_sent();
                return IOResult::ok;
            }
        };

        struct RecvPacketHistory {
           private:
            time::Time last_recv = time::invalid;
            time::Timer ack_delay;
            std::uint64_t ack_sent_count = 0;
            ACKRangeVec buffer;
            RecvSpaceHistory initial, handshake, app;

           public:
            void reset() {
                last_recv = time::invalid;
                ack_delay.cancel();
                ack_sent_count = 0;
                buffer.clear();
                initial.reset();
                handshake.reset();
                app.reset();
            }

            bool is_duplicated(status::PacketNumberSpace space, packetnum::Value pn) const {
                switch (space) {
                    default:
                        return false;
                    case status::PacketNumberSpace::initial:
                        return initial.is_duplicated(pn);
                    case status::PacketNumberSpace::handshake:
                        return handshake.is_duplicated(pn);
                    case status::PacketNumberSpace::application:
                        return app.is_duplicated(pn);
                }
            }

            bool on_packet_processed(status::PacketNumberSpace space, packetnum::Value pn, bool is_ack_eliciting,
                                     const status::InternalConfig& config) {
                switch (space) {
                    default:
                        return false;  // bug?
                    case status::PacketNumberSpace::initial:
                        return initial.on_packet_processed(pn, is_ack_eliciting);
                    case status::PacketNumberSpace::handshake:
                        return handshake.on_packet_processed(pn, is_ack_eliciting);
                    case status::PacketNumberSpace::application: {
                        bool res = app.on_packet_processed(pn, is_ack_eliciting);
                        if (ack_delay.not_working() &&
                            app.ack_eliciting_packets_since_last_ack() < config.delay_ack_packet_count) {
                            last_recv = config.clock.now();
                            auto delay = last_recv + config.clock.to_clock_granularity(config.local_max_ack_delay);
                            ack_delay.set_deadline(delay);
                        }
                        return res;
                    }
                }
            }

            bool should_send_app_ack(const status::InternalConfig& config) const {
                return !config.use_ack_delay ||
                       (ack_delay.timeout(config.clock) ||
                        app.ack_eliciting_packets_since_last_ack() >= config.delay_ack_packet_count);
            }

           private:
            void on_ack_sent() {
                ack_delay.cancel();  // cancel timer
                last_recv = time::invalid;
            }

           public:
            void on_packet_number_space_discarded(status::PacketNumberSpace space) {
                switch (space) {
                    default:
                        return;
                    case status::PacketNumberSpace::initial:
                        initial.reset();
                        return;
                    case status::PacketNumberSpace::handshake:
                        handshake.reset();
                        return;
                }
            }

            void delete_onertt_under(packetnum::Value pn) {
                app.delete_under(pn);
            }

            IOResult send(frame::fwriter& w, status::PacketNumberSpace space, const status::InternalConfig& config, packetnum::Value& largest_ack) {
                switch (space) {
                    default:
                        return IOResult::fatal;
                    case status::PacketNumberSpace::initial:
                        return initial.send(w, buffer, nullptr, config, largest_ack);
                    case status::PacketNumberSpace::handshake:
                        return handshake.send(w, buffer, nullptr, config, largest_ack);
                    case status::PacketNumberSpace::application: {
                        if (!should_send_app_ack(config)) {
                            return IOResult::no_data;
                        }
                        time::utime_t delay = 0;
                        if (!ack_delay.not_working()) {
                            delay = config.clock.now() - last_recv;
                        }
                        auto res = app.send(w, buffer, &delay, config, largest_ack);
                        if (res == IOResult::ok) {
                            on_ack_sent();
                        }
                        return res;
                    }
                }
            }

            constexpr time::Time get_deadline() const {
                return ack_delay.get_deadline();
            }
        };

    }  // namespace fnet::quic::ack
}  // namespace futils
