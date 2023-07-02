/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// rtp - Real-Time Transport Protocol
#pragma once
#include <cstdint>
#include "../core/byte.h"
#include "../view/span.h"
#include "../view/iovec.h"
#include "../binary/number.h"
#include "../binary/flags.h"

namespace utils {
    namespace fnet::rtp {
        struct RTPFlags {
           private:
            binary::flags_t<std::uint16_t, 2, 1, 1, 4, 1, 7> flags;

           public:
            bits_flag_alias_method(flags, 0, version);
            bits_flag_alias_method(flags, 1, padding);
            bits_flag_alias_method(flags, 2, extension);
            bits_flag_alias_method(flags, 3, csrc_count);
            bits_flag_alias_method(flags, 4, marker);
            bits_flag_alias_method(flags, 5, payload_type);

            constexpr bool parse(binary::reader& r) {
                return binary::read_num(r, flags.as_value());
            }

            constexpr bool render(binary::writer& w) const {
                return binary::write_num(w, flags.as_value());
            }
        };

        namespace test {
            constexpr bool check_rtp_flags() {
                RTPFlags flags;
                if (!flags.set_version(2) ||
                    !flags.set_csrc_count(12)) {
                    return false;
                }
                flags.set_extension(true);
                return flags.version() == 2 &&
                       flags.csrc_count() == 12 &&
                       flags.extension();
            }

            static_assert(check_rtp_flags());
        }  // namespace test

        struct SequenceNumber {
            std::uint16_t seq = 0;

            constexpr bool parse(binary::reader& r) {
                return binary::read_num(r, seq);
            }

            constexpr bool render(binary::writer& w) const {
                return binary::write_num(w, seq);
            }
        };

        struct TimeStamp {
            std::uint32_t stamp = 0;

            constexpr bool parse(binary::reader& r) {
                return binary::read_num(r, stamp);
            }

            constexpr bool render(binary::writer& w) const {
                return binary::write_num(w, stamp);
            }
        };

        struct RTPPacket {
            RTPFlags flags;
            SequenceNumber sequence_number;
            TimeStamp time_stamp;
            std::uint32_t ssrc = 0;
            view::rspan<std::uint32_t> csrc;
            std::uint16_t profile_defined = 0;
            std::uint16_t ext_length = 0;
            view::rspan<std::uint32_t> extensions;
            view::rvec payload;

            constexpr bool parse(binary::reader& r, auto&& csrcbuf, auto&& exts_buf) {
                if (!flags.parse(r) ||
                    !sequence_number.parse(r) ||
                    !time_stamp.parse(r) ||
                    !binary::read_num(r, ssrc)) {
                    return false;
                }
                csrcbuf.resize(flags.csrc_count());
                csrc = csrcbuf;
                if (csrc.size() != flags.csrc_count()) {
                    return false;
                }
                for (auto& s : csrc) {
                    if (!binary::read_num(r, s)) {
                        return false;
                    }
                }
                if (flags.extension()) {
                    if (!binary::read_num(r, profile_defined) ||
                        !binary::read_num(r, ext_length)) {
                        return false;
                    }
                    exts_buf.resize(ext_length);
                    extensions = exts_buf;
                    if (extensions.size() != ext_length) {
                        return false;
                    }
                    for (auto& ext : extensions) {
                        if (!binary::read_num(r, ext)) {
                            return false;
                        }
                    }
                }
                payload = r.remain();
                r.offset(r.remain().size());
                return true;
            }

            constexpr bool render_header(binary::writer& w) const {
                auto flag_copy = flags;
                if (!flag_copy.set_version(2)) {
                    return false;  // BUG!!
                }
                if (!flag_copy.set_csrc_count(csrc.size())) {
                    return false;  // too large
                }
                if (!flag_copy.render(w) ||
                    !sequence_number.render(w) ||
                    !time_stamp.render(w) ||
                    !binary::write_num(w, ssrc)) {
                    return false;
                }
                for (auto& s : csrc) {
                    if (!binary::write_num(w, s)) {
                        return false;
                    }
                }
                if (flag_copy.extension()) {
                    if (extensions.size() > 0xffff) {
                        return false;
                    }
                    if (!binary::write_num(w, profile_defined) ||
                        !binary::write_num(w, std::uint16_t(extensions.size()))) {
                        return false;
                    }
                    for (auto& ext : extensions) {
                        if (!binary::write_num(w, ext)) {
                            return false;
                        }
                    }
                }
                return true;
            }

            constexpr bool render(binary::writer& w) const {
                return render_header(w) &&
                       w.write(payload);
            }
        };

        struct RTCPFlags {
           private:
            binary::flags_t<std::uint16_t, 2, 1, 5, 8> flags;

           public:
            bits_flag_alias_method(flags, 0, version);
            bits_flag_alias_method(flags, 1, padding);
            bits_flag_alias_method(flags, 2, report_count);
            bits_flag_alias_method(flags, 3, packet_type);

            constexpr bool parse(binary::reader& r) {
                return binary::read_num(r, flags);
            }

            constexpr bool render(binary::writer& w) const {
                return binary::write_num(w, flags);
            }
        };

        struct RTCPPacket {
            RTCPFlags flags;
            std::uint16_t length = 0;
            view::rvec payload;

            constexpr bool parse(binary::reader& r) {
                return flags.parse(r) &&
                       binary::read_num(r, length) &&
                       (payload = r.remain(), true);
            }

            constexpr bool render_header(binary::writer& w) const {
                return flags.render(w) &&
                       binary::write_num(w, length);
            }

            constexpr bool render(binary::writer& w) const {
                return render_header(w) &&
                       w.write(payload);
            }
        };
    }  // namespace fnet::rtp
}  // namespace utils
