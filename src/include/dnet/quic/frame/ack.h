/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// ack - ACKFrame
#pragma once
#include "base.h"
#include "../test.h"

namespace utils {
    namespace dnet::quic::frame {
        struct WireACKRange {
            varint::Value gap;
            varint::Value len;
        };

        struct ACKRange {
            std::uint64_t largest;
            std::uint64_t smallest;
        };

        struct ECNCounts {
            varint::Value ect0;
            varint::Value ect1;
            varint::Value ecn_ce;
        };

        template <template <class...> class Vec>
        struct ACKFrame : Frame {
            varint::Value largest_ack;
            varint::Value ack_delay;
            varint::Value ack_range_count;
            varint::Value first_ack_range;
            Vec<WireACKRange> ack_ranges;
            ECNCounts ecn_counts;

            constexpr FrameType get_type(bool on_cast = false) const noexcept {
                if (on_cast) {
                    return FrameType::ACK;
                }
                return type.type_detail() == FrameType::ACK_ECN
                           ? FrameType::ACK_ECN
                           : FrameType::ACK;
            }

            constexpr bool parse_ack_ranges(io::reader& r) {
                ack_ranges.clear();
                for (size_t i = 0; i < ack_range_count; i++) {
                    WireACKRange range;
                    if (!varint::read(r, range.gap) ||
                        !varint::read(r, range.len)) {
                        return false;
                    }
                    ack_ranges.push_back(range);
                }
                return true;
            }

            constexpr bool parse_ecn_counts(io::reader& r) noexcept {
                if (type.type_detail() != FrameType::ACK_ECN) {
                    return true;
                }
                return varint::read(r, ecn_counts.ecn_ce) &&
                       varint::read(r, ecn_counts.ect0) &&
                       varint::read(r, ecn_counts.ect1);
            }

            constexpr bool parse(io::reader& r) {
                return parse_check(r, FrameType::ACK) &&
                       varint::read(r, largest_ack) &&
                       varint::read(r, ack_delay) &&
                       varint::read(r, ack_range_count) &&
                       varint::read(r, first_ack_range) &&
                       parse_ack_ranges(r) &&
                       parse_ecn_counts(r);
            }

            constexpr size_t ack_ranges_len() const noexcept {
                size_t len = 0;
                for (WireACKRange range : ack_ranges) {
                    len += varint::len(range.gap);
                    len += varint::len(range.len);
                }
                return len;
            }

            constexpr size_t ecn_counts_len() const noexcept {
                if (type.type_detail() != FrameType::ACK_ECN) {
                    return 0;
                }
                return varint::len(ecn_counts.ecn_ce) +
                       varint::len(ecn_counts.ect0) +
                       varint::len(ecn_counts.ect1);
            }

            constexpr size_t len(bool = false) const {
                return type_minlen(FrameType::ACK) +
                       varint::len(largest_ack) +
                       varint::len(ack_delay) +
                       varint::len(ack_range_count) +
                       varint::len(first_ack_range) +
                       ack_ranges_len() +
                       ecn_counts_len();
            }

            constexpr bool render_ack_range(io::writer& w) const noexcept {
                if (ack_ranges.size() != ack_range_count.value) {
                    return false;
                }
                for (WireACKRange range : ack_ranges) {
                    if (!varint::write(w, range.gap) ||
                        !varint::write(w, range.len)) {
                        return false;
                    }
                }
                return true;
            }

            constexpr bool render_ecn_counts(io::writer& w) const noexcept {
                if (type.type_detail() != FrameType::ACK_ECN) {
                    return true;
                }
                return varint::write(w, ecn_counts.ecn_ce) &&
                       varint::write(w, ecn_counts.ect0) &&
                       varint::write(w, ecn_counts.ect1);
            }

            constexpr bool render(io::writer& w) const noexcept {
                return type_minwrite(w, get_type()) &&
                       varint::write(w, largest_ack) &&
                       varint::write(w, ack_delay) &&
                       varint::write(w, ack_range_count) &&
                       varint::write(w, first_ack_range) &&
                       render_ack_range(w) &&
                       render_ecn_counts(w);
            }
        };

        constexpr bool is_sorted_ack_ranges(auto&& ranges) {
            ACKRange prev;
            bool first = true;
            for (ACKRange range : ranges) {
                if (range.largest < range.smallest) {
                    return false;
                }
                if (first) {
                    prev = range;
                    first = false;
                    continue;
                }
                if (prev.smallest <= range.smallest ||
                    prev.largest <= range.largest) {
                    return false;
                }
                prev = range;
            }
            if (first) {
                return false;  // least one required
            }
            return true;
        }

        // render ack directly from ACKRange
        template <template <class...> class Vec>
        constexpr bool render_ack_direct(io::writer& w, Vec<ACKRange>& ranges, std::uint64_t ack_delay, ECNCounts ecn = {}) {
            if (!is_sorted_ack_ranges(ranges)) {
                return false;
            }
            const bool has_ecn = ecn.ect0 > 0 || ecn.ect1 > 0 || ecn.ecn_ce > 0;
            if (!varint::write(w, has_ecn ? std::uint64_t(FrameType::ACK_ECN) : std::uint64_t(FrameType::ACK))) {
                return false;
            }
            bool first = true;
            ACKRange prev{};
            for (ACKRange range : ranges) {
                if (first) {
                    if (!varint::write(w, range.largest) ||                    // largest_ack
                        !varint::write(w, ack_delay) ||                        // ack_delay
                        !varint::write(w, ranges.size() - 1) ||                // ack_range_count
                        !varint::write(w, ranges.largest - range.smallest)) {  // first_ack_range
                        return false;
                    }
                    first = false;
                }
                else {
                    const auto gap = prev.smallest - range.largest - 2;
                    const auto len = range.largest - range.smallest;
                    if (!varint::write(w, gap) ||  // gap
                        !varint::write(w, len)) {  // ack_range
                        return false;
                    }
                }
                prev = range;
            }
            if (has_ecn) {
                if (!varint::write(w, ecn.ecn_ce) ||
                    !varint::write(w, ecn.ect0) ||
                    !varint::write(w, ecn.ect1)) {
                    return false;
                }
            }
            return true;
        }

        template <template <class...> class Vec>
        constexpr std::pair<ACKFrame<Vec>, bool> convert_to_ACKFrame(const Vec<ACKRange>& ranges) {
            if (!is_sorted_ack_ranges(ranges)) {
                return {{}, false};
            }
            ACKFrame<Vec> frame;
            frame.type = FrameType::ACK;
            size_t i = 0;
            ACKRange prev{};
            for (ACKRange range : ranges) {
                if (i == 0) {
                    frame.largest_ack = range.largest;
                    frame.first_ack_range = range.largest - range.smallest;
                    prev = range;
                }
                else {
                    // see RFC 9000 19.3.1 ACK Ranges
                    // https://tex2e.github.io/rfc-translater/html/rfc9000.html#19-3-1--ACK-Ranges
                    auto wire_range = WireACKRange{.gap = prev.smallest - range.largest - 2,
                                                   .len = range.largest - range.smallest};
                    frame.ack_ranges.push_back(wire_range);
                    prev = range;
                }
                i++;
            }
            frame.ack_range_count = i - 1;
            return {std::move(frame), true};
        }

        template <template <class...> class Vec>
        constexpr std::pair<Vec<ACKRange>, bool> convert_from_ACKFrame(const ACKFrame<Vec>& frame) {
            Vec<ACKRange> ranges;
            auto prev = ACKRange{
                .largest = frame.largest_ack.value,
                .smallest = frame.largest_ack.value - frame.first_ack_range.value,
            };
            ranges.push_back(prev);
            for (WireACKRange wire : frame.ack_ranges) {
                ACKRange range;
                // see RFC 9000 19.3.1 ACK Ranges
                // https://tex2e.github.io/rfc-translater/html/rfc9000.html#19-3-1--ACK-Ranges
                range.largest = prev.smallest - wire.gap - 2;
                range.smallest = range.largest - wire.len;
                ranges.push_back(range);
                prev = range;
            }
            bool ok = is_sorted_ack_ranges(ranges);
            return {std::move(ranges), ok};
        }

        constexpr std::uint64_t encode_ack_delay(std::uint64_t base, std::uint64_t exponent) {
            return base / (1 << exponent);
        }

        constexpr std::uint64_t decode_ack_delay(std::uint64_t base, std::uint64_t exponent) {
            return base * (1 << exponent);
        }

        namespace test {

            constexpr bool check_ack() {
                quic::test::FixedTestVec<ACKRange> range;
                ACKRange cmp[] = {
                    ACKRange{.largest = 92339, .smallest = 92333},
                    ACKRange{.largest = 32322, .smallest = 32321},
                    ACKRange{.largest = 32232, .smallest = 32231},
                };
                for (auto c : cmp) {
                    range.push_back(c);
                }
                ACKFrame<quic::test::FixedTestVec> frame;
                bool ok;
                std::tie(frame, ok) = convert_to_ACKFrame(range);
                if (!ok) {
                    return false;
                }
                frame.ack_delay = 21;
                ok = do_test(frame, [&] {
                    return frame.type.type_detail() == FrameType::ACK &&
                           frame.largest_ack == 92339 &&
                           frame.ack_delay == 21 &&
                           frame.largest_ack - frame.first_ack_range == 92333 &&
                           frame.ack_range_count == 2;
                });
                if (!ok) {
                    return false;
                }
                std::tie(range, ok) = convert_from_ACKFrame(frame);
                if (!ok) {
                    return false;
                }
                ok = [&] {
                    return range.size() == 3 &&
                           range[0].largest == cmp[0].largest &&
                           range[0].smallest == cmp[0].smallest &&
                           range[1].largest == cmp[1].largest &&
                           range[1].smallest == cmp[1].smallest &&
                           range[2].largest == cmp[2].largest &&
                           range[2].smallest == cmp[2].smallest;
                }();
                if (!ok) {
                    return false;
                }
                frame.ecn_counts.ecn_ce = 92;
                frame.ecn_counts.ect0 = 9;
                frame.ecn_counts.ect1 = 1232;
                frame.type = FrameType::ACK_ECN;
                ok = do_test(frame, [&] {
                    return frame.type.type_detail() == FrameType::ACK_ECN &&
                           frame.ecn_counts.ecn_ce == 92 &&
                           frame.ecn_counts.ect0 == 9 &&
                           frame.ecn_counts.ect1 == 1232;
                });
                return ok;
            }
        }  // namespace test

    }  // namespace dnet::quic::frame
}  // namespace utils
